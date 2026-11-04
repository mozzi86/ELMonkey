"""
Zweck: Aus dem Bodenraster (grid.npz von e57_grid.py) eine Mesh-JSON für
       Tapir CreateMeshes erzeugen (Bodenfilter, Downsampling, Sublines-je-Zeile).
Aufruf: python3 make_mesh.py --grid grid_1m.npz --origin X Y Z [--step 2] [--out mesh.json]
        (--origin = Archicad-Objektursprung der Punktwolke, aus GetDetailsOfElements)
Abhängigkeiten: uv venv .venv && uv pip install --python .venv/bin/python pye57 numpy scipy
Live-Verifikation: 2026-09-07 (THN Außenanlage).
"""
import argparse
import numpy as np
import json
from scipy import ndimage

parser = argparse.ArgumentParser()
parser.add_argument("--grid", default="grid_1m.npz")
parser.add_argument("--origin", type=float, nargs=3, required=True, metavar=("X", "Y", "Z"))
parser.add_argument("--step", type=int, default=2)
parser.add_argument("--out", default="mesh.json")
args = parser.parse_args()

g = np.load(args.grid)
zmin = g["zmin"]; cnt = g["cnt"]; X0 = float(g["X0"]); Y0 = float(g["Y0"]); wb = g["wb"]

# Offset E57 -> Archicad (BBox-Min der Wolke -> Objektursprung)
DX = args.origin[0] - wb[0]
DY = args.origin[1] - wb[1]
DZ = args.origin[2] - wb[2]
STEP = args.step

valid = (cnt >= 5) & np.isfinite(zmin)
z = np.where(valid, zmin, np.nan)

# Bodenfilter: lokales Minimum über 7x7, Zellen >0,6 m darüber raus (Wände, Bäume, Dächer)
zf = np.where(valid, zmin, np.inf)
lmin = ndimage.minimum_filter(zf, size=7, mode="nearest")
ground = valid & (zmin <= lmin + 0.6)
zg = np.where(ground, zmin, np.nan)


# Median-Glättung nur über Bodenzellen
def nanmed(a):
    m = ~np.isnan(a)
    return np.median(a[m]) if m.sum() >= 4 else np.nan


zs = ndimage.generic_filter(zg, nanmed, size=3, mode="constant", cval=np.nan)
# 2. Durchgang: isolierte Hochflächen (Dächer/Terrassen) >1,5 m über 21x21-Median raus
lmed = ndimage.generic_filter(zs, nanmed, size=21, mode="constant", cval=np.nan)
zs = np.where(np.isnan(lmed) | (zs <= lmed + 1.5), zs, np.nan)

ys, xs = np.where(~np.isnan(zs))
print("ground cells", len(xs), "of valid", valid.sum(), "z range", np.nanmin(zs), np.nanmax(zs))

# Downsampling auf STEP-Raster; jede Rasterzeile = eine Subline
sub = {}
for iy, ix in zip(ys, xs):
    if iy % STEP or ix % STEP:
        continue
    sub.setdefault(iy, []).append((ix, zs[iy, ix]))

sublines = []
npts = 0
for iy in sorted(sub):
    pts = sorted(sub[iy])
    if len(pts) < 2:
        continue
    coords = [{"x": round(X0 + ix + 0.5 + DX, 3), "y": round(Y0 + iy + 0.5 + DY, 3),
               "z": round(float(zz) + DZ, 3)} for ix, zz in pts]
    sublines.append({"coordinates": coords})
    npts += len(coords)

# Umriss = Wolken-BBox, z = lokaler Boden-Median
zmed = float(np.nanmedian(zs)) + DZ
x0, y0, x1, y1 = wb[0] + DX, wb[1] + DY, wb[3] + DX, wb[4] + DY


def zc(x, y):
    ix = int(x - DX - X0)
    iy = int(y - DY - Y0)
    win = zs[max(iy - 10, 0):iy + 10, max(ix - 10, 0):ix + 10]
    return round(float(np.nanmedian(win)) + DZ, 3) if np.isfinite(np.nanmedian(win)) else round(zmed, 3)


poly = [{"x": x0, "y": y0, "z": zc(x0, y0)}, {"x": x1, "y": y0, "z": zc(x1, y0)},
        {"x": x1, "y": y1, "z": zc(x1, y1)}, {"x": x0, "y": y1, "z": zc(x0, y1)}]
for p in poly:
    p["x"] = round(float(p["x"]), 3)
    p["y"] = round(float(p["y"]), 3)

mesh = {"floorIndex": 0, "level": 0.0, "skirtType": "SolidBodyWithSkirt", "skirtLevel": 3.0,
        "ridges": "AllSmooth", "showLines": False, "polygonCoordinates": poly, "sublines": sublines}
json.dump({"meshesData": [mesh]}, open(args.out, "w"))
print("step", STEP, "sublines", len(sublines), "points", npts, "outline z", [p["z"] for p in poly],
      "ground z AC range", round(np.nanmin(zs) + DZ, 2), round(np.nanmax(zs) + DZ, 2))
