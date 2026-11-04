#!/usr/bin/env python3
"""
Decken + Deckendurchbrüche aus 2D-Konturen (Werkplan-Bestandsplan).

Pipeline (live-verifiziert 2026-07-14, THN: 21 Decken, ~300 Löcher, 5 Geschosse):
  1. Footprint pro Geschoss = Union aller geschlossenen Regionen aus Wand- UND
     Begrenzungs-Ebenen (Fassade, Tür-/Fensterlinien schließen die Öffnungslücken!)
  2. Durchbrüche = AUSSPAR_BODEN des Geschosses + AUSSPAR_DECKE des Geschosses
     darunter; Durchbruch-Kreuze werden per polygonize+union zum Rechteck bereinigt
  3. Erzeugen via Tapir CreateSlabs — Löcher direkt als `holes` (ein Arbeitsgang),
     danach SetDetailsOfElements: Ziel-Ebene + Geschoss

ABSTURZ-HÄRTUNG (Archicad crasht bei Riesen-Requests — live erlebt):
  - EINE Decke pro Request, Fortschritts-Datei nach jedem Erfolg
  - Polygone: buffer(0) → simplify(1cm) → Spike-Buffer (-2cm/+2cm gegen
    APIERR_IRREGULARPOLY) → mm-Rundung → NACH der Rundung erneut validieren
    (Rundung kann Selbstschnitte erzeugen!)
  - bei Verbindungsabbruch sofort stoppen, nie blind neu senden (Duplikat/Crash)

Aufruf:
  python3 decken_aus_konturen.py --dry-run
  python3 decken_aus_konturen.py --floors -1 0 1 --dicke 0.30 --yes
"""
import argparse, json, os, sys, time
from collections import defaultdict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "konturen_zu_waende"))
from konturen_zu_waende import tapir, addon, find_port, geometry_to_linestring, \
    layer_names_by_index, layer_index_by_name

BEGRENZUNG = {"A_01_TRAGWAND", "A_02_LEICHTWAND", "A_021_TRENNWAND",
              "A_05_FASSADE", "A_05_FASSADE_ELEMENTE",
              "A_013_TUER", "A_011_FENSTER", "A_013_TUER_FENSTER_KOMBI",
              "A_014_STUETZE"}
AUSSPAR_DECKE = {"A_22_AUSSPAR_DECKE"}
AUSSPAR_BODEN = {"A_23_AUSSPAR_BODEN"}


def safe_ring(poly, ccw=True):
    """Polygon → Punktliste für Archicad: mm-Rundung + Validitäts-Reparatur DANACH
    (Rundung kann Ring-Selbstschnitte erzeugen — live erlebt am EG-Footprint)."""
    from shapely.geometry import Polygon
    from shapely.geometry.polygon import orient
    r = Polygon([(round(x, 3), round(y, 3)) for x, y in poly.exterior.coords])
    if not r.is_valid:
        r = r.buffer(0)
        if r.geom_type == "MultiPolygon":
            r = max(r.geoms, key=lambda g: g.area)
    r = orient(r, sign=1.0 if ccw else -1.0)
    pts = list(r.exterior.coords)[:-1]
    out = [pts[0]]
    for q in pts[1:]:
        if q != out[-1]:
            out.append(q)
    return out


def spike_clean(poly, eps=0.02, min_area=20.0):
    """Nadel-Spikes/Selbstberührungen entfernen (APIERR_IRREGULARPOLY-Gegenmittel):
    negativ-positiv-Puffern; kann in Teile zerfallen → alle relevanten zurückgeben."""
    c = poly.buffer(-eps).buffer(eps).simplify(0.01, preserve_topology=True)
    parts = list(c.geoms) if c.geom_type == "MultiPolygon" else [c]
    return [p for p in parts if p.area > min_area]


def sammle_segmente(port, gruppen):
    """Alle 2D-Elemente des Grundrisses lesen (geschossübergreifend!) und
    nach (Geschoss, Gruppe) sortieren. gruppen: {layerName: gruppenKey}."""
    idx2name = layer_names_by_index(port)
    idx2grp = {i: gruppen[n] for i, n in idx2name.items() if n in gruppen}
    segs = defaultdict(list)
    for etype in ("Line", "Arc", "Circle", "PolyLine"):
        guids = [e["elementId"]["guid"] for e in
                 tapir(port, "GetElementsByType", {"elementType": etype}).get("elements", [])]
        for i in range(0, len(guids), 500):
            batch = [{"elementId": {"guid": g}} for g in guids[i:i + 500]]
            resp = addon(port, "ELM_SAB", "Get2DGeometryOfElements", {"elements": batch})
            for g in resp.get("geometryOfElements", []):
                if not g.get("success"):
                    continue
                grp = idx2grp.get(g.get("layerIndex"))
                if grp is None:
                    continue
                s = geometry_to_linestring(g)
                if s is not None:
                    segs[(g.get("floorIndex"), grp)].append(s)
    return segs


def main():
    ap = argparse.ArgumentParser(description="Decken + Durchbrüche aus 2D-Konturen")
    ap.add_argument("--floors", type=int, nargs="*", help="Geschosse (Default: alle mit Daten)")
    ap.add_argument("--dicke", type=float, default=0.30)
    ap.add_argument("--min-flaeche", type=float, default=20.0, help="kleinste Deckenteil-Fläche m²")
    ap.add_argument("--ziel-ebene", default="A_09_TRAGDECKE")
    ap.add_argument("--progress", default="decken_progress.json")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--yes", action="store_true")
    args = ap.parse_args()

    from shapely.ops import polygonize, unary_union
    from shapely.geometry import Polygon

    port = find_port()
    st = tapir(port, "GetStories").get("stories", [])
    if isinstance(st, dict):
        st = st.get("stories", [])
    levels = {s["index"]: s["level"] for s in st}

    gruppen = {n: "rand" for n in BEGRENZUNG}
    gruppen.update({n: "decke" for n in AUSSPAR_DECKE})
    gruppen.update({n: "boden" for n in AUSSPAR_BODEN})
    print("Lese 2D-Geometrie (alle Geschosse)...")
    segs = sammle_segmente(port, gruppen)

    floors = args.floors or sorted({f for f, g in segs if g == "rand"})

    def regionen(sl):
        return [Polygon(p.exterior) for p in polygonize(unary_union(sl))]

    def loecher(sl):
        if not sl:
            return []
        u = unary_union([r for r in regionen(sl) if 0.005 < r.area < 50.0])
        hs = list(u.geoms) if u.geom_type == "MultiPolygon" else ([u] if not u.is_empty else [])
        return [Polygon(h.exterior) for h in hs]

    plan = []
    for f in floors:
        regs = [r for r in regionen(segs.get((f, "rand"), [])) if r.area > 0.01]
        if not regs or f not in levels:
            continue
        fp = unary_union(regs)
        parts = list(fp.geoms) if fp.geom_type == "MultiPolygon" else [fp]
        hs = loecher(segs.get((f, "boden"), [])) + loecher(segs.get((f - 1, "decke"), []))
        n_holes = 0
        for p in parts:
            if p.area < args.min_flaeche:
                continue
            for cp in spike_clean(p, min_area=args.min_flaeche):
                inner = cp.buffer(-0.02)
                ph = [h for h in hs if h.within(inner)]
                n_holes += len(ph)
                plan.append({"floor": f, "poly": cp, "holes": ph})
        print(f"Geschoss {f}: {sum(1 for x in plan if x['floor'] == f)} Deckenteile, "
              f"{n_holes} Löcher, Level {levels[f]}")

    print(f"GESAMT: {len(plan)} Decken, {sum(len(x['holes']) for x in plan)} Löcher, "
          f"Dicke {args.dicke} m, Ebene {args.ziel_ebene}")
    if args.dry_run or not plan:
        return
    if not args.yes and input("Erzeugen? [ja/nein] ").strip().lower() not in ("ja", "j", "yes", "y"):
        return

    layer_idx = layer_index_by_name(port, args.ziel_ebene)
    done = json.load(open(args.progress)) if os.path.exists(args.progress) else []
    for n, x in enumerate(plan, 1):
        if n <= len(done):        # Resume nach Abbruch
            continue
        slab = {"level": levels[x["floor"]], "thickness": args.dicke,
                "referencePlaneLocation": "Top",
                "polygonCoordinates": [{"x": a, "y": b} for a, b in safe_ring(x["poly"])]}
        if x["holes"]:
            slab["holes"] = [{"polygonOutline": [{"x": a, "y": b} for a, b in safe_ring(h, ccw=False)]}
                             for h in x["holes"]]
        try:
            resp = tapir(port, "CreateSlabs", {"slabsData": [slab]})
        except Exception as e:
            sys.exit(f"✗ ABBRUCH bei {n}/{len(plan)} (Archicad prüfen!): {e}")
        els = resp.get("elements", [])
        if els and "elementId" in els[0]:
            guid = els[0]["elementId"]["guid"]
            tapir(port, "SetDetailsOfElements", {"elementsWithDetails": [
                {"elementId": {"guid": guid},
                 "details": {"floorIndex": x["floor"], "layerIndex": layer_idx}}]})
            done.append({"floor": x["floor"], "flaeche": round(x["poly"].area, 1),
                         "holes": len(x["holes"]), "guid": guid})
            json.dump(done, open(args.progress, "w"))
            print(f"✓ {n}/{len(plan)} Geschoss {x['floor']}: "
                  f"{round(x['poly'].area, 1)} m², {len(x['holes'])} Löcher")
        else:
            print(f"✗ {n}/{len(plan)} Geschoss {x['floor']}: {json.dumps(els)[:120]}")
        time.sleep(0.3)
    print(f"FERTIG: {len(done)} Decken (GUIDs in {args.progress})")


if __name__ == "__main__":
    main()
