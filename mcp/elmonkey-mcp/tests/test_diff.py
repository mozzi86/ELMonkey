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
        if name == "GetProjectInfo":
            return {"projectName": "Haus"}
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


G3 = "11111111-2222-3333-4444-555555555503"
QUADRAT = {"vertices": [{"x": 0, "y": 0, "z": 0}, {"x": 1, "y": 0, "z": 0}, {"x": 1, "y": 1, "z": 0},
                        {"x": 0, "y": 1, "z": 0}], "polygons": [{"vertexIds": [0, 1, 2, 3]}]}


class NetzAttrappe(Attrappe):
    """G1 in Archicad verlängert (Plattform = Basis), G3 neu in Archicad."""

    def __init__(self, projekt_danach="Haus"):
        super().__init__()
        self.projekt_danach = projekt_danach

    def __call__(self, port, ns, name, params=None, timeout=600.0):
        if name == "GetProjectInfo" and any(n == "GetDiffDataELM" for _, n, _ in self.aufrufe):
            self.aufrufe.append((ns, name, {}))
            return {"projectName": self.projekt_danach}
        if name == "GetDiffDataELM" and params["offset"] == 1:
            self.aufrufe.append((ns, name, dict(params)))
            return {"format": "elm-diff-daten/1", "readOnly": True, "elements": [_el(G2, BOX), _el(G3, BOX)],
                    "context": {}}
        if name == "Get3DGeometryOfElements":
            self.aufrufe.append((ns, name, dict(params)))
            return {"geometries3D": [{"bodies": [QUADRAT]} for _ in params["elements"]]}
        return super().__call__(port, ns, name, params, timeout)


def _stand_mit_basis(tmp_path):
    p = _stand(tmp_path)
    st = json.loads(p.read_text(encoding="utf-8"))
    st["basis"] = {"paare": {ab.ifc_guid(G1): G1}, "werte": {}, "geometrie": {ab.ifc_guid(G1): BOX}}
    p.write_text(json.dumps(st), encoding="utf-8")
    return p


def test_diff_mit_netzen_nur_neu_und_archicad_geaendert(tmp_path):
    import gzip
    a = NetzAttrappe()
    aus = diff.diff_lauf(19723, str(_stand_mit_basis(tmp_path)), ordner=str(tmp_path / "lauf"), netze=True, rufen=a)
    assert aus["netze"]["neu"] == 1 and aus["netze"]["geaendert"] == 1 and aus["netze"]["fehler"] == 0
    (geo,) = [x for x in a.aufrufe if x[1] == "Get3DGeometryOfElements"]
    assert [e["elementId"]["guid"] for e in geo[2]["elements"]] == [G3.upper(), G1.upper()]
    d = json.loads(gzip.decompress((tmp_path / "lauf" / "archicad_netze.json.gz").read_bytes()))
    assert d["projekt"] == "Haus" and d["lauf"] == "ab-1"
    assert [(b["art"], b["plattformGuid"]) for b in d["bauteile"]] == [("neu", None), ("geaendert", ab.ifc_guid(G1))]
    assert not any(n.startswith(("Set", "Reserve", "Create", "Delete")) for _, n, _ in a.aufrufe)
    assert json.loads((tmp_path / "lauf" / "archicad.json").read_text(encoding="utf-8"))["projekt"] == "Haus"


def test_diff_ohne_netze_liest_keine_geometrie(tmp_path):
    a = NetzAttrappe()
    aus = diff.diff_lauf(19723, str(_stand_mit_basis(tmp_path)), ordner=str(tmp_path / "lauf"), rufen=a)
    assert aus["netze"] is None and not (tmp_path / "lauf" / "archicad_netze.json.gz").exists()
    assert not any(n == "Get3DGeometryOfElements" for _, n, _ in a.aufrufe)


def test_diff_netze_projekt_gewechselt_liest_keine_geometrie(tmp_path):
    a = NetzAttrappe(projekt_danach="Anderes")
    with pytest.raises(ValueError, match="Anderes"):
        diff.diff_lauf(19723, str(_stand_mit_basis(tmp_path)), ordner=str(tmp_path / "lauf"), netze=True, rufen=a)
    assert not any(n == "Get3DGeometryOfElements" for _, n, _ in a.aufrufe)
