"""plattform_abgleich.py gegen ein nachgestelltes Archicad (kein Archicad, kein Netz).

    python3 -m pytest skills/archicad/scripts/plattform_abgleich -q
"""
import json
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import plattform_abgleich as pa  # noqa: E402

G1, G2, G3 = ("11111111-1111-1111-1111-111111111111", "22222222-2222-2222-2222-222222222222",
              "33333333-3333-3333-3333-333333333333")
P_BS, P_ST, P_KI, P_AUS = "p-bs", "p-stempel", "p-ki", "p-ausdruck"


class FalschesArchicad:
    """Merkt sich Eigenschaftswerte wie Archicad; G3 ist eine Linie (kein Bauteil)."""

    def __init__(self):
        self.typ = {G1: "Wall", G2: "Slab", G3: "Line"}
        self.werte = {(G1, P_BS): "F30", (G2, P_BS): "", (G1, P_ST): "", (G2, P_ST): ""}
        self.reserviert, self.aufrufe = [], []
        self.schreiben_wirkt = True
        self.extern = {G1: "0ExternWandGlobalId001"}
        self.diff_befehl = True

    def __call__(self, body):
        if body["command"] != "API.ExecuteAddOnCommand":          # eingebauter Befehl (API.*) direkt
            name = body["command"].replace(".", "_")
            self.aufrufe.append(name)
            return {"succeeded": True, "result": getattr(self, name)(body["parameters"])}
        cid = body["parameters"]["addOnCommandId"]
        name, p = cid["commandName"], body["parameters"]["addOnCommandParameters"]
        self.aufrufe.append(name)
        return {"succeeded": True, "result": {"addOnCommandResponse": getattr(self, name)(p)}}

    def API_GetElementsByType(self, p):
        return {"elements": [{"elementId": {"guid": g}} for g, t in self.typ.items() if t == p["elementType"]]}

    def API_GetAllClassificationSystems(self, p):
        return {"classificationSystems": [{"classificationSystemId": {"guid": "sys"}}]}

    def API_GetClassificationsOfElements(self, p):
        return {"elementClassifications": [{"classificationIds": [
            {"classificationId": {"classificationItemId": {"guid": "klasse"}}}]} for _ in p["elements"]]}

    def API_SetPropertyValuesOfElements(self, p):
        return self.SetPropertyValuesOfElements(p)

    def GetProjectInfo(self, p):
        return {"projectName": "P001 Teamwork", "isTeamwork": True, "projectLocation": "bimcloud://…"}

    def GetStories(self, p):
        return {"stories": [{"index": 0, "name": "EG"}, {"index": 1, "name": "1.OG"}]}

    def GetAllElements(self, p):
        return {"elements": [{"elementId": {"guid": g}} for g in (G1, G2, G3)]}

    def GetAllProperties(self, p):
        def d(pid, gruppe, name, typ="UserDefined", edit=True):
            return {"propertyId": {"guid": pid}, "propertyType": typ, "propertyGroupName": gruppe,
                    "propertyName": name, "propertyIsEditable": edit}
        return {"properties": [d(P_BS, "Brandschutz", "Klasse"), d(P_ST, "Allgemeine Werte", "Plattform-GUID"),
                               d(P_KI, "Allgemeine Werte", "KI generiert"),
                               d(P_AUS, "Mengen", "Fläche", edit=False), d("p-b", "General", "Element ID", "BuiltIn")]}

    def GetDetailsOfElements(self, p):
        return {"detailsOfElements": [{"type": self.typ[e["elementId"]["guid"]], "id": "E-" + e["elementId"]["guid"][:2],
                                       "floorIndex": 0, "layerIndex": 1} for e in p["elements"]]}

    def GetIFCIdsOfElements(self, p):
        # Antwortform wie live (0.11.12, AC29): elementIFCIds, je Eintrag ifcId + externalIFCId
        return {"elementIFCIds": [{"elementId": e["elementId"], "ifcId": "IFC" + e["elementId"]["guid"][:2],
                                   "externalIFCId": self.extern.get(e["elementId"]["guid"], "")}
                                  for e in p["elements"]]}

    def GetDiffDataELM(self, p):
        if not self.diff_befehl:
            return {"error": {"code": 2002, "message": "Command 'GetDiffDataELM' not found"}}
        box = {G1: [0, 5.1, 0, 0.24, 0, 3], G2: [0, 5, 0, 4, -0.2, 0]}
        els = [{"elementId": {"guid": g}, "type": self.typ[g], "ifcType": {"Wall": "IfcWall", "Slab": "IfcSlab"}[self.typ[g]],
                "ifcGlobalId": "IFC" + g[:2], "externalIfcGlobalId": self.extern.get(g, ""),
                "ifcAttributes": [{"name": "Name", "value": "Bauteil"}],
                "ifcProperties": [{"pset": "Brandschutz", "name": "Klasse", "value": self.werte[(g, P_BS)]}],
                "storyName": "EG", "renovationStatus": "Existing",
                "geometry": {"source": "3dModel", "box": box[g], "bodies": 1}} for g in (G1, G2)]
        return {"format": "elm-diff-daten/1", "readOnly": True, "elements": els, "total": 3,
                "context": {"geometry3DModel": 2}}

    def GetPropertyValuesOfElements(self, p):
        zeilen = []
        for e in p["elements"]:
            g = e["elementId"]["guid"]
            zeile = []
            for q in p["properties"]:
                pid = q["propertyId"]["guid"]
                zeile.append({"propertyValue": {"value": self.werte[(g, pid)]}} if (g, pid) in self.werte
                             else {"error": {"code": -1, "message": "Not available"}})
            zeilen.append({"propertyValues": zeile})
        return {"propertyValuesForElements": zeilen}

    def ReserveElements(self, p):
        self.reserviert += [e["elementId"]["guid"] for e in p["elements"]]
        return {}

    def SetPropertyValuesOfElements(self, p):
        for x in p["elementPropertyValues"]:
            if self.schreiben_wirkt:
                self.werte[(x["elementId"]["guid"], x["propertyId"]["guid"])] = x["propertyValue"]["value"]
        return {"executionResults": [{"success": True} for _ in p["elementPropertyValues"]]}


@pytest.fixture
def welt(tmp_path, monkeypatch):
    falsch = FalschesArchicad()
    monkeypatch.setattr(pa, "_http", lambda port: falsch)
    return SimpleNamespace(ac=falsch, ordner=tmp_path,
                           o=lambda **kw: SimpleNamespace(ordner=str(tmp_path), port=1, **kw))


def _stand(*elemente, basis=None):
    return {"format": pa.ab.FORMAT, "lauf": "ab-1", "projekt": {"nummer": "P001"}, "elemente": list(elemente),
            "basis": basis or {"paare": {}, "werte": {}}}


def _pf(guid, herkunft="archicad", **eig):
    return {"guid": guid, "name": "W", "klasse": "IfcWall", "geschoss": "EG", "herkunft": herkunft,
            "ausgeblendet": False, "eigenschaften": eig}


def test_lesen_nimmt_nur_bauteile_und_eigene_eigenschaften(welt):
    aus = pa.cmd_archicad_lesen(welt.o(typen="", alle_eigenschaften=False))
    assert aus == {"projekt": "P001 Teamwork", "teamwork": True, "bauteile": 2, "eigenschaften": 4,
                   "hinweis": aus["hinweis"]}
    stand = json.loads((welt.ordner / "archicad.json").read_text())
    e1 = next(e for e in stand["elemente"] if e["guid"] == G1)
    assert e1["typ"] == "Wall" and e1["geschoss"] == "EG" and e1["ifcId"] == "0ExternWandGlobalId001" and e1["name"] == "E-11"
    assert e1["eigenschaften"] == {"Brandschutz.Klasse": "F30", "Allgemeine Werte.Plattform-GUID": ""}
    assert "General.Element ID" not in stand["eigenschaften_katalog"]
    assert stand["eigenschaften_katalog"]["Mengen.Fläche"]["editierbar"] is False


def test_ganzer_lauf_bis_zum_schreiben_mit_ruecklese(welt):
    pa.cmd_archicad_lesen(welt.o(typen="", alle_eigenschaften=False))
    pa._schreiben(welt.ordner / "stand.json", _stand(
        _pf(G1, **{"Brandschutz.Klasse": "F90"}),            # beide haben einen Wert → Frage
        _pf(G2, **{"Brandschutz.Klasse": "F60"}),            # Archicad leer → füllt Archicad
        _pf("PF-NEU", "plattform")))                         # nur Plattform → in Archicad bauen
    plan = pa.cmd_planen(welt.o(stand_link=""))
    assert (plan["nach_archicad_neu"], plan["nach_archicad_eigenschaften"], plan["fragen_bauteile"]) == (1, 1, 1)
    assert (welt.ordner / "fragen.md").read_text().count("## F1") == 1

    pa._schreiben(welt.ordner / "antworten.json", {"F1": "plattform"})
    erg = pa.cmd_entscheiden(welt.o(antworten=""))
    assert erg["archicad_eigenschaften"] == 2 and erg["archicad_neu_bauen"] == 1 and erg["offen"] == 0
    assert json.loads((welt.ordner / "ergebnis.json").read_text())["lauf"] == "ab-1"

    trocken = pa.cmd_archicad_schreiben(welt.o(ausfuehren=False))
    assert trocken["ausgefuehrt"] is False and trocken["moeglich"] == 2
    assert "SetPropertyValuesOfElements" not in welt.ac.aufrufe and welt.ac.werte[(G1, P_BS)] == "F30"

    echt = pa.cmd_archicad_schreiben(welt.o(ausfuehren=True))
    assert echt["geschrieben"] == 2 and echt["fehler"] == 0
    assert welt.ac.werte[(G1, P_BS)] == "F90" and welt.ac.werte[(G2, P_BS)] == "F60"
    assert sorted(welt.ac.reserviert) == [G1, G2]            # Teamwork: vorher reserviert


def test_ruecklese_deckt_wirkungsloses_schreiben_auf(welt):
    welt.ac.schreiben_wirkt = False
    kat = {"Brandschutz.Klasse": {"propertyId": {"guid": P_BS}, "editierbar": True}}
    aus = pa.eigenschaften_schreiben(pa.Archicad(welt.ac), [{"archicad": G1, "feld": "Brandschutz.Klasse", "wert": "F90"}],
                                     kat, teamwork=False, ausfuehren=True)
    assert aus["geschrieben"] == [] and aus["fehler"][0]["ist"] == "F30"


def test_unbekannte_und_gesperrte_eigenschaften_werden_gemeldet(welt):
    kat = {"Mengen.Fläche": {"propertyId": {"guid": P_AUS}, "editierbar": False}}
    aus = pa.eigenschaften_schreiben(pa.Archicad(welt.ac), [
        {"archicad": G1, "feld": "Mengen.Fläche", "wert": 1}, {"archicad": G1, "feld": "Gibt.Nicht", "wert": 1}],
        kat, teamwork=False, ausfuehren=True)
    assert aus["moeglich"] == 0 and [x["grund"][:20] for x in aus["nicht_moeglich"]] == [
        "Eigenschaft ist in A", "Eigenschaft gibt es "]
    assert "SetPropertyValuesOfElements" not in welt.ac.aufrufe


def test_schreiben_verweigert_bei_anderem_projekt(welt):
    pa.cmd_archicad_lesen(welt.o(typen="", alle_eigenschaften=False))
    pa._schreiben(welt.ordner / "ergebnis.json", {"archicad": {"eigenschaften": [], "loeschen_von_hand": []}})
    welt.ac.GetProjectInfo = lambda p: {"projectName": "Anderes Projekt", "isTeamwork": False}
    with pytest.raises(pa.Fehler, match="anderes Projekt"):
        pa.cmd_archicad_schreiben(welt.o(ausfuehren=True))


def test_stempeln_setzt_plattform_guid_und_ki(welt):
    gebaut = welt.ordner / "gebaut.json"
    pa._schreiben(gebaut, [{"plattformGuid": "PF-NEU", "archicadGuid": G1}])
    welt.ac.werte[(G1, P_KI)] = ""
    aus = pa.cmd_stempeln(welt.o(gebaut=str(gebaut), ausfuehren=True))
    assert aus["geschrieben"] == 2
    assert welt.ac.werte[(G1, P_ST)] == "PF-NEU" and welt.ac.werte[(G1, P_KI)].startswith("KI generiert ")
    assert welt.ac.reserviert == []                          # Stempeln reserviert nicht


def test_links_nur_https(welt):
    with pytest.raises(pa.Fehler, match="https"):
        pa.herunterladen("file:///etc/passwd")
    with pytest.raises(pa.Fehler, match="https"):
        pa.ablegen("http://unsicher", b"x", "application/json")


def test_archicad_fehler_kommt_als_klartext():
    ac = pa.Archicad(lambda body: {"succeeded": False, "error": {"code": 4010, "message": "Command not found"}})
    with pytest.raises(pa.Fehler, match="Command not found"):
        ac.befehl("GibtEsNicht")


# ─────────────────────────────────────────────────────────── Diff-Checker (Hülle um elmonkey.diff_plattform)

def _dpf(guid, **k):
    return {"guid": guid, "name": "Bauteil", "klasse": k.pop("klasse", "IfcWall"), "herkunft": "archicad",
            "ausgeblendet": False, "eigenschaften": k.pop("eigenschaften", {}), **k}


def test_diff_liest_archicad_direkt_und_meldet_nur_unterschiede(welt):
    stand = {"format": pa.ab.FORMAT, "lauf": "ab-20261116-120000", "elemente": [
        _dpf("0ExternWandGlobalId001", bbox=[0, 5, 0, 0.24, 0, 3], eigenschaften={"Brandschutz.Klasse": "F90"}),
        _dpf("IFC22", klasse="IfcSlab", bbox=[0, 5, 0, 4, -0.2, 0]), _dpf("WEG-1")],
        "basis": {"paare": {}, "werte": {}}}
    (welt.ordner / "stand.json").write_text(json.dumps(stand), encoding="utf-8")
    aus = pa.cmd_diff(welt.o(stand_link="", toleranz=0.003))
    erg = json.loads((welt.ordner / "ergebnis.json").read_text(encoding="utf-8"))
    assert "IFCFileOperation" not in welt.ac.aufrufe and "SetPropertyValuesOfElements" not in welt.ac.aufrufe
    assert erg["basis"]["paare"] == {"0ExternWandGlobalId001": G1, "IFC22": G2}       # externe Kennung vor eigener
    assert erg["diff"]["geometrie"][0]["seiten"] == {"x+": 0.1}
    assert [w["plattform"] for w in erg["diff"]["weg"]] == ["WEG-1"] and erg["diff"]["neu"] == []
    assert aus["geometrie"] == 1 and aus["lauf"] == "ab-20261116-120000"


def test_diff_ohne_neuen_befehl_bricht_klar_ab(welt):
    welt.ac.diff_befehl = False
    (welt.ordner / "stand.json").write_text(json.dumps({"format": pa.ab.FORMAT, "elemente": []}), encoding="utf-8")
    with pytest.raises(pa.Fehler, match="0.11.13"):
        pa.cmd_diff(welt.o(stand_link="", toleranz=0.003))
