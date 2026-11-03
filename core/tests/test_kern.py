"""ELMonkey-Kern gegen bekannte Wahrheit: ein Mini-Modell, dessen Inhalt hier feststeht.

Portiert aus der Schwarz Plattform (backend/tests/test_elmonkey.py, PR #95); die Tests, die dort den
Schreibweg der Plattform brauchen, bleiben in der Plattform.
"""
import pytest

import elmonkey as em
from elmonkey.regeln import F_STATUS, Regeln

TUER = "2N1qAqz7X0uQ8lT2hYb6sA"
SCHALTER = "3vB9kPq1Z4xW7mN2cR5tYe"
DECKE = "1aF4gH6jK8lM0nP2qR4sTu"
WAND = "0B3C9A11-6E2F-4C1D-9A55-0D7E3B1F2A01"


def _el(guid, typ, name, **props):
    return {"guid": guid, "type": typ, "name": name, "properties": props}


MODELL = [
    _el(TUER, "IfcDoor", "Tür 1.01", **{"Verortung.Geschoss": "OG - Obergeschoss", F_STATUS: "Neubau"}),
    _el(SCHALTER, "IfcSwitchingDevice", "Schalter", **{"Verortung.Geschoss": "OG - Obergeschoss", F_STATUS: "Neubau"}),
    _el(DECKE, "IfcCovering", "Unterdecke", **{"Verortung.Geschoss": "OG - Obergeschoss", F_STATUS: "Bestand"}),
    _el(WAND, "IfcWall", "IW 11,5 Trockenbau", **{"Verortung.Geschoss": "EG - Erdgeschoss",
                                                  "Pset_WallCommon.FireRating": "F30",
                                                  "Pset_WallCommon.Width": "0,115", F_STATUS: "Neubau"}),
]


def _index():
    return {e["guid"]: e for e in MODELL}


def _guids(treffer):
    return {e["guid"] for e in treffer}


# ─────────────────────────────────────────── Finden

def test_finden_und_oder_nicht():
    assert _guids(em.finden(MODELL, {"typ": "IfcDoor"})) == {TUER}
    assert _guids(em.finden(MODELL, {"typ": ["ifcdoor", "IfcWall"]})) == {TUER, WAND}
    assert _guids(em.finden(MODELL, {"oder": [{"typ": "IfcCovering"}, {"name": "schalter"}]})) == {DECKE, SCHALTER}
    assert _guids(em.finden(MODELL, {"nicht": {"geschoss": "Obergeschoss"}})) == {WAND}
    assert _guids(em.finden(MODELL, {"geschoss": "OG", "status": "Bestand"})) == {DECKE}   # zwei Schlüssel = und
    assert _guids(em.finden(MODELL, {"und": [{"status": ["neubau", "Abbruch"]},
                                            {"nicht": {"typ": "IfcSwitchingDevice"}}]})) == {TUER, WAND}


def test_finden_eigenschaften_und_zahlen():
    assert _guids(em.finden(MODELL, {"eigenschaft": "Pset_WallCommon.FireRating"})) == {WAND}
    assert _guids(em.finden(MODELL, {"eigenschaft": "Pset_WallCommon.FireRating", "wert": {"gleich": "f30"}})) == {WAND}
    assert not em.finden(MODELL, {"eigenschaft": "Pset_WallCommon.FireRating", "wert": {"gleich": "F90"}})
    assert _guids(em.finden(MODELL, {"eigenschaft": {"name": "Pset_WallCommon.FireRating", "vorhanden": False}})) \
        == {TUER, SCHALTER, DECKE}
    # Dezimalkomma wie in Archicad-Exporten
    assert _guids(em.finden(MODELL, {"eigenschaft": "Pset_WallCommon.Width", "wert": {"min": 0.1, "max": "0,12"}})) == {WAND}
    assert not em.finden(MODELL, {"eigenschaft": "Pset_WallCommon.Width", "wert": {"min": 0.2}})
    assert _guids(em.finden(MODELL, {"name": {"regex": r"^IW \d"}})) == {WAND}
    assert _guids(em.finden(MODELL, {"guids": [TUER.lower()]})) == {TUER}


def test_finden_meldet_falsche_kriterien():
    with pytest.raises(em.ElmFehler, match="unbekanntes Kriterium"):
        em.finden(MODELL, {"farbe": "rot"})
    with pytest.raises(em.ElmFehler, match="status"):
        em.finden(MODELL, {"status": "irgendwas"})
    with pytest.raises(em.ElmFehler, match="Liste"):
        em.finden(MODELL, {"und": {"typ": "IfcWall"}})
    with pytest.raises(em.ElmFehler, match="regex"):
        em.finden(MODELL, {"name": {"regex": "("}})


def test_schluessel_zaehlt_eigenschaften():
    s = {x["eigenschaft"]: x["bauteile"] for x in em.schluessel(MODELL)}
    assert s["Verortung.Geschoss"] == 4 and s["Pset_WallCommon.FireRating"] == 1


def test_eigene_regeln_fuer_andere_quellen():
    """Archicad: Geschoss und Status stehen woanders — der Kern folgt den Regeln, nicht festen Namen."""
    ac = [{"guid": "A1", "type": "Wall", "name": "W", "properties": {"Geschoss": "1. OG", "Renovierungsstatus": "Abbruch"}}]
    r = Regeln(f_geschoss="Geschoss", status_felder=("Renovierungsstatus",))
    assert _guids(em.finden(ac, {"geschoss": "OG", "status": "abbruch"}, r)) == {"A1"}
    assert not em.finden(ac, {"geschoss": "OG"})                     # Standard-Regeln kennen „Geschoss“ nicht


# ─────────────────────────────────────────── Vorprüfung

def test_ops_ausrollen_teilt_eigenschaften_je_bauteil():
    w = _index()
    ops, auswahl, befunde = em.ops_ausrollen(
        [{"op": "eigenschaften_setzen", "finden": {"geschoss": "OG"}, "status": "Abbruch"},
         {"op": "ausblenden", "finden": {"typ": "IfcWall"}}], w, w)
    assert not befunde
    assert sorted(o["guid"] for o in ops[:3]) == sorted([TUER, SCHALTER, DECKE])
    assert all("finden" not in o for o in ops)
    assert ops[3] == {"op": "ausblenden", "guids": [WAND]}
    assert [a["anzahl"] for a in auswahl] == [3, 1]


def test_ops_ausrollen_grenzen():
    w = _index()
    _, _, befunde = em.ops_ausrollen([{"op": "loeschen", "finden": {"typ": "IfcBeam"}}], w, w)
    assert befunde[0]["code"] == "NOT_FOUND"
    _, _, befunde = em.ops_ausrollen([{"op": "loeschen", "finden": {"geschoss": "OG"}}], w, w, hoechstens=2)
    assert befunde[0]["code"] == "ZU_VIELE" and em.blockiert(befunde)
    with pytest.raises(em.ElmFehler, match="nur bei"):
        em.ops_ausrollen([{"op": "ersetzen", "finden": {"typ": "IfcDoor"}}], w, w)
    with pytest.raises(em.ElmFehler, match="nicht beides"):
        em.ops_ausrollen([{"op": "loeschen", "finden": {"typ": "IfcDoor"}, "guids": [TUER]}], w, w)
    # wiederherstellen sucht unter den gelöschten
    wirksam = {g: e for g, e in w.items() if g != DECKE}
    ops, _, befunde = em.ops_ausrollen([{"op": "wiederherstellen", "finden": {"status": "Bestand"}}], wirksam, w)
    assert ops == [{"op": "wiederherstellen", "guids": [DECKE]}] and not befunde


def test_vorpruefen_codes():
    alle = _index()
    wirksam = {g: e for g, e in alle.items() if g != DECKE}
    wirksam[SCHALTER] = {**wirksam[SCHALTER], "ausgeblendet": True}
    ops = [{"op": "loeschen", "guids": ["GIBT-ES-NICHT", "Planung KW 42/tuer"]},
           {"op": "eigenschaften_setzen", "guid": DECKE, "name": "x"},
           {"op": "wiederherstellen", "guids": [TUER]},
           {"op": "ausblenden", "guids": [SCHALTER]},
           {"op": "einblenden", "guids": [TUER]},
           {"op": "eigenschaften_setzen", "guid": WAND, "status": "Neubau"},
           {"op": "eigenschaften_setzen", "guid": WAND, "status": "quatsch"},
           {"op": "eigenschaften_setzen", "guid": SCHALTER, "name": "Schalter neu"}]
    offen = [{"nr": 7, "guids": [WAND]}]
    b = em.vorpruefen(ops, wirksam, alle, skizzen={"tuer"}, offene_vorschlaege=offen)
    codes = {(x["op_nr"], x["guid"], x["code"]) for x in b}
    assert codes == {
        (1, "GIBT-ES-NICHT", "NOT_FOUND"),
        (2, DECKE, "GELOESCHT"),
        (3, TUER, "NO_EFFECT"),
        (4, SCHALTER, "NO_EFFECT"),
        (5, TUER, "NO_EFFECT"),
        (6, WAND, "NO_EFFECT"), (6, WAND, "VORSCHLAG_OFFEN"),
        (7, WAND, "UNGUELTIG"),                                     # blockiert — keine Hinweise mehr dazu
        (8, SCHALTER, "AUSGEBLENDET"),
    }
    assert {x["code"] for x in em.blockiert(b)} == {"NOT_FOUND", "GELOESCHT", "UNGUELTIG"}


def test_neutraler_patch_psets_und_eigenschaften():
    from elmonkey import STANDARD
    patch, diff = STANDARD.patch({"name": "Tür T30", "psets": {"Pset_DoorCommon": {"FireRating": "T30-RS"}},
                                  "eigenschaften": {"Bauteilname": "T 1.01"}}, MODELL[0])
    assert patch["name"] == "Tür T30"
    assert patch["properties"] == {"IFC.Name": "Tür T30", "Pset_DoorCommon.FireRating": "T30-RS", "Bauteilname": "T 1.01"}
    assert {d["feld"] for d in diff} == {"name", "IFC.Name", "Pset_DoorCommon.FireRating", "Bauteilname"}


# ─────────────────────────────────────────── Rücklese

def test_ruecklese_ok_und_abweichung():
    vorher = _index()
    vorschau = [{"op": "eigenschaften_setzen", "guid": TUER, "aenderungen": [{"feld": "name", "nachher": "Tür T30"}]},
                {"op": "loeschen", "guid": DECKE},
                {"op": "ausblenden", "guid": SCHALTER}]
    gut = {g: e for g, e in vorher.items() if g != DECKE}
    gut[TUER] = {**gut[TUER], "name": "Tür T30"}
    gut[SCHALTER] = {**gut[SCHALTER], "ausgeblendet": True}
    r = em.ruecklesen(vorschau, vorher, gut)
    assert r["verifiziert"] and r["ok"] == 3 and r["abweichend"] == 0
    # verfälschter Stand: Decke noch da, falscher Name — die Rücklese glaubt nicht der Antwort
    schlecht = {**gut, DECKE: vorher[DECKE], TUER: {**gut[TUER], "name": "Tür T31"}}
    r = em.ruecklesen(vorschau, vorher, schlecht)
    assert not r["verifiziert"] and r["abweichend"] == 2
    falsch = {z["guid"]: z for z in r["abweichungen"]}
    assert falsch[DECKE]["text"] == "ist noch im wirksamen Stand"
    assert {"feld": "name", "soll": "Tür T30", "ist": "Tür T31"} in falsch[TUER]["felder"]


def test_ruecklese_geometrie_und_skizze():
    alt = {"guid": TUER, "vertices": [0, 0, 0, 1, 0, 0]}
    vorschau = [{"op": "geometrie", "werkzeug": "verschieben", "guid": TUER},
                {"op": "loeschen", "skizze": "eb1/tuer", "guid": "eb1/tuer"}]
    bewegt = em.ruecklesen(vorschau, {TUER: alt}, {TUER: {**alt, "vertices": [1, 0, 0, 2, 0, 0]}})
    assert bewegt["verifiziert"] and bewegt["ungeprueft"] == 1
    stehen = em.ruecklesen(vorschau, {TUER: alt}, {TUER: alt})
    assert stehen["abweichungen"][0]["text"] == "Geometrie unverändert"
