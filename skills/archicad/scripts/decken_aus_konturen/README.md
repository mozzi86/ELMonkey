# Decken aus Konturen

Erzeugt pro Geschoss **Decken (Slabs) mit Durchbruch-Löchern** aus dem
2D-Linienwerk — Footprint aus Wand-+Fassaden-+Öffnungslinien,
Löcher aus AUSSPAR_BODEN/AUSSPAR_DECKE. Details, SAB-Regeln und die
**Absturz-Lehren** (Häppchen-Requests, Spike-Buffer, safe_ring):
[`recipes/decken-und-durchbrueche.md`](../../recipes/decken-und-durchbrueche.md).

Braucht ELM_SAB_Add-On ≥ v0.4 (Geometrie-Lesen) + Tapir ≥ 1.5.3 (CreateSlabs)
+ das Schwester-Script `konturen_zu_waende/` (Import).

```bash
python3 decken_aus_konturen.py --dry-run            # Vorschau alle Geschosse
python3 decken_aus_konturen.py --floors -1 0 1 3 4 --dicke 0.30 --yes
```

Fortschritt liegt in `decken_progress.json` (Resume nach Abbruch);
bei Verbindungsabbruch stoppt das Script sofort — Archicad prüfen, nie blind neu senden.
