#!/usr/bin/env python3
"""
Konturen → Wände: erzeugt Polygon-Wände (Polywand) aus geschlossenen 2D-Konturen.

Magic-Wand-Logik als Pipeline (Vorbild: dwg_to_ceilings):
  1. Konturen sammeln — live aus Archicad (Ebene in Grundriss/Arbeitsblatt,
     aktuelle SELEKTION, oder DXF-Datei)
  2. Klassifizieren — dünne, längliche Polygone = Wand-Umrisse
     (geschätzte Dicke = 2*Fläche/Umfang); große Flächen = Räume → übersprungen
  3. Erzeugen — ELM_SAB.CreatePolygonWalls (Add-On ≥ v0.3)

Beispiele:
  # Vorschau (nichts wird erzeugt):
  python3 konturen_zu_waende.py --layer "SAB_BSN_Nagel_BUNTSCHRAFFUR" --dry-run

  # Aus Arbeitsblatt, Wände 3.20 m hoch im EG erzeugen:
  python3 konturen_zu_waende.py --layer "2MWTRAG_0" --worksheet "BSN_NAGEL 00" \
      --height 3.2 --floor 0 --yes

  # SELEKTIONS-MODUS (Markierungsrahmen + Cmd+A in Archicad, dann):
  # verarbeitet alle SAB-Wand-Ebenen, Geschoss + Höhe automatisch,
  # Ziel-Ebene = Quell-Ebene, Duplikat-Schutz via Registry:
  python3 konturen_zu_waende.py --selektion --dry-run
  python3 konturen_zu_waende.py --selektion --yes

  # Aus DXF-Datei (Linien werden zu Polygonen verkettet):
  python3 konturen_zu_waende.py --dxf plan.dxf --layer "A_01_TRAGWAND" --yes
"""
import argparse, json, math, os, sys, urllib.request
from collections import defaultdict

# SAB-Konvention: nur diese Ebenen tragen Wand-Konturen (Selektions-Modus).
WALL_LAYERS = {"A_01_TRAGWAND", "A_02_LEICHTWAND", "A_021_TRENNWAND"}


# ---------------------------------------------------------------- Archicad-API

def call(port, cmd, params=None, timeout=300):
    body = {"command": cmd}
    if params is not None:
        body["parameters"] = params
    req = urllib.request.Request(f"http://localhost:{port}/", json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=timeout).read())


def addon(port, ns, cmd, params=None):
    r = call(port, "API.ExecuteAddOnCommand",
             {"addOnCommandId": {"commandNamespace": ns, "commandName": cmd},
              "addOnCommandParameters": params or {}})
    if not r.get("succeeded"):
        raise RuntimeError(f"{ns}.{cmd}: {json.dumps(r.get('error'))[:200]}")
    return r["result"]["addOnCommandResponse"]


def tapir(port, cmd, params=None):
    return addon(port, "TapirCommand", cmd, params)


def find_port():
    for p in range(19723, 19744):
        try:
            r = call(p, "API.GetProductInfo", timeout=2)
            if r.get("succeeded"):
                return p
        except Exception:
            pass
    sys.exit("Kein laufendes Archicad gefunden (Ports 19723-19743).")


# --------------------------------------------------------- Geometrie-Helfer

def arc_points(cx, cy, r, a0, a1, ratio=1.0, rot=0.0, reflected=False, n_per_rad=8):
    if reflected:
        a0, a1 = a1, a0
    while a1 < a0:
        a1 += 2 * math.pi
    steps = max(2, int((a1 - a0) * n_per_rad))
    pts = []
    for i in range(steps + 1):
        t = a0 + (a1 - a0) * i / steps
        ex, ey = r * math.cos(t), r * ratio * math.sin(t)
        pts.append((cx + ex * math.cos(rot) - ey * math.sin(rot),
                    cy + ex * math.sin(rot) + ey * math.cos(rot)))
    return pts


def flatten_poly_arcs(coords, arcs):
    """Polyline-Bogensegmente (arcAngle zwischen begIndex/endIndex) plätten."""
    if not arcs:
        return coords
    arc_by_beg = {a["begIndex"]: a["arcAngle"] for a in arcs}
    out = [coords[0]]
    for i in range(1, len(coords)):
        ang = arc_by_beg.get(i)     # 1-basierte Indizes aus dem Add-On
        x1, y1 = coords[i - 1]
        x2, y2 = coords[i]
        if not ang:
            out.append((x2, y2))
            continue
        # Kreisbogen aus Sehne + Winkel rekonstruieren
        chord = math.hypot(x2 - x1, y2 - y1)
        if chord < 1e-9:
            out.append((x2, y2))
            continue
        r = chord / (2 * math.sin(abs(ang) / 2))
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        h = r * math.cos(abs(ang) / 2)
        nx, ny = -(y2 - y1) / chord, (x2 - x1) / chord
        sign = 1.0 if ang > 0 else -1.0
        cx, cy = mx - sign * h * nx, my - sign * h * ny
        a0 = math.atan2(y1 - cy, x1 - cx)
        steps = max(2, int(abs(ang) * 8))
        for k in range(1, steps + 1):
            t = a0 + ang * k / steps
            out.append((cx + r * math.cos(t), cy + r * math.sin(t)))
    return out


def geometry_to_linestring(g):
    """Eine Get2DGeometryOfElements-Antwort → shapely LineString (oder None)."""
    from shapely.geometry import LineString
    t = g.get("elementType")
    if t == "Line":
        b, e = g["begCoordinate"], g["endCoordinate"]
        if (b["x"], b["y"]) != (e["x"], e["y"]):
            return LineString([(b["x"], b["y"]), (e["x"], e["y"])])
    elif t in ("Arc", "Circle"):
        o = g["origin"]
        a0, a1 = (0.0, 2 * math.pi) if g.get("whole") else (g["begAngle"], g["endAngle"])
        return LineString(arc_points(o["x"], o["y"], g["radius"], a0, a1,
                                     g.get("ratio", 1.0), g.get("angle", 0.0),
                                     g.get("reflected", False)))
    elif t == "PolyLine":
        coords = [(c["x"], c["y"]) for c in g.get("coordinates", [])]
        if len(coords) >= 2:
            return LineString(flatten_poly_arcs(coords, g.get("arcs", [])))
    return None


def read_geometry_batched(port, guids):
    """ELM_SAB.Get2DGeometryOfElements in 500er-Batches; liefert erfolgreiche Einträge."""
    out = []
    for i in range(0, len(guids), 500):
        batch = [{"elementId": {"guid": g}} for g in guids[i:i + 500]]
        resp = addon(port, "ELM_SAB", "Get2DGeometryOfElements", {"elements": batch})
        out.extend(g for g in resp.get("geometryOfElements", []) if g.get("success"))
        done = min(i + 500, len(guids))
        if len(guids) > 5000 and done % 5000 < 500:
            print(f"  ... {done}/{len(guids)} gelesen")
    return out


# ------------------------------------------------------------- Konturen holen

def layer_index_by_name(port, name):
    att = tapir(port, "GetAttributesByType", {"attributeType": "Layer"})
    for a in att.get("attributes", []):
        if a["name"] == name:
            return int(a["index"])
    sys.exit(f"Ebene '{name}' nicht gefunden.")


def layer_names_by_index(port):
    att = tapir(port, "GetAttributesByType", {"attributeType": "Layer"})
    return {int(a["index"]): a["name"] for a in att.get("attributes", [])}


def worksheet_db(port, name_part):
    pm = call(port, "API.GetNavigatorItemTree", {"navigatorTreeId": {"type": "ProjectMap"}})
    root = pm["result"]["navigatorTree"]["rootItem"]

    def walk(n):
        ni = n.get("navigatorItem", n)
        yield ni
        for c in ni.get("children", []):
            yield from walk(c)

    for ni in walk(root):
        label = (ni.get("prefix", "") + " " + ni.get("name", "")).strip()
        if ni.get("type") == "WorksheetItem" and name_part.lower() in label.lower():
            db = tapir(port, "GetDatabaseIdFromNavigatorItemId",
                       {"navigatorItemIds": [{"navigatorItemId": ni["navigatorItemId"]}]})
            return db["databases"][0]["databaseId"], label
    sys.exit(f"Arbeitsblatt mit '{name_part}' nicht gefunden.")


def polygons_live(port, layer_idx, db=None):
    """Alle 2D-Elemente (Line/Arc/Circle/PolyLine) einer Ebene lesen und verketten."""
    from shapely.ops import polygonize, unary_union

    segs = []
    for etype in ("Line", "Arc", "Circle", "PolyLine"):
        params = {"elementType": etype}
        if db:
            params["databases"] = [{"databaseId": db}]
        guids = [e["elementId"]["guid"] for e in tapir(port, "GetElementsByType", params).get("elements", [])]
        for g in read_geometry_batched(port, guids):
            if g.get("layerIndex") != layer_idx:
                continue
            seg = geometry_to_linestring(g)
            if seg is not None:
                segs.append(seg)
    print(f"  2D-Segmente gelesen: {len(segs)} — polygonisiere...")
    merged = unary_union(segs)
    return [list(p.exterior.coords)[:-1] for p in polygonize(merged)]


def polygons_from_dxf(path, layer_name):
    """Linien/Bögen/Polylinien einer DXF-Ebene per shapely zu Polygonen verketten."""
    import ezdxf
    from shapely.ops import polygonize, unary_union
    from shapely.geometry import LineString
    doc = ezdxf.readfile(path)
    segs = []

    def harvest(space):
        for e in space:
            if e.dxf.get("layer") != layer_name:
                continue
            t = e.dxftype()
            if t == "LINE":
                segs.append(LineString([(e.dxf.start.x, e.dxf.start.y), (e.dxf.end.x, e.dxf.end.y)]))
            elif t == "LWPOLYLINE":
                pts = [(p[0], p[1]) for p in e.get_points()]
                if e.closed:
                    pts.append(pts[0])
                if len(pts) >= 2:
                    segs.append(LineString(pts))
            elif t in ("ARC", "CIRCLE"):
                segs.append(LineString(list(e.flattening(0.01))))

    harvest(doc.modelspace())
    for block in doc.blocks:
        harvest(block)
    merged = unary_union(segs)
    return [list(p.exterior.coords)[:-1] for p in polygonize(merged)]


# ------------------------------------------------------------ Klassifizierung

def area_perimeter(pts):
    a = 0.0
    per = 0.0
    n = len(pts)
    for i in range(n):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % n]
        a += x1 * y2 - x2 * y1
        per += ((x2 - x1) ** 2 + (y2 - y1) ** 2) ** 0.5
    return abs(a) / 2.0, per


def classify(polys, min_d, max_d, min_area):
    walls, rooms, tiny = [], [], []
    for pts in polys:
        a, p = area_perimeter(pts)
        if a < min_area:
            tiny.append((pts, a))
            continue
        d = 2 * a / p if p > 0 else 0     # Dicken-Schätzung für dünne Streifen
        (walls if min_d <= d <= max_d else rooms).append((pts, a, d))
    return walls, rooms, tiny


# ---------------------------------------- Duplikat-Registry (Selektions-Modus)

def poly_sig(pts):
    """Rotations-invariante Signatur eines Polygons (gerundet auf mm)."""
    r = [(round(x, 3), round(y, 3)) for x, y in pts]
    k = r.index(min(r))
    return tuple(r[k:] + r[:k])


def registry_load(path):
    """Registry: { geschoss(str): [ [[x,y],...], ... ] } bereits erzeugter Wände.
    Geschoss-bewusst — übereinanderliegende Geschosse haben oft identische
    XY-Konturen, die dürfen NICHT als Duplikat gelten (live erlebt 2026-07-14)."""
    if not path or not os.path.exists(path):
        return {}
    return json.load(open(path))


def registry_sigs(reg, floor):
    sigs = set()
    for pts in reg.get(str(floor), []):
        p = [tuple(pt) for pt in pts]
        sigs.add(poly_sig(p))
        sigs.add(poly_sig(list(reversed(p))))
    return sigs


def registry_add(reg, floor, walls_pts):
    reg.setdefault(str(floor), []).extend([[list(pt) for pt in pts] for pts in walls_pts])


# --------------------------------------------------------------- Wand-Erzeugung

def create_walls(port, walls, height, floor, layer_idx, extra=None):
    """CreatePolygonWalls in 200er-Batches; liefert (erzeugte_pts, guids, fail)."""
    ok_pts, guids, fail = [], [], 0
    base = {"height": height, "layerIndex": layer_idx, **(extra or {})}
    if floor is not None:               # None = aktives Geschoss (Add-On-Default)
        base["floorIndex"] = floor
    for i in range(0, len(walls), 200):
        batch = walls[i:i + 200]
        data = [{"polygonCoordinates": [{"x": x, "y": y} for x, y in pts],
                 **base} for pts, _, _ in batch]
        resp = addon(port, "ELM_SAB", "CreatePolygonWalls", {"wallsData": data})
        # Antwort ist positionsgleich zum Request → Erfolg pro Polygon zuordenbar
        for (pts, _, _), r in zip(batch, resp.get("elements", [])):
            if "elementId" in r:
                ok_pts.append(pts)
                guids.append(r["elementId"]["guid"])
            else:
                fail += 1
    return ok_pts, guids, fail


# ------------------------------------------------------------ Selektions-Modus

def story_height(port, floor):
    """Wandhöhe = Level-Differenz zum nächsten Geschoss; oberstes: wie darunter."""
    st = tapir(port, "GetStories").get("stories", [])
    if isinstance(st, dict):
        st = st.get("stories", [])
    lvl = {s["index"]: s["level"] for s in st}
    if floor + 1 in lvl:
        return round(lvl[floor + 1] - lvl[floor], 3)
    if floor - 1 in lvl:  # oberstes Geschoss: Höhe des darunterliegenden übernehmen
        h = round(lvl[floor] - lvl[floor - 1], 3)
        print(f"  Hinweis: Geschoss {floor} ist das oberste — Höhe {h} m vom Geschoss darunter übernommen.")
        return h
    return 3.0


def run_selektion(port, args):
    """Quelle = aktuelle Selektion (Markierungsrahmen + Cmd+A in Archicad).
    Verarbeitet alle SAB-Wand-Ebenen zugleich; Geschoss + Höhe automatisch;
    Ziel-Ebene = Quell-Ebene; Duplikat-Schutz pro Geschoss via Registry."""
    from shapely.ops import polygonize, unary_union

    idx2name = layer_names_by_index(port)
    want = {args.layer} if args.layer else WALL_LAYERS
    wall_idx = {i for i, n in idx2name.items() if n in want}

    sel = tapir(port, "GetSelectedElements").get("elements", [])
    guids = [e["elementId"]["guid"] for e in sel]
    if not guids:
        sys.exit("Nichts selektiert — in Archicad Markierungsrahmen ziehen + Cmd+A drücken.")
    print(f"Selektion: {len(guids)} Elemente")

    segs_by_layer, floors = defaultdict(list), defaultdict(int)
    for g in read_geometry_batched(port, guids):
        if g.get("layerIndex") not in wall_idx:
            continue
        seg = geometry_to_linestring(g)
        if seg is not None:
            segs_by_layer[g["layerIndex"]].append(seg)
            floors[g.get("floorIndex")] += 1

    if not segs_by_layer:
        sys.exit(f"Keine Konturen auf Wand-Ebenen ({', '.join(sorted(want))}) in der Selektion.")

    floor = args.floor if args.floor is not None else max(floors, key=floors.get)
    height = args.height if args.height is not None else story_height(port, floor)
    reg = registry_load(args.registry)
    prev = registry_sigs(reg, floor)

    plan = {}
    for li, segs in segs_by_layer.items():
        polys = [list(p.exterior.coords)[:-1] for p in polygonize(unary_union(segs))]
        walls, rooms, tiny = classify(polys, args.min_dicke, args.max_dicke, args.min_flaeche)
        fresh = [w for w in walls if poly_sig(w[0]) not in prev]
        plan[li] = fresh
        print(f"{idx2name[li]}: {len(segs)} Segmente → {len(polys)} Regionen → "
              f"{len(fresh)} Wände (Raum: {len(rooms)}, Mini: {len(tiny)}, "
              f"Duplikat: {len(walls) - len(fresh)})")

    total = sum(len(w) for w in plan.values())
    print(f"Geschoss {floor} (Verteilung {dict(floors)}), Höhe {height} m, gesamt {total} Wände")
    if args.dry_run or total == 0:
        return
    if not args.yes:
        if input(f"{total} Polygon-Wände erzeugen? [ja/nein] ").strip().lower() not in ("ja", "j", "yes", "y"):
            print("Abgebrochen.")
            return

    all_guids, total_ok, total_fail = {}, 0, 0
    for li, walls in plan.items():
        if not walls:
            continue
        ok_pts, wguids, fail = create_walls(port, walls, height, floor, li)
        registry_add(reg, floor, ok_pts)
        all_guids[idx2name[li]] = wguids
        total_ok += len(ok_pts)
        total_fail += fail
        print(f"{idx2name[li]}: ✓ {len(ok_pts)}  ✗ {fail}")
    if args.registry:
        json.dump(reg, open(args.registry, "w"))
    if args.guids_out:
        json.dump(all_guids, open(args.guids_out, "w"))
        print(f"GUIDs → {args.guids_out}")
    print(f"GESAMT: ✓ {total_ok}   ✗ {total_fail}")


# ----------------------------------------------------------------------- Main

def main():
    ap = argparse.ArgumentParser(description="Geschlossene 2D-Konturen als Polygon-Wände erzeugen")
    ap.add_argument("--layer", help="Quell-Ebene der Konturen (Pflicht außer bei --selektion)")
    ap.add_argument("--selektion", action="store_true",
                    help="Quelle = aktuelle Selektion (Markierungsrahmen + Cmd+A); "
                         "alle SAB-Wand-Ebenen, Geschoss/Höhe automatisch")
    ap.add_argument("--worksheet", help="Arbeitsblatt-Name (Teilstring); sonst Grundriss/aktive DB")
    ap.add_argument("--dxf", help="Alternativ: DXF-Datei statt Live-Lesen (Linien werden verkettet)")
    ap.add_argument("--height", type=float, default=None,
                    help="Wandhöhe m (Default: Geschoss-Level-Differenz bzw. 3.0)")
    ap.add_argument("--floor", type=int, help="Ziel-Geschoss-Index (Default: aktives/erkanntes)")
    ap.add_argument("--ziel-ebene", help="Ebene für neue Wände (Name)")
    ap.add_argument("--composite", help="Mehrschicht-Aufbau (Name)")
    ap.add_argument("--registry", default="konturen_registry.json",
                    help="Duplikat-Registry-Datei pro Projekt ('' = aus)")
    ap.add_argument("--guids-out", help="GUIDs der erzeugten Wände als JSON sichern")
    ap.add_argument("--min-dicke", type=float, default=0.05)
    ap.add_argument("--max-dicke", type=float, default=0.65)
    ap.add_argument("--min-flaeche", type=float, default=0.01)
    ap.add_argument("--dry-run", action="store_true", help="nur analysieren")
    ap.add_argument("--yes", action="store_true", help="ohne Rückfrage erzeugen")
    args = ap.parse_args()

    port = find_port()

    if args.selektion:
        run_selektion(port, args)
        return

    if not args.layer:
        sys.exit("--layer ist Pflicht (außer bei --selektion).")

    # 1) Konturen sammeln
    if args.dxf:
        polys = polygons_from_dxf(args.dxf, args.layer)
        quelle = f"DXF {args.dxf}"
    else:
        db = None
        quelle = "Grundriss"
        if args.worksheet:
            db, quelle = worksheet_db(port, args.worksheet)
        layer_idx = layer_index_by_name(port, args.layer)
        polys = polygons_live(port, layer_idx, db)
    print(f"Quelle: {quelle}, Ebene '{args.layer}': {len(polys)} geschlossene Konturen")

    # 2) Klassifizieren
    walls, rooms, tiny = classify(polys, args.min_dicke, args.max_dicke, args.min_flaeche)
    print(f"  → Wand-Kandidaten: {len(walls)}  (Dicke {args.min_dicke}-{args.max_dicke} m)")
    print(f"  → übersprungen als Raum/Fläche: {len(rooms)}, Mini-Artefakte: {len(tiny)}")
    if walls:
        ds = sorted(d for _, _, d in walls)
        print(f"  Dicken: min {ds[0]:.3f} / median {ds[len(ds)//2]:.3f} / max {ds[-1]:.3f} m")
    if args.dry_run or not walls:
        return

    height = args.height if args.height is not None else 3.0
    if not args.yes:
        if input(f"{len(walls)} Polygon-Wände erzeugen (Höhe {height} m)? [ja/nein] ").strip().lower() not in ("ja", "j", "yes", "y"):
            print("Abgebrochen.")
            return

    # 3) Attribute auflösen
    extra = {}
    if args.ziel_ebene:
        extra["layerIndex"] = layer_index_by_name(port, args.ziel_ebene)
    else:
        extra["layerIndex"] = layer_index_by_name(port, args.layer)
    if args.composite:
        att = tapir(port, "GetAttributesByType", {"attributeType": "Composite"})
        for a in att.get("attributes", []):
            if a["name"] == args.composite:
                extra["compositeIndex"] = int(a["index"])
                break
        else:
            sys.exit(f"Aufbau '{args.composite}' nicht gefunden.")

    # 4) Erzeugen
    layer_idx = extra.pop("layerIndex")
    ok_pts, guids, fail = create_walls(port, walls, height, args.floor, layer_idx, extra)
    if args.guids_out:
        json.dump({args.layer: guids}, open(args.guids_out, "w"))
    print(f"✓ erzeugt: {len(ok_pts)}   ✗ fehlgeschlagen: {fail}")


if __name__ == "__main__":
    main()
