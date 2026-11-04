#!/usr/bin/env python3
"""
Lueckenloses Strassen-Komplement:
  1. site_envelope = concave_hull aller Linien-Endpunkte aus Strasse/Bordstein/Gebaeude-Layern
  2. buildings_union = Union aller Gebaeude-Polygone
  3. existing_road = Union aller Strasse + Bordstein Polygone (aus pickles)
  4. fill_road = site_envelope - buildings_union - existing_road
  5. fill_road wird in Strasse-Pickle als zusaetzliche Polygone abgelegt

Erzeugt: strasse_v5.pkl (kopiert v4 + Filler)
"""
import sys, math, pickle, time
from collections import defaultdict

import ezdxf
import shapely
from shapely.geometry import LineString, MultiLineString, Polygon, MultiPolygon, Point, MultiPoint
from shapely.ops import unary_union

DXF = sys.argv[1] if len(sys.argv) > 1 else "BuPol_BS_Gesamtdatei_umgebung.dxf"

ALL_LAYERS = [
    # Strasse
    "road_site", "Cement_Street", "Zaunstrasse_BUPOL", "A_01_Strasse_neu",
    "60 Straße", "parking area 305 - lighting", "parking area 325 - cobble stone",
    "parking area 325 - default", "pa 325 - concrete joints", "Apron#",
    # Bordstein
    "sidewalks", "BORDSTEIN", "BETON", "RINNE", "Concrete_area", "BASE",
    # Gebaeude
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
    # Optionale Umgrenzung (Boeschung = Boeschungslinien begrenzen Aussenanlagen)
    "BOESCHUNG",
]


def main():
    t0 = time.time()
    print(f"[{time.time()-t0:.1f}s] DXF lesen...", flush=True)
    doc = ezdxf.readfile(DXF)
    msp = doc.modelspace()

    layers_set = set(ALL_LAYERS)
    pts = []
    n_line = 0
    for e in msp:
        if e.dxf.layer not in layers_set: continue
        t = e.dxftype()
        if t == "LINE":
            pts.append((float(e.dxf.start[0]), float(e.dxf.start[1])))
            pts.append((float(e.dxf.end[0]),   float(e.dxf.end[1])))
            n_line += 1
        elif t == "LWPOLYLINE":
            try:
                for p in e.flattening(distance=0.5):
                    pts.append((float(p[0]), float(p[1])))
            except Exception:
                for p in e.get_points("xy"):
                    pts.append((float(p[0]), float(p[1])))
            n_line += 1
    print(f"[{time.time()-t0:.1f}s] {len(pts)} Punkte aus {n_line} Entities", flush=True)

    # Konvexe Hülle als grobe Site-Begrenzung; concave_hull mit moderater Ratio
    # fuer engere Hüllung (entlang der Bebauung)
    print(f"[{time.time()-t0:.1f}s] MultiPoint -> concave_hull (ratio=0.05) ...", flush=True)
    mp = MultiPoint(pts)
    # ratio = 0 (konvexe Huelle) ... 1 (sehr eng).
    site_hull = shapely.concave_hull(mp, ratio=0.05, allow_holes=False)
    print(f"[{time.time()-t0:.1f}s] site_hull Flaeche: {site_hull.area:.0f} m^2  type={site_hull.geom_type}", flush=True)

    # Lade existierende Polygone aus v4-Pickles
    print(f"[{time.time()-t0:.1f}s] Lade pickles...", flush=True)
    pickles = {}
    for cls in ["strasse","bordstein","gebaeude"]:
        with open(f"{cls}_v4.pkl", "rb") as fp:
            pickles[cls] = pickle.load(fp)

    def to_poly_list(d):
        out = []
        for lab, outer, holes in d["polys"]:
            try:
                p = Polygon(outer, holes)
                if not p.is_valid: p = p.buffer(0)
                if p.is_valid and p.area > 0:
                    out.append(p)
            except: pass
        return out

    buildings = to_poly_list(pickles["gebaeude"])
    strassen  = to_poly_list(pickles["strasse"])
    bordsteine= to_poly_list(pickles["bordstein"])
    print(f"  Strasse: {len(strassen)}, Bordstein: {len(bordsteine)}, Gebaeude: {len(buildings)}")

    # Gebaeude leicht puffern damit Bordstein eng am Gebaeude (z.B. Traufstreifen)
    # nicht als Strasse erscheint
    print(f"[{time.time()-t0:.1f}s] Union Gebaeude (buffer 0.2m) ...", flush=True)
    bld_buf = [p.buffer(0.2) for p in buildings]
    bld_union = unary_union(bld_buf) if bld_buf else None
    print(f"[{time.time()-t0:.1f}s] Gebaeude-Union Flaeche: {bld_union.area:.0f} m^2", flush=True)

    print(f"[{time.time()-t0:.1f}s] Union vorhandene Strassen+Bordsteine ...", flush=True)
    existing_road = unary_union(strassen + bordsteine) if (strassen or bordsteine) else None
    print(f"[{time.time()-t0:.1f}s] Existing-Road Flaeche: {existing_road.area:.0f} m^2", flush=True)

    # Strategie: site_hull als grosser Fueller, KEINE Subtraktion der Gebaeude/Strassen.
    # Die Gebaeude liegen visuell darueber (z=0..6), bestehende Strassen liegen auf gleicher
    # Hoehe. Damit es kein Z-Fighting gibt, legen wir den Fueller NIEDRIGER (z=-0.20 statt -0.15).
    # In step_build_single.py setzen wir dafuer eine spezielle "Strasse_fill"-Klasse.
    print(f"[{time.time()-t0:.1f}s] Verwende site_hull direkt als Fueller (ohne Subtraktion)", flush=True)
    fill_simplified = site_hull.simplify(tolerance=0.2, preserve_topology=True)
    print(f"[{time.time()-t0:.1f}s] Vereinfacht: Flaeche={fill_simplified.area:.0f} m^2  type={fill_simplified.geom_type}", flush=True)

    fill_polys = []
    if isinstance(fill_simplified, Polygon):
        fill_polys = [fill_simplified]
    elif isinstance(fill_simplified, MultiPolygon):
        fill_polys = list(fill_simplified.geoms)
    fill_polys = [p for p in fill_polys if p.area >= 5.0]
    total_pts = sum(len(p.exterior.coords) + sum(len(h.coords) for h in p.interiors) for p in fill_polys)
    print(f"[{time.time()-t0:.1f}s] {len(fill_polys)} Fueller-Polygone, {total_pts} Punkte total", flush=True)

    # NEU: Wir bauen eine eigene Pickle "fill_v5.pkl" mit nur dem Fueller.
    # Im IFC bekommt der Fueller eine eigene tiefere Z-Position (-0.30..-0.20),
    # damit die existierenden Strassen oberhalb visuell dominieren.
    new_polys = []
    added = 0
    for p in fill_polys:
        outer = list(p.exterior.coords)
        if outer and outer[0] == outer[-1]: outer = outer[:-1]
        holes = []
        for ring in p.interiors:
            h = list(ring.coords)
            if h and h[0] == h[-1]: h = h[:-1]
            if len(h) >= 3: holes.append(h)
        if len(outer) >= 3:
            new_polys.append(("fill", outer, holes))
            added += 1

    out_pkl = "fill_v5.pkl"
    with open(out_pkl, "wb") as fp:
        pickle.dump({"class":"Strasse_Fill", "polys": new_polys}, fp)
    print(f"[{time.time()-t0:.1f}s] {out_pkl}: {len(new_polys)} Fueller", flush=True)


if __name__ == "__main__":
    main()
