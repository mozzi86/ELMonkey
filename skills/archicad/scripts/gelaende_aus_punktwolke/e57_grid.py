"""
Zweck: E57-Punktwolke auf ein 1-m-Bodenraster reduzieren (min(z) + Punktzahl je Zelle).
Aufruf: python3 e57_grid.py <input.e57> [output.npz]
        (Default-Ausgabe: grid_1m.npz)
Laufzeit: ~6 s für 50 Mio Punkte.
Abhängigkeiten: uv venv .venv && uv pip install --python .venv/bin/python pye57 numpy scipy
Live-Verifikation: 2026-09-07 (THN Außenanlage, 59 Scans / 50,4 Mio Punkte).
"""
import pye57, numpy as np, time, sys

f = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else "grid_1m.npz"

e = pye57.E57(f)
CELL = 1.0
# Welt-Fenster in E57-Weltkoordinaten (nach transform=True); bei anderen Wolken anpassen.
X0, Y0 = -400, -300
NX, NY = 800, 600   # 800x600 m Fenster, großzügig bemessen

zmin = np.full((NY, NX), np.inf, dtype=np.float32)
cnt = np.zeros((NY, NX), dtype=np.int32)
wb = [np.inf, np.inf, np.inf, -np.inf, -np.inf, -np.inf]
t = time.time()
for i in range(e.scan_count):
    d = e.read_scan(i, transform=True, ignore_missing_fields=True)
    x, y, z = d["cartesianX"], d["cartesianY"], d["cartesianZ"]
    m = np.isfinite(x) & np.isfinite(y) & np.isfinite(z)
    x, y, z = x[m], y[m], z[m]
    wb = [min(wb[0], x.min()), min(wb[1], y.min()), min(wb[2], z.min()),
          max(wb[3], x.max()), max(wb[4], y.max()), max(wb[5], z.max())]
    ix = ((x - X0) / CELL).astype(int)
    iy = ((y - Y0) / CELL).astype(int)
    ok = (ix >= 0) & (ix < NX) & (iy >= 0) & (iy < NY)
    ix, iy, z = ix[ok], iy[ok], z[ok].astype(np.float32)
    flat = iy * NX + ix
    np.minimum.at(zmin.ravel(), flat, z)
    np.add.at(cnt.ravel(), flat, 1)
    print(i, len(x), f"{time.time()-t:.0f}s", flush=True)

np.savez(out, zmin=zmin, cnt=cnt, X0=X0, Y0=Y0, CELL=CELL, wb=np.array(wb))
print("world bbox", wb, "dims", wb[3] - wb[0], wb[4] - wb[1], wb[5] - wb[2])
