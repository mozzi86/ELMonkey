"""Form einmal + Platzierung (elmonkey.formen) — synthetische Körper, ohne Archicad."""
import math

import pytest

from elmonkey import formen as fo

WUERFEL_P = [[0, 0, 0], [1, 0, 0], [1, 2, 0], [0, 2, 0], [0, 0, 3], [1, 0, 3], [1, 2, 3], [0, 2, 3]]
WUERFEL_D = [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5],
             [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]]


def _gestellt(p, dx, dy, dz, w):
    c, s = math.cos(w), math.sin(w)
    return [[dx + c * x - s * y, dy + s * x + c * y, dz + z] for x, y, z in p]


def _bauteil(guid, punkte, dreiecke=WUERFEL_D, **mehr):
    return {"archicadGuid": guid, "art": "neu", "name": guid, "koerper": [{"punkte": punkte, "dreiecke": dreiecke}],
            **mehr}


def _datei(*bauteile):
    return {"format": fo.FORMAT_1, "lauf": "ab-test", "bauteile": list(bauteile), "fehler": [],
            "zahlen": {"neu": len(bauteile), "dreiecke": sum(len(k["dreiecke"]) for b in bauteile for k in b["koerper"])}}


def test_gleiche_koerper_werden_eine_form_und_rollen_zurueck():
    d = _datei(_bauteil("A", WUERFEL_P), _bauteil("B", _gestellt(WUERFEL_P, 10, 5, 2, 1.1)),
               _bauteil("C", _gestellt(WUERFEL_P, -3, 7, 0, -2.6)))
    n = fo.formen_bilden(d)
    assert n["format"] == fo.FORMAT_2 and len(n["formen"]) == 1
    assert all("form" in b and "koerper" not in b for b in n["bauteile"])
    assert n["zahlen"]["dreiecke_einmalig"] == 12 and n["zahlen"]["dreiecke"] == 36
    probe = fo.abweichung(d, fo.ausrollen(n))
    assert probe["aufbau_gleich"] and probe["groesste_abweichung_m"] < 1e-4


def test_einzelstueck_und_andere_form_bleiben_koerper():
    breiter = [[x * 1.5, y, z] for x, y, z in WUERFEL_P]
    d = _datei(_bauteil("A", WUERFEL_P), _bauteil("B", _gestellt(WUERFEL_P, 4, 0, 0, 0.3)), _bauteil("C", breiter))
    n = fo.formen_bilden(d)
    assert [("form" in b) for b in n["bauteile"]] == [True, True, False]
    assert n["bauteile"][2]["koerper"][0]["punkte"] == breiter
    assert fo.abweichung(d, fo.ausrollen(n))["aufbau_gleich"]


def test_gespiegelt_ist_keine_drehung():
    gespiegelt = [[-x, y, z] for x, y, z in WUERFEL_P]
    schief = [[0, 0, 0], [1, 0, 0], [1.4, 2, 0], [0, 2, 0], [0, 0, 3], [1, 0, 3], [1.4, 2, 3], [0, 2, 3]]
    d = _datei(_bauteil("A", schief), _bauteil("B", [[-x, y, z] for x, y, z in schief]))
    assert not fo.formen_bilden(d)["formen"]
    assert gespiegelt  # Quader ist spiegelsymmetrisch — deshalb oben die schiefe Form


def test_andere_dreiecke_andere_form():
    anders = [list(reversed(t)) for t in WUERFEL_D]
    d = _datei(_bauteil("A", WUERFEL_P), _bauteil("B", WUERFEL_P, anders))
    assert not fo.formen_bilden(d)["formen"]


def test_felder_bleiben_und_reihenfolge():
    d = _datei(_bauteil("A", WUERFEL_P, ifc={"klasse": "IfcFurniture"}, box=[0, 0, 0, 1, 2, 3]),
               _bauteil("B", _gestellt(WUERFEL_P, 2, 2, 0, 0.5), geschoss="EG"))
    n = fo.formen_bilden(d)
    assert [b["archicadGuid"] for b in n["bauteile"]] == ["A", "B"]
    assert n["bauteile"][0]["ifc"] == {"klasse": "IfcFurniture"} and n["bauteile"][1]["geschoss"] == "EG"
    z = fo.ausrollen(n)
    assert z["format"] == fo.FORMAT_1 and "formen" not in z and "formen" not in z["zahlen"]


def test_falsches_format_und_fehlende_form():
    with pytest.raises(fo.FormenFehler):
        fo.formen_bilden({"format": fo.FORMAT_2, "bauteile": []})
    with pytest.raises(fo.FormenFehler):
        fo.ausrollen({"format": fo.FORMAT_2, "formen": {}, "bauteile": [
            {"archicadGuid": "A", "form": "f9", "platzierung": {"ursprung": [0, 0, 0], "winkel": 0}}]})


def test_senkrechte_nadel_ohne_radius():
    nadel_p = [[0, 0, 0], [0, 0, 1], [0, 0, 2]]
    d = _datei(_bauteil("A", nadel_p, [[0, 1, 2]]), _bauteil("B", [[5, 5, z] for z in (1, 2, 3)], [[0, 1, 2]]))
    n = fo.formen_bilden(d)
    assert len(n["formen"]) == 1 and fo.abweichung(d, fo.ausrollen(n))["groesste_abweichung_m"] < 1e-6


def test_teilen_haelt_die_grenze_und_die_reihenfolge():
    stuecke = [_bauteil(f"G{i}", _gestellt(WUERFEL_P, i * 3, 0, 0, i * 0.2)) for i in range(7)]
    d = _datei(*stuecke)
    d["fehler"] = [{"archicadGuid": "X", "grund": "kein 3D-Körper"}]
    teile = fo.teilen(d, max_dreiecke=30)                      # 12 je Würfel → 2 + 2 + 2 + 1
    assert [len(t["bauteile"]) for t in teile] == [2, 2, 2, 1]
    assert [t["teil"] for t in teile] == [{"nr": n, "von": 4} for n in (1, 2, 3, 4)]
    assert [b["archicadGuid"] for t in teile for b in t["bauteile"]] == [f"G{i}" for i in range(7)]
    assert teile[0]["fehler"] and not teile[1]["fehler"] and teile[2]["zahlen"]["dreiecke"] == 24
    for t in teile:                                            # jeder Teil für sich: Formen + Gegenprobe
        n = fo.formen_bilden(t)
        assert n["teil"] == t["teil"] and fo.abweichung(t, fo.ausrollen(n))["aufbau_gleich"]


def test_teilen_ein_teil_bleibt_unveraendert_und_riese_steht_allein():
    d = _datei(_bauteil("A", WUERFEL_P), _bauteil("B", WUERFEL_P))
    assert fo.teilen(d) == [d] and "teil" not in fo.teilen(d)[0]
    teile = fo.teilen(d, max_dreiecke=5)                       # jedes Bauteil über der Grenze → je eins
    assert [len(t["bauteile"]) for t in teile] == [1, 1]
