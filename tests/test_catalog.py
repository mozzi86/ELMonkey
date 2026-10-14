"""Offline-Tests fuer Suche und Namensaufloesung (ohne Archicad): python -m pytest tests  oder  python tests/test_catalog.py"""
from elmonkey_mcp import catalog as c

E = [{"name": f"{ns}.{n}", "namespace": ns, "command": n, "group": g, "description": d, "version": "", "inputSchema": None}
     for ns, n, g, d in [
         ("TapirCommand", "GetDetailsOfElements", "Element Commands", "Gets the details of the given elements (geometry parameters etc)."),
         ("TapirCommand", "GetStories", "Project Commands", "Retrieves information about the story sructure of the currently loaded project."),
         ("TapirCommand", "CreateLabels", "Element Commands", "Creates label elements."),
         ("ELM_SAB", "CreateLabels", "ELM_SAB", "ELM_SAB: Legt assoziative Symbol-Etiketten an."),
         ("ELM_SAB", "Create2DElements", "ELM_SAB", "ELM_SAB: Legt 2D-Elemente an (Line, Arc, Circle, PolyLine, Spline, Hatch)."),
         ("ELM_SAB", "SetLayerOfElements", "ELM_SAB", "ELM_SAB: Setzt die Ebene von Elementen (layerId oder layerName)."),
         ("ELM_SAB", "FindElements", "ELM_SAB", "ELM_SAB: Elementsuche wie Suchen & aktivieren."),
     ]]


def test_search_german():
    assert c.search(E, "Schraffur anlegen")[0]["command"] == "Create2DElements"
    assert c.search(E, "Ebene von Elementen setzen")[0]["command"] == "SetLayerOfElements"
    assert c.search(E, "Geschosse lesen")[0]["command"] == "GetStories"


def test_resolve():
    assert c.resolve(E, "elements_get_details_of_elements") == ("TapirCommand", "GetDetailsOfElements")
    assert c.resolve(E, "GetStories") == ("TapirCommand", "GetStories")
    assert c.resolve(E, "ELM_SAB.FindElements") == ("ELM_SAB", "FindElements")
    assert c.resolve(E, "API.GetElementsByType") == ("API", "GetElementsByType")
    try:
        c.resolve(E, "CreateLabels")
        raise AssertionError("mehrdeutig erwartet")
    except ValueError as e:
        assert "mehrdeutig" in str(e)


if __name__ == "__main__":
    test_search_german(); test_resolve(); print("alle Tests ok")
