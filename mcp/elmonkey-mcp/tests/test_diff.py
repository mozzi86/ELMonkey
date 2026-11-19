"""elmonkey_diff_plattform ohne Archicad: die Archicad-Aufrufe kommen aus einer Attrappe."""
import asyncio
import json

import pytest

from elmonkey import abgleich as ab
from elmonkey_mcp import server
from elmonkey_mcp.addons.archicad import diff

G1 = "11111111-2222-3333-4444-555555555501"
G2 = "11111111-2222-3333-4444-555555555502"
BOX = [0.0, 5.0, 0.0, 0.3, 0.0, 2.8]


def _el(guid, box, props=None):
    return {"elementId": {"guid": guid}, "type": "Wall", "ifcType": "IfcWall", "ifcGlobalId": ab.ifc_guid(guid),
            "ifcAttributes": [{"name": "Name", "value": "Wand"}],
            "ifcProperties": [{"pset": "Pset_WallCommon", "name": "IsExternal", "value": "True"}] + list(props or []),
            "storyName": "EG", "renovationStatus": "Existing", "geometry": {"source": "3dModel", "box": box}}


class Attrappe:
    def __init__(self):
        self.aufrufe = []

    def __call__(self, port, ns, name, params=None, timeout=600.0):
        self.aufrufe.append((ns, name, dict(params or {})))
        if name == "GetDiffDataELM":
            if params["offset"] == 0:
                return {"format": "elm-diff-daten/1", "readOnly": True, "elements": [_el(G1, [0, 5.2, 0, 0.3, 0, 2.8])],
                        "nextOffset": 1, "context": {}}
            return {"format": "elm-diff-daten/1", "readOnly": True, "elements": [_el(G2, BOX)], "context": {}}
        if name == "GetAllProperties":
            return {"properties": [{"propertyId": {"guid": "P"}, "propertyGroupName": "Allgemeine Werte",
                                    "propertyName": "Plattform-GUID"}]}
        if name == "GetPropertyValuesOfElements":
            return {"propertyValuesForElements": [
                {"propertyValues": [{"propertyValue": {"value": "PF-NACHBAU" if e["elementId"]["guid"] == G2 else ""}}]}
                for e in params["elements"]]}
        raise AssertionError(f"unerwarteter Aufruf {ns}.{name}")


def _stand(tmp_path):
    st = {"format": ab.FORMAT, "lauf": "ab-1", "geschosse": [{"name": "EG", "archicadName": "EG"}],
          "elemente": [{"guid": ab.ifc_guid(G1), "name": "Wand", "klasse": "IfcWall", "herkunft": "archicad",
                        "eigenschaften": {"IFC.Klasse": "IfcWall", "Pset_WallCommon.IsExternal": ".T."}, "bbox": BOX},
                       {"guid": "PF-NACHBAU", "name": "Traverse", "klasse": "IfcWall", "herkunft": "plattform",
                        "eigenschaften": {}}],
          "basis": {"paare": {}, "werte": {}}}
    p = tmp_path / "stand.json"
    p.write_text(json.dumps(st), encoding="utf-8")
    return p


def test_diff_lauf_liest_seitenweise_und_schreibt_ergebnis(tmp_path):
    a = Attrappe()
    aus = diff.diff_lauf(19723, str(_stand(tmp_path)), ordner=str(tmp_path / "lauf"), rufen=a)
    erg = json.loads((tmp_path / "lauf" / "ergebnis.json").read_text(encoding="utf-8"))
    assert erg["format"] == "sab-archicad-abgleich/1" and erg["lauf"] == "ab-1"
    assert [x[2]["offset"] for x in a.aufrufe if x[1] == "GetDiffDataELM"] == [0, 1]
    assert not any(n.startswith(("Set", "Reserve", "Create", "Delete")) for _, n, _ in a.aufrufe)   # liest nur
    assert aus["zahlen"]["geometrie"] == 1 and erg["diff"]["geometrie"][0]["seiten"] == {"x+": 0.2}
    assert aus["zahlen"]["eigenschaften"] == 0                      # True ~ .T.
    assert aus["gebaut_vorschlag"] == [{"plattformGuid": "PF-NACHBAU", "archicadGuid": G2}]
    assert aus["zahlen"]["neu"] == 0 and aus["zahlen"]["nur_plattform"] == 0


def test_nur_https_links(tmp_path):
    with pytest.raises(ValueError):
        diff._https_oder_datei("http://beispiel.invalid/stand.json")
    with pytest.raises(ValueError):
        diff._https_oder_datei(str(tmp_path / "fehlt.json"))


def test_falsches_stand_format(tmp_path):
    p = tmp_path / "x.json"
    p.write_text(json.dumps({"format": "anders"}), encoding="utf-8")
    with pytest.raises(ValueError):
        diff.diff_lauf(19723, str(p), ordner=str(tmp_path), rufen=Attrappe())


def test_werkzeug_registriert():
    namen = {t.name for t in asyncio.run(server.mcp.list_tools())}
    assert "elmonkey_diff_plattform" in namen
