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
         ("ELM_SAB", "GetPenOfElements", "ELM_SAB", "ELM_SAB: Liest Stifte inkl. RGB-Overrides von 2D-Elementen."),
         ("ELM_SAB", "SetPenOfElements", "ELM_SAB", "ELM_SAB: Setzt Stifte (Kontur/Schraffur-VG/HG, Text-Deckung) von 2D-Elementen; optional ueber mehrere Datenbanken. 0.9.24: Aufruffehler kommen schemakonform beim Client an (statt 4009)."),
         ("ELM_SAB", "GetConstructionPensOfElements", "ELM_SAB", "ELM_SAB: Liest die Stifte von Bauteilen (Wand, Stuetze, Unterzug, Decke, Dach, Schale, Fassade, Morph): Schnitt-/Kontur-, unverdeckte und Ueberkopf-Linien, Schnittflaechen-Ueberschreibung (overridden/pen), Deckflaechen, dazu Typ, Ebene, Geschoss, Aufbau; includeComposites liefert die Konturstifte der Verbundschichten. Nur lesen."),
         ("ELM_SAB", "SetConstructionPensOfElements", "ELM_SAB", "ELM_SAB: Setzt die Stifte von Bauteilen: cutLinePen/uncutLinePen/overheadLinePen je Typ auf das passende Feld, clearCutFillOverride (Schraffur wieder aus Baustoff/Verbund), Rohfelder ueber fields; dryRun, Vorpruefung [CODE], Teamwork-Reservierung, ein Undo-Schritt, Ruecklese je Element."),
     ]]


def test_search_german():
    assert c.search(E, "Schraffur anlegen")[0]["command"] == "Create2DElements"
    assert c.search(E, "Ebene von Elementen setzen")[0]["command"] == "SetLayerOfElements"
    assert c.search(E, "Geschosse lesen")[0]["command"] == "GetStories"


def test_search_bauteilstifte():
    assert c.search(E, "Stifte von Waenden lesen")[0]["command"] == "GetConstructionPensOfElements"
    assert c.search(E, "Bauteilstifte lesen")[0]["command"] == "GetConstructionPensOfElements"
    assert c.search(E, "Schnittflaeche Ueberschreibung loeschen Bauteile")[0]["command"] == "SetConstructionPensOfElements"


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
    test_search_german(); test_search_bauteilstifte(); test_resolve(); print("alle Tests ok")
