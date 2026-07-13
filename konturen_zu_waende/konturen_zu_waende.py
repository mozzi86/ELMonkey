#!/usr/bin/env python3
"""
Konturen → Wände: erzeugt Polygon-Wände (Polywand) aus geschlossenen 2D-Konturen.

Magic-Wand-Logik als Pipeline (Vorbild: dwg_to_ceilings):
  1. Konturen sammeln — live aus Archicad (geschlossene Polylinien einer Ebene)
     und/oder aus einer DXF-Datei (Linien-Suppe wird per shapely polygonisiert)
  2. Klassifizieren — dünne, längliche Polygone = Wand-Umrisse
     (geschätzte Dicke = 2*Fläche/Umfang); große Flächen = Räume → übersprungen
  3. Erzeugen — ELM_SAB.CreatePolygonWalls (Add-On ≥ v0.3)

Beispiele:
  # Vorschau (nichts wird erzeugt):
  python3 konturen_zu_waende.py --layer "SAB_BSN_Nagel_BUNTSCHRAFFUR" --dry-run

  # Aus Arbeitsblatt, Wände 3.20 m hoch im EG erzeugen:
  python3 konturen_zu_waende.py --layer "2MWTRAG_0" --worksheet "BSN_NAGEL 00" \
      --height 3.2 --floor 0 --yes

  # Aus DXF-Datei (Linien werden zu Polygonen verkettet):
  python3 konturen_zu_waende.py --dxf plan.dxf --layer "A_01_TRAGWAND" --yes
"""
import argparse, json, sys, urllib.request
from collections import defaultdict


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


# ------------------------------------------------------------- Konturen holen

def layer_index_by_name(port, name):
    att = tapir(port, "GetAttributesByType", {"attributeType": "Layer"})
    for a in att.get("attributes", []):
        if a["name"] == name:
            return int(a["index"])
    sys.exit(f"Ebene '{name}' nicht gefunden.")


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
    """Alle 2D-Elemente (Line/Arc/Circle/PolyLine) einer Ebene via
    ELM_SAB.Get2DGeometryOfElements lesen und per shapely zu Regionen verketten."""
    import math
    from shapely.ops import polygonize, unary_union
    from shapely.geometry import LineString

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

    segs = []
    for etype in ("Line", "Arc", "Circle", "PolyLine"):
        params = {"elementType": etype}
        if db:
            params["databases"] = [{"databaseId": db}]
        guids = [e["elementId"]["guid"] for e in tapir(port, "GetElementsByType", params).get("elements", [])]
        for i in range(0, len(guids), 500):
            batch = [{"elementId": {"guid": g}} for g in guids[i:i + 500]]
            resp = addon(port, "ELM_SAB", "Get2DGeometryOfElements", {"elements": batch})
            for g in resp.get("geometryOfElements", []):
                if not g.get("success") or g.get("layerIndex") != layer_idx:
                    continue
                t = g.get("elementType")
                if t == "Line":
                    b, e = g["begCoordinate"], g["endCoordinate"]
                    if (b["x"], b["y"]) != (e["x"], e["y"]):
                        segs.append(LineString([(b["x"], b["y"]), (e["x"], e["y"])]))
                elif t in ("Arc", "Circle"):
                    o = g["origin"]
                    a0, a1 = (0.0, 2 * math.pi) if g.get("whole") else (g["begAngle"], g["endAngle"])
                    pts = arc_points(o["x"], o["y"], g["radius"], a0, a1,
                                     g.get("ratio", 1.0), g.get("angle", 0.0), g.get("reflected", False))
                    segs.append(LineString(pts))
                elif t == "PolyLine":
                    coords = [(c["x"], c["y"]) for c in g.get("coordinates", [])]
                    if len(coords) >= 2:
                        segs.append(LineString(flatten_poly_arcs(coords, g.get("arcs", []))))
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


# ----------------------------------------------------------------------- Main

def main():
    ap = argparse.ArgumentParser(description="Geschlossene 2D-Konturen als Polygon-Wände erzeugen")
    ap.add_argument("--layer", required=True, help="Quell-Ebene der Konturen")
    ap.add_argument("--worksheet", help="Arbeitsblatt-Name (Teilstring); sonst Grundriss/aktive DB")
    ap.add_argument("--dxf", help="Alternativ: DXF-Datei statt Live-Lesen (Linien werden verkettet)")
    ap.add_argument("--height", type=float, default=3.0, help="Wandhöhe m (Default 3.0)")
    ap.add_argument("--floor", type=int, help="Ziel-Geschoss-Index (Default: aktives)")
    ap.add_argument("--ziel-ebene", help="Ebene für neue Wände (Name)")
    ap.add_argument("--composite", help="Mehrschicht-Aufbau (Name)")
    ap.add_argument("--min-dicke", type=float, default=0.05)
    ap.add_argument("--max-dicke", type=float, default=0.65)
    ap.add_argument("--min-flaeche", type=float, default=0.01)
    ap.add_argument("--dry-run", action="store_true", help="nur analysieren")
    ap.add_argument("--yes", action="store_true", help="ohne Rückfrage erzeugen")
    args = ap.parse_args()

    port = find_port()

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

    if not args.yes:
        if input(f"{len(walls)} Polygon-Wände erzeugen (Höhe {args.height} m)? [ja/nein] ").strip().lower() not in ("ja", "j", "yes", "y"):
            print("Abgebrochen.")
            return

    # 3) Attribute auflösen
    extra = {}
    if args.floor is not None:
        extra["floorIndex"] = args.floor
    if args.ziel_ebene:
        extra["layerIndex"] = layer_index_by_name(port, args.ziel_ebene)
    if args.composite:
        att = tapir(port, "GetAttributesByType", {"attributeType": "Composite"})
        for a in att.get("attributes", []):
            if a["name"] == args.composite:
                extra["compositeIndex"] = int(a["index"])
                break
        else:
            sys.exit(f"Aufbau '{args.composite}' nicht gefunden.")

    # 4) Erzeugen
    ok, fail = 0, 0
    for i in range(0, len(walls), 200):
        batch = walls[i:i + 200]
        data = [{"polygonCoordinates": [{"x": x, "y": y} for x, y in pts],
                 "height": args.height, **extra} for pts, _, _ in batch]
        resp = addon(port, "ELM_SAB", "CreatePolygonWalls", {"wallsData": data})
        for r in resp.get("elements", []):
            if "elementId" in r:
                ok += 1
            else:
                fail += 1
    print(f"✓ erzeugt: {ok}   ✗ fehlgeschlagen: {fail}")


if __name__ == "__main__":
    main()
