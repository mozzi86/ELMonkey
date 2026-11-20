"""Tests für skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py — ohne Archicad (gefälschtes senden())."""
from __future__ import annotations

import datetime
import importlib.util
import json
import os
import shutil
import subprocess
import sys
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


# ───────────────────────────────────────────── ablegen

_LINK = "https://example.test/blob?sig=GEHEIM"


def _zert_kaputt(*a, **k):
    raise urllib.error.URLError("[SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed")


def test_ablegen_ssl_fallback_curl(monkeypatch):
    aufrufe = []

    def fake_run(cmd, **k):
        pfad = cmd[cmd.index("--data-binary") + 1][1:]
        aufrufe.append((cmd, k, Path(pfad).read_bytes(), pfad))
        return subprocess.CompletedProcess(cmd, 0, stdout=b"201", stderr=b"")

    monkeypatch.setattr(pa.urllib.request, "urlopen", _zert_kaputt)
    monkeypatch.setattr(pa.subprocess, "run", fake_run)
    pa.ablegen(_LINK, b"DATEN", "application/json")
    cmd, k, inhalt, pfad = aufrufe[0]
    assert cmd[0] == "curl" and cmd[cmd.index("-X") + 1] == "PUT"
    assert "x-ms-blob-type: BlockBlob" in cmd and "Content-Type: application/json" in cmd
    assert cmd[-1] == _LINK
    assert inhalt == b"DATEN"
    assert not k.get("shell")
    assert not Path(pfad).exists()


def test_ablegen_curl_403_fehler_ohne_signatur(monkeypatch):
    def fake_run(cmd, **k):
        return subprocess.CompletedProcess(cmd, 0, stdout=b"403", stderr=b"denied " + _LINK.encode())

    monkeypatch.setattr(pa.urllib.request, "urlopen", _zert_kaputt)
    monkeypatch.setattr(pa.subprocess, "run", fake_run)
    with pytest.raises(pa.Fehler) as e:
        pa.ablegen(_LINK, b"x", "application/json")
    assert "403" in str(e.value) and "sig=" not in str(e.value)


def test_ablegen_anderer_urlerror_nicht_ueber_curl(monkeypatch):
    def refused(*a, **k):
        raise urllib.error.URLError("Connection refused")

    def kein_run(*a, **k):
        raise AssertionError("curl darf nicht laufen")

    monkeypatch.setattr(pa.urllib.request, "urlopen", refused)
    monkeypatch.setattr(pa.subprocess, "run", kein_run)
    with pytest.raises(urllib.error.URLError):
        pa.ablegen(_LINK, b"x", "application/json")


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


def _voxel_netz(zellen):
    """Oberflaeche aus Einheitszellen (Gitterpunkte teilen sich einen Index), Flaechen nach aussen."""
    zellen = set(zellen)
    idx: dict = {}
    P: list = []
    F: list = []

    def pt(p):
        if p not in idx:
            idx[p] = len(P)
            P.append(list(p))
        return idx[p]

    for c in sorted(zellen):
        for a in range(3):
            for s in (1, -1):
                nb = list(c)
                nb[a] += s
                if tuple(nb) in zellen:
                    continue
                u, v = (a + 1) % 3, (a + 2) % 3
                ecken = []
                for du, dv in ((0, 0), (1, 0), (1, 1), (0, 1)):
                    p = list(c)
                    p[a] += 1 if s == 1 else 0
                    p[u] += du
                    p[v] += dv
                    ecken.append(pt(tuple(p)))
                if s == -1:
                    ecken.reverse()
                F.append([ecken[0], ecken[1], ecken[2]])
                F.append([ecken[0], ecken[2], ecken[3]])
    return P, F


def _kanten_zaehler(F):
    z: dict = {}
    for f in F:
        for a in range(3):
            k = tuple(sorted((f[a], f[(a + 1) % 3])))
            z[k] = z.get(k, 0) + 1
    return z


def test_zerlegen_loest_mehrfachkante_auf():
    zellen = [(0, 0, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)]
    P, F = _voxel_netz(zellen)
    assert max(_kanten_zaehler(F).values()) == 4          # Fixture: eine Kante mit 4 Dreiecken
    koerper = pa.zerlegen(P, F)
    assert len(koerper) == 1
    Pk, Fk = koerper[0]
    assert max(_kanten_zaehler(Fk).values()) <= 2
    assert len(Fk) == len(F)
    assert len(Pk) == len(P) + 2
    assert abs(pa.volumen(Pk, Fk)) == pytest.approx(6.0)


def test_mannigfaltig_laesst_wuerfel_unveraendert():
    P, F = _wuerfel()
    koerper = pa.zerlegen(P, F)
    assert len(koerper) == 1 and len(koerper[0][1]) == 12 and len(koerper[0][0]) == 8
    P2, F2 = pa.mannigfaltig(P, F)
    assert len(P2) == 8 and [list(f) for f in F2] == [list(f) for f in F]


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


def test_sab_klassen_live_antwortform_kinder_am_item_und_details_schluessel():
    """Antwortform live BFW07 19.11.2026 (AC29, Tapir 1.7.2): Kinder hängen am classificationItem, die Ids kommen
    erst über GetDetailsOfClassificationItems unter dem Schlüssel „classificationItems“."""
    baum = {"classificationItems": [{"classificationItem": {
        "classificationItemId": {"guid": guid(10)},
        "children": [{"classificationItem": {"classificationItemId": {"guid": guid(11)},
                                             "children": [{"classificationItem": {
                                                 "classificationItemId": {"guid": guid(12)}}}]}}]}}]}
    namen = {guid(10): "ELEMENTE", guid(11): "Komponente der TGA", guid(12): "Lüftungskanal"}
    ac = pa.Archicad(FakeSenden({
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"name": "SAB_Klassifizierung_29", "classificationSystemId": {"guid": guid(1)}}]},
        "API.GetAllClassificationsInSystem": baum,
        "API.GetDetailsOfClassificationItems": lambda p: {"classificationItems": [
            {"classificationItem": {"id": namen[x["classificationItemId"]["guid"]]}}
            for x in p["classificationItemIds"]]},
    }))
    sid, klassen, sysname = pa.sab_klassen(ac)
    assert sysname == "SAB_Klassifizierung_29"
    assert sid == guid(1)
    assert klassen == {"ELEMENTE": guid(10), "Komponente der TGA": guid(11), "Lüftungskanal": guid(12)}


def test_ifc_netze_aggregation_ohne_ownerhistory(tmp_path):
    """Plattform-IFC (live BFW07 19.11.2026): OwnerHistory ist „$“ — dann ist der ERSTE Verweis das Elternteil.
    Vorher hingen die Kinder am ersten Geschwister, Verbünde (Kopf ohne Körper) blieben leer."""
    k = ["0" * 21 + c for c in "123"]
    _mini_ifc(tmp_path / "m.ifc", *k)
    p = tmp_path / "m.ifc"
    p.write_text(p.read_text(encoding="latin-1").replace(",#91,$,$,#6,(#7));", ",$,$,$,#6,(#7,#5));"),
                 encoding="latin-1")
    netze = pa.ifc_netze(p)
    assert len(netze[pa.guid_lang(k[1])]) == 2               # Kopf S2: Körper von P1 und S1
    assert len(netze[pa.guid_lang(k[2])]) == 1               # P1 bekommt keine Geschwister angehängt


# ───────────────────────────────────────────── Verbünde: Kopf + Unterteile

def _verbund_ifc(pfad: Path, gk: str, ga: str, gb: str) -> None:
    pts = "(" + ",".join("(%s.,%s.,%s.)" % e for e in _ECKEN) + ")"
    fl = "(" + ",".join("(%d,%d,%d)" % tuple(i + 1 for i in f) for f in _WUERFEL) + ")"
    zeilen = [
        "ISO-10303-21;", "DATA;",
        f"#1=IFCCARTESIANPOINTLIST3D({pts});",
        f"#2=IFCTRIANGULATEDFACESET(#1,$,$,{fl},$);",
        "#3=IFCSHAPEREPRESENTATION(#90,'Body','Tessellation',(#2));",
        "#4=IFCPRODUCTDEFINITIONSHAPE($,$,(#3));",
        f"#5=IFCBUILDINGELEMENTPROXY('{gk}',$,'Kopf',$,$,$,$,$);",
        f"#6=IFCPIPESEGMENT('{ga}',$,'Kind A',$,$,$,#4,$);",
        f"#7=IFCCOVERING('{gb}',$,'K\\X2\\00E4\\X0\\lte',$,$,$,#4,$);",
        f"#8=IFCRELAGGREGATES('{'0' * 21}9',$,$,$,#5,(#6,#7));",
        "ENDSEC;", "END-ISO-10303-21;"]
    pfad.write_text("\n".join(zeilen), encoding="latin-1")


def _verbund_setup(tmp_path):
    k = ["0" * 21 + c for c in "ABC"]
    _verbund_ifc(tmp_path / "v.ifc", *k)
    gk, ga, gb = (pa.guid_lang(x) for x in k)
    bauliste = {"bauteile": [_bauteil(gk, "IfcBuildingElementProxy", "Kopf")]}
    register = {"neu": [{"guid": gk, "status": "Neubau"}]}
    return gk, ga, gb, bauliste, register


def test_ifc_teile_eigene_kinder_info(tmp_path):
    gk, ga, gb, _, _ = _verbund_setup(tmp_path)
    eigene, kinder, info = pa.ifc_teile(tmp_path / "v.ifc")
    assert set(eigene) == {ga, gb} and gk not in eigene
    assert all(len(v) == 1 for v in eigene.values())
    assert kinder == {gk: [ga, gb]}
    assert info[gb]["name"] == "Kälte"
    assert info[ga]["klasse"] == "IfcPipeSegment" and info[gb]["klasse"] == "IfcCovering"
    assert len(pa.ifc_netze(tmp_path / "v.ifc")[gk]) == 2


def test_bauplan_verbund_einzeln_zusammen_filter(tmp_path):
    gk, ga, gb, bauliste, register = _verbund_setup(tmp_path)
    teile = pa.ifc_teile(tmp_path / "v.ifc")
    z = {"IfcBuildingElementProxy": "Decke", "IfcPipeSegment": "Rohr"}
    kl = {"Decke": "k1", "Rohr": "k2"}
    plan, aus = pa.bauplan(bauliste, teile, register, z, kl, STORIES)
    assert aus == [] and len(plan) == 2
    assert all(p["kopf"] == gk and p["ebene"] == "E1" and p["status"] == "New" for p in plan)
    por = {p["g"]: p for p in plan}
    assert por[ga]["klasse"] == "Rohr" and por[gb]["klasse"] == "Decke"
    assert por[ga]["name"] == "Kopf / Kind A" and por[gb]["name"] == "Kopf / Kälte"
    plan_z, _ = pa.bauplan(bauliste, teile, register, z, kl, STORIES, verbund="zusammen")
    assert len(plan_z) == 1 and plan_z[0]["g"] == gk and plan_z[0]["kopf"] is None
    assert pa.bauplan(bauliste, teile, register, z, kl, STORIES, ohne=(gk,))[0] == []
    assert [p["g"] for p in pa.bauplan(bauliste, teile, register, z, kl, STORIES, ohne=(ga,))[0]] == [gb]
    assert [p["g"] for p in pa.bauplan(bauliste, teile, register, z, kl, STORIES, nur={gb})[0]] == [gb]
    with pytest.raises(pa.Fehler):
        pa.bauplan(bauliste, teile, register, z, kl, STORIES, verbund="x")


def _bau_fake(gebaut_ids: dict, mit_verbund: bool, katalog_klasse="Decke"):
    """Gefälschtes Archicad für bauen(): schreibt Eigenschaften in ein Dict und liest sie zurück."""
    gespeichert: dict = {}
    reihe = iter(range(1, 100))
    boxen: list = []
    PID = {"Plattform-GUID": "p-guid", "KI generiert": "p-ki", "Plattform-Verbund": "p-verbund"}

    def prop_ids(p):
        aus = []
        for q in p["properties"]:
            n = (q.get("localizedName") or [None, q.get("nonLocalizedName")])[1]
            if n == "Plattform-Verbund" and not mit_verbund:
                aus.append({"error": {"code": 1, "message": "nicht da"}})
            else:
                aus.append({"propertyId": {"guid": PID.get(n, "p-status")}})
        return {"properties": aus}

    def morphs(p):
        els = []
        for m in p["morphsData"]:
            els.append({"elementId": {"guid": guid(next(reihe))}})
            boxen.append(None)
        gebaut_ids["els"] = gebaut_ids.get("els", []) + els
        return {"elements": els}

    def setzen(p):
        for x in p["elementPropertyValues"]:
            gespeichert[(x["elementId"]["guid"], x["propertyId"]["guid"])] = x["propertyValue"]["value"]
        return {}

    def lesen(p):
        return {"propertyValuesForElements": [{"propertyValues": [
            {"propertyValue": {"value": gespeichert.get((e["elementId"]["guid"], q["propertyId"]["guid"]))}}
            for q in p["properties"]]} for e in p["elements"]]}

    def bbox(p):
        return {"boundingBoxes3D": [{"boundingBox3D": dict(zip(
            ("xMin", "yMin", "zMin", "xMax", "yMax", "zMax"), gebaut_ids["box"]))} for _ in p["elements"]]}

    return FakeSenden({
        "GetProjectInfo": {"projectName": "P"},
        "GetStories": {"stories": STORIES},
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"classificationSystemId": {"guid": "sys"}, "name": pa.SAB_SYSTEM}]},
        "API.GetAllClassificationsInSystem": {"classificationItems": [
            {"classificationItem": {"classificationItemId": {"guid": "k-decke"}, "id": "Decke"}, "children": []}]},
        "API.GetPropertyIds": prop_ids,
        "CreateMorphs": morphs,
        "SetLayerOfElements": {},
        "API.SetClassificationsOfElements": {},
        "API.SetPropertyValuesOfElements": setzen,
        "API.Get3DBoundingBoxes": bbox,
        "GetPropertyValuesOfElements": lesen,
        "API.GetPropertyValuesOfElements": lesen,
        "API.GetClassificationsOfElements": lambda p: {"elementClassifications": [
            {"classificationIds": [{"classificationId": {"classificationItemId": {"guid": "k-decke"}}}]}
            for _ in p["elements"]]},
        "GetDetailsOfElements": lambda p: {"detailsOfElements": [
            {"floorIndex": 0, "details": {"body": {"isClosed": True}}} for _ in p["elements"]]},
    }), gespeichert


def test_bauen_verbund_trockenlauf(tmp_path):
    gk, ga, gb, bauliste, register = _verbund_setup(tmp_path)
    f, _ = _bau_fake({}, True)
    aus = pa.bauen(pa.Archicad(f), tmp_path, tmp_path / "v.ifc", bauliste, register, {"IfcBuildingElementProxy": "Decke"},
                   nur=None, ohne=(), vorhanden={}, ausfuehren=False)
    assert aus["zu_bauen"] == 2 and aus["verbuende"] == 1 and aus["unterteile"] == 2
    for schreibend in ("CreateMorphs", "SetLayerOfElements", "API.SetPropertyValuesOfElements"):
        assert schreibend not in f.namen()


def test_bauen_verbund_ausfuehren_stempelt_kopf(tmp_path):
    gk, ga, gb, bauliste, register = _verbund_setup(tmp_path)
    info = {"box": [0, 0, 0, 1, 1, 1]}
    f, gespeichert = _bau_fake(info, True)
    aus = pa.bauen(pa.Archicad(f), tmp_path, tmp_path / "v.ifc", bauliste, register, {"IfcBuildingElementProxy": "Decke"},
                   nur=None, ohne=(), vorhanden={}, ausfuehren=True)
    assert aus["gebaut"] == 2 and aus["stimmt"] == 2 and aus["verbuende"] == 1 and aus["unterteile"] == 2
    assert "hinweis" not in aus
    ev = f.params("API.SetPropertyValuesOfElements")[0]["elementPropertyValues"]
    guids = {x["propertyValue"]["value"] for x in ev if x["propertyId"]["guid"] == "p-guid"}
    koepfe = [x["propertyValue"]["value"] for x in ev if x["propertyId"]["guid"] == "p-verbund"]
    assert guids == {ga, gb} and koepfe == [gk, gk]
    for datei in ("gebaut.json", "angelegt.json"):
        assert all(x["kopf"] == gk for x in json.loads((tmp_path / datei).read_text()))
    assert all(x["kopf"] == gk for x in json.loads((tmp_path / "ruecklese.json").read_text())["rueck"])


def test_bauen_ohne_verbund_eigenschaft_nur_hinweis(tmp_path):
    gk, ga, gb, bauliste, register = _verbund_setup(tmp_path)
    f, gespeichert = _bau_fake({"box": [0, 0, 0, 1, 1, 1]}, False)
    aus = pa.bauen(pa.Archicad(f), tmp_path, tmp_path / "v.ifc", bauliste, register, {"IfcBuildingElementProxy": "Decke"},
                   nur=None, ohne=(), vorhanden={}, ausfuehren=True)
    assert aus["stimmt"] == 2 and "Plattform-Verbund fehlt – Kopf nur in gebaut.json" in aus["hinweis"]
    assert all(k[1] != "p-verbund" for k in gespeichert)
    assert all(x["kopf"] == gk for x in json.loads((tmp_path / "gebaut.json").read_text()))


def test_cli_verbund_vorgabe_einzeln(monkeypatch, tmp_path, capsys):
    gesehen = []
    monkeypatch.setattr(pa, "cmd_bauen", lambda o: gesehen.append(o.verbund) or {})
    base = ["bauen", "--ordner", str(tmp_path), "--ifc", "a", "--bauliste", "b", "--register", "c"]
    assert pa.main(base) == 0 and pa.main(base + ["--verbund", "zusammen"]) == 0
    assert gesehen == ["einzeln", "zusammen"]


# ───────────────────────────────────────── Neue Bauliste (sab-bauliste/1) mit echten Netzen

def _neu_bauteil(g, koerper=None, geometrie="netz", status="offen", meldung=None, kennzahlen=None,
                 klasse="IfcSlab", name="N", ebene="E1"):
    ks = [{"punkte": P, "dreiecke": F} for P, F in (koerper if koerper is not None else [_wuerfel()])]
    return {"plattformGuid": g, "name": name, "ebene": ebene, "ifc": {"klasse": klasse, "predefined": None},
            "werkzeug": "Morph", "parameter": {"koerper": ks}, "geometrie": geometrie, "status": status,
            "meldung": meldung, "netz_kennzahlen": kennzahlen or {}}


def _liste(*bauteile, nicht=(), gruende=None):
    return {"format": "sab-bauliste/1", "projekt": "P", "bauteile": list(bauteile),
            "nicht_bearbeitet": list(nicht), "nicht_bearbeitet_grund": gruende or {}}


def test_bauliste_laden_zusammenfuehren_spaetere_gewinnt(tmp_path):
    g1, g2, g3 = (pa.guid_lang("0" * 21 + c) for c in "123")
    (tmp_path / "a.json").write_text(json.dumps(_liste(_neu_bauteil(g1, name="alt"), _neu_bauteil(g2),
                                                       nicht=[g3], gruende={g3: "zu groß"})))
    (tmp_path / "b.json").write_text(json.dumps(_liste(_neu_bauteil(g1.lower(), name="neu"), _neu_bauteil(g3))))
    b = pa.bauliste_laden([str(tmp_path / "a.json"), str(tmp_path / "b.json")])
    assert [x["name"] for x in b["bauteile"]] == ["neu", "N", "N"] and len(b["bauteile"]) == 3
    assert b["nicht_bearbeitet"] == []          # g3 nachgeholt
    assert b["quellen"] == 2
    einzeln = pa.bauliste_laden(str(tmp_path / "a.json"))
    assert einzeln["nicht_bearbeitet"] == [g3.upper()] and einzeln["nicht_bearbeitet_grund"][g3.upper()] == "zu groß"


def test_bauliste_laden_link_altes_und_fremdes_format(tmp_path, monkeypatch):
    g1 = pa.guid_lang("0" * 21 + "1")
    gesehen = []
    monkeypatch.setattr(pa, "herunterladen", lambda l: gesehen.append(l) or _liste(_neu_bauteil(g1)))
    assert len(pa.bauliste_laden(["https://x/y?sig=1"])["bauteile"]) == 1 and gesehen == ["https://x/y?sig=1"]
    monkeypatch.undo()
    with pytest.raises(pa.Fehler):
        pa.bauliste_laden(["http://x/y"])
    (tmp_path / "alt.json").write_text(json.dumps({"bauteile": [_bauteil(g1)]}))
    assert len(pa.bauliste_laden([str(tmp_path / "alt.json")])["bauteile"]) == 1
    (tmp_path / "fremd.json").write_text(json.dumps({"format": "anderes/1", "bauteile": []}))
    with pytest.raises(pa.Fehler):
        pa.bauliste_laden([str(tmp_path / "fremd.json")])


def test_bauliste_netze_nur_mit_koerper():
    g1, g2 = (pa.guid_lang("0" * 21 + c) for c in "12")
    b = _liste(_neu_bauteil(g1, koerper=[_wuerfel(), _wuerfel((3, 0, 0))]), _neu_bauteil(g2, koerper=[]))
    n = pa.bauliste_netze(b)
    assert list(n) == [g1.upper()] and len(n[g1.upper()]) == 2


def test_nicht_bearbeitet_pruefen():
    g = pa.guid_lang("0" * 21 + "1")
    b = {"nicht_bearbeitet": [g], "nicht_bearbeitet_grund": {g: "zu groß"}}
    with pytest.raises(pa.Fehler) as e:
        pa.nicht_bearbeitet_pruefen(b)
    assert g in str(e.value) and "zu groß" in str(e.value) and "archicad_abgleich_bauliste" in str(e.value)
    pa.nicht_bearbeitet_pruefen(b, trotzdem=True)
    pa.nicht_bearbeitet_pruefen({"nicht_bearbeitet": []})


_Z = {"IfcSlab": "Decke", "IfcBuildingElementProxy": "Decke", "IfcPipeSegment": "Rohr"}
_K = {"Decke": "k1", "Rohr": "k2"}


def test_bauplan_aus_bauliste_ohne_ifc():
    g = pa.guid_lang("0" * 21 + "1")
    kz = {"nicht_mannigfaltige_kanten": 1, "offene_kanten": 0}
    bl = _liste(_neu_bauteil(g, koerper=[_wuerfel(), _wuerfel((3, 0, 0))], kennzahlen=kz))
    plan, aus = pa.bauplan(bl, None, {"neu": [{"guid": g, "status": "Neubau"}]}, _Z, _K, STORIES)
    assert aus == [] and len(plan) == 1
    p = plan[0]
    assert p["quelle"] == "bauliste" and p["kopf"] is None and p["netz_kennzahlen"] == kz
    assert len(p["morph"]["body"]["polygons"]) == 24 and p["soll"] == [0, 0, 0, 4, 1, 1]
    pr = pa.netz_pruefung(plan)
    assert pr["quelle"] == {"bauliste": 1, "ifc": 0}
    assert pr["netz_kennzahlen"] == {"nicht_mannigfaltige_kanten": 1, "offene_kanten": 0}
    assert pr["netz_pruefen"] == [{"plattformGuid": g, "name": "N", "nicht_mannigfaltige_kanten": 1,
                                   "offene_kanten": 0}]


def test_bauplan_status_fehler_und_huelle():
    g1, g2, g3 = (pa.guid_lang("0" * 21 + c) for c in "123")
    bl = _liste(_neu_bauteil(g1, status="fehler", meldung="kaputt"),
                _neu_bauteil(g2, koerper=[], geometrie="huelle"),
                _neu_bauteil(g3, koerper=[], geometrie="werkzeug"))
    reg = {"neu": [{"guid": x, "status": "Neubau"} for x in (g1, g2, g3)]}
    plan, aus = pa.bauplan(bl, None, reg, _Z, _K, STORIES)
    assert plan == []
    gr = {a["plattformGuid"]: a["grund"] for a in aus}
    assert gr == {g1: "status fehler: kaputt", g2: "nur Hülle (geometrie=huelle)", g3: "nur Hülle (geometrie=werkzeug)"}
    # status fehler auch mit IFC-Netz
    plan2, aus2 = pa.bauplan(bl, {g1: [_wuerfel()]}, reg, _Z, _K, STORIES)
    assert plan2 == [] and {a["plattformGuid"]: a["grund"] for a in aus2}[g1] == "status fehler: kaputt"
    assert {a["plattformGuid"]: a["grund"] for a in aus2}[g2] == "keine Geometrie im IFC"


def test_bauplan_ifc_unterteile_gewinnen_gegen_bauliste_koerper(tmp_path):
    gk, ga, gb, _, register = _verbund_setup(tmp_path)
    bl = _liste(_neu_bauteil(gk, klasse="IfcBuildingElementProxy", name="Kopf",
                             koerper=[_wuerfel(), _wuerfel((3, 0, 0))]))
    teile = pa.ifc_teile(tmp_path / "v.ifc")
    plan, _ = pa.bauplan(bl, teile, register, _Z, _K, STORIES)
    assert {p["g"] for p in plan} == {ga, gb} and all(p["kopf"] == gk and p["quelle"] == "ifc" for p in plan)
    plan_z, _ = pa.bauplan(bl, teile, register, _Z, _K, STORIES, verbund="zusammen")
    assert len(plan_z) == 1 and plan_z[0]["quelle"] == "bauliste" and plan_z[0]["g"] == gk


def test_bauplan_ifc_netz_ohne_koerper_wie_bisher(tmp_path):
    g = pa.guid_lang("0" * 21 + "1")
    bl = _liste(_neu_bauteil(g, koerper=[], geometrie="huelle"))
    plan, aus = pa.bauplan(bl, {g: [_wuerfel()]}, {"neu": [{"guid": g, "status": "Neubau"}]}, _Z, _K, STORIES)
    assert aus == [] and plan[0]["quelle"] == "ifc"


def test_bauen_trockenlauf_ohne_ifc_mit_kennzahlen(tmp_path):
    g1, g2 = (pa.guid_lang("0" * 21 + c) for c in "12")
    bl = _liste(_neu_bauteil(g1, kennzahlen={"nicht_mannigfaltige_kanten": 2, "offene_kanten": 3}),
                _neu_bauteil(g2, koerper=[], geometrie="huelle"))
    reg = {"neu": [{"guid": g1, "status": "Neubau"}, {"guid": g2, "status": "Neubau"}]}
    f = FakeSenden({
        "GetProjectInfo": {"projectName": "P"},
        "GetStories": {"stories": STORIES},
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"classificationSystemId": {"guid": "sys"}, "name": pa.SAB_SYSTEM}]},
        "API.GetAllClassificationsInSystem": {"classificationItems": [
            {"classificationItem": {"classificationItemId": {"guid": "k-decke"}, "id": "Decke"}, "children": []}]},
    })
    aus = pa.bauen(pa.Archicad(f), tmp_path, None, bl, reg, {"IfcSlab": "Decke"},
                   nur=None, ohne=(), vorhanden={}, ausfuehren=False)
    assert aus["zu_bauen"] == 1 and aus["ausgelassen"] == 1
    assert aus["quelle"] == {"bauliste": 1, "ifc": 0}
    assert aus["netz_kennzahlen"] == {"nicht_mannigfaltige_kanten": 2, "offene_kanten": 3}
    assert len(aus["netz_pruefen"]) == 1
    assert json.loads((tmp_path / "netz_pruefen.json").read_text())[0]["plattformGuid"] == g1
    assert "CreateMorphs" not in f.namen()


def test_cli_nicht_bearbeitet_bricht_vor_archicad_ab(tmp_path, capsys, monkeypatch):
    g1, g2 = (pa.guid_lang("0" * 21 + c) for c in "12")
    (tmp_path / "a.json").write_text(json.dumps(_liste(_neu_bauteil(g1))))
    (tmp_path / "b.json").write_text(json.dumps(_liste(_neu_bauteil(g1), nicht=[g2], gruende={g2: "x"})))
    (tmp_path / "r.json").write_text("{}")

    def nie(port):
        raise AssertionError("Archicad darf nicht angesprochen werden")

    monkeypatch.setattr(pa, "_http", nie)
    rc = pa.main(["bauen", "--ordner", str(tmp_path), "--bauliste", str(tmp_path / "a.json"),
                  str(tmp_path / "b.json"), "--register", str(tmp_path / "r.json")])
    assert rc == 1 and g2.upper() in capsys.readouterr().err


# ───────────────────────────────────────── Spiegel-Layout (skill/scripts/_kern/elmonkey)

def _saubere_env() -> dict:
    env = dict(os.environ)
    env.pop("PYTHONPATH", None)
    return env


def _spiegel_bauen(tmp_path: Path, mit_kern: bool) -> Path:
    skript_dir = tmp_path / "skill" / "scripts" / "plattform_abgleich"
    skript_dir.mkdir(parents=True)
    ziel = skript_dir / "plattform_abgleich.py"
    shutil.copy(SKRIPT, ziel)
    if mit_kern:
        kern = Path(__file__).resolve().parents[1] / "elmonkey"
        shutil.copytree(kern, tmp_path / "skill" / "scripts" / "_kern" / "elmonkey",
                        ignore=shutil.ignore_patterns("__pycache__"))
    return ziel


def _skip_wenn_installiert(tmp_path: Path) -> None:
    r = subprocess.run([sys.executable, "-I", "-c", "import elmonkey"], cwd=tmp_path,
                       env=_saubere_env(), capture_output=True, text=True, timeout=60)
    if r.returncode == 0:
        pytest.skip("elmonkey ist site-installiert, Spiegelfall nicht prüfbar")


def test_spiegel_layout_mit_kern(tmp_path):
    _skip_wenn_installiert(tmp_path)
    skript = _spiegel_bauen(tmp_path, True)
    r = subprocess.run([sys.executable, "-I", str(skript), "--help"], cwd=tmp_path,
                       env=_saubere_env(), capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "archicad-lesen" in r.stdout


def test_spiegel_layout_ohne_kern(tmp_path):
    _skip_wenn_installiert(tmp_path)
    skript = _spiegel_bauen(tmp_path, False)
    r = subprocess.run([sys.executable, "-I", str(skript), "--help"], cwd=tmp_path,
                       env=_saubere_env(), capture_output=True, text=True, timeout=60)
    assert r.returncode != 0
    assert "elmonkey-core" in r.stderr and "spiegeln" in r.stderr
    assert "IndexError" not in r.stderr and "Traceback" not in r.stderr


def _systeme_ac(*namen):
    return pa.Archicad(FakeSenden({
        "API.GetAllClassificationSystems": {"classificationSystems": [
            {"name": n, "classificationSystemId": {"guid": guid(i + 1)}} for i, n in enumerate(namen)]},
        "API.GetAllClassificationsInSystem": {"classificationItems": [
            {"classificationItem": {"classificationItemId": {"guid": guid(50)}, "id": "Wand"}}]},
    }))


def test_sab_klassen_findet_graphisoft_klassifizierung():
    sid, klassen, name = pa.sab_klassen(_systeme_ac("Archicad Klassifizierung", "Graphisoft Klassifizierung"))
    assert name == "Graphisoft Klassifizierung" and sid == guid(2) and klassen == {"Wand": guid(50)}


def test_sab_klassen_expliziter_name_falsch_nennt_beide_listen():
    import pytest
    with pytest.raises(pa.Fehler) as e:
        pa.sab_klassen(_systeme_ac("Graphisoft Klassifizierung", "Sonst"), "Gibt es nicht")
    assert "Gibt es nicht" in str(e.value) and "Graphisoft Klassifizierung" in str(e.value) and "Sonst" in str(e.value)
    with pytest.raises(pa.Fehler) as e:
        pa.sab_klassen(_systeme_ac("Sonst"))
    assert "SAB_Klassifizierung_29" in str(e.value) and "Graphisoft Klassifizierung" in str(e.value) \
        and "Sonst" in str(e.value)


def test_zuordnung_enthaelt_wall_window_discrete_accessory():
    z = pa.zuordnung_laden(pa.ZUORDNUNG)
    assert z["IfcWall"] == "Wand" and z["IfcWindow"] == "Fenster"
    assert z["IfcDiscreteAccessory"] == "Zusatzgerät / Einbauteil"
