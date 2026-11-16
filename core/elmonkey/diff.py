"""Diff-Checker Archicad ↔ Schwarz Plattform — nur die Unterschiede, ohne IFC-Datei. Reiner Kern.

Anlass: Abgleich AFWC am 16.11.2026 (Anforderung Sophia Pickel). Der Umweg über ein Komplett-IFC dauert lange und
erzeugt Schein-Unterschiede. Dieser Kern vergleicht den direkt gelesenen Archicad-Stand mit `stand.json` der
Plattform und meldet nur, was sich wirklich unterscheidet:

    neu            Bauteile, die nur Archicad kennt
    weg            Bauteile aus Archicad, die im offenen Projekt fehlen
    eigenschaften  je Paar die Felder mit verschiedenem Wert
    geometrie      je Paar die Seiten (x-/x+/y-/y+/z-/z+), die um mehr als die Toleranz abweichen

Getrennt gemeldet, nie als „neu“ oder „weg“: Räume (IfcSpace/Zone), Öffnungen (ihre GUID ist je Export neu),
Plattform-eigene Bauteile ohne Partner und Bauteile, deren Geometrie nicht vergleichbar ist.

Formate wie in `abgleich` (`sab-archicad-abgleich/1`), dazu je Bauteil optional:

    "bbox"         [xmin, xmax, ymin, ymax, zmin, zmax] in Archicad-Metern
    "bbox_quelle"  "koerper" (3D-Körper nach Anschlüssen, so wie im IFC) | "roh" (Hüllbox, nicht vergleichbar)

Der Kern schreibt nirgends.
"""
from __future__ import annotations

import re
from typing import Any, Iterable, Optional

from . import abgleich as ab

#: 3 mm — darunter gilt Geometrie als gleich.
TOLERANZ = 0.003
SEITEN = ("x-", "x+", "y-", "y+", "z-", "z+")
KOERPER, ROH = "koerper", "roh"

#: Räume: die Plattform führt keine, sie sind nie „neu“.
RAUM = ("ifcspace", "zone")
#: Öffnungen: GUID je Export neu, sie werden über Tür/Fenster abgeglichen.
OEFFNUNG = ("ifcopeningelement", "opening")

#: Mengen ändern sich ohne Geometrie (Öffnungen, Anschlüsse) — kein Unterschied.
_MENGEN_PSET = re.compile(r"^(qto_|basequantities$|.*mengen$|.*quantit)", re.I)

_IFC_TYP = re.compile(r"^\(?\s*IFC[A-Z0-9]+\((.*)\)\s*\)?$", re.S)
_X2 = re.compile(r"\\X2\\([0-9A-Fa-f]+)\\X0\\")
_X = re.compile(r"\\X\\([0-9A-Fa-f]{2})")
_WAHR = {".t.": True, ".f.": False, "t": True, "f": False, ".true.": True, ".false.": False}


def ist_menge(feld: str) -> bool:
    """„Qto_WallBaseQuantities.Length“, „BaseQuantities.NetVolume“ … — Mengen, kein Fachinhalt."""
    return bool(_MENGEN_PSET.match(str(feld).split(".", 1)[0].strip()))


def _entschluesseln(s: str) -> str:
    s = _X2.sub(lambda m: "".join(chr(int(m.group(1)[i:i + 4], 16)) for i in range(0, len(m.group(1)) - 3, 4)), s)
    return _X.sub(lambda m: chr(int(m.group(1), 16)), s)


def wert_norm(v: Any) -> Any:
    """Wie `abgleich.wert_norm`, dazu die Schreibweisen des IFC: `.T.`/„Ja“, `IFCLABEL('NEW')`/`['NEW']`, `\\X\\E4`."""
    if isinstance(v, (list, tuple)) and len(v) == 1:
        v = v[0]
    if not isinstance(v, str):
        return ab.wert_norm(v)
    s = _entschluesseln(v.strip())
    for _ in range(3):                                   # IFCLABEL('…'), ['…'], ('…') — auch verschachtelt
        m = _IFC_TYP.match(s)
        if m:
            s = m.group(1).strip()
            continue
        if len(s) >= 2 and s[0] + s[-1] in ("[]", "()"):
            innen = s[1:-1].strip()
            if "," not in innen:
                s = innen
                continue
        break
    if len(s) >= 2 and s[0] == s[-1] and s[0] in "'\"":
        s = s[1:-1].replace("''", "'")
    s = re.sub(r"\s+", " ", s).strip()
    if s.lower() in _WAHR:
        return _WAHR[s.lower()]
    if s in ("$", "*"):
        return None
    return ab.wert_norm(s)


def gleich(a: Any, b: Any) -> bool:
    """Gleich nach Normalisierung; Zahlen mit 2 mm bzw. 0,01 % Spielraum (Rundung im IFC)."""
    na, nb = wert_norm(a), wert_norm(b)
    if isinstance(na, float) and isinstance(nb, float):
        return abs(na - nb) <= max(0.002, 1e-4 * abs(na))
    return na == nb


# ─────────────────────────────────────────────────────────── Geometrie

def _bbox(e: dict) -> Optional[list[float]]:
    b = e.get("bbox")
    if isinstance(b, dict):
        b = [b.get(k) for k in ("xMin", "xMax", "yMin", "yMax", "zMin", "zMax")]
    if not isinstance(b, (list, tuple)) or len(b) != 6:
        return None
    try:
        return [float(x) for x in b]
    except (TypeError, ValueError):
        return None


def bbox_vereinen(boxen: Iterable[Any]) -> Optional[list[float]]:
    """Mehrere Boxen zu einer — für Bauteile, deren Geometrie nur in den Teilen steckt (Mehrschicht im IFC)."""
    gueltig = [b for b in (_bbox({"bbox": x}) for x in boxen) if b]
    if not gueltig:
        return None
    return [f(b[i] for b in gueltig) for i, f in enumerate((min, max, min, max, min, max))]


def seiten(plattform: list[float], archicad: list[float], toleranz: float = TOLERANZ) -> dict[str, float]:
    """{Seite: Delta in Metern}, nur Seiten über der Toleranz. Delta = Archicad − Plattform, in Achsrichtung:
    „x+“: +0,10 heißt, die Seite liegt in Archicad 10 cm weiter in +x."""
    return {s: round(archicad[i] - plattform[i], 4) for i, s in enumerate(SEITEN)
            if abs(archicad[i] - plattform[i]) > toleranz}


# ─────────────────────────────────────────────────────────── Diff

def _art(e: dict) -> str:
    return str(e.get("klasse") or e.get("ifcTyp") or e.get("typ") or "").strip().lower()


def _ist(e: dict, arten: tuple) -> bool:
    return _art(e) in arten or str(e.get("typ") or "").strip().lower() in arten


def _kurz(e: dict) -> dict:
    return {k: e.get(k) for k in ("guid", "name", "klasse", "ifcTyp", "typ", "geschoss") if e.get(k) is not None}


def _felder(p: dict, a: dict, nie: tuple) -> dict[str, dict]:
    pe, ae = p.get("eigenschaften") or {}, a.get("eigenschaften") or {}
    aus = {}
    for f in sorted(set(pe) & set(ae)):
        if ist_menge(f) or any(f == n or f.endswith("." + n) for n in nie):
            continue
        if not gleich(pe[f], ae[f]):
            aus[f] = {"plattform": pe[f], "archicad": ae[f]}
    return aus


def diff(archicad: list[dict], plattform: list[dict], basis: Optional[dict] = None, *,
         toleranz: float = TOLERANZ, nie: Iterable[str] = ab.NIE) -> dict:
    """Unterschiede zwischen direkt gelesenem Archicad-Stand und Plattform-Stand. Siehe Modulkopf.

    `archicad` muss ALLE Bauteile enthalten, auch die vom Umbau-Filter ausgeblendeten — sonst sehen
    Abbruch-Bauteile wie gelöscht aus (das Lesen ist Sache des Skripts)."""
    nie = tuple(nie)
    raeume = [a for a in archicad if _ist(a, RAUM)]
    oeffnungen_a = [a for a in archicad if _ist(a, OEFFNUNG)]
    oeffnungen_p = [p for p in plattform if _ist(p, OEFFNUNG)]
    ac = [a for a in archicad if not _ist(a, RAUM + OEFFNUNG)]
    pf = [p for p in plattform if not _ist(p, RAUM + OEFFNUNG)]
    z = ab.paaren(ac, pf, basis)

    aus: dict = {"format": ab.FORMAT, "toleranz": toleranz, "paare": {}, "neu": [], "weg": [],
                 "eigenschaften": [], "geometrie": [], "nur_plattform": [], "ausgeblendet_in_plattform": [],
                 "raeume": [_kurz(a) for a in raeume],
                 "oeffnungen": {"archicad": len(oeffnungen_a), "plattform": len(oeffnungen_p),
                                "hinweis": "Öffnungen haben je Export eine neue GUID — Abgleich über die Tür "
                                           "bzw. das Fenster, nicht über die Öffnung."},
                 "geometrie_ungeprueft": [], "hinweise": []}
    ohne_p = ohne_a = roh = 0
    for p, a in z["paare"]:
        pg, ag = p.get("guid"), a.get("guid")
        aus["paare"][pg] = ag
        if p.get("ausgeblendet"):
            aus["ausgeblendet_in_plattform"].append({"plattform": pg, "archicad": ag, **_kurz(p)})
            continue
        felder = _felder(p, a, nie)
        if felder:
            aus["eigenschaften"].append({"plattform": pg, "archicad": ag, "name": p.get("name"), "felder": felder})
        bp, ba = _bbox(p), _bbox(a)
        if bp is None:
            ohne_p += 1
        elif ba is None:
            ohne_a += 1
            aus["geometrie_ungeprueft"].append({"plattform": pg, "archicad": ag, "grund": "Archicad ohne Geometrie"})
        elif a.get("bbox_quelle", KOERPER) != KOERPER:
            roh += 1
            aus["geometrie_ungeprueft"].append({"plattform": pg, "archicad": ag,
                                                "grund": "nur Hüllbox, kein 3D-Körper (im 3D ausgeblendet?)"})
        else:
            d = seiten(bp, ba, toleranz)
            if d:
                aus["geometrie"].append({"plattform": pg, "archicad": ag, "name": p.get("name"), "seiten": d,
                                         "bbox_archicad": [round(x, 4) for x in ba],
                                         "bbox_plattform": [round(x, 4) for x in bp]})
    for p in z["nur_plattform"]:
        if p.get("ausgeblendet"):
            continue
        if p.get("herkunft") == "plattform":
            aus["nur_plattform"].append(_kurz(p))           # Plattform-eigen: bauen oder über `gebaut` paaren
        else:
            aus["weg"].append(_kurz(p))
    aus["neu"] = [_kurz(a) for a in z["nur_archicad"]]

    if ohne_p:
        aus["hinweise"].append(f"{ohne_p} Paare ohne Geometrievergleich: der Plattform-Stand trägt für sie keine "
                               "bbox (ältere Plattform oder Bauteil ohne Geometrie).")
    if roh or ohne_a:
        aus["hinweise"].append(f"{roh + ohne_a} Paare ohne Geometrievergleich auf Archicad-Seite — siehe "
                               "geometrie_ungeprueft. Häufigster Grund: der Umbau-Filter blendet Abbruch im 3D aus.")
    for d in z["doppelt"]:
        aus["hinweise"].append(f"„Plattform-GUID“ {d['plattformGuid']} steht an mehreren Archicad-Bauteilen.")
    aus["zahlen"] = {"paare": len(aus["paare"]), "neu": len(aus["neu"]), "weg": len(aus["weg"]),
                     "eigenschaften_bauteile": len(aus["eigenschaften"]),
                     "eigenschaften_felder": sum(len(x["felder"]) for x in aus["eigenschaften"]),
                     "geometrie_bauteile": len(aus["geometrie"]),
                     "geometrie_ungeprueft": len(aus["geometrie_ungeprueft"]) + ohne_p,
                     "nur_plattform": len(aus["nur_plattform"]), "raeume": len(aus["raeume"]),
                     "ausgeblendet_in_plattform": len(aus["ausgeblendet_in_plattform"])}
    return aus


def ergebnis(d: dict, lauf: Optional[str] = None) -> dict:
    """Diff → `ergebnis.json` für `archicad_abgleich_melden`: Archicad ist die Quelle, die Plattform bekommt
    Vorschläge. „weg“ wird NICHT ausgeblendet (das entscheidet der Zeichner), nur gelistet.

    `plattform.geometrie` ist eine Erweiterung des Formats: eine Plattform, die sie nicht kennt, übergeht sie."""
    eig = [{"plattform": x["plattform"], "archicad": x["archicad"], "feld": f, "wert": w["archicad"],
            "vorher": w["plattform"]} for x in d["eigenschaften"] for f, w in x["felder"].items()]
    werte: dict[str, dict] = {}
    for x in eig:
        werte.setdefault(x["plattform"], {})[x["feld"]] = x["vorher"]
    return {"format": ab.FORMAT, "lauf": lauf, "art": "diff",
            "plattform": {"neu_importieren": [x["guid"] for x in d["neu"]], "eigenschaften": eig,
                          "geometrie": [{k: x[k] for k in ("plattform", "archicad", "seiten", "bbox_archicad")}
                                        for x in d["geometrie"]],
                          "ausblenden": [], "einblenden": []},
            "archicad": {"neu_bauen": [], "eigenschaften": [], "loeschen_von_hand": []},
            "offen": [{"id": "weg", "text": "Stammt aus Archicad, fehlt im offenen Projekt.", "anzahl": len(d["weg"])}]
            if d["weg"] else [],
            "basis": {"paare": dict(d["paare"]), "werte": werte},
            "zahlen": dict(d["zahlen"])}
