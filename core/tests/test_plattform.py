"""elmonkey.plattform mit einem Fake-Plattformmodul (ms) — ohne Plattform-Code."""
import pathlib
from types import SimpleNamespace

import pytest

import elmonkey as em
from elmonkey.plattform import Plattform, regeln_aus, stand_nach
from elmonkey.regeln import ElmFehler


class PlattformFehler(ValueError):
    pass


def _patch(op, e):
    if "wert" not in op:
        raise PlattformFehler("wert fehlt")
    props = e.get("properties") or {}
    if props.get("Plattform.Beschreibung") == op["wert"]:
        return {}, []
    return ({"properties": {"Plattform.Beschreibung": op["wert"]}},
            [{"feld": "Plattform.Beschreibung", "vorher": props.get("Plattform.Beschreibung"), "nachher": op["wert"]}])


def _anwenden(basis, saetze):
    out = []
    for b in basis:
        b = {**b, "properties": dict(b.get("properties") or {})}
        for s in saetze:
            b["properties"].update((s.get("patch") or {}).get(b["guid"], {}))
        out.append(b)
    return out


ms = SimpleNamespace(
    F_BESCHREIBUNG="Plattform.Beschreibung",
    STATUS={"bestand": "Bestand", "neubau": "Neubau", "abbruch": "Abbruch"},
    status_von=lambda e: (e.get("properties") or {}).get("Umbau"),
    beruehrt=lambda satz: set(satz.get("bauteile") or []),
    eigenschaften_patch=_patch,
    falten=lambda saetze, v: [s for s in saetze if s.get("status") == "aktiv"],
    anwenden=_anwenden,
)

A, B = "guid-a", "guid-b"
MODELL = [
    {"guid": A, "type": "IfcWall", "name": "A", "properties": {"Umbau": "Abbruch", "Verortung.Geschoss": "EG"}},
    {"guid": B, "type": "IfcWall", "name": "B", "properties": {"Umbau": "Neubau", "Status": "Abbruch"}},
]
ALLE = {e["guid"]: e for e in MODELL}


def test_regeln():
    r = regeln_aus(ms)
    assert r.f_geschoss == "Verortung.Geschoss"
    assert r.f_beschreibung == "Plattform.Beschreibung"
    assert r.status_werte == ms.STATUS and r.status_werte is not ms.STATUS
    assert r.status_von is ms.status_von and r.beruehrt is ms.beruehrt
    assert r.eigenschaften_patch is ms.eigenschaften_patch


def test_status_ueber_ms():
    p = Plattform(ms)
    assert [e["guid"] for e in p.finden(MODELL, {"status": "Abbruch"})] == [A]
    assert p.kurz(MODELL[1])["status"] == "Neubau"


def test_vorschlag_offen():
    b = Plattform(ms).vorpruefen([{"op": "eigenschaften_setzen", "guid": A, "wert": "x"}], ALLE, ALLE,
                                 offene_vorschlaege=[{"nr": 7, "bauteile": [A]}])
    assert [x["code"] for x in b] == ["VORSCHLAG_OFFEN"]
    assert "#7" in b[0]["text"]


def test_ungueltig():
    p = Plattform(ms)
    b = p.vorpruefen([{"op": "eigenschaften_setzen", "guid": A}], ALLE, ALLE)
    assert b[0]["code"] == "UNGUELTIG" and b[0]["text"] == "wert fehlt"
    assert p.blockiert(b) == b


def test_ops_ausrollen():
    ops, auswahl, befunde = Plattform(ms).ops_ausrollen(
        [{"op": "eigenschaften_setzen", "finden": {"status": "Abbruch"}, "wert": "x"}], ALLE, ALLE)
    assert ops == [{"op": "eigenschaften_setzen", "wert": "x", "guid": A}]
    assert auswahl[0]["anzahl"] == 1 and befunde == []


def test_stand_nach():
    basis = [{"guid": A, "properties": {}}]
    saetze = [{"id": "v1", "status": "vorschlag", "patch": {A: {"X": "1"}}}]
    kopie = [dict(s) for s in saetze]
    assert stand_nach(ms, basis, saetze, 1)[A]["properties"] == {}
    st = stand_nach(ms, basis, saetze, 1, als_aktiv="v1")
    assert st[A]["properties"] == {"X": "1"}
    assert saetze == kopie
    assert Plattform(ms).stand_nach(basis, saetze, 1, als_aktiv="v1") == st


def test_durchreichungen():
    assert Plattform.ElmFehler is ElmFehler and Plattform.HOECHSTENS == em.HOECHSTENS
    assert Plattform.BLOCKIEREND is em.BLOCKIEREND and Plattform.MIT_FINDEN is em.MIT_FINDEN


def test_kein_plattform_import():
    q = (pathlib.Path(em.__file__).parent / "plattform.py").read_text()
    code = [z for z in q.splitlines() if z.startswith(("import ", "from "))]
    assert not any("modell_stand" in z or z.startswith("from core") for z in code)
