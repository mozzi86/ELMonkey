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
