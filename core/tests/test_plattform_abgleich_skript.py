"""Tests für skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py — ohne Archicad (gefälschtes senden())."""
from __future__ import annotations

import datetime
import importlib.util
import json
import subprocess
import types
import urllib.error
from pathlib import Path

import pytest

SKRIPT = (Path(__file__).resolve().parents[2]
          / "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py")
_spec = importlib.util.spec_from_file_location("plattform_abgleich_skript", SKRIPT)
pa = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pa)


class Roh:
    """Antwort-Umschlag unverändert zurückgeben (für Fehlerfälle)."""

    def __init__(self, umschlag: dict):
        self.umschlag = umschlag


class FakeSenden:
    """Fälschung von senden(body). Beantwortet Befehle aus {Befehlsname: Antwort | Funktion(params) | Roh}.
    Befehlsname = commandName bei ExecuteAddOnCommand, sonst command. Alle Bodies landen in `.bodies`."""

    def __init__(self, antworten: dict):
        self.antworten = antworten
        self.bodies: list[dict] = []

    def namen(self) -> list[str]:
        return [self.name(b) for b in self.bodies]

    @staticmethod
    def name(body: dict) -> str:
        if body["command"] == "API.ExecuteAddOnCommand":
            return body["parameters"]["addOnCommandId"]["commandName"]
        return body["command"]

    def params(self, name: str) -> list[dict]:
        return [(b["parameters"]["addOnCommandParameters"] if b["command"] == "API.ExecuteAddOnCommand"
                 else b["parameters"]) for b in self.bodies if self.name(b) == name]

    def __call__(self, body: dict) -> dict:
        self.bodies.append(body)
        name = self.name(body)
        roh = body["command"] == "API.ExecuteAddOnCommand"
        params = body["parameters"]["addOnCommandParameters"] if roh else body["parameters"]
        if name not in self.antworten:
            raise AssertionError(f"unerwarteter Befehl {name}")
        a = self.antworten[name]
        a = a(params) if callable(a) else a
        if isinstance(a, Roh):
            return a.umschlag
        return {"succeeded": True, "result": {"addOnCommandResponse": a} if roh else a}


def guid(n: int) -> str:
    return f"00000000-0000-0000-0000-{n:012d}"


# ───────────────────────────────────────────── Katalog, Lesen, api

def test_katalog_nimmt_custom_und_userdefined_nicht_builtin():
    props = [{"propertyType": t, "propertyGroupName": "G", "propertyName": t, "propertyId": {"guid": t}}
             for t in ("Custom", "UserDefined", "BuiltIn")]
    ac = pa.Archicad(FakeSenden({"GetAllProperties": {"properties": props}}))
    assert sorted(pa.eigenschaften_katalog(ac)) == ["G.Custom", "G.UserDefined"]
    assert len(pa.eigenschaften_katalog(ac, alle=True)) == 3


def test_api_sendet_direkt_und_gibt_result_zurueck():
    f = FakeSenden({"API.GetStories": {"x": 1}})
    assert pa.Archicad(f).api("API.GetStories", {"a": 2}) == {"x": 1}
    assert f.bodies == [{"command": "API.GetStories", "parameters": {"a": 2}}]


def test_archicad_lesen_je_typ_ohne_getallelements():
    def details(p):
        # Archicad meldet hier einen anderen Typ-Namen als den abgefragten
        return {"detailsOfElements": [{"type": "Irgendwas", "id": "x", "floorIndex": 0} for _ in p["elements"]]}

    def je_typ(p):
        t = p["elementType"]
        return {"elements": [{"elementId": {"guid": guid(1 if t == "Wall" else 2)}}]}

    f = FakeSenden({
        "GetProjectInfo": {"projectName": "P", "isTeamwork": False},
        "GetStories": {"stories": [{"index": 0, "name": "EG"}]},
        "GetAllProperties": {"properties": []},
        "API.GetElementsByType": je_typ,
        "GetDetailsOfElements": details,
        "GetIFCIdsOfElements": lambda p: {"elementIFCIds": [{} for _ in p["elements"]]},
    })
    stand = pa.archicad_lesen(pa.Archicad(f), typen=("Wall", "Slab"))
    assert "GetAllElements" not in f.namen()
    assert f.params("API.GetElementsByType") == [{"elementType": "Wall"}, {"elementType": "Slab"}]
    assert [(e["guid"], e["typ"]) for e in stand["elemente"]] == [(guid(1), "Wall"), (guid(2), "Slab")]
    assert stand["elemente"][0]["geschoss"] == "EG"


def test_4001_wartet_dann_weiter():
    antworten = [Roh({"succeeded": False, "error": {"code": 4001, "message": "busy"}}),
                 Roh({"succeeded": False, "error": {"code": 4001, "message": "busy"}}), {"ok": 1}]
    schlaf: list[float] = []
    f = FakeSenden({"API.X": lambda p: antworten.pop(0)})
    assert pa.Archicad(f, schlafen=schlaf.append).api("API.X") == {"ok": 1}
    assert schlaf == [15, 15]


def test_4001_gibt_nach_60_versuchen_auf():
    f = FakeSenden({"API.X": Roh({"succeeded": False, "error": {"code": 4001, "message": "busy"}})})
    schlaf: list[float] = []
    with pytest.raises(pa.Fehler, match="gesperrt"):
        pa.Archicad(f, schlafen=schlaf.append).api("API.X")
    assert len(f.bodies) == 60


def test_4001_no_open_project_sofort_fehler():
    f = FakeSenden({"API.X": Roh({"succeeded": False,
                                  "error": {"code": 4001, "message": "There is no open project"}})})
    schlaf: list[float] = []
    with pytest.raises(pa.Fehler):
        pa.Archicad(f, schlafen=schlaf.append).api("API.X")
    assert schlaf == [] and len(f.bodies) == 1


# ───────────────────────────────────────────── herunterladen

def test_herunterladen_ssl_fallback_curl(monkeypatch):
    def kaputt(*a, **k):
        raise urllib.error.URLError("[SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed")

    aufrufe = []

    def fake_run(cmd, **k):
        aufrufe.append((cmd, k))
        return subprocess.CompletedProcess(cmd, 0, stdout=json.dumps({"a": 1}).encode(), stderr=b"")

    monkeypatch.setattr(pa.urllib.request, "urlopen", kaputt)
    monkeypatch.setattr(pa.subprocess, "run", fake_run)
    assert pa.herunterladen("https://example.test/x") == {"a": 1}
    assert aufrufe[0][0] == ["curl", "-sS", "--fail", "https://example.test/x"]
    assert not aufrufe[0][1].get("shell")


def test_herunterladen_weist_http_ab():
    with pytest.raises(pa.Fehler):
        pa.herunterladen("http://example.test/x")


# ───────────────────────────────────────────── stempeln

def _stempel_fake(klassifiziert_guids: set[str]):
    props = [{"propertyType": "Custom", "propertyGroupName": "Allgemeine Werte", "propertyName": n,
              "propertyId": {"guid": f"p-{n}"}} for n in ("Plattform-GUID", "KI generiert")]
    gespeichert: dict = {}

    def setzen(p):
        for x in p["elementPropertyValues"]:
            gespeichert[(x["elementId"]["guid"], x["propertyId"]["guid"])] = x["propertyValue"]["value"]
        return {"executionResults": [{"success": True} for _ in p["elementPropertyValues"]]}

    def lesen(p):
        return {"propertyValuesForElements": [
            {"propertyValues": [{"propertyValue": {"value": gespeichert.get((e["elementId"]["guid"],
                                                                             q["propertyId"]["guid"]))}}
                                for q in p["properties"]]} for e in p["elements"]]}

    def klass(p):
        return {"elementClassifications": [
            {"classificationIds": ([{"classificationId": {"classificationItemId": {"guid": "k"}}}]
                                   if e["elementId"]["guid"] in klassifiziert_guids else [{"classificationId": {}}])}
            for e in p["elements"]]}

    return FakeSenden({
        "GetAllProperties": {"properties": props},
        "GetProjectInfo": {"projectName": "P", "isTeamwork": True},
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"classificationSystemId": {"guid": "s1"}}]},
        "API.GetClassificationsOfElements": klass,
        "API.SetPropertyValuesOfElements": setzen,
        "GetPropertyValuesOfElements": lesen,
        "ReserveElements": {"conflicts": []},
    }), gespeichert


def test_stempeln_ohne_reservierung_volle_wertform_und_6703(tmp_path, monkeypatch):
    gebaut = [{"plattformGuid": "PG-1", "archicadGuid": guid(1)},
              {"plattformGuid": "PG-2", "archicadGuid": guid(2)}]
    datei = tmp_path / "gebaut.json"
    datei.write_text(json.dumps(gebaut))
    f, gespeichert = _stempel_fake({guid(1)})
    monkeypatch.setattr(pa, "_http", lambda port: f)
    o = types.SimpleNamespace(gebaut=str(datei), port=1, ausfuehren=True)
    aus = pa.cmd_stempeln(o)
    assert "ReserveElements" not in f.namen()
    heute = datetime.date.today().isoformat()
    ev = f.params("API.SetPropertyValuesOfElements")[0]["elementPropertyValues"]
    assert all(x["propertyValue"]["type"] == "string" and x["propertyValue"]["status"] == "normal" for x in ev)
    assert {x["elementId"]["guid"] for x in ev} == {guid(1)}
    assert gespeichert[(guid(1), "p-KI generiert")] == f"KI generiert {heute}"
    assert gespeichert[(guid(1), "p-Plattform-GUID")] == "PG-1"
    assert aus["geschrieben"] == 2
    gruende = [x["grund"] for x in aus["nicht_moeglich"]]
    assert gruende == ["nicht klassifiziert (6703)"]
    assert aus["nicht_moeglich"][0]["plattformGuid"] == "PG-2"


# ───────────────────────────────────────────── IFC → Morph (bauen)

#: Würfel, Flächen nach außen (positives Volumen); Punkte 0-3 unten, 4-7 oben
_WUERFEL = [(0, 3, 2), (0, 2, 1), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5),
            (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
_ECKEN = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]


def _wuerfel(versatz=(0, 0, 0), innen=False):
    P = [[a + versatz[0], b + versatz[1], c + versatz[2]] for a, b, c in _ECKEN]
    F = [list(reversed(f)) if innen else list(f) for f in _WUERFEL]
    return P, F


def test_zerlegen_trennt_koerper_die_nur_eine_kante_teilen():
    A, FA = _wuerfel()
    B, FB = _wuerfel((1, 1, 0))                       # teilt die senkrechte Kante A2-A6
    P = A + [p for i, p in enumerate(B) if i not in (0, 4)]
    karte = {0: 2, 4: 6}
    nxt = 8
    for i in range(8):
        if i not in karte:
            karte[i] = nxt
            nxt += 1
    F = [list(f) for f in FA] + [[karte[i] for i in f] for f in FB]
    F.append([0, 0, 1])                               # entartet: zwei gleiche Punkte
    koerper = pa.zerlegen(P, F)
    assert len(koerper) == 2
    for Pk, Fk in koerper:
        assert len(Fk) == 12 and len(Pk) == 8


def test_morph_body_dreht_innenliegende_flaechen_um():
    P, F = _wuerfel(innen=True)
    assert pa.volumen(P, F) < 0
    body, box = pa.morph_body([(P, F)])
    V = [[v["x"], v["y"], v["z"]] for v in body["vertices"]]
    Fo = [p["vertexIds"] for p in body["polygons"]]
    assert pa.volumen(V, Fo) > 0
    assert body["bodyType"] == "Solid" and set(body["vertices"][0]) == {"x", "y", "z"}
    assert box == [0, 0, 0, 1, 1, 1]


def test_zuordnung_und_status(tmp_path):
    z = pa.zuordnung_laden(pa.ZUORDNUNG)
    assert z["IfcPlate"] == "Bauelement - beliebig" and not any(k.startswith("_") or "?" in k for k in z)
    d = tmp_path / "z.json"
    d.write_text(json.dumps({"_hinweis": "x", "IfcWall?": "Wand"}))
    assert pa.zuordnung_laden(d) == {"IfcWall": "Wand"}
    assert [pa.status_archicad(x) for x in ("Abbruch", "Bestand", "Neubau", "?", None)] == [
        "ToBeDemolished", "Existing", "New", None, None]


STORIES = [{"index": -1, "level": -3.0}, {"index": 0, "level": 0.0}, {"index": 1, "level": 3.0}]


def test_geschoss_index():
    assert pa.geschoss_index(STORIES, 3.5) == 1
    assert pa.geschoss_index(STORIES, 2.9999) == 0
    assert pa.geschoss_index(STORIES, -9) == -1


def _bauteil(g, klasse="IfcSlab", name="B", ebene="E1"):
    return {"plattformGuid": g, "name": name, "ebene": ebene, "ifc": {"klasse": klasse}}


def test_bauplan_ausgelassen_und_filter():
    ga, gb, gc, gd = (pa.guid_lang("0" * 21 + c) for c in "1234")
    netze = {ga: [_wuerfel((0, 0, 3))], gb: [_wuerfel()], gd: [_wuerfel()]}
    bauliste = {"bauteile": [_bauteil(ga), _bauteil(gb, "IfcUnbekannt"), _bauteil(gc), _bauteil(gd)]}
    register = {"neu": [{"guid": ga.lower(), "status": "Neubau"}, {"guid": gb, "status": "Bestand"},
                        {"guid": gc, "status": "Neubau"}, {"guid": gd, "status": None}]}
    klassen = {"Decke": "k-decke"}
    plan, aus = pa.bauplan(bauliste, netze, register, {"IfcSlab": "Decke"}, klassen, STORIES)
    assert [p["g"] for p in plan] == [ga]
    p = plan[0]
    assert (p["klasse"], p["status"], p["ebene"]) == ("Decke", "New", "E1")
    assert p["soll"] == [0, 0, 3, 1, 1, 4] and p["morph"]["floorIndex"] == 1
    assert p["morph"]["basePoint"] == {"x": 0, "y": 0, "z": 0}
    gruende = {a["plattformGuid"]: a["grund"] for a in aus}
    assert gruende[gc] == "keine Geometrie im IFC"
    assert gruende[gb].startswith("Klasse/Status fehlt (IfcUnbekannt") and gruende[gd].startswith("Klasse/Status fehlt")
    # Filter
    plan2, aus2 = pa.bauplan(bauliste, netze, register, {"IfcSlab": "Decke"}, klassen, STORIES, nur={gb})
    assert plan2 == [] and [a["plattformGuid"] for a in aus2] == [gb]
    plan3, _ = pa.bauplan(bauliste, netze, register, {"IfcSlab": "Decke"}, klassen, STORIES, ohne=(ga,))
    assert plan3 == []


def _mini_ifc(pfad: Path, g1: str, g2: str, g3: str) -> None:
    pts = "(" + ",".join("(%s.,%s.,%s.)" % e for e in _ECKEN) + ")"
    fl = "(" + ",".join("(%d,%d,%d)" % tuple(i + 1 for i in f) for f in _WUERFEL) + ")"
    zeilen = [
        "ISO-10303-21;", "DATA;",
        f"#1=IFCCARTESIANPOINTLIST3D({pts});",
        f"#2=IFCTRIANGULATEDFACESET(#1,$,$,{fl},$);",
        "#3=IFCSHAPEREPRESENTATION(#90,'Body','Tessellation',(#2));",
        "#4=IFCPRODUCTDEFINITIONSHAPE($,$,(#3));",
        f"#5=IFCSLAB('{g1}',$,'S1',$,$,$,#4,$);",
        f"#6=IFCSLAB('{g2}',$,'S2',$,$,$,$,$);",
        f"#7=IFCPLATE('{g3}',$,'P1',$,$,$,#4,$);",
        f"#8=IFCRELAGGREGATES('{'0' * 21}9',#91,$,$,#6,(#7));",
        "ENDSEC;", "END-ISO-10303-21;"]
    pfad.write_text("\n".join(zeilen), encoding="latin-1")


def test_ifc_netze_mit_unterteil(tmp_path):
    k = ["0" * 21 + c for c in "123"]
    _mini_ifc(tmp_path / "m.ifc", *k)
    netze = pa.ifc_netze(tmp_path / "m.ifc")
    assert set(netze) == {pa.guid_lang(k[0]), pa.guid_lang(k[1]), pa.guid_lang(k[2])}
    assert len(netze[pa.guid_lang(k[1])]) == 1             # S2 hat nur über das Unterteil P1 ein Netz
    assert len(netze[pa.guid_lang(k[0])][0][1]) == 12


def test_bauen_trockenlauf_sendet_nichts_schreibendes(tmp_path):
    k = ["0" * 21 + c for c in "123"]
    _mini_ifc(tmp_path / "m.ifc", *k)
    g1, g2 = pa.guid_lang(k[0]), pa.guid_lang(k[1])
    bauliste = {"bauteile": [_bauteil(g1, name="S1"), _bauteil(g2, name="S2"), _bauteil(pa.guid_lang("0" * 21 + "7"))]}
    register = {"neu": [{"guid": g1, "status": "Neubau"}, {"guid": g2, "status": "Abbruch"}]}
    f = FakeSenden({
        "GetProjectInfo": {"projectName": "P"},
        "GetStories": {"stories": STORIES},
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"classificationSystemId": {"guid": "sys"}, "name": pa.SAB_SYSTEM}]},
        "API.GetAllClassificationsInSystem": {"classificationItems": [
            {"classificationItem": {"classificationItemId": {"guid": "k-decke"}, "id": "Decke"}, "children": []}]},
    })
    aus = pa.bauen(pa.Archicad(f), tmp_path, tmp_path / "m.ifc", bauliste, register, {"IfcSlab": "Decke"},
                   nur=None, ohne=(), vorhanden={}, ausfuehren=False)
    assert aus["trockenlauf"] is True and aus["zu_bauen"] == 2 and aus["ausgelassen"] == 1
    assert json.loads((tmp_path / "ausgelassen.json").read_text())[0]["grund"] == "keine Geometrie im IFC"
    for schreibend in ("CreateMorphs", "SetLayerOfElements", "API.SetClassificationsOfElements",
                       "API.SetPropertyValuesOfElements"):
        assert schreibend not in f.namen()
