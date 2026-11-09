#!/usr/bin/env python3
"""Prüft einen Export-Ordner `sab-oberflaechen/1` (ohne Archicad, ohne Netz).

    python3 pruefen.py "<Downloads>/<Projekt>_oberflaechen_<Zeitstempel>"

Meldet Formatfehler, fehlende Bilder und Zählerabweichungen. Rückgabe 0 = in Ordnung.
"""
import json
import sys
from pathlib import Path


def main(ordner: str) -> int:
    o = Path(ordner)
    fehler, hinweise = [], []
    datei = o / "oberflaechen.json"
    if not datei.is_file():
        print(f"FEHLER: {datei} fehlt")
        return 1
    try:
        p = json.loads(datei.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        print(f"FEHLER: oberflaechen.json nicht lesbar: {e}")
        return 1

    if p.get("format") != "sab-oberflaechen/1":
        fehler.append(f"format ist „{p.get('format')}“")
    obs = p.get("oberflaechen")
    if not isinstance(obs, list):
        fehler.append("oberflaechen fehlt oder ist keine Liste")
        obs = []

    mit_bild = bild_fehlt = ohne = 0
    bilder = set()
    for ob in obs:
        n = ob.get("name", "?")
        if not str(ob.get("farbe", "")).startswith("#"):
            fehler.append(f"„{n}“: farbe fehlt")
        t = ob.get("textur")
        if t is None:
            ohne += 1
            continue
        for f in ("breite_m", "hoehe_m"):
            if not isinstance(t.get(f), (int, float)) or t.get(f) <= 0:
                hinweise.append(f"„{n}“: {f} = {t.get(f)}")
        if t.get("datei"):
            pfad = (o / t["datei"]).resolve()
            if o.resolve() not in pfad.parents:
                fehler.append(f"„{n}“: Bildpfad zeigt aus dem Ordner")
            elif not pfad.is_file() or pfad.stat().st_size == 0:
                fehler.append(f"„{n}“: {t['datei']} fehlt oder ist leer")
            else:
                bilder.add(t["datei"])
            mit_bild += 1
        else:
            bild_fehlt += 1
            hinweise.append(f"„{n}“: kein Bild ({t.get('bild_fehlt', 'ohne Grund')})")

    z = p.get("zaehler") or {}
    soll = {"oberflaechen": len(obs), "mit_bild": mit_bild, "bild_fehlt": bild_fehlt,
            "ohne_textur": ohne, "bilder": len(bilder)}
    for k, v in soll.items():
        if z.get(k) != v:
            fehler.append(f"zaehler.{k} = {z.get(k)}, gezählt {v}")
    auf_platte = {f"texturen/{f.name}" for f in (o / "texturen").glob("*") if f.is_file()}
    for f in sorted(auf_platte - bilder):
        hinweise.append(f"{f} liegt im Ordner, keine Oberfläche nutzt es")

    print(f"{p.get('projekt')}: {len(obs)} Oberflächen, {mit_bild} mit Bild, {bild_fehlt} ohne Bilddatei, "
          f"{ohne} ohne Textur, {len(bilder)} Bilder")
    for h in hinweise[:40]:
        print("  Hinweis:", h)
    if len(hinweise) > 40:
        print(f"  … {len(hinweise) - 40} weitere Hinweise")
    for f in fehler:
        print("  FEHLER:", f)
    print("In Ordnung." if not fehler else f"{len(fehler)} Fehler.")
    return 0 if not fehler else 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
