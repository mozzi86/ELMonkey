"""Bearbeitungskern des Bauteilkatalogs — die EINZIGE Stelle, die Typen verändert (Spec §8).

MCP-Werkzeuge und das spätere Rad rufen nur `anwenden()` auf. Jede Änderung erzeugt einen neuen Typ bzw. eine
neue Version; der Grundbestand im Repo wird nie überschrieben:
- Grundbestand-Typ ändern → neuer Laufzeit-Typ `<schluessel>-<fingerabdruck[:6]>` mit `basiert_auf`.
- Laufzeit-Typ ändern     → neue Version desselben Schlüssels (Bauteile behalten ihre Version, Entscheidung 11).
- Rohling ändern          → neuer Typ, Name aus den Bestandteilen (Entscheidung 12).
- Ergebnis mit gleichem Aufbau wie ein vorhandener Typ → dieser Typ wird zurückgegeben (E3), nichts Neues.
"""
from __future__ import annotations

import copy
from typing import Any, Optional

from .modell import Katalog, KatalogFehler, benennen, doppelt, fingerabdruck, pruefen

OPERATIONEN = {
    "schicht_hinzufuegen": "{position (1…n+1, Standard ans Ende), baustoff, dicke_m, rolle?}",
    "schicht_loeschen": "{position} — die letzte Schicht bleibt (IfcMaterialLayerSet braucht ≥ 1)",
    "schicht_verschieben": "{von, nach}",
    "schicht_aendern": "{position, baustoff?, dicke_m?, rolle?}",
    "staenderwerk_setzen": "{position, profil (CW50|CW75|CW100|KVH…), achsabstand_m?, uw_anschluss?}",
    "staenderwerk_entfernen": "{position}",
    "querriegel_setzen": "{position, hoehen_m: [..]}",
    "eigenschaft_setzen": "{feld: IsExternal|LoadBearing|FireRating|AcousticRating|…, wert}",
    "oeffnung_aendern": "{breite_m?, hoehe_m?, oeffnungsart?} — nur Tür/Fenster",
    "umbenennen": "{kurzname} — fester Kurzname statt des erzeugten",
}


def _schichten(t: dict) -> list:
    if "schichten" not in t:
        raise KatalogFehler(f"{t['schluessel']} hat keine Schichten (Tür/Fenster: oeffnung_aendern)")
    return t["schichten"]


def _pos(t: dict, p: Any, plus: int = 0) -> int:
    n = len(_schichten(t))
    try:
        i = int(p)
    except (TypeError, ValueError):
        raise KatalogFehler(f"position muss eine Zahl 1…{n + plus} sein")
    if not 1 <= i <= n + plus:
        raise KatalogFehler(f"position {i} außerhalb 1…{n + plus}")
    return i - 1


def _eine(t: dict, k: Katalog, op: str, a: dict) -> None:
    if op == "schicht_hinzufuegen":
        k.baustoff(a.get("baustoff"))
        i = _pos(t, a.get("position", len(_schichten(t)) + 1), plus=1)
        neu = {"baustoff": a["baustoff"], "dicke_m": float(a.get("dicke_m", 0))}
        if a.get("rolle"):
            neu["rolle"] = a["rolle"]
        _schichten(t).insert(i, neu)
    elif op == "schicht_loeschen":
        if len(_schichten(t)) == 1:
            raise KatalogFehler("letzte Schicht kann nicht gelöscht werden — ein Schichtaufbau braucht mindestens eine")
        _schichten(t).pop(_pos(t, a.get("position")))
    elif op == "schicht_verschieben":
        s = _schichten(t)
        x = s.pop(_pos(t, a.get("von")))
        s.insert(_pos(t, a.get("nach"), plus=1) if int(a.get("nach", 0)) <= len(s) + 1 else len(s), x)
    elif op == "schicht_aendern":
        s = _schichten(t)[_pos(t, a.get("position"))]
        if "baustoff" in a:
            k.baustoff(a["baustoff"])
            s["baustoff"] = a["baustoff"]
        if "dicke_m" in a:
            s["dicke_m"] = float(a["dicke_m"])
        if "rolle" in a:
            s["rolle"] = a["rolle"] or None
    elif op == "staenderwerk_setzen":
        s = _schichten(t)[_pos(t, a.get("position"))]
        alt = s.get("staenderwerk") or {}
        s["staenderwerk"] = {"profil": a.get("profil", alt.get("profil")),
                             "achsabstand_m": float(a.get("achsabstand_m", alt.get("achsabstand_m", 0.625))),
                             "uw_anschluss": bool(a.get("uw_anschluss", alt.get("uw_anschluss", True))),
                             "querriegel": alt.get("querriegel", []), "norm": "DIN 18183-1"}
    elif op == "staenderwerk_entfernen":
        _schichten(t)[_pos(t, a.get("position"))].pop("staenderwerk", None)
    elif op == "querriegel_setzen":
        s = _schichten(t)[_pos(t, a.get("position"))]
        if not s.get("staenderwerk"):
            raise KatalogFehler("querriegel nur in einer Schicht mit staenderwerk")
        s["staenderwerk"]["querriegel"] = [{"hoehe_m": float(h)} for h in a.get("hoehen_m") or []]
    elif op == "eigenschaft_setzen":
        if not a.get("feld"):
            raise KatalogFehler("feld fehlt")
        t.setdefault("pset_common", {})[a["feld"]] = a.get("wert")
    elif op == "oeffnung_aendern":
        if t["ifc"]["klasse"] not in ("IfcDoor", "IfcWindow"):
            raise KatalogFehler("oeffnung_aendern nur für Tür/Fenster")
        for f in ("breite_m", "hoehe_m", "oeffnungsart"):
            if f in a:
                t[f] = a[f] if f == "oeffnungsart" else float(a[f])
    elif op == "umbenennen":
        t["kurzname_fest"] = str(a.get("kurzname") or "").strip()[:120] or None
    else:
        raise KatalogFehler(f"operation {op!r} unbekannt ({', '.join(OPERATIONEN)})")


def anwenden(k: Katalog, typ_ref: str, operationen: list[dict], autor: Optional[str] = None,
             zeit: Optional[str] = None) -> dict:
    """Wendet Operationen an und gibt {typ, neu (bool), grund} zurück. Schreibt NICHT — speichern tut `speicher`."""
    if not isinstance(operationen, list) or not operationen:
        raise KatalogFehler("operationen fehlt — [{op: …, …}]")
    alt = k.typ(typ_ref)
    t = copy.deepcopy(alt)
    for n, o in enumerate(operationen, 1):
        if not isinstance(o, dict) or not o.get("op"):
            raise KatalogFehler(f"operation {n}: {{op: …}} fehlt")
        try:
            _eine(t, k, o["op"], o)
        except KatalogFehler as e:
            raise KatalogFehler(f"operation {n} ({o['op']}): {e}")
    pruefen(t, k)
    if fingerabdruck(t) == fingerabdruck(alt) and t.get("kurzname_fest") == alt.get("kurzname_fest") \
            and t.get("pset_common") == alt.get("pset_common"):
        return {"typ": benennen(alt, k), "neu": False, "grund": "keine Änderung"}
    gleich = doppelt(t, k)
    if gleich and t.get("pset_common") == k.typ(gleich).get("pset_common"):
        return {"typ": benennen(k.typ(gleich), k), "neu": False, "grund": f"gleicher Aufbau wie {gleich} (E3)"}
    t["rohling"] = False
    t["aenderung"] = {"autor": autor, "zeit": zeit, "operationen": operationen,
                      "von": f"{alt['schluessel']}@{alt.get('version', 1)}"}
    if alt.get("ebene") == "laufzeit" and not alt.get("rohling"):
        t["version"] = max(k.versionen[alt["schluessel"]]) + 1
        grund = "neue Version"
    else:
        t["basiert_auf"] = f"{alt['schluessel']}@{alt.get('version', 1)}"
        t["schluessel"] = f"{alt['schluessel']}-{fingerabdruck(t)[:6]}"
        t["version"] = 1
        if "umbenennen" not in {o["op"] for o in operationen}:
            t.pop("kurzname_fest", None)       # der alte feste Kurzname beschreibt den neuen Aufbau nicht mehr
        grund = "neuer Typ aus Rohling" if alt.get("rohling") else "neuer Typ aus dem Grundbestand"
    t["ebene"] = "laufzeit"
    return {"typ": benennen(t, k), "neu": True, "grund": grund}


def nutzung(k: Katalog, bauteile: list[dict]) -> dict:
    """Welche Katalogtypen ein Modell benutzt (Bauteile mit `katalog {schluessel, version}`)."""
    from collections import Counter
    genutzt: Counter = Counter()
    veraltet, unbekannt, ohne = [], [], 0
    for b in bauteile:
        kat = b.get("katalog") or {}
        if not kat.get("schluessel"):
            ohne += 1
            continue
        s, v = kat["schluessel"], int(kat.get("version", 1))
        genutzt[f"{s}@{v}"] += 1
        if s not in k.versionen:
            unbekannt.append(b.get("id") or b.get("name"))
        elif v < max(k.versionen[s]):
            veraltet.append({"bauteil": b.get("id") or b.get("name"), "typ": s, "version": v, "neueste": max(k.versionen[s])})
    return {"typen": dict(genutzt), "veraltet": veraltet, "unbekannt": unbekannt, "ohne_katalogtyp": ohne}
