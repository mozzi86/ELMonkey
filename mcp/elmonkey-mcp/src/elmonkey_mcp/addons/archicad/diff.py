"""Diff-Checker Archicad ↔ Plattform: ELM_SAB.GetDiffDataELM seitenweise lesen, mit stand.json vergleichen
(elmonkey.diff_plattform), ergebnis.json als Datei schreiben. Liest Archicad nur; schreibt nur in den Arbeitsordner.
"""
from __future__ import annotations

import json
import os
import re
import urllib.request
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Optional

from elmonkey import diff_plattform as dp

from .api import ArchicadError, call

STEMPEL = "Plattform-GUID"
PAKET = 400


def _https_oder_datei(quelle: str) -> Any:
    """JSON aus einem https-Link (z. B. stand_link aus archicad_abgleich_lesen) oder einer lokalen Datei."""
    q = str(quelle or "").strip()
    if not q:
        raise ValueError("Quelle fehlt")
    if q.startswith("https://"):
        with urllib.request.urlopen(q, timeout=900) as r:
            return json.loads(r.read().decode("utf-8"))
    if re.match(r"^[a-z][a-z0-9+.-]*://", q, re.IGNORECASE):
        raise ValueError("Links nur mit https:// (aus archicad_abgleich_lesen)")
    p = Path(os.path.expanduser(q))
    if not p.is_file():
        raise ValueError(f"{p} gibt es nicht")
    return json.loads(p.read_text(encoding="utf-8"))


def plattform_geometrie_laden(quelle: Optional[str]) -> dict:
    """Plattform-GUID/GlobalId → Box aus JSON ({guid: [6 Zahlen]}) oder aus einem IFC der Plattform (.ifc)."""
    if not quelle:
        return {}
    p = Path(os.path.expanduser(str(quelle)))
    if p.suffix.lower() == ".ifc":
        return dp.boxen_aus_ifc(p.read_text(encoding="latin-1"))
    return _https_oder_datei(str(quelle))


def archicad_lesen(port: int, rufen: Callable = call, limit: int = 500, ifc_eigenschaften: bool = True,
                   oberflaechen: bool = True) -> list[dict]:
    """Alle Seiten von ELM_SAB.GetDiffDataELM (offset/limit bis nextOffset fehlt)."""
    seiten, offset = [], 0
    while True:
        s = rufen(port, "ELM_SAB", "GetDiffDataELM", {"offset": offset, "limit": limit,
                                                       "includeIfcProperties": ifc_eigenschaften,
                                                       "includeSurfaces": oberflaechen}, timeout=1800)
        seiten.append(s)
        nxt = s.get("nextOffset")
        if nxt is None:
            return seiten
        if int(nxt) <= offset:
            raise ArchicadError(f"GetDiffDataELM: nextOffset {nxt} geht nicht weiter (offset {offset})")
        offset = int(nxt)


def stempel_lesen(port: int, guids: list[str], rufen: Callable = call) -> dict[str, dict[str, Any]]:
    """Archicad-Eigenschaft „…Plattform-GUID“ (jede Gruppe) je Element — der IFC-Übersetzer gibt sie nicht immer aus.
    → {guid: {"Gruppe.Plattform-GUID": wert}}; leer, wenn es die Eigenschaft im Projekt nicht gibt."""
    props = [p for p in (rufen(port, "TapirCommand", "GetAllProperties", {}).get("properties") or [])
             if str(p.get("propertyName") or "") == STEMPEL]
    if not props or not guids:
        return {}
    aus: dict[str, dict[str, Any]] = {}
    for i in range(0, len(guids), PAKET):
        teil = guids[i:i + PAKET]
        werte = rufen(port, "TapirCommand", "GetPropertyValuesOfElements", {
            "elements": [{"elementId": {"guid": g}} for g in teil],
            "properties": [{"propertyId": p["propertyId"]} for p in props]}).get("propertyValuesForElements") or []
        for g, zeile in zip(teil, werte):
            for p, w in zip(props, (zeile or {}).get("propertyValues") or []):
                v = ((w or {}).get("propertyValue") or {}).get("value")
                if v not in (None, ""):
                    aus.setdefault(g, {})[f"{p.get('propertyGroupName')}.{STEMPEL}"] = v
    return aus


def _ordner(ordner: Optional[str], lauf: Optional[str]) -> Path:
    if ordner:
        p = Path(os.path.expanduser(ordner))
    else:
        p = Path.home() / "ELMonkey" / "diff-plattform" / (lauf or datetime.now().strftime("lauf-%Y%m%d-%H%M%S"))
    p.mkdir(parents=True, exist_ok=True)
    return p


def diff_lauf(port: int, stand: str, *, basis: Optional[str] = None, plattform_geometrie: Optional[str] = None,
              gebaut: Optional[list[dict]] = None, ordner: Optional[str] = None,
              versatz: Optional[list[float]] = None, toleranz: float = dp.TOLERANZ, limit: int = 500,
              rufen: Callable = call) -> dict:
    """Der ganze Lauf; Rückgabe = kurze Zusammenfassung, das Ergebnis liegt als Datei im Arbeitsordner."""
    st = _https_oder_datei(stand)
    if st.get("format") != dp.FORMAT:
        raise ValueError(f"stand.json: Format „{st.get('format')}“ — erwartet {dp.FORMAT}")
    b = _https_oder_datei(basis) if basis else None
    geo = plattform_geometrie_laden(plattform_geometrie)
    seiten = archicad_lesen(port, rufen, limit)
    namen = {g.get("archicadName"): g.get("name") for g in st.get("geschosse") or []
             if g.get("archicadName") and g.get("name")}
    ac = dp.archicad_bauteile(seiten, geschoss_namen=namen)
    dp.stempel_eintragen(ac, stempel_lesen(port, [e["guid"] for e in ac["elemente"]], rufen))
    v = tuple(float(x) for x in (versatz or (0.0, 0.0, 0.0)))
    if len(v) != 3:
        raise ValueError("versatz = [dx, dy, dz] in Metern")
    erg = dp.vergleichen(ac, st, gebaut=gebaut, plattform_geometrie=geo, basis=b, toleranz=toleranz,
                         versatz=v)                                     # type: ignore[arg-type]
    o = _ordner(ordner, st.get("lauf"))
    (o / "ergebnis.json").write_text(json.dumps(erg, ensure_ascii=False, indent=1), encoding="utf-8")
    (o / "archicad.json").write_text(json.dumps({"seiten": seiten}, ensure_ascii=False), encoding="utf-8")
    d = erg["diff"]
    return {"ergebnis": str(o / "ergebnis.json"), "archicad_daten": str(o / "archicad.json"),
            "lauf": erg.get("lauf"), "zahlen": d["zahlen"], "hinweise": d["hinweise"], "kontext": d["kontext"],
            "beispiele": {"neu": d["neu"][:10], "weg": d["weg"][:10], "eigenschaften": d["eigenschaften"][:10],
                          "geometrie": d["geometrie"][:10], "geometrie_ungeprueft": d["geometrie_ungeprueft"][:5]},
            "gebaut_vorschlag": d["gebaut_vorschlag"][:50],
            "naechster_schritt": "Zahlen dem Zeichner zeigen. ergebnis.json auf den Ablage-Link des Laufs laden "
                                 "(plattform_abgleich.py hochladen --ordner <Ordner> --ablage <ergebnis_ablage>), dann "
                                 "archicad_abgleich_melden {projekt, lauf, gebaut} — erst Vorschau, nach dem Ja "
                                 "bestaetigt=true. Die Plattform macht aus diff.geometrie Geometrie-Vorschläge; "
                                 "geometrie.nicht_automatisch und plattform_geaendert aus ihrer Antwort dem Zeichner "
                                 "zeigen."}
