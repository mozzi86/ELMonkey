"""Werkstoffklassen des Bauteilkatalogs: DIN-Schraffur, IFC-Kategorie und Plattform-Textur je Klasse.

Schraffuren nach DIN statt der Archicad-Muster; jeder Baustoff bekommt vollautomatisch
eine Klasse (nie „unbestimmt“); Texturen kommen aus der einer Texturbibliothek des Aufrufers.

Schraffur = höchstens zwei Linienscharen (so viel bildet `IfcFillAreaStyleHatching` sauber ab). Wo die Norm
ein Symbol zeichnet (Dämmung als Wellen-/Zickzacklinie, Beton als Punktierung), steht die Näherung als
Linienschar hier und der Normbezug in `din`. Winkel in Grad, Abstand in Metern (Papiermaß 1:1 bei 1:50
gedacht: 2,5 mm Abstand = 0,0025).
"""
from __future__ import annotations

import re
from typing import Optional

#: Linienschar: (Winkel °, Abstand m, gestrichelt)
Schar = tuple[float, float, bool]

#: schluessel → (Bezeichnung, IFC-Material-Category, Linienscharen, DIN-Bezug, Plattform-Texturklasse)
KLASSEN: dict[str, tuple[str, str, tuple[Schar, ...], str, Optional[str]]] = {
    "mauerwerk":       ("Mauerwerk", "masonry", ((45.0, 0.0025, False),), "DIN 1356-1: Mauerwerk künstliche Steine, Schraffur 45°", "mauerwerk"),
    "stahlbeton":      ("Stahlbeton", "concrete", ((45.0, 0.0025, False), (135.0, 0.0025, False)), "DIN 1356-1: Stahlbeton, Kreuzschraffur", "beton"),
    "beton":           ("Beton unbewehrt", "concrete", ((45.0, 0.005, True),), "DIN 1356-1: Beton unbewehrt (Punktierung), genähert als Strichschar", "beton"),
    "leichtbeton":     ("Leicht-/Porenbeton", "concrete", ((45.0, 0.004, True), (135.0, 0.008, True)), "DIN 1356-1: Leichtbeton, genähert", "beton"),
    "estrich":         ("Estrich", "concrete", ((0.0, 0.0015, True),), "DIN 1356-1: Estrich, genähert als waagerechte Strichschar", "beton"),
    "daemmung_weich":  ("Dämmstoff weich", "insulation", ((60.0, 0.004, False), (120.0, 0.004, False)), "DIN 1356-1: Dämmschicht weich (Wellenlinie), genähert als Zickzack-Scharen", "daemmung"),
    "daemmung_hart":   ("Dämmstoff hart", "insulation", ((45.0, 0.004, False), (135.0, 0.004, False)), "DIN 1356-1: Dämmschicht hart (Zickzack), genähert als Kreuzschar", "daemmung"),
    "holz":            ("Vollholz", "wood", ((45.0, 0.0015, False), (45.0, 0.006, False)), "DIN 1356-1: Holz im Querschnitt", "holz"),
    "holzwerkstoff":   ("Holzwerkstoff", "wood", ((45.0, 0.0015, False),), "DIN 1356-1: Holzwerkstoffplatte", "holz"),
    "gipsplatte":      ("Gipsplatte", "gypsum", ((45.0, 0.001, False),), "DIN 1356-1: Gips/Gipsplatte, enge Schraffur", "putz"),
    "putz":            ("Putz/Mörtel", "plaster", ((0.0, 0.001, True),), "DIN 1356-1: Putz, Mörtel (Punktierung), genähert", "putz"),
    "metall":          ("Metall", "steel", ((45.0, 0.0008, False),), "DIN 1356-1: Metall (voll/eng geschraffert)", "metall"),
    "glas":            ("Glas", "glass", ((45.0, 0.01, False),), "DIN ISO 128-50: Glas, weite Schraffur", "glas"),
    "abdichtung":      ("Abdichtung/Folie", "membrane", ((0.0, 0.0005, False),), "DIN 1356-1: Abdichtung, dicke Linie", "kunststoff"),
    "kunststoff":      ("Kunststoff/elastischer Belag", "plastic", ((0.0, 0.002, False), (90.0, 0.002, False)), "DIN ISO 128-50: Kunststoff", "kunststoff"),
    "keramik":         ("Fliesen/Keramik", "ceramic", ((90.0, 0.003, False),), "DIN ISO 128-50: Keramik", "stein"),
    "naturstein":      ("Naturstein", "stone", ((30.0, 0.003, False), (120.0, 0.006, False)), "DIN 1356-1: Naturstein", "stein"),
    "textil":          ("Textiler Belag", "textile", ((0.0, 0.002, True),), "DIN ISO 128-50: Textil, genähert", "teppich"),
    "erdreich":        ("Erdreich/Schüttung", "soil", ((45.0, 0.006, False), (135.0, 0.012, True)), "DIN 1356-1: gewachsener Boden / Schüttung", "boden"),
    "dachdeckung":     ("Dachdeckung", "ceramic", ((90.0, 0.004, False),), "DIN ISO 128-50: Dachdeckung, genähert", "mauerwerk"),
    "bitumen":         ("Asphalt/Bitumen", "asphalt", ((0.0, 0.002, False), (45.0, 0.004, True)), "DIN 1356-1: Asphalt, genähert", "kunststoff"),
    "luft":            ("Luftschicht", "air", (), "keine Schraffur (Hohlraum)", None),
    "sonstiges":       ("Sonstiges/Modellierung", "other", ((45.0, 0.01, True),), "keine Normschraffur — Modellier- bzw. Hilfsbaustoff", None),
}

#: Archicad-Kategorie (Feld „ID“ des Baustoffs) → Klasse; Name schlägt Kategorie, wo die Kategorie zu grob ist.
_KATEGORIE = {
    "beton": "beton", "mauerziegel": "mauerwerk", "mw": "mauerwerk", "kunststein": "mauerwerk",
    "betonstein": "mauerwerk", "dämmung": "daemmung_hart", "holz": "holz", "holzwerkstoff": "holzwerkstoff",
    "gipskarton": "gipsplatte", "putz": "putz", "mörtel": "putz", "metall": "metall", "glas": "glas",
    "kunststoff": "kunststoff", "keramikfliese": "keramik", "keramik": "keramik", "naturstein": "naturstein",
    "textil": "textil", "natur": "erdreich", "dachziegel": "dachdeckung", "asphalt": "bitumen", "luft": "luft",
    "belag": "kunststoff", "verbundwerkstoff": "holzwerkstoff", "modellierung": "sonstiges",
}

#: Namensregeln, in dieser Reihenfolge (erste passende gewinnt)
_NAME = [
    ("luft", r"luftschicht|hohlraum"),
    ("dachdeckung", r"dachziegel|dachstein|faserzement|reet"),
    ("mauerwerk", r"kalksand|\bks\b|ziegel|betonblock|mauer|wand (bestand|neubau)|pflaster"),
    ("stahlbeton", r"stahlbeton|wu-?beton|fertigteil|sichtbeton"),
    ("leichtbeton", r"leichtbeton|porenbeton"),
    ("estrich", r"estrich"),
    ("beton", r"\bbeton"),
    ("daemmung_weich", r"mineralwolle [12]|glaswolle|zellulose|holzwolle|weich"),
    ("daemmung_hart", r"eps|xps|dämm|daemm|mineralwolle|entkopplung|trittschall"),
    ("gipsplatte", r"\bgk|gipskarton|ausbauplatte"),
    ("putz", r"putz|mörtel|moertel"),
    ("abdichtung", r"folie|sperrschicht|abdicht|trennschicht"),
    ("keramik", r"fliese|keramik"),
    ("bitumen", r"asphalt|bitumen"),
    ("textil", r"teppich"),
    ("kunststoff", r"linoleum|pvc|gummi|kunststoff"),
    ("holzwerkstoff", r"osb|span|sperrholz|brettschicht"),
    ("holz", r"holz|parkett|geländer"),
    ("metall", r"stahl|alu|kupfer|zink|stehfalz|metall|edelstahl"),
    ("glas", r"glas"),
    ("naturstein", r"marmor|(?<!kalk)sandstein|schiefer|granit|naturstein"),
    ("erdreich", r"erd|kies|sand|lehm|schüttung|wasser"),
]
_NAME_RE = [(k, re.compile(m, re.IGNORECASE)) for k, m in _NAME]


def klasse_fuer(name: str, kategorie: str = "", schraffur: str = "") -> str:
    """Vollautomatisch, immer eine Klasse (Vorgabe): Name → Archicad-Kategorie → Schraffurname → sonstiges."""
    n = (name or "").strip()
    if (kategorie or "").strip().lower() == "modellierung":
        return "sonstiges"                         # Hilfsbaustoffe (Bauteil_500 …) nie als echten Werkstoff deuten
    for k, r in _NAME_RE:
        if r.search(n):
            return k
    k = _KATEGORIE.get((kategorie or "").strip().lower())
    if k:
        return k
    for k, r in _NAME_RE:
        if r.search(schraffur or ""):
            return k
    return "sonstiges"


def textur_fuer(klasse: str, vorschlaege: Optional[dict[str, str]] = None) -> Optional[str]:
    """Textur (vollautomatisch): `klasse:<tk>`; `ambientcg:<id>`, wo `vorschlaege` (Texturklasse → ambientCG-Asset-ID) einen führt."""
    tk = KLASSEN[klasse][4]
    if tk is None:
        return None
    aid = (vorschlaege or {}).get(tk)
    return f"ambientcg:{aid}" if aid else f"klasse:{tk}"


def schraffur(klasse: str) -> dict:
    bez, kat, scharen, din, _ = KLASSEN[klasse]
    return {"klasse": klasse, "scharen": [{"winkel": w, "abstand_m": a, "gestrichelt": g} for w, a, g in scharen],
            "din": din}
