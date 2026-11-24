"""elmonkey.abgleich — Archicad ↔ Plattform, ohne Archicad und ohne Plattform."""
import pytest

from elmonkey import abgleich as ab

AC1 = "11111111-1111-1111-1111-111111111111"
AC2 = "22222222-2222-2222-2222-222222222222"
AC3 = "33333333-3333-3333-3333-333333333333"


def ac(guid, **eig):
    return {"guid": guid, "typ": "Wall", "name": "W-" + guid[:2], "geschoss": "EG", "eigenschaften": eig}


def pf(guid, herkunft="archicad", ausgeblendet=False, **eig):
    return {"guid": guid, "klasse": "IfcWall", "name": "W-" + guid[:2], "geschoss": "EG", "herkunft": herkunft,
            "ausgeblendet": ausgeblendet, "eigenschaften": eig}


def test_ifc_guid_ist_22_zeichen_und_umkehrbar_eindeutig():
    a, b = ab.ifc_guid(AC1), ab.ifc_guid(AC2)
    assert len(a) == 22 and a != b
    assert ab.ifc_guid("kein-guid") is None
    assert ab.ifc_guid("{" + AC1.lower() + "}") == a


def test_werte_gleich_trotz_schreibweise():
    assert ab.gleich("0,24", 0.24)
    assert ab.gleich("", None)
    assert ab.gleich("Ja", True)
    assert ab.gleich(" F30 ", "F30")
    assert not ab.gleich("F30", "F90")


def test_paaren_ueber_guid_ifcid_stempel_und_basis():
    archicad = [ac(AC1), ac(AC2), ac(AC3, **{"Allgemeine Werte.Plattform-GUID": "PF-NEU"}), ac("4" * 8 + AC1[8:])]
    plattform = [pf(AC1.lower()), pf(ab.ifc_guid(AC2)), pf("PF-NEU", "plattform"), pf("PF-ALT", "plattform"),
                 pf("PF-OHNE", "plattform")]
    basis = {"paare": {"PF-ALT": "4" * 8 + AC1[8:]}}
    z = ab.paaren(archicad, plattform, basis)
    assert {(p["guid"], a["guid"]) for p, a in z["paare"]} == {
        (AC1.lower(), AC1), (ab.ifc_guid(AC2), AC2), ("PF-NEU", AC3), ("PF-ALT", "4" * 8 + AC1[8:])}
    assert [p["guid"] for p in z["nur_plattform"]] == ["PF-OHNE"]
    assert z["nur_archicad"] == []


def test_doppelter_stempel_wird_gemeldet():
    archicad = [ac(AC1, **{"X.Plattform-GUID": "PF"}), ac(AC2, **{"X.Plattform-GUID": "PF"})]
    plan = ab.planen(archicad, [pf("PF", "plattform")])
    assert plan["paare"] == {"PF": AC1}
    assert plan["hinweise"] and "mehreren" in plan["hinweise"][0]
    # Die Dublette trägt einen Plattform-Stempel — sie wandert nicht als „neu“ zurück zur Plattform (24.11.2026)
    assert plan["nach_plattform"]["neu_importieren"] == []
    assert [x["archicad"] for x in plan["nach_plattform"]["plattform_herkunft"]] == [AC2]


def test_drei_wege_nur_eine_seite_geaendert():
    p = pf(AC1, **{"Brandschutz.Klasse": "F90", "A.B": "x"})
    a = ac(AC1, **{"Brandschutz.Klasse": "F30", "A.B": "y"})
    v = ab.vergleichen(p, a, {"Brandschutz.Klasse": "F30", "A.B": "x"})
    assert [e["feld"] for e in v["nach_archicad"]] == ["Brandschutz.Klasse"]      # Plattform hat geändert
    assert [e["feld"] for e in v["nach_plattform"]] == ["A.B"]                    # Archicad hat geändert
    assert v["konflikte"] == []


def test_drei_wege_beide_geaendert_ist_konflikt():
    v = ab.vergleichen(pf(AC1, **{"A.B": "p"}), ac(AC1, **{"A.B": "a"}), {"A.B": "alt"})
    assert [e["feld"] for e in v["konflikte"]] == ["A.B"] and v["konflikte"][0]["basis"] == "alt"


def test_ohne_basis_fuellt_wert_die_leere_seite_sonst_frage():
    v = ab.vergleichen(pf(AC1, **{"A.B": "", "C.D": "p", "E.F": 1}), ac(AC1, **{"A.B": "a", "C.D": "", "E.F": 2}))
    assert [e["feld"] for e in v["nach_plattform"]] == ["A.B"]
    assert [e["feld"] for e in v["nach_archicad"]] == ["C.D"]
    assert [e["feld"] for e in v["konflikte"]] == ["E.F"]


def test_stempel_und_technik_werden_nie_verglichen():
    v = ab.vergleichen(pf(AC1, **{"X.Plattform-GUID": "a", "X.KI generiert": "ja"}),
                       ac(AC1, **{"X.Plattform-GUID": "b", "X.KI generiert": "nein"}))
    assert not (v["nach_archicad"] or v["nach_plattform"] or v["konflikte"] or v["gleich"])


def test_einseitiges_feld_zaehlt_nur_mit_basis():
    v = ab.vergleichen(pf(AC1, **{"Nur.Plattform": "x"}), ac(AC1))
    assert not (v["nach_archicad"] or v["konflikte"])
    v = ab.vergleichen(pf(AC1, **{"Nur.Plattform": "neu"}), ac(AC1), {"Nur.Plattform": "alt"})
    assert [e["feld"] for e in v["konflikte"]] == ["Nur.Plattform"]


def _beispiel():
    archicad = [ac(AC1, **{"A.B": "a"}), ac(AC2, **{"A.B": "a"}), ac(AC3)]
    plattform = [pf(AC1, **{"A.B": "p"}), pf(AC2, **{"A.B": "p"}), pf("PF-NEU", "plattform"),
                 pf("PF-WEG", "archicad")]
    return archicad, plattform


def test_planen_neue_bauteile_in_beide_richtungen():
    plan = ab.planen(*_beispiel())
    assert [x["plattform"] for x in plan["nach_archicad"]["neu_bauen"]] == ["PF-NEU"]
    assert [x["archicad"] for x in plan["nach_plattform"]["neu_importieren"]] == [AC3]
    assert plan["zahlen"]["paare"] == 2


def test_gleiche_unklarheit_wird_eine_frage():
    plan = ab.planen(*_beispiel())
    eig = [f for f in plan["fragen"] if f["art"] == "eigenschaft"]
    assert len(eig) == 1 and eig[0]["anzahl"] == 2 and eig[0]["feld"] == "A.B"
    weg = [f for f in plan["fragen"] if f["art"] == "fehlt_in_archicad"]
    assert len(weg) == 1 and weg[0]["bauteile"] == [{"plattform": "PF-WEG", "archicad": None}]
    assert [f["id"] for f in plan["fragen"]] == ["F1", "F2"]


def test_entscheiden_ohne_antwort_schreibt_nichts_zur_frage():
    plan = ab.planen(*_beispiel())
    erg = ab.entscheiden(plan)
    assert erg["archicad"]["eigenschaften"] == [] and erg["plattform"]["eigenschaften"] == []
    assert erg["zahlen"]["offen"] == 3
    assert "A.B" not in erg["basis"]["werte"][AC1]            # offen → kommt beim nächsten Mal wieder


def test_entscheiden_mit_antwort_und_ausnahme():
    plan = ab.planen(*_beispiel())
    f_eig = next(f["id"] for f in plan["fragen"] if f["art"] == "eigenschaft")
    f_weg = next(f["id"] for f in plan["fragen"] if f["art"] == "fehlt_in_archicad")
    erg = ab.entscheiden(plan, {f_eig: {"wahl": "archicad", "ausnahmen": {AC2: "plattform"}},
                                f_weg: "plattform_ausblenden"})
    assert erg["plattform"]["eigenschaften"] == [
        {"plattform": AC1, "archicad": AC1, "feld": "A.B", "wert": "a", "vorher": "p"}]
    assert erg["archicad"]["eigenschaften"] == [
        {"plattform": AC2, "archicad": AC2, "feld": "A.B", "wert": "p", "vorher": "a"}]
    assert erg["plattform"]["ausblenden"] == ["PF-WEG"]
    assert erg["offen"] == []
    # Basis = Wert der empfangenden Seite vor der Übertragung
    assert erg["basis"]["werte"][AC1]["A.B"] == "p" and erg["basis"]["werte"][AC2]["A.B"] == "a"


def test_falsche_antwort_wird_abgewiesen():
    plan = ab.planen(*_beispiel())
    with pytest.raises(ab.AbgleichFehler):
        ab.entscheiden(plan, {"F1": "irgendwas"})
    with pytest.raises(ab.AbgleichFehler):
        ab.entscheiden(plan, {"F99": "archicad"})


def test_zweiter_lauf_nach_dem_abgleich_ist_ruhig():
    archicad, plattform = _beispiel()
    plan = ab.planen(archicad, plattform)
    erg = ab.entscheiden(plan, {f["id"]: ("archicad" if f["art"] == "eigenschaft" else "lassen")
                                for f in plan["fragen"]})
    # beide Seiten so, wie das Ergebnis es will: Plattform übernimmt „a“, PF-NEU ist in Archicad gebaut
    for p in plattform[:2]:
        p["eigenschaften"]["A.B"] = "a"
    neu = "55555555-5555-5555-5555-555555555555"
    archicad.append(ac(neu, **{"X.Plattform-GUID": "PF-NEU"}))
    plattform.append(pf(AC3))                                   # AC3 per IFC importiert
    basis = ab.gebaut_eintragen(erg["basis"], {"PF-NEU": neu})
    plan2 = ab.planen(archicad, plattform, basis)
    z = plan2["zahlen"]
    assert (z["nach_archicad_neu"], z["nach_archicad_eigenschaften"], z["nach_plattform_neu"],
            z["nach_plattform_eigenschaften"]) == (0, 0, 0, 0)
    assert [f["art"] for f in plan2["fragen"]] == ["fehlt_in_archicad"]      # PF-WEG „lassen“ → bleibt Frage


def test_nach_abgleich_geloescht_wird_gefragt_nicht_neu_gebaut():
    basis = {"paare": {"PF-1": AC1, "PF-2": AC2}}
    plan = ab.planen([ac(AC1)], [pf("PF-2", "plattform")], basis)
    arten = {f["art"] for f in plan["fragen"]}
    assert arten == {"fehlt_in_archicad", "fehlt_in_plattform"}
    assert plan["nach_archicad"]["neu_bauen"] == [] and plan["nach_plattform"]["neu_importieren"] == []


def test_in_plattform_ausgeblendet_fragt_statt_zu_vergleichen():
    plan = ab.planen([ac(AC1, **{"A.B": "a"})], [pf(AC1, ausgeblendet=True, **{"A.B": "p"})])
    assert [f["art"] for f in plan["fragen"]] == ["in_plattform_ausgeblendet"]
    erg = ab.entscheiden(plan, {"F1": "in_archicad_loeschen"})
    assert erg["archicad"]["loeschen_von_hand"] == [AC1]


def test_uebertragung_noch_nicht_angekommen_wird_ohne_frage_wiederholt():
    archicad, plattform = [ac(AC1, **{"A.B": "a"})], [pf(AC1, **{"A.B": "p"})]
    plan = ab.planen(archicad, plattform)
    erg = ab.entscheiden(plan, {"F1": "archicad"})            # Plattform soll „a“ bekommen (als Vorschlag)
    plan2 = ab.planen(archicad, plattform, erg["basis"])      # Vorschlag noch nicht freigegeben
    assert plan2["fragen"] == []
    assert [(x["feld"], x["wert"]) for x in plan2["nach_plattform"]["eigenschaften"]] == [("A.B", "a")]
    assert plan2["nach_archicad"]["eigenschaften"] == []


def test_gestempelte_unterteile_und_ebenen_bauteile_sind_nie_neu_aus_archicad():
    """BFW07 24.11.2026: 302 Verbund-Unterteile und 120 Gerüst-Morphs (Plattform-Ebene) tragen eine Plattform-GUID,
    die der Stand nicht als eigenes Bauteil führt — sie dürfen nicht als neue Archicad-Bauteile zurückwandern."""
    kopf = pf("KOPF-1", herkunft="plattform")
    kopf["unterteile"] = [{"guid": "UT-1"}, {"guid": "UT-2"}]
    halb = pf("KOPF-2", herkunft="plattform")
    halb["unterteile"] = [{"guid": "UT-3"}, {"guid": "UT-4"}]
    archicad = [ac("AAAAAAAA-0000-0000-0000-000000000001", **{"Allgemeine Werte.Plattform-GUID": "UT-1"}),
                ac("AAAAAAAA-0000-0000-0000-000000000002", **{"Allgemeine Werte.Plattform-GUID": "UT-2"}),
                ac("AAAAAAAA-0000-0000-0000-000000000003", **{"Allgemeine Werte.Plattform-GUID": "UT-3"}),
                ac("AAAAAAAA-0000-0000-0000-000000000004", **{"Allgemeine Werte.Plattform-GUID": "GERUEST-1"}),
                ac("AAAAAAAA-0000-0000-0000-000000000005")]
    plan = ab.planen(archicad, [kopf, halb, pf("PF-NEU", herkunft="plattform")], {})
    assert [x["archicad"] for x in plan["nach_plattform"]["neu_importieren"]] == ["AAAAAAAA-0000-0000-0000-000000000005"]
    assert len(plan["nach_plattform"]["plattform_herkunft"]) == 4
    assert [x["plattform"] for x in plan["nach_archicad"]["neu_bauen"]] == ["PF-NEU"]      # Köpfe gelten als gebaut
    assert {v["plattform"]: (v["in_archicad"], v["unterteile"]) for v in plan["verbuende_ueber_unterteile"]} == {
        "KOPF-1": (2, 2), "KOPF-2": (1, 2)}
    assert plan["zahlen"]["plattform_herkunft_in_archicad"] == 4 and plan["zahlen"]["verbuende_ueber_unterteile"] == 2
    assert any("nicht als neu gemeldet" in h for h in plan["hinweise"])
    assert any("nur teilweise" in h and "KOPF-2" in h for h in plan["hinweise"])
    assert not plan["fragen"]


def test_gestempeltes_bauteil_dessen_plattform_partner_weg_ist_bleibt_eine_frage():
    """Bugbot zu #10: der Stempel darf die Frage „war abgeglichen, in der Plattform nicht mehr da“ nicht verdecken."""
    a1 = ac(AC1, **{"Allgemeine Werte.Plattform-GUID": "PF-WEG"})
    plan = ab.planen([a1], [], {"paare": {"PF-WEG": AC1}})
    assert plan["nach_plattform"]["plattform_herkunft"] == [] and plan["nach_plattform"]["neu_importieren"] == []
    assert [f["art"] for f in plan["fragen"]] == ["fehlt_in_plattform"]
