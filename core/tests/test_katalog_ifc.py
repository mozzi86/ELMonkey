"""elmonkey.katalog.ifc — braucht ifcopenshell (Extra [ifc]); sonst übersprungen."""
import sys
from pathlib import Path

import pytest

pytest.importorskip("ifcopenshell")
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import ifcopenshell.util.element as ue

from elmonkey.katalog import ifc as ib
from elmonkey.katalog.modell import Katalog, KatalogFehler

FIXTURE = Path(__file__).parent / "daten" / "katalog"
ROLLEN = {"aussenwand": "schichtwand-ks-wdvs", "innenwand": "schichtwand-ks-wdvs", "staenderwand": "staenderwand-cw75-gk2",
          "rohdecke": "decke-stb-20", "tuer": "tuer-innen-1fl"}
SCHEMEN = ["IFC4", "IFC4X3"]


@pytest.fixture(scope="module")
def k():
    return Katalog.laden(FIXTURE)


@pytest.mark.parametrize("schema", SCHEMEN)
def test_bibliotheken_gueltig_mit_where_regeln(k, schema):
    m = ib.pruefen(ib.materialbibliothek(k, schema))
    assert m["gueltig"], m["fehler"]
    assert m["materialien"] == len(k.baustoffe)
    t = ib.pruefen(ib.typbibliothek(k, schema))
    assert t["gueltig"], t["fehler"]
    assert t["typen"] == len(k.typen) and t["proxys"] == 0


@pytest.mark.parametrize("schema", SCHEMEN)
def test_muster_projekt(k, schema):
    f, info = ib.muster_projekt(k, ROLLEN, schema)       # fenster/fussboden weggelassen → entfallen
    p = ib.pruefen(f)
    assert p["gueltig"], p["fehler"]
    assert p["proxys"] == 0 and p["ohne_typ"] == []
    usages = {(u.LayerSetDirection, u.DirectionSense) for u in f.by_type("IfcMaterialLayerSetUsage")}
    assert ("AXIS2", "POSITIVE") in usages and ("AXIS3", "NEGATIVE") in usages
    gk = next(w for w in f.by_type("IfcWall") if w.Name.startswith("CW75"))
    assert [r.RepresentationIdentifier for r in gk.Representation.Representations] == ["Axis"]
    teile = gk.IsDecomposedBy[0].RelatedObjects
    assert any(t.is_a("IfcMember") and t.ObjectType == "Wandverstärkung" for t in teile)
    assert not f.by_type("IfcWindow") and len(f.by_type("IfcDoor")) == 1
    assert len(f.by_type("IfcRelFillsElement")) == 1 and len(f.by_type("IfcRelVoidsElement")) == 1
    assert len(info["bauplaene"]) == 3


def test_muster_mit_allen_rollen(k):
    f, _ = ib.muster_projekt(k, {**ROLLEN, "fenster": "fenster-1fl", "fussboden": "boden-estrich-15"})
    p = ib.pruefen(f)
    assert p["gueltig"], p["fehler"]
    assert len(f.by_type("IfcWindow")) == 1 and len(f.by_type("IfcCovering")) == 1 and p["proxys"] == 0


@pytest.mark.parametrize("fehlt", ["aussenwand", "staenderwand", "rohdecke"])
def test_fehlende_pflichtrolle(k, fehlt):
    with pytest.raises(KatalogFehler, match=fehlt):
        ib.muster_projekt(k, {r: t for r, t in ROLLEN.items() if r != fehlt})


def test_muster_staenderwand_als_schichten(k):
    f, _ = ib.muster_projekt(k, ROLLEN, "IFC4", "schichten")
    assert ib.pruefen(f)["gueltig"]
    gk = next(w for w in f.by_type("IfcWall") if w.Name.startswith("CW75"))
    assert ue.get_material(gk).is_a("IfcMaterialLayerSetUsage") and not gk.IsDecomposedBy
    assert ue.get_psets(gk)["Pset_Katalog_Staenderwerk_Bauteil"]["Staender"] == 6
    assert not f.by_type("IfcMember")


def test_pset_praefix_wirkt(k):
    f = ib.typbibliothek(k)
    t = f.by_type("IfcWallType")[0]
    assert "Pset_Katalog_Katalog" in ue.get_psets(t)
    f2 = ib.typbibliothek(k, pset_praefix="Pset_X")
    assert "Pset_X_Katalog" in ue.get_psets(f2.by_type("IfcWallType")[0])
    assert any(m for m in f2.by_type("IfcMaterial") if "Pset_X_Material" in ue.get_psets(m))


def test_namen_und_textur_url(k):
    f = ib.materialbibliothek(k, name="Meine Bibliothek", textur_url="https://example.org/t/")
    assert f.by_type("IfcProject")[0].Name == "Meine Bibliothek"
    urls = [t.URLReference for t in f.by_type("IfcImageTexture")]
    assert urls and all(u.startswith("https://example.org/t/") for u in urls)


def test_pruefung_erkennt_fehler(k):
    f = ib.typbibliothek(k)
    w = f.by_type("IfcWallType")[0]
    w.PredefinedType, w.ElementType = "USERDEFINED", None
    assert not ib.pruefen(f)["gueltig"]


def test_material_traegt_schraffur_und_farbe(k):
    f = ib.materialbibliothek(k)
    m = next(x for x in f.by_type("IfcMaterial") if x.Name == "Kalksandstein")
    stile = m.HasRepresentation[0].Representations[0].Items[0].Styles
    assert {s.is_a() for s in stile} == {"IfcSurfaceStyle", "IfcFillAreaStyle"}
    assert m.Category == "masonry"
