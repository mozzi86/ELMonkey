#!/usr/bin/env python3
"""
Polygonize v4 — pro Layer eigene Snap-Toleranz, zweistufig:

  Stage 1: SNAP_TIGHT (0.10 m) -> saubere Polygone
  Stage 2: SNAP_LOOSE pro Layer auf den UEBRIG GEBLIEBENEN Linien,
           die nicht in Stage-1-Polygonen liegen.

So bleibt die Geometrie an aufgeraeumten Stellen exakt und nur an offenen
Stellen werden Luecken mit groesserer Toleranz geschlossen.
"""
import sys, math, pickle, time
from collections import defaultdict
import ezdxf
import shapely
from shapely.geometry import LineString, Polygon, MultiLineString, MultiPolygon, Point
from shapely.ops import polygonize, unary_union

LAYER_MAP = {
    "Strasse": [
        "road_site", "Cement_Street", "Zaunstrasse_BUPOL", "A_01_Strasse_neu",
        "60 Straße", "parking area 305 - lighting", "parking area 325 - cobble stone",
        "parking area 325 - default", "pa 325 - concrete joints", "Apron#",
    ],
    "Bordstein": ["sidewalks", "BORDSTEIN", "BETON", "RINNE", "Concrete_area", "BASE"],
    "Gebaeude": [
        "buildings", "GARAGE", "Bunker", "Gazebo", "Antenna", "Flag_Pole",
        "Tanks", "WashRack", "HAZERDOUS_WASTE",
        "A_01_Leichtbauhalle", "assigned_Bldgs",
        "C_Besucherz", "C_Büro", "C_WC´s",
        "Gebäude_Phase1+2", "Gebäude_Phase1+2+3", "Gebäude_Phase2",
        "Gebäude_Phase1+2_neu_grün", "Gebäude_Phase1_2_Küchen",
        "Gebäude_Phase1_RBW-Hallen", "Gebäude_Phase1_alle grau",
        "Gebäude_Phase2_Busse", "Gebäude_Phase2_RBW-Hallen",
        "Gebäude_nurPhase2_RWB_BIMA", "Gebäude_nurPhase2_Sport",
        "Gebäude_nurPhase2_UK_BIMA", "Gebäude_nurPhase2_VS_BIMA",
        "Gebäude_nurPhase2_VW_BIMA", "Gebäude_nur_Garagen",
        "Gebäude_Grün_Phase2_7487", "Gebäude_Phase 1 _Unterkunft",
    ],
}

# Per-Layer Toleranz (loose) – wird in Stage 2 verwendet
SNAP_TIGHT  = 0.10
LAYER_LOOSE = {
    "road_site":   1.00,
    "sidewalks":   0.75,
    "BASE":        0.75,
    "Cement_Street": 0.50,
    "BORDSTEIN":   0.50,
    "Apron#":      0.50,
    "buildings":   0.30,
    "GARAGE":      0.20,
    "Bunker":      0.30,
    "Gazebo":      0.30,
}
DEFAULT_LOOSE = 0.30

MIN_AREA = 1.0
MAX_AREA = 80000
FLATTEN_TOL = 0.05


def arc_pts(arc):
    cx, cy = arc.dxf.center[0], arc.dxf.center[1]
    r = arc.dxf.radius
    a0 = math.radians(arc.dxf.start_angle); a1 = math.radians(arc.dxf.end_angle)
    if a1 < a0: a1 += 2*math.pi
    n = max(2, min(200, int(r*(a1-a0)/max(FLATTEN_TOL,1e-3))))
    return [(cx + r*math.cos(a0 + (a1-a0)*i/n), cy + r*math.sin(a0 + (a1-a0)*i/n)) for i in range(n+1)]


def lw_pts(pl):
    try:
        pts = [(float(p[0]), float(p[1])) for p in pl.flattening(distance=FLATTEN_TOL)]
    except Exception:
        pts = [(float(p[0]), float(p[1])) for p in pl.get_points("xy")]
    out = []
    for p in pts:
        if not out or abs(p[0]-out[-1][0])>1e-6 or abs(p[1]-out[-1][1])>1e-6:
            out.append(p)
    return out


def collect_per_layer(msp, layers):
    out = defaultdict(lambda: {"closed":[], "lines":[]})
    for e in msp:
        layer = e.dxf.layer
        if layer not in layers: continue
        t = e.dxftype()
        if t == "LINE":
            p1 = (float(e.dxf.start[0]), float(e.dxf.start[1]))
            p2 = (float(e.dxf.end[0]),   float(e.dxf.end[1]))
            if p1 != p2:
                out[layer]["lines"].append(LineString([p1, p2]))
        elif t == "ARC":
            pts = arc_pts(e)
            if len(pts) >= 2: out[layer]["lines"].append(LineString(pts))
        elif t == "LWPOLYLINE":
            pts = lw_pts(e)
            if len(pts) < 2: continue
            if e.closed:
                if pts[0] != pts[-1]: pts.append(pts[0])
                try:
                    poly = Polygon(pts)
                    if not poly.is_valid: poly = poly.buffer(0)
                    if isinstance(poly, Polygon) and poly.area >= MIN_AREA:
                        out[layer]["closed"].append(poly)
                    elif isinstance(poly, MultiPolygon):
                        for q in poly.geoms:
                            if q.area >= MIN_AREA:
                                out[layer]["closed"].append(q)
                except: pass
                out[layer]["lines"].append(LineString(pts))
            else:
                out[layer]["lines"].append(LineString(pts))
    return out


def polygonize_set(lines, snap_tol):
    if not lines: return []
    ml = MultiLineString(lines)
    ml = shapely.set_precision(ml, grid_size=snap_tol)
    merged = unary_union(ml)
    return [p for p in polygonize(merged) if MIN_AREA <= p.area <= MAX_AREA]


def dedup(polys, used_union):
    if used_union is None or used_union.is_empty:
        return polys
    out = []
    for p in polys:
        inter = p.intersection(used_union).area
        if p.area > 0 and inter / p.area > 0.8:
            continue
        out.append(p)
    return out


def lines_outside_polys(lines, polys_union, snap_tol):
    """Liefert Linien zurueck, deren Mittelpunkt NICHT in polys_union liegt."""
    if polys_union is None or polys_union.is_empty:
        return lines
    out = []
    # leicht puffern, damit Stage-1-Linien die auf Polygonkanten liegen, nicht
    # versehentlich rausgefiltert werden
    for ls in lines:
        mid = ls.interpolate(0.5, normalized=True)
        if not polys_union.contains(mid):
            out.append(ls)
    return out


def poly_to_outer_holes(p):
    outer = list(p.exterior.coords)
    if outer and outer[0] == outer[-1]: outer = outer[:-1]
    holes = []
    for ring in p.interiors:
        h = list(ring.coords)
        if h and h[0] == h[-1]: h = h[:-1]
        if len(h) >= 3: holes.append(h)
    return outer, holes


def main():
    dxf_path = sys.argv[1]
    cls_name = sys.argv[2]
    out_pkl  = sys.argv[3]

    layers = set(LAYER_MAP[cls_name])
    t0 = time.time()
    print(f"[{time.time()-t0:.1f}s] DXF lesen...", flush=True)
    doc = ezdxf.readfile(dxf_path)
    msp = doc.modelspace()
    per_layer = collect_per_layer(msp, layers)
    print(f"[{time.time()-t0:.1f}s] {len(per_layer)} Layer in '{cls_name}'", flush=True)

    result_polys = []

    for layer in sorted(per_layer):
        info = per_layer[layer]
        nL = len(info["lines"]); nC = len(info["closed"])
        loose_tol = LAYER_LOOSE.get(layer, DEFAULT_LOOSE)
        print(f"[{time.time()-t0:.1f}s] {layer:42s} L={nL:5d} C={nC:4d} (loose={loose_tol} m)", flush=True)

        # existierende closed
        existing_polys = list(info["closed"])
        for p in existing_polys:
            outer, holes = poly_to_outer_holes(p)
            if len(outer) >= 3:
                result_polys.append((layer, outer, holes))

        if nL < 2:
            continue

        # Stage 1: tight snap
        polys_s1 = polygonize_set(info["lines"], SNAP_TIGHT)
        polys_s1 = dedup(polys_s1, unary_union(existing_polys) if existing_polys else None)
        for p in polys_s1:
            if not p.is_valid: p = p.buffer(0)
            if isinstance(p, Polygon):
                outer, holes = poly_to_outer_holes(p)
                if len(outer) >= 3:
                    result_polys.append((f"{layer}#s1", outer, holes))
            elif isinstance(p, MultiPolygon):
                for q in p.geoms:
                    outer, holes = poly_to_outer_holes(q)
                    if len(outer) >= 3:
                        result_polys.append((f"{layer}#s1", outer, holes))
        n_s1 = len(polys_s1)
        print(f"   Stage1 (tight={SNAP_TIGHT}m): {n_s1}", flush=True)

        # Stage 2: loose snap auf Linien, deren Mittelpunkt NICHT in einem
        # Stage-1- oder closed-Polygon liegt
        if loose_tol > SNAP_TIGHT and nL > 0:
            used = existing_polys + polys_s1
            used_union = unary_union(used) if used else None
            leftover = lines_outside_polys(info["lines"], used_union, loose_tol)
            print(f"   Leftover Linien: {len(leftover)}/{nL}", flush=True)
            if leftover:
                polys_s2 = polygonize_set(leftover, loose_tol)
                polys_s2 = dedup(polys_s2, used_union)
                for p in polys_s2:
                    if not p.is_valid: p = p.buffer(0)
                    if isinstance(p, Polygon):
                        outer, holes = poly_to_outer_holes(p)
                        if len(outer) >= 3:
                            result_polys.append((f"{layer}#s2", outer, holes))
                    elif isinstance(p, MultiPolygon):
                        for q in p.geoms:
                            outer, holes = poly_to_outer_holes(q)
                            if len(outer) >= 3:
                                result_polys.append((f"{layer}#s2", outer, holes))
                print(f"   Stage2 (loose={loose_tol}m): {len(polys_s2)}", flush=True)

    print(f"[{time.time()-t0:.1f}s] Total fuer {cls_name}: {len(result_polys)}", flush=True)
    with open(out_pkl, "wb") as fp:
        pickle.dump({"class": cls_name, "polys": result_polys}, fp)
    print(f"[{time.time()-t0:.1f}s] Geschrieben: {out_pkl}", flush=True)


if __name__ == "__main__":
    main()
