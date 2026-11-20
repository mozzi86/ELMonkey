"""Netz-Übergabe Archicad → Plattform (elmonkey.netze) — synthetische Körper, ohne Archicad."""
import gzip
import json
import math

import pytest

from elmonkey import abgleich as ab
from elmonkey import diff_plattform as dp
from elmonkey import netze as nz


def _flaeche(p, t):
    a, b, c = (p[i] for i in t)
    u = [b[k] - a[k] for k in range(3)]
    v = [c[k] - a[k] for k in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    return n, math.sqrt(sum(x * x for x in n)) / 2


def _summe(p, d):
    return sum(_flaeche(p, t)[1] for t in d)


def _normalen_gleich(p, d, soll):
    for t in d:
        n, _ = _flaeche(p, t)
        assert sum(n[k] * soll[k] for k in range(3)) > 0, t


QUADRAT = [[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]]


def test_quadrat_zwei_dreiecke():
    p, d = nz.triangulieren(QUADRAT, [{"vertexIds": [0, 1, 2, 3]}])
    assert len(d) == 2 and _summe(p, d) == pytest.approx(1.0)
    _normalen_gleich(p, d, (0, 0, 1))
    assert p == [[float(x) for x in v] for v in QUADRAT]          # Vertexliste bleibt wie sie ist


def test_vertex_als_dict_wie_tapir_schema():
    p, d = nz.triangulieren([{"x": v[0], "y": v[1], "z": v[2]} for v in QUADRAT], [{"vertexIds": [0, 1, 2, 3]}])
    assert len(d) == 2


def test_quadrat_mit_quadratischem_loch():
    P = [[0, 0, 0], [4, 0, 0], [4, 4, 0], [0, 4, 0], [1, 1, 0], [3, 1, 0], [3, 3, 0], [1, 3, 0]]
    for loch in ([4, 7, 6, 5], [4, 5, 6, 7]):                     # Umlaufsinn des Lochs egal
        p, d = nz.triangulieren(P, [{"vertexIds": [0, 1, 2, 3], "holes": [{"vertexIds": loch}]}])
        assert len(d) == 8 and _summe(p, d) == pytest.approx(12.0)
        _normalen_gleich(p, d, (0, 0, 1))
        # kein Dreieck liegt im Loch
        for t in d:
            cx = sum(p[i][0] for i in t) / 3
            cy = sum(p[i][1] for i in t) / 3
            assert not (1 < cx < 3 and 1 < cy < 3)


def test_zwei_loecher():
    P = [[0, 0, 0], [10, 0, 0], [10, 4, 0], [0, 4, 0],
         [1, 1, 0], [3, 1, 0], [3, 3, 0], [1, 3, 0], [6, 1, 0], [8, 1, 0], [8, 3, 0], [6, 3, 0]]
    p, d = nz.triangulieren(P, [{"vertexIds": [0, 1, 2, 3], "holes": [{"vertexIds": [4, 7, 6, 5]},
                                                                      {"vertexIds": [8, 11, 10, 9]}]}])
    assert _summe(p, d) == pytest.approx(40 - 8)
    _normalen_gleich(p, d, (0, 0, 1))


def test_l_form_konkav():
    P = [[0, 0, 0], [2, 0, 0], [2, 1, 0], [1, 1, 0], [1, 2, 0], [0, 2, 0]]
    p, d = nz.triangulieren(P, [{"vertexIds": [0, 1, 2, 3, 4, 5]}])
    assert len(d) == 4 and _summe(p, d) == pytest.approx(3.0)
    _normalen_gleich(p, d, (0, 0, 1))
    for t in d:                                                    # nichts im Ausschnitt oben rechts
        cx, cy = (sum(p[i][k] for i in t) / 3 for k in (0, 1))
        assert not (cx > 1 and cy > 1)


def test_schiefe_ebene_und_umlaufsinn_von_unten():
    # L-Form in einer gekippten Ebene: Orthonormalbasis u=(1,1,0)/√2, v=(-0.6,0.6,0.8·√2)/√2 → Normale u×v
    r = 1 / math.sqrt(2)
    u, v = (r, r, 0.0), (-0.6 * r, 0.6 * r, 0.8)
    uv = [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
    P = [[5 + a * u[k] + b * v[k] for k in range(3)] for a, b in uv]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    p, d = nz.triangulieren(P, [{"vertexIds": [0, 1, 2, 3, 4, 5]}])
    # 3 oder 4 Dreiecke: die Diagonale (2,0)-(1,1)-(0,2) ist kollinear, ihr Nulldreieck fliegt raus
    assert len(d) in (3, 4) and _summe(p, d) == pytest.approx(3.0)
    _normalen_gleich(p, d, n)
    # derselbe Ring rückwärts → alle Normalen andersherum
    p, d = nz.triangulieren(P, [{"vertexIds": [5, 4, 3, 2, 1, 0]}])
    _normalen_gleich(p, d, tuple(-x for x in n))


def test_wuerfel_normalen_nach_aussen():
    P = [[x, y, z] for z in (0, 1) for y in (0, 1) for x in (0, 1)]
    i = lambda x, y, z: x + 2 * y + 4 * z                          # noqa: E731
    flaechen = [[i(0, 0, 0), i(0, 1, 0), i(1, 1, 0), i(1, 0, 0)],  # unten, von außen (unten) gegen Uhrzeiger
                [i(0, 0, 1), i(1, 0, 1), i(1, 1, 1), i(0, 1, 1)],  # oben
                [i(0, 0, 0), i(1, 0, 0), i(1, 0, 1), i(0, 0, 1)],  # y=0
                [i(1, 1, 0), i(0, 1, 0), i(0, 1, 1), i(1, 1, 1)],  # y=1
                [i(0, 1, 0), i(0, 0, 0), i(0, 0, 1), i(0, 1, 1)],  # x=0
                [i(1, 0, 0), i(1, 1, 0), i(1, 1, 1), i(1, 0, 1)]]  # x=1
    p, d = nz.triangulieren(P, [{"vertexIds": f} for f in flaechen])
    assert len(d) == 12
    for t in d:                                                    # Normale zeigt vom Mittelpunkt weg
        n, _ = _flaeche(p, t)
        c = [sum(p[j][k] for j in t) / 3 - 0.5 for k in range(3)]
        assert sum(n[k] * c[k] for k in range(3)) > 0


def test_entartete_weg():
    P = [[0, 0, 0], [1, 0, 0], [2, 0, 0], [0, 0, 0], [1, 1, 0], [0, 1, 0]]
    # kollineare Ecke (1,0) und doppelter Punkt (Index 3 = Index 0) im Ring
    p, d = nz.triangulieren(P, [{"vertexIds": [0, 1, 2, 4, 5, 3]}, {"vertexIds": [0, 1, 2]}, {"vertexIds": [0, 3, 1]}])
    assert all(_flaeche(p, t)[1] > 1e-9 for t in d)
    assert all(len(set(t)) == 3 for t in d)
    assert _summe(p, d) == pytest.approx(1.5)                      # Trapez (0,0)(2,0)(1,1)(0,1)


def test_ungueltiger_index():
    with pytest.raises(nz.NetzFehler):
        nz.triangulieren(QUADRAT, [{"vertexIds": [0, 1, 9]}])


def test_teilen_ueber_max():
    p, d = nz.triangulieren(QUADRAT, [{"vertexIds": [0, 1, 2, 3]}])
    assert nz.teilen(p, d, 2) == [{"punkte": p, "dreiecke": d}]
    gitter = [[x, y, 0.0] for y in range(11) for x in range(11)]
    dr = []
    for y in range(10):
        for x in range(10):
            a = x + 11 * y
            dr += [[a, a + 1, a + 12], [a, a + 12, a + 11]]
    teile = nz.teilen(gitter, dr, 75)
    assert [len(t["dreiecke"]) for t in teile] == [75, 75, 50]
    zurueck = [[t["punkte"][i] for i in tr] for t in teile for tr in t["dreiecke"]]
    assert zurueck == [[gitter[i] for i in tr] for tr in dr]          # gleiche Dreiecke, eigene Punktlisten
    assert all(len(t["punkte"]) < len(gitter) for t in teile)


# ─────────────────────────────────────────────────────────── Auswahl, Datei

G_NEU, G_NEU2, G_AC, G_PF, G_BEIDE, G_ALT = (f"aaaaaaaa-0000-0000-0000-00000000000{i}" for i in range(1, 7))


def test_auswahl_aus_ergebnis():
    erg = {"plattform": {"neu_importieren": [G_NEU, {"archicad": G_NEU2}, G_NEU.upper()]},
           "diff": {"geometrie": [
               {"plattform": "P-AC", "archicad": G_AC, "richtung": "archicad→plattform", "vergleich_mit": "plattform"},
               {"plattform": "P-PF", "archicad": G_PF, "richtung": "plattform→archicad", "vergleich_mit": "plattform"},
               {"plattform": "P-BE", "archicad": G_BEIDE, "richtung": "beide", "vergleich_mit": "plattform"},
               {"plattform": "P-ALT", "archicad": G_ALT, "vergleich_mit": "basis"},           # ältere Datei
               {"plattform": "P-X", "archicad": "bbbbbbbb-0000-0000-0000-000000000009", "vergleich_mit": "plattform"},
               {"plattform": "P-N", "archicad": G_NEU, "richtung": "archicad->plattform"}]}}
    w = nz.auswahl(erg)
    assert [(x["archicadGuid"], x["art"], x["plattformGuid"]) for x in w] == [
        (G_NEU.upper(), "neu", None), (G_NEU2.upper(), "neu", None),
        (G_AC.upper(), "geaendert", "P-AC"), (G_ALT.upper(), "geaendert", "P-ALT")]


def test_diff_setzt_richtung_gegen_basis():
    basis = [0, 5, 0, 0.3, 0, 2.8]
    assert dp.geometrie_richtung([0, 5.2, 0, 0.3, 0, 2.8], basis, "plattform", basis) == "archicad→plattform"
    assert dp.geometrie_richtung(basis, [0, 5.2, 0, 0.3, 0, 2.8], "plattform", basis) == "plattform→archicad"
    assert dp.geometrie_richtung([0, 5.2, 0, 0.3, 0, 2.8], [0, 4, 0, 0.3, 0, 2.8], "plattform", basis) == "beide"
    assert dp.geometrie_richtung([0, 5.2, 0, 0.3, 0, 2.8], basis, "plattform", None) == "unklar"
    assert dp.geometrie_richtung([0, 5.2, 0, 0.3, 0, 2.8], basis, "basis", basis) == "archicad→plattform"


def test_metadaten_aus_beiden_quellen_ohne_mengen():
    lesen = {"projekt": "P", "elemente": [{"guid": G_NEU.lower(), "ifcId": "X" * 22, "typ": "Wall", "name": "W-1",
                                           "geschoss": "EG", "eigenschaften": {"A.b": 1, "Qto_WallBaseQuantities.L": 2}}]}
    m = nz.metadaten(lesen)[G_NEU.upper()]
    assert m["typ"] == "Wall" and m["eigenschaften"] == {"A.b": 1} and m["ifc"]["globalId"] == "X" * 22
    seite = {"format": "elm-diff-daten/1", "elements": [{
        "elementId": {"guid": G_NEU}, "type": "Wall", "ifcType": "IfcWall", "ifcGlobalId": ab.ifc_guid(G_NEU),
        "ifcAttributes": [{"name": "Name", "value": "Wand-684"}], "storyName": "EG", "layer": "10 Wand innen",
        "ifcProperties": [{"pset": "BaseQuantities", "name": "Length", "value": 5}]}]}
    m = nz.metadaten({"projekt": "P", "seiten": [seite]})[G_NEU.upper()]
    assert m == {"typ": "Wall", "ifc": {"klasse": "IfcWall", "globalId": ab.ifc_guid(G_NEU)}, "name": "Wand-684",
                 "geschoss": "EG", "ebene": "10 Wand innen",
                 "eigenschaften": {"IFC.Name": "Wand-684", "IFC.Klasse": "IfcWall", "Verortung.Geschoss": "EG"}}


def _koerper(dx=0.0):
    return {"vertices": [{"x": v[0] + dx, "y": v[1], "z": v[2]} for v in QUADRAT],
            "polygons": [{"vertexIds": [0, 1, 2, 3]}]}


def test_netze_lesen_pakete_fehler_und_datei():
    gew = [{"archicadGuid": g, "art": "neu", "plattformGuid": None} for g in (G_NEU, G_NEU2)] + \
          [{"archicadGuid": G_AC, "art": "geaendert", "plattformGuid": "P-AC"}]
    aufrufe = []

    def holen(guids):
        aufrufe.append(list(guids))
        return [{"error": {"code": 4012, "message": "ausgeblendet"}} if g == G_NEU2 else {"bodies": [_koerper()]}
                for g in guids]

    meta = {G_NEU: {"typ": "Wall", "ifc": {"klasse": "IfcWall", "globalId": "g"}, "name": "W", "geschoss": "EG",
                    "ebene": "E", "eigenschaften": {}}, G_NEU2: {}}
    bt, fe = nz.netze_lesen(holen, gew, meta, paket=2)
    assert aufrufe == [[G_NEU, G_NEU2], [G_AC]]
    assert [b["archicadGuid"] for b in bt] == [G_NEU, G_AC]
    assert bt[0]["box"] == [0, 0, 0, 1, 1, 0] and bt[1]["plattformGuid"] == "P-AC"
    assert fe[0] == {"archicadGuid": G_NEU2, "art": "neu", "grund": "4012 ausgeblendet"}
    assert fe[1]["archicadGuid"] == G_AC and fe[1]["nur_hinweis"]
    d = nz.netze_datei("Projekt", "ab-1", bt, fe, erzeugt="2026-11-20T00:00:00+00:00")
    assert d["format"] == "sab-archicad-netze/1" and d["rahmen"] == {"system": "archicad", "einheit": "m"}
    assert d["zahlen"] == {"neu": 1, "geaendert": 1, "dreiecke": 4, "fehler": 1}
    assert json.loads(gzip.decompress(nz.gz_bytes(d))) == d
    assert nz.gz_bytes(d) == nz.gz_bytes(d)


def test_leere_koerper_sind_fehler():
    with pytest.raises(nz.NetzFehler, match="kein 3D-Körper"):
        nz.bauteil({"archicadGuid": G_NEU, "art": "neu"}, None, {"bodies": []})


def test_projekt_pruefen():
    nz.projekt_pruefen("A", "A")
    with pytest.raises(nz.NetzFehler):
        nz.projekt_pruefen("A", "B")
    with pytest.raises(nz.NetzFehler):
        nz.projekt_pruefen(None, "B")


def test_kamm_und_vieleck_mit_loechern_flaeche_stimmt():
    # Kamm (stark konkav) in der x-z-Ebene, Normale -y
    ring = [(0, 0), (13, 0), (13, 1)]
    for i in range(6, 0, -1):
        ring += [(2 * i, 1), (2 * i, 4), (2 * i - 1, 4), (2 * i - 1, 1)]
    ring += [(0, 1)]
    P = [[x, 0.0, z] for x, z in ring]
    p, d = nz.triangulieren(P, [{"vertexIds": list(range(len(P)))}])
    assert _summe(p, d) == pytest.approx(13 + 6 * 3)
    _normalen_gleich(p, d, (0, -1, 0))
    # 40-Eck mit zwei 7-Eck-Löchern
    aussen = [[10 * math.cos(2 * math.pi * k / 40), 10 * math.sin(2 * math.pi * k / 40), 1.0] for k in range(40)]
    l1 = [[3 + 2 * math.cos(-2 * math.pi * k / 7), 1 + 2 * math.sin(-2 * math.pi * k / 7), 1.0] for k in range(7)]
    l2 = [[-4 + 1.5 * math.cos(2 * math.pi * k / 7), -3 + 1.5 * math.sin(2 * math.pi * k / 7), 1.0] for k in range(7)]
    P = aussen + l1 + l2
    poly = {"vertexIds": list(range(40)), "holes": [{"vertexIds": list(range(40, 47))},
                                                     {"vertexIds": list(range(47, 54))}]}
    p, d = nz.triangulieren(P, [poly])
    flaeche = lambda r, n: n / 2 * r * r * math.sin(2 * math.pi / n)   # noqa: E731
    assert _summe(p, d) == pytest.approx(flaeche(10, 40) - flaeche(2, 7) - flaeche(1.5, 7))
    _normalen_gleich(p, d, (0, 0, 1))
