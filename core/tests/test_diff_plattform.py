"""Diff-Checker Archicad ↔ Plattform (elmonkey.diff_plattform) — synthetische Daten, ohne Archicad, ohne Plattform.

Je Zeile der Fallen-Tabelle aus dem ersten Live-Lauf mindestens ein Test (Kennzeichen FALLE n im Testnamen-Docstring).
"""
import pytest

from elmonkey import abgleich as ab
from elmonkey import diff_plattform as dp

# ─────────────────────────────────────────────────────────── Bausteine

G_WAND = "11111111-2222-3333-4444-555555555501"
G_DECKE = "11111111-2222-3333-4444-555555555502"
G_TUER = "11111111-2222-3333-4444-555555555503"
G_STUHL = "11111111-2222-3333-4444-555555555504"
G_ZONE = "11111111-2222-3333-4444-555555555505"
G_ABBRUCH = "11111111-2222-3333-4444-555555555506"
G_NACHGEBAUT = "11111111-2222-3333-4444-555555555507"
G_OEFF = "11111111-2222-3333-4444-555555555508"


def ac_el(guid, ifc_type, *, typ="Wall", name=None, props=None, box=None, quelle="3dModel", status="Existing",
          story="EG", host=None, zone=None, attrs=None):
    x = {"elementId": {"guid": guid}, "type": typ, "ifcType": ifc_type, "ifcGlobalId": ab.ifc_guid(guid),
         "ifcAttributes": [{"name": "Name", "value": name or "Bauteil"}] + list(attrs or []),
         "ifcProperties": [{"pset": k.split(".", 1)[0], "name": k.split(".", 1)[1], "value": v}
                           for k, v in (props or {}).items()],
         "storyName": story, "renovationStatus": status, "elementIdText": "E-" + guid[-2:], "layer": "Ebene",
         "layerHidden": False, "classifications": [], "surfaces": [],
         "geometry": {"source": quelle, "box": box} if box else {"source": "none"}}
    if host:
        x["hostElementId"] = {"guid": host}
    if zone:
        x["zone"] = zone
    return x


def seite(*elemente, **context):
    return {"format": "elm-diff-daten/1", "readOnly": True, "elements": list(elemente),
            "context": {"elmSabVersion": "0.11.13", **context}, "skipped": {"documentation": 3}}


def pf_el(guid, klasse, *, name=None, herkunft="archicad", props=None, bbox=None, teil_von=None, ausgeblendet=False,
          gid=None):
    eig = {"IFC.Klasse": klasse, "IFC.Name": name or "Bauteil"}
    if gid is not None:
        eig["IFC.GlobalId"] = gid
    eig.update(props or {})
    e = {"guid": guid, "name": name or "Bauteil", "klasse": klasse, "geschoss": "EG", "herkunft": herkunft,
         "ausgeblendet": ausgeblendet, "eigenschaften": eig}
    if bbox:
        e["bbox"] = bbox
    if teil_von:
        e["teil_von"] = teil_von
    return e


def stand(*elemente, basis=None):
    return {"format": ab.FORMAT, "lauf": "ab-20261116-120000", "elemente": list(elemente),
            "basis": basis or {"paare": {}, "werte": {}}}


def lauf(ac_seiten, st, **kw):
    return dp.vergleichen(dp.archicad_bauteile(ac_seiten), st, **kw)


BOX_WAND = [0.0, 5.0, 0.0, 0.3, 0.0, 2.8]


# ─────────────────────────────────────────────────────────── Format

def test_ergebnis_im_meldeformat():
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall", bbox=BOX_WAND))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st)
    assert erg["format"] == "sab-archicad-abgleich/1" and erg["lauf"] == "ab-20261116-120000"
    for seite_ in ("plattform", "archicad", "basis"):
        assert isinstance(erg[seite_], dict)
    assert erg["basis"]["paare"] == {ab.ifc_guid(G_WAND): G_WAND}
    assert erg["diff"]["zahlen"]["neu"] == 0 and erg["diff"]["zahlen"]["weg"] == 0
    assert erg["diff"]["geometrie"] == []


def test_falsches_format_wird_abgelehnt():
    with pytest.raises(ab.AbgleichFehler):
        dp.archicad_bauteile([{"format": "anders/1", "elements": []}])
    with pytest.raises(ab.AbgleichFehler):
        dp.vergleichen({"elemente": []}, {"format": "anders/1", "elemente": []})


def test_ac27_globalid_als_guid_wird_komprimiert():
    x = ac_el(G_WAND, "IfcWall", box=BOX_WAND)
    x["ifcGlobalId"] = G_WAND                      # AC27: unkomprimierte GUID
    a = dp.archicad_bauteile([seite(x)])["elemente"][0]
    assert a["ifcId"] == ab.ifc_guid(G_WAND) and len(a["ifcId"]) == 22


# ─────────────────────────────────────────────────────────── FALLE 1: rohe Box vs. getrimmte Geometrie

def test_falle1_geometrie_je_seite_mit_delta_und_toleranz():
    """FALLE 1 — Abweichung je Seite mit Delta; unter 3 mm zählt nicht."""
    ac_box = [0.0, 5.25, 0.0, 0.3, 0.0, 2.802]             # x+ 25 cm länger, z+ 2 mm (unter Toleranz)
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall", bbox=BOX_WAND))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=ac_box))], st)
    (g,) = erg["diff"]["geometrie"]
    assert g["seiten"] == {"x+": 0.25} and g["vergleich_mit"] == "plattform"


def test_falle1_rohe_box_wird_nicht_als_aenderung_gewertet():
    """FALLE 1 — eine Box ohne 3D-Modell (calcBounds) ist kein Beleg: ungeprüft statt geändert."""
    roh = [-0.2, 5.35, 0.0, 0.3, 0.0, 3.1]
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall", bbox=BOX_WAND))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=roh, quelle="calcBounds"))], st)
    assert erg["diff"]["geometrie"] == []
    (u,) = erg["diff"]["geometrie_ungeprueft"]
    assert "rohe Box" in u["grund"]


def test_seiten_delta_und_versatz():
    assert dp.seiten_delta([1, 2, 3, 4, 5, 6], [1, 2, 3, 4, 5, 6]) == {}
    assert dp.seiten_delta([0.996, 2, 3, 4, 5, 6.5], [1, 2, 3, 4, 5, 6]) == {"x-": -0.004, "z+": 0.5}
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall", bbox=[0, 5, 0, 0.3, 10, 12.8]))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st, versatz=(0.0, 0.0, 10.0))
    assert erg["diff"]["geometrie"] == []


def test_geometrie_gegen_basis_wenn_plattform_ohne_box():
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWall"), basis={"paare": {pg: G_WAND}, "werte": {}, "geometrie": {pg: BOX_WAND}})
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=[0, 5, 0, 0.3, 0, 3.0]))], st)
    (g,) = erg["diff"]["geometrie"]
    assert g["seiten"] == {"z+": 0.2} and g["vergleich_mit"] == "basis"
    assert erg["basis"]["geometrie"][pg] == [0, 5, 0, 0.3, 0, 3.0]


def test_ohne_plattform_geometrie_ungeprueft():
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall"))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st)
    assert erg["diff"]["geometrie_ungeprueft"][0]["grund"].startswith("Plattform")


def test_plattform_geometrie_von_aussen():
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWall"))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st,
               plattform_geometrie={pg: [0, 4.9, 0, 0.3, 0, 2.8]})
    assert erg["diff"]["geometrie"][0]["seiten"] == {"x+": 0.1}


# ─────────────────────────────────────────────────────────── FALLE 2: Mehrschicht → IfcBuildingElementPart

def test_falle2_teile_werden_zusammengefasst_und_sind_nie_weg():
    """FALLE 2 — Wand ohne eigene Geometrie, zwei Schichten als Teile: Box = Vereinigung, Teile nicht „weg“."""
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWall"),
               pf_el("TEIL-A", "IfcBuildingElementPart", teil_von=pg, bbox=[0, 5, 0, 0.1, 0, 2.8]),
               pf_el("TEIL-B", "IfcBuildingElementPart", teil_von=pg, bbox=[0, 5, 0.1, 0.3, 0, 2.8]))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st)
    assert erg["diff"]["weg"] == [] and erg["diff"]["geometrie"] == []
    assert erg["diff"]["geometrie_ungeprueft"] == []
    assert erg["diff"]["bauteilteile"] == {"plattform": 2, "zugeordnet": 2, "ohne_bauteil": []}


def test_falle2_boxen_aus_ifc_fassen_teile_zusammen():
    """FALLE 2 — Rückfall: Boxen aus dem Plattform-IFC, Teile über IfcRelAggregates."""
    ifc = "\n".join([
        "#1=IFCWALL('0aaaaaaaaaaaaaaaaaaaaa',#2,'W',$,$,#3,$,$,$);",
        "#4=IFCBUILDINGELEMENTPART('1bbbbbbbbbbbbbbbbbbbbb',#2,'T1',$,$,#3,#10,$,$);",
        "#5=IFCBUILDINGELEMENTPART('2ccccccccccccccccccccc',#2,'T2',$,$,#3,#11,$,$);",
        "#6=IFCRELAGGREGATES('3ddddddddddddddddddddd',#2,$,$,#1,(#4,#5));",
        "#10=IFCPRODUCTDEFINITIONSHAPE($,$,(#12));",
        "#11=IFCPRODUCTDEFINITIONSHAPE($,$,(#13));",
        "#12=IFCSHAPEREPRESENTATION(#9,'Body','Tessellation',(#14));",
        "#13=IFCSHAPEREPRESENTATION(#9,'Body','Tessellation',(#15));",
        "#14=IFCTRIANGULATEDFACESET(#16,$,$,((1,2,3)),$);",
        "#15=IFCTRIANGULATEDFACESET(#17,$,$,((1,2,3)),$);",
        "#16=IFCCARTESIANPOINTLIST3D(((0.,0.,0.),(5.,0.1,2.8),(1.,0.05,1.)));",
        "#17=IFCCARTESIANPOINTLIST3D(((0.,0.1,0.),(5.,0.3,2.8),(1.,0.2,1.)));",
    ])
    boxen = dp.boxen_aus_ifc(ifc)
    assert boxen["1bbbbbbbbbbbbbbbbbbbbb"] == [0.0, 5.0, 0.0, 0.1, 0.0, 2.8]
    assert boxen["0aaaaaaaaaaaaaaaaaaaaa"] == [0.0, 5.0, 0.0, 0.3, 0.0, 2.8]


# ─────────────────────────────────────────────────────────── FALLE 3: Umbau-Filter blendet Abbruch aus

def test_falle3_abbruch_bauteile_werden_gelesen_und_verglichen():
    """FALLE 3 — Abbruch-Bauteile aus Archicad (Filter ignoriert) paaren normal, nichts „weg“."""
    pg = ab.ifc_guid(G_ABBRUCH)
    st = stand(pf_el(pg, "IfcWall", props={"SAB_Planung.Umbau-Status": "Abbruch"}, bbox=BOX_WAND))
    erg = lauf([seite(ac_el(G_ABBRUCH, "IfcWall", status="ToBeDemolished", box=BOX_WAND))], st)
    assert erg["diff"]["weg"] == [] and erg["diff"]["eigenschaften"] == []


def test_falle3_fehlende_abbruch_bauteile_geben_einen_hinweis():
    """FALLE 3 — fehlen Abbruch-Bauteile trotzdem, steht ein Hinweis da (nicht still „gelöscht“)."""
    st = stand(pf_el("PF-ABBRUCH", "IfcWall", props={"SAB_Planung.Umbau-Status": "Abbruch"}))
    erg = lauf([seite()], st)
    assert erg["diff"]["weg"][0]["plattform"] == "PF-ABBRUCH"
    assert any("Abbruch" in h for h in erg["diff"]["hinweise"])


# ─────────────────────────────────────────────────────────── FALLE 4: IfcOpeningElement-GUIDs je Export neu

def test_falle4_oeffnung_einer_tuer_ist_nicht_weg():
    """FALLE 4 — Öffnung mit fremder GUID an der Tür → gehört zur Tür, nicht „weg“, nicht „neu“."""
    tuer_box = [1.0, 2.0, 0.0, 0.1, 0.0, 2.1]
    st = stand(pf_el(ab.ifc_guid(G_WAND), "IfcWall", bbox=BOX_WAND),
               pf_el(ab.ifc_guid(G_TUER), "IfcDoor", bbox=tuer_box),
               pf_el("NEUE-OEFFNUNGS-GUID", "IfcOpeningElement", bbox=[0.99, 2.01, -0.01, 0.31, 0.0, 2.1]))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND),
                      ac_el(G_TUER, "IfcDoor", typ="Door", box=tuer_box, host=G_WAND))], st)
    d = erg["diff"]
    assert d["weg"] == [] and d["neu"] == []
    assert d["oeffnungen"]["an_tuer_fenster"] == [{"plattform": "NEUE-OEFFNUNGS-GUID", "archicad": G_TUER}]


def test_falle4_oeffnung_ohne_partner_ist_nie_weg():
    st = stand(pf_el("OEFF-X", "IfcOpeningElement"))
    erg = lauf([seite()], st)
    assert erg["diff"]["weg"] == [] and erg["diff"]["oeffnungen"]["ohne_partner"] == ["OEFF-X"]


def test_falle4_wandoeffnung_paart_ueber_lage():
    """FALLE 4 — Wandöffnungs-Element: neue GUID im Export, gleiche Lage → Paar (nicht neu + weg)."""
    box = [3.0, 4.0, 0.0, 0.3, 1.0, 2.0]
    st = stand(pf_el("OEFF-NEU", "IfcOpeningElement", bbox=[3.01, 4.01, 0.0, 0.3, 1.0, 2.0]))
    erg = lauf([seite(ac_el(G_OEFF, "IfcOpeningElement", typ="Opening", box=box))], st)
    assert erg["diff"]["neu"] == [] and erg["basis"]["paare"]["OEFF-NEU"] == G_OEFF


# ─────────────────────────────────────────────────────────── FALLE 5: Klassenliste unvollständig

@pytest.mark.parametrize("klasse", ["IfcFurniture", "IfcFurnishingElement", "IfcSanitaryTerminal", "IfcMember",
                                    "IfcBuildingElementProxy", "IfcDiscreteAccessory", "IfcIrgendwasNeues"])
def test_falle5_jede_ifc_klasse_zaehlt(klasse):
    """FALLE 5 — keine feste Liste: auch ein Stuhl (IfcFurniture) oder eine unbekannte Klasse ist ein Bauteil."""
    erg = lauf([seite(ac_el(G_STUHL, klasse, typ="Object", name="Stuhl 01", box=[0, 0.5, 0, 0.5, 0, 0.9]))], stand())
    assert [x["archicad"] for x in erg["diff"]["neu"]] == [G_STUHL]
    assert erg["plattform"]["neu_importieren"] == [G_STUHL]


def test_falle5_nicht_bauteile_bleiben_draussen():
    erg = lauf([seite(ac_el(G_STUHL, "IfcAnnotation", typ="Object"))], stand(pf_el("RASTER", "IfcGrid")))
    assert erg["diff"]["neu"] == [] and erg["diff"]["weg"] == []
    assert erg["diff"]["nicht_verglichen"] == {"archicad": 1, "plattform": 1}


# ─────────────────────────────────────────────────────────── FALLE 6: IfcSpace (Zonen)

def test_falle6_zonen_getrennt_nie_neu():
    """FALLE 6 — Zonen: eigene Liste, nicht „neu“, nicht im Import."""
    z = ac_el(G_ZONE, "IfcSpace", typ="Zone", name="WC", zone={"name": "WC Damen", "number": "0.12"},
              box=[0, 2, 0, 2, 0, 2.5])
    erg = lauf([seite(z)], stand())
    assert erg["diff"]["neu"] == [] and erg["plattform"]["neu_importieren"] == []
    assert erg["diff"]["zonen"]["nur_archicad"] == [
        {"archicad": G_ZONE, "name": "WC Damen", "nummer": "0.12", "geschoss": "EG"}]


# ─────────────────────────────────────────────────────────── FALLE 7: Schreibweisen

@pytest.mark.parametrize("a,b", [
    (".T.", "Ja"), (".F.", "Nein"), ("True", True), ("IFCBOOLEAN(.T.)", "wahr"),
    ("IFCLABEL('NEW')", "['NEW']"), ("IFCLABEL('NEW')", "NEW"), ("(IFCLABEL('NEW'))", "('NEW')"),
    ("Gr\\X\\FCn", "Grün"), ("T\\X2\\00FC\\X0\\r", "Tür"), ("WAND_A", "  WAND_A "),
    ("0,24", 0.24), ("IFCLENGTHMEASURE(0.24)", "0,24"), (".NOTDEFINED.", "NOTDEFINED"),
])
def test_falle7_schreibweisen_gleich(a, b):
    """FALLE 7 — .T./„Ja“, IFCLABEL('X')/['X'], \\X\\E4 und Verwandte gelten als gleich."""
    assert dp.gleich(a, b)


@pytest.mark.parametrize("a,b", [("NEW", "OLD"), (".T.", "Nein"), ("0,24", "0,30"), ("Grün", "Gruen")])
def test_falle7_echte_unterschiede_bleiben(a, b):
    assert not dp.gleich(a, b)


def test_falle7_kein_schein_unterschied_im_ganzen_lauf():
    """FALLE 7 — Archicad liefert „True“/Klartext, die Plattform „.T.“/IFCLABEL: kein Eintrag."""
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWallStandardCase", bbox=BOX_WAND, props={
        "Pset_WallCommon.IsExternal": ".T.", "Pset_WallCommon.Reference": "IFCLABEL('AW 36,5')",
        "SAB_Planung.Raum": "K\\X\\FCche", "SAB_Planung.Umbau-Status": "Bestand"}))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND, props={
        "Pset_WallCommon.IsExternal": "True", "Pset_WallCommon.Reference": "AW 36,5",
        "SAB_Planung.Raum": "Küche"}))], st)
    assert erg["diff"]["eigenschaften"] == []
    assert erg["plattform"]["eigenschaften"] == [] and erg["archicad"]["eigenschaften"] == []


def test_echte_eigenschaftsaenderung_mit_basis_geht_nach_plattform():
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWall", bbox=BOX_WAND, props={"Pset_WallCommon.FireRating": "IFCLABEL('F30')"}),
               basis={"paare": {pg: G_WAND}, "werte": {pg: {"Pset_WallCommon.FireRating": "F30"}}})
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND, props={"Pset_WallCommon.FireRating": "F90"}))], st)
    (x,) = erg["plattform"]["eigenschaften"]
    assert x["feld"] == "Pset_WallCommon.FireRating" and x["wert"] == "F90"
    assert erg["diff"]["eigenschaften"][0]["richtung"] == "archicad→plattform"


def test_erster_abgleich_verschiedene_werte_wird_frage():
    pg = ab.ifc_guid(G_WAND)
    st = stand(pf_el(pg, "IfcWall", bbox=BOX_WAND, props={"Pset_WallCommon.FireRating": "F30"}))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND, props={"Pset_WallCommon.FireRating": "F90"}))], st)
    assert erg["diff"]["eigenschaften"][0]["richtung"] == "unklar"
    assert erg["offen"][0]["anzahl"] == 1


# ─────────────────────────────────────────────────────────── FALLE 8: Mengen

def test_falle8_mengen_zaehlen_nicht():
    """FALLE 8 — Qto_*/BaseQuantities weichen ab (Öffnungen/Anschlüsse), Geometrie gleich → kein Unterschied."""
    pg = ab.ifc_guid(G_DECKE)
    box = [0, 4, 0, 4, 2.8, 3.0]
    st = stand(pf_el(pg, "IfcSlab", bbox=box, props={"Qto_SlabBaseQuantities.NetVolume": "3,2",
                                                    "BaseQuantities.NetArea": 16.0}),
               basis={"paare": {pg: G_DECKE}, "werte": {pg: {"Qto_SlabBaseQuantities.NetVolume": "3,2"}}})
    erg = lauf([seite(ac_el(G_DECKE, "IfcSlab", typ="Slab", box=box, props={
        "Qto_SlabBaseQuantities.NetVolume": "3,05", "BaseQuantities.NetArea": "15,4"}))], st)
    assert erg["diff"]["eigenschaften"] == [] and erg["diff"]["geometrie"] == []


def test_ist_menge():
    assert dp.ist_menge("Qto_WallBaseQuantities.Length") and dp.ist_menge("BaseQuantities.GrossArea")
    assert dp.ist_menge("ArchiCADQuantities.Volumen") and not dp.ist_menge("Pset_WallCommon.IsExternal")
    assert not dp.ist_menge("IFC.Name")


# ─────────────────────────────────────────────────────────── FALLE 9: Plattform-Bauteile, in Archicad nachgebaut

def test_falle9_nachbau_mit_stempel_ist_paar_und_wird_vorgeschlagen():
    """FALLE 9 — Plattform-eigenes Bauteil (Traverse) in Archicad nachgebaut, Stempel „Plattform-GUID“: Paar,
    nicht doppelt (kein neu_bauen, kein neu_importieren), Vorschlag für `gebaut`."""
    st = stand(pf_el("PF-TRAVERSE-1", "IfcBeam", herkunft="plattform", bbox=[0, 3, 0, 0.2, 2.5, 2.7]))
    x = ac_el(G_NACHGEBAUT, "IfcBeam", typ="Beam", box=[0, 3, 0, 0.2, 2.5, 2.7],
              props={"Allgemeine Werte.Plattform-GUID": "PF-TRAVERSE-1"})
    erg = lauf([seite(x)], st)
    assert erg["archicad"]["neu_bauen"] == [] and erg["plattform"]["neu_importieren"] == []
    assert erg["basis"]["paare"] == {"PF-TRAVERSE-1": G_NACHGEBAUT}
    assert erg["diff"]["gebaut_vorschlag"] == [{"plattformGuid": "PF-TRAVERSE-1", "archicadGuid": G_NACHGEBAUT}]


def test_falle9_nachbau_ueber_gebaut_parameter():
    st = stand(pf_el("PF-TRAVERSE-2", "IfcBeam", herkunft="plattform"))
    erg = lauf([seite(ac_el(G_NACHGEBAUT, "IfcBeam", typ="Beam", box=[0, 3, 0, 0.2, 2.5, 2.7]))], st,
               gebaut=[{"plattformGuid": "PF-TRAVERSE-2", "archicadGuid": G_NACHGEBAUT}])
    assert erg["archicad"]["neu_bauen"] == [] and erg["plattform"]["neu_importieren"] == []
    assert erg["diff"]["neu"] == [] and erg["diff"]["gebaut_vorschlag"] == []


def test_ohne_nachbau_bleibt_plattform_bauteil_zu_bauen():
    st = stand(pf_el("PF-TRAVERSE-3", "IfcBeam", herkunft="plattform"))
    erg = lauf([seite()], st)
    assert erg["archicad"]["neu_bauen"] == ["PF-TRAVERSE-3"]
    assert erg["diff"]["nur_plattform"][0]["plattform"] == "PF-TRAVERSE-3" and erg["diff"]["weg"] == []


# ─────────────────────────────────────────────────────────── neu / weg / Paar über GlobalId

def test_paar_ueber_globalid_eigenschaft():
    """Die Plattform-GUID ist nicht die GlobalId: gepaart wird über IFC.GlobalId."""
    st = stand(pf_el("PLATTFORM-INTERN-1", "IfcWall", gid=ab.ifc_guid(G_WAND), bbox=BOX_WAND))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st)
    assert erg["basis"]["paare"] == {"PLATTFORM-INTERN-1": G_WAND} and erg["diff"]["neu"] == []


def test_neu_und_weg():
    st = stand(pf_el("PF-ALT", "IfcWall"))
    erg = lauf([seite(ac_el(G_WAND, "IfcWall", box=BOX_WAND))], st)
    assert [x["archicad"] for x in erg["diff"]["neu"]] == [G_WAND]
    assert [x["plattform"] for x in erg["diff"]["weg"]] == ["PF-ALT"]
    z = erg["diff"]["zahlen"]
    assert z["neu"] == 1 and z["weg"] == 1


def test_archicad_kontext_und_geschossnamen():
    a = dp.archicad_bauteile([seite(ac_el(G_WAND, "IfcWall", story="Erdgeschoss", box=BOX_WAND),
                                    databaseSwitched=True, databaseRestored=True)],
                             geschoss_namen={"Erdgeschoss": "EG"})
    e = a["elemente"][0]
    assert e["geschoss"] == "EG" and e["eigenschaften"]["Verortung.Geschoss"] == "EG"
    assert e["eigenschaften"]["SAB_Planung.Umbau-Status"] == "Bestand"
    assert a["kontext"]["datenbank_umgeschaltet"] is True and a["kontext"]["uebersprungen"] == {"documentation": 3}
