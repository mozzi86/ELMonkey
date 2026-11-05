"""elmonkey.katalog (Kern, nur Standardbibliothek): Daten, Namen, GUID, Bauen, Bearbeiten."""
import copy
import json
import subprocess
import sys
import uuid
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from elmonkey.katalog import bauen, bearbeiten, werkstoff as ws
from elmonkey.katalog.modell import Katalog, KatalogFehler, benennen, fingerabdruck, ifc_guid_komprimieren, pruefen, typ_guid

FIXTURE = Path(__file__).parent / "daten" / "katalog"


@pytest.fixture(scope="module")
def k():
    return Katalog.laden(FIXTURE)


# ── Daten / Laden

def test_fixture_laedt_und_paketdaten_kommen_mit(k):
    assert len(k.baustoffe) >= 9 and len(k.typen) >= 5
    assert {"CW50", "CW75"} <= set(k.profile)                 # aus den Paketdaten
    assert "stuetzklappgriff-din18040" in k.verstaerkungen
    assert not (FIXTURE / "profile.json").exists()


def test_profile_ueberschreiben_per_datei_und_parameter(tmp_path):
    eigen = {"profile": [{"schluessel": "CW75", "kurz": "X75", "art": "metall", "steg_m": 0.075, "flansch_m": 0.05,
                          "ifc_predefined_type": "STUD", "baustoff": "stahl-verzinkt-760"}]}
    for n in ("baustoffe.json",):
        (tmp_path / n).write_text((FIXTURE / n).read_text(encoding="utf-8"), encoding="utf-8")
    (tmp_path / "typen").mkdir()
    (tmp_path / "profile.json").write_text(json.dumps(eigen), encoding="utf-8")
    k = Katalog.laden(tmp_path)
    assert list(k.profile) == ["CW75"] and k.profile["CW75"]["kurz"] == "X75"
    k2 = Katalog.laden(FIXTURE, profile=eigen["profile"])
    assert list(k2.profile) == ["CW75"]
    with pytest.raises(KatalogFehler, match="profil"):              # die Fixture-Ständerwand braucht CW75 — vorhanden
        Katalog.laden(FIXTURE, profile=[{**eigen["profile"][0], "schluessel": "CW99"}])


def test_import_zieht_kein_ifcopenshell():
    code = "import sys, elmonkey.katalog as k; k.bauen; k.bearbeiten; k.werkstoff; assert 'ifcopenshell' not in sys.modules"
    r = subprocess.run([sys.executable, "-c", code], cwd=Path(__file__).resolve().parents[1], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr


def test_jeder_baustoff_hat_klasse_und_hoechstens_zwei_scharen(k):
    for b in k.baustoffe.values():
        assert b["werkstoffklasse"] in ws.KLASSEN
        assert len(b["schraffur"]["scharen"]) <= 2
        assert b["werkstoffklasse"] == "luft" or b["schraffur"]["scharen"]


@pytest.mark.parametrize("name,kat,erwartet", [
    ("Kalksandstein_100", "Kunststein", "mauerwerk"), ("Sandstein_510", "Naturstein", "naturstein"),
    ("Dachziegel rot_780", "Dachziegel", "dachdeckung"), ("Stahlbeton_WU_800", "Beton", "stahlbeton"),
    ("Glaswolle_220", "Dämmung", "daemmung_weich"), ("XPS_730", "Dämmung", "daemmung_hart"),
    ("Ausbauplatte GKB_355", "Gipskarton", "gipsplatte"), ("Bauteil_500", "Modellierung", "sonstiges"),
    ("Irgendwas", "", "sonstiges")])
def test_werkstoffklasse_immer_vergeben(name, kat, erwartet):
    assert ws.klasse_fuer(name, kat) == erwartet


def test_textur_ohne_und_mit_vorschlaegen():
    assert ws.textur_fuer("holz") == "klasse:holz"
    assert ws.textur_fuer("holz", {"holz": "Wood049"}) == "ambientcg:Wood049"
    assert ws.textur_fuer("luft") is None


def test_alle_typen_gueltig(k):
    for t in k.typen.values():
        pruefen(t, k)
        assert t["ifc"]["klasse"] and t["ifc"]["predefined_type"]


def test_mindestens_eine_schicht(k):
    t = copy.deepcopy(k.typ("schichtwand-ks-wdvs"))
    t["schichten"] = []
    with pytest.raises(KatalogFehler, match="mindestens eine Schicht"):
        pruefen(t, k)


def test_namen(k):
    assert benennen(k.typ("schichtwand-ks-wdvs"), k)["kurzname"] == "KS + WDVS 42 cm"       # fester Kurzname
    t = copy.deepcopy(k.typ("schichtwand-ks-wdvs"))
    t.pop("kurzname_fest")
    assert benennen(t, k)["kurzname"] == "KS 24 + XPS 16 – 42 cm"
    assert benennen(k.typ("staenderwand-cw75-gk2"), k)["langname"].startswith(
        "GKB 12,5 mm · GKB 12,5 mm · MW 75 mm (CW75 e=62,5 cm)")


# ── GUID

def test_typ_guid_referenzwerte():
    assert typ_guid({"schluessel": "schichtwand-ks-wdvs", "version": 1}) == "3NOuD3yNzMfvNMZskeTqxd"
    assert typ_guid({"schluessel": "schichtwand-ks-wdvs", "version": 2}) == "3yCx8GL0nHxvke98QiYBtU"
    assert ifc_guid_komprimieren("0" * 32) == "0" * 22
    assert ifc_guid_komprimieren("f" * 32) == "3" + "$" * 21


def test_typ_guid_fest_und_versionsabhaengig(k):
    t = k.typ("schichtwand-ks-wdvs")
    assert typ_guid(t) == typ_guid(copy.deepcopy(t)) and len(typ_guid(t)) == 22
    assert typ_guid({**t, "version": 2}) != typ_guid(t)


def test_typ_guid_gleich_ifcopenshell():
    guid = pytest.importorskip("ifcopenshell.guid")
    for h in ["0" * 32, "f" * 32] + [uuid.uuid4().hex for _ in range(50)]:
        assert ifc_guid_komprimieren(h) == guid.compress(h)


# ── Bauen

def test_staenderraster():
    assert bauen.staenderraster(3.2, 0.625, 0.05) == pytest.approx([0.025, 0.625, 1.25, 1.875, 2.5, 3.175])
    with pytest.raises(KatalogFehler):
        bauen.staenderraster(0.08, 0.625, 0.05)


def test_schichtwand_ist_ein_koerper(k):
    p = bauen.wand(k, "schichtwand-ks-wdvs", 5.0, 2.75)
    assert p.art == "schichten" and len(p.koerper) == 1 and p.dicke == pytest.approx(0.42)


def test_staenderwand_mit_verstaerkung(k):
    p = bauen.wand(k, "staenderwand-cw75-gk2", 3.2, 2.75, [{"art": "stuetzklappgriff-din18040", "abstand_m": 0.95, "hoehe_m": 0.85}])
    z = p.zusammenfassung()["teile"]
    assert z["IfcMember STUD"] == 6 and z["IfcMember PLATE"] == 2 and z["IfcPlate SHEET"] == 4
    assert z["IfcMember Wandverstärkung"] == 1
    v = next(x for x in p.koerper if x.object_type == "Wandverstärkung")
    for d in p.koerper:
        if d.predefined_type == "INSULATION" and d.x[0] < 0.95 < d.x[1] and d.z[0] >= 0.65 - 1e-9 and d.z[1] <= 1.05 + 1e-9:
            assert d.y[1] <= v.y[0] + 1e-9


def test_verstaerkung_auf_staender_wird_abgelehnt(k):
    with pytest.raises(KatalogFehler, match="Ständer"):
        bauen.wand(k, "staenderwand-cw75-gk2", 3.2, 2.75, [{"art": "traverse-haengelast", "abstand_m": 0.625}])


def test_verstaerkung_nur_in_staenderwand(k):
    with pytest.raises(KatalogFehler, match="Ständerwänden"):
        bauen.wand(k, "schichtwand-ks-wdvs", 3.0, 2.75, [{"art": "traverse-haengelast", "abstand_m": 1.0}])


def test_option_staenderwand_als_schichten(k):
    v = [{"art": "stuetzklappgriff-din18040", "abstand_m": 0.95, "hoehe_m": 0.85}]
    p = bauen.wand(k, "staenderwand-cw75-gk2", 3.2, 2.75, v, "schichten")
    assert p.art == "schichten" and len(p.koerper) == 1
    assert p.staenderwerk["Staender"] == 6 and p.staenderwerk["UW"] == 2 and "Stützklappgriff" in p.staenderwerk["Verstaerkungen"]
    with pytest.raises(KatalogFehler, match="staenderwand_als"):
        bauen.wand(k, "staenderwand-cw75-gk2", 3.2, 2.75, [], "beides")


# ── Bearbeiten

def test_letzte_schicht_bleibt(k):
    with pytest.raises(KatalogFehler, match="letzte Schicht"):
        bearbeiten.anwenden(k, "decke-stb-20", [{"op": "schicht_loeschen", "position": 1}])


def test_grundbestand_aendern_gibt_neuen_typ(k):
    e = bearbeiten.anwenden(k, "schichtwand-ks-wdvs", [{"op": "schicht_aendern", "position": 2, "dicke_m": 0.2}])
    assert e["neu"] and e["typ"]["schluessel"].startswith("schichtwand-ks-wdvs-")
    assert e["typ"]["basiert_auf"] == "schichtwand-ks-wdvs@1" and e["typ"]["ebene"] == "laufzeit"
    assert e["typ"]["kurzname"] == "KS 24 + XPS 20 – 46 cm"
    assert k.typ("schichtwand-ks-wdvs")["schichten"][1]["dicke_m"] == 0.16     # Grundbestand unverändert


def test_laufzeit_typ_aendern_gibt_neue_version():
    k = Katalog.laden(FIXTURE)
    e1 = bearbeiten.anwenden(k, "staenderwand-cw75-gk2", [{"op": "querriegel_setzen", "position": 3, "hoehen_m": [1.2]}])
    k.aufnehmen(e1["typ"], laufzeit=True)
    e2 = bearbeiten.anwenden(k, e1["typ"]["schluessel"], [{"op": "staenderwerk_setzen", "position": 3, "achsabstand_m": 0.4166}])
    assert e2["neu"] and e2["typ"]["schluessel"] == e1["typ"]["schluessel"] and e2["typ"]["version"] == 2


def test_rohling_wird_zum_benannten_typ(k):
    e = bearbeiten.anwenden(k, "rohling-wand", [
        {"op": "schicht_aendern", "position": 1, "baustoff": "kalksandstein", "dicke_m": 0.175, "rolle": "LoadBearing"},
        {"op": "schicht_hinzufuegen", "position": 1, "baustoff": "putz", "dicke_m": 0.01, "rolle": "Outer finish"},
        {"op": "schicht_hinzufuegen", "baustoff": "putz", "dicke_m": 0.01, "rolle": "Inner finish"}])
    assert e["neu"] and not e["typ"]["rohling"]
    assert e["typ"]["kurzname"] == "KS 17,5 – 19,5 cm"


def test_gleicher_aufbau_ergibt_vorhandenen_typ(k):
    # Belag löschen, Estrich wird oberste Schicht, Trittschalldämmung 70 → 85 mm = genau „boden-estrich-15“ (E3).
    e = bearbeiten.anwenden(k, "boden-estrich-parkett-15", [
        {"op": "schicht_loeschen", "position": 1},
        {"op": "schicht_aendern", "position": 2, "dicke_m": 0.085}])
    assert not e["neu"] and e["typ"]["schluessel"] == "boden-estrich-15"


def test_nutzung_findet_veraltete_versionen():
    k = Katalog.laden(FIXTURE)
    e = bearbeiten.anwenden(k, "staenderwand-cw75-gk2", [{"op": "querriegel_setzen", "position": 3, "hoehen_m": [1.0]}])
    k.aufnehmen(e["typ"], laufzeit=True)
    e2 = bearbeiten.anwenden(k, e["typ"]["schluessel"], [{"op": "querriegel_setzen", "position": 3, "hoehen_m": [1.5]}])
    k.aufnehmen(e2["typ"], laufzeit=True)
    n = bearbeiten.nutzung(k, [{"id": "w1", "katalog": {"schluessel": e["typ"]["schluessel"], "version": 1}},
                                {"id": "w2", "katalog": {"schluessel": "schichtwand-ks-wdvs", "version": 1}}, {"id": "x"}])
    assert n["ohne_katalogtyp"] == 1 and n["veraltet"][0]["bauteil"] == "w1" and n["veraltet"][0]["neueste"] == 2


def test_laufzeit_ueberdeckt_grundbestand_nie(k):
    with pytest.raises(KatalogFehler, match="Grundbestand"):
        Katalog.laden(FIXTURE, laufzeit=[{**k.typ("schichtwand-ks-wdvs"), "version": 7}])


def test_kurzname_staenderwand_plattendicke_in_mm():
    # 2 × 12,5 mm Beplankung — nicht „2×1,2“ (Zentimeter gerundet), Fund beim Umzug 05.11.2026
    import json
    import pathlib
    from elmonkey.katalog.modell import Katalog, benennen
    ordner = pathlib.Path(__file__).parent / "daten" / "katalog"
    k = Katalog.laden(ordner)
    sw = next(t for t in k.typen.values() if any(s.get("staenderwerk") for s in t.get("schichten") or []))
    t = {**sw, "kurzname_fest": None}
    assert "2×12,5" in benennen(t, k)["kurzname"]
