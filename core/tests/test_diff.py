"""Diff-Checker: die Fallen aus dem Live-Lauf Projekt-H (16.11.2026) als Tests. Ohne Archicad, ohne Plattform."""
from elmonkey import diff as df

G = "3F2504E0-4F89-11D3-9A0C-0305E82C3301"


def _p(guid, **k):
    return {"guid": guid, "name": k.pop("name", "Wand"), "klasse": k.pop("klasse", "IfcWall"),
            "herkunft": k.pop("herkunft", "archicad"), "ausgeblendet": k.pop("ausgeblendet", False),
            "eigenschaften": k.pop("eigenschaften", {}), **k}


def _a(guid, **k):
    return {"guid": guid, "typ": k.pop("typ", "Wall"), "eigenschaften": k.pop("eigenschaften", {}), **k}


def test_schreibweisen_sind_kein_unterschied():
    assert df.gleich(".T.", "Ja") and df.gleich(".F.", "nein") and df.gleich("T", True)
    assert df.gleich("IFCLABEL('NEW')", ["NEW"]) and df.gleich("IFCLABEL('NEW')", "['NEW']")
    assert df.gleich(r"Geb\X\E4ude", "Gebäude") and df.gleich("\\X2\\00E4\\X0\\", "ä")
    assert df.gleich("0,24", 0.2401) and not df.gleich("0,24", 0.25)
    assert df.gleich("$", None) and not df.gleich("NEW", "EXISTING")


def test_mengen_zaehlen_nicht():
    assert df.ist_menge("Qto_WallBaseQuantities.NetVolume") and df.ist_menge("BaseQuantities.Length")
    assert not df.ist_menge("Pset_WallCommon.FireRating")
    d = df.diff([_a(G, eigenschaften={"Qto_WallBaseQuantities.NetVolume": 3.1, "Pset_WallCommon.FireRating": "F90"})],
                [_p(G, eigenschaften={"Qto_WallBaseQuantities.NetVolume": 2.9, "Pset_WallCommon.FireRating": "F30"})])
    assert d["eigenschaften"][0]["felder"] == {"Pset_WallCommon.FireRating": {"plattform": "F30", "archicad": "F90"}}


def test_geometrie_je_seite_mit_toleranz():
    bp = [0, 5, 0, 0.24, 0, 3]
    assert df.seiten(bp, [0, 5.002, 0, 0.24, 0, 3]) == {}
    d = df.diff([_a(G, bbox=[0, 5.1, 0, 0.24, -0.05, 3], bbox_quelle="koerper")], [_p(G, bbox=bp)])
    assert d["geometrie"][0]["seiten"] == {"x+": 0.1, "z-": -0.05}
    assert d["zahlen"]["geometrie_bauteile"] == 1


def test_rohe_box_wird_nicht_verglichen():
    d = df.diff([_a(G, bbox=[0, 5.3, 0, 0.24, 0, 3], bbox_quelle="roh")], [_p(G, bbox=[0, 5, 0, 0.24, 0, 3])])
    assert d["geometrie"] == [] and d["geometrie_ungeprueft"][0]["archicad"] == G
    assert d["zahlen"]["geometrie_ungeprueft"] == 1 and d["hinweise"]


def test_plattform_ohne_bbox_ist_hinweis_kein_unterschied():
    d = df.diff([_a(G, bbox=[0, 5, 0, 1, 0, 3])], [_p(G)])
    assert d["geometrie"] == [] and d["zahlen"]["geometrie_ungeprueft"] == 1 and "bbox" in d["hinweise"][0]


def test_raeume_und_oeffnungen_sind_weder_neu_noch_weg():
    ac = [_a("Z1", typ="Zone"), _a("O1", typ="Opening"), _a("N1", typ="Object", ifcTyp="IfcFurniture")]
    pf = [_p("OP1", klasse="IfcOpeningElement"), _p("W1")]
    d = df.diff(ac, pf)
    assert [x["guid"] for x in d["neu"]] == ["N1"] and [x["guid"] for x in d["weg"]] == ["W1"]
    assert d["zahlen"]["raeume"] == 1 and d["oeffnungen"]["archicad"] == 1 and d["oeffnungen"]["plattform"] == 1


def test_plattform_eigene_ohne_partner_sind_nicht_weg():
    d = df.diff([], [_p("T1", herkunft="plattform", name="Traverse")])
    assert d["weg"] == [] and d["nur_plattform"][0]["guid"] == "T1"


def test_paar_ueber_ifc_kennung_und_stempel():
    ac = [_a("AC-1", ifcId="0abc", eigenschaften={}),
          _a("AC-2", eigenschaften={"Allgemeine Werte.Plattform-GUID": "PF-2"})]
    d = df.diff(ac, [_p("0abc"), _p("PF-2", herkunft="plattform")])
    assert d["paare"] == {"0abc": "AC-1", "PF-2": "AC-2"} and not d["neu"] and not d["weg"]


def test_bbox_vereinen_fuer_teile():
    assert df.bbox_vereinen([[0, 1, 0, 1, 0, 1], {"xMin": -1, "xMax": 0.5, "yMin": 0, "yMax": 2, "zMin": 0, "zMax": 1},
                             None]) == [-1, 1, 0, 2, 0, 1]
    assert df.bbox_vereinen([]) is None


def test_ergebnis_format_fuer_die_plattform():
    d = df.diff([_a(G, bbox=[0, 5.1, 0, 0.24, 0, 3], eigenschaften={"Pset_WallCommon.FireRating": "F90"}),
                 _a("NEU", typ="Slab")],
                [_p(G, bbox=[0, 5, 0, 0.24, 0, 3], eigenschaften={"Pset_WallCommon.FireRating": "F30"}), _p("WEG")])
    e = df.ergebnis(d, "ab-20261116-120000")
    assert e["format"] == "sab-archicad-abgleich/1" and e["lauf"] == "ab-20261116-120000"
    assert e["plattform"]["eigenschaften"] == [{"plattform": G, "archicad": G, "feld": "Pset_WallCommon.FireRating",
                                                "wert": "F90", "vorher": "F30"}]
    assert e["plattform"]["geometrie"][0]["seiten"] == {"x+": 0.1}
    assert e["plattform"]["neu_importieren"] == ["NEU"] and e["plattform"]["ausblenden"] == []
    assert e["offen"][0]["anzahl"] == 1 and e["basis"]["paare"] == {G: G}
    assert e["basis"]["werte"][G] == {"Pset_WallCommon.FireRating": "F30"}
