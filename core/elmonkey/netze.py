"""Netze Archicad → Plattform ohne IFC (Format ``sab-archicad-netze/1``) — reiner Kern, nur Standardbibliothek.

Weisung Mudi 20.11.2026: keine Gesamt-IFCs mehr. Über den Diff-Checker gehen nur die **neuen** und die **in Archicad
geänderten** Bauteile mit echter Geometrie an die Plattform (``archicad_netze.json.gz``, liest
``archicad_abgleich_melden``).

Bausteine:

    triangulieren(vertices, polygons)   ein Körper aus ``TapirCommand.Get3DGeometryOfElements`` → (punkte, dreiecke);
                                        planare Polygone mit Löchern, Ohrenschneiden in der Polygonebene, Umlaufsinn
                                        bleibt (außen gegen den Uhrzeigersinn von außen), entartete Dreiecke fliegen raus
    teilen(punkte, dreiecke)            Körper > 20.000 Dreiecke in Teile mit eigenen Punktlisten (Plattform-Grenze)
    auswahl(ergebnis)                   welche Archicad-GUIDs gehen mit (neu / geaendert) — Regel siehe dort
    metadaten(archicad)                 Name, Typ, IFC, Geschoss, Ebene, Eigenschaften je GUID aus archicad.json
    netze_lesen(holen, auswahl, meta)   Pakete lesen, triangulieren, Fehler je Element sammeln
    netze_datei(...)                    das Dict der Datei (format, projekt, lauf, rahmen, erzeugt, bauteile, fehler, zahlen)
"""
from __future__ import annotations

import datetime
import gzip
import json
from typing import Any, Callable, Iterable, Optional

from . import abgleich as ab
from . import diff_plattform as dp

FORMAT = "sab-archicad-netze/1"
DATEI = "archicad_netze.json.gz"
MAX_DREIECKE = 20000                       # Plattform-Grenze NETZ_MAX_DREIECKE je Netz
PAKET = 50                                 # GUIDs je Get3DGeometryOfElements
OHNE_KOERPER = ("kein 3D-Körper — in Archicad: Ansicht → Elemente in 3D-Ansicht → Alles in 3D zeigen (⌥⌘F5), "
               "3D-Schnittebenen aus; Ebene eingeblendet? Umbau-Filter?")
FLAECHE_MIN = 1e-12                        # m² (doppelte Dreiecksfläche) — darunter entartet
NACH_PLATTFORM = ("archicad→plattform", "archicad->plattform")

Punkt = tuple[float, float, float]


class NetzFehler(ValueError):
    pass


# ─────────────────────────────────────────────────────────── Vektoren

def _sub(a: Punkt, b: Punkt) -> Punkt:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _kreuz(a: Punkt, b: Punkt) -> Punkt:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _punkt(v: Any) -> Punkt:
    """Vertex als {x,y,z} (Tapir-Schema Coordinate3D) oder [x,y,z]."""
    if isinstance(v, dict):
        return (float(v["x"]), float(v["y"]), float(v["z"]))
    return (float(v[0]), float(v[1]), float(v[2]))


def newell(punkte: list[Punkt], ring: list[int]) -> Punkt:
    """Flächennormale (Länge = doppelte Fläche) eines geschlossenen Rings nach Newell — auch für nicht konvexe Ringe."""
    nx = ny = nz = 0.0
    for i, a in enumerate(ring):
        p, q = punkte[a], punkte[ring[(i + 1) % len(ring)]]
        nx += (p[1] - q[1]) * (p[2] + q[2])
        ny += (p[2] - q[2]) * (p[0] + q[0])
        nz += (p[0] - q[0]) * (p[1] + q[1])
    return (nx, ny, nz)


# ─────────────────────────────────────────────────────────── 2D-Ohrenschneiden

def _kreuz2(o, a, b) -> float:
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def _flaeche2(ring: list[int], uv: dict[int, tuple[float, float]]) -> float:
    s = 0.0
    for i, a in enumerate(ring):
        p, q = uv[a], uv[ring[(i + 1) % len(ring)]]
        s += p[0] * q[1] - q[0] * p[1]
    return s / 2


def _bereinigen(ring: list[int], uv: dict[int, tuple[float, float]]) -> list[int]:
    """Aufeinanderfolgende gleiche Indizes/Lagen raus (auch über das Ringende)."""
    aus: list[int] = []
    for i in ring:
        if aus and (aus[-1] == i or uv[aus[-1]] == uv[i]):
            continue
        aus.append(i)
    while len(aus) > 1 and (aus[0] == aus[-1] or uv[aus[0]] == uv[aus[-1]]):
        aus.pop()
    return aus


def _im_dreieck(p, a, b, c) -> bool:
    """p in oder auf dem (gegen den Uhrzeigersinn laufenden) Dreieck a, b, c."""
    return _kreuz2(a, b, p) >= 0 and _kreuz2(b, c, p) >= 0 and _kreuz2(c, a, p) >= 0


def _loch_einhaengen(aussen: list[int], loch: list[int], uv: dict[int, tuple[float, float]]) -> list[int]:
    """Loch (im Uhrzeigersinn) über eine Brücke in den äußeren Ring (gegen den Uhrzeigersinn) einhängen
    (Eberly, „Triangulation by Ear Clipping“): Strahl vom rechtesten Lochpunkt M nach +x, nächste Kante, deren
    Endpunkt P mit dem größeren x; liegen reflexe Ecken im Dreieck M-I-P, die mit dem kleinsten Winkel zum Strahl."""
    m = max(range(len(loch)), key=lambda i: (uv[loch[i]][0], -uv[loch[i]][1]))
    mx, my = uv[loch[m]]
    n = len(aussen)
    best_x, best_i = None, None
    for i in range(n):
        a, b = uv[aussen[i]], uv[aussen[(i + 1) % n]]
        if a[1] == b[1] or my < min(a[1], b[1]) or my > max(a[1], b[1]):
            continue                                          # waagrechte Kanten fängt die Nachbarkante
        x = a[0] + (my - a[1]) / (b[1] - a[1]) * (b[0] - a[0])
        if x < mx or (best_x is not None and x >= best_x):
            continue
        best_x = x
        best_i = i if a[0] >= b[0] else (i + 1) % n
    if best_i is None:                                       # Loch liegt nicht im Ring — trotzdem anhängen
        best_i = min(range(n), key=lambda i: (uv[aussen[i]][0] - mx) ** 2 + (uv[aussen[i]][1] - my) ** 2)
        return aussen[:best_i + 1] + loch[m:] + loch[:m + 1] + aussen[best_i:]
    p = uv[aussen[best_i]]
    if p != (best_x, my):                                    # Strahl trifft keine Ecke: verdeckende reflexe Ecken?
        i_pt, mp = (best_x, my), (mx, my)
        tri = (mp, i_pt, p) if _kreuz2(mp, i_pt, p) >= 0 else (mp, p, i_pt)
        bester = None
        for j in range(n):
            q = uv[aussen[j]]
            if q == p or _kreuz2(uv[aussen[j - 1]], q, uv[aussen[(j + 1) % n]]) > 0:
                continue                                      # konvexe Ecken verdecken nicht
            if not _im_dreieck(q, *tri):
                continue
            dx, dy = q[0] - mx, q[1] - my
            schluessel = (abs(dy) / ((dx * dx + dy * dy) ** 0.5 or 1.0), dx * dx + dy * dy)
            if bester is None or schluessel < bester:
                bester, best_i = schluessel, j
    return aussen[:best_i + 1] + loch[m:] + loch[:m + 1] + aussen[best_i:]


def _auf_strecke(p, a, c) -> bool:
    """p liegt (kollinear) echt zwischen a und c."""
    if p == a or p == c or abs(_kreuz2(a, c, p)) > 1e-18:
        return False
    return (p[0] - a[0]) * (c[0] - a[0]) + (p[1] - a[1]) * (c[1] - a[1]) > 0 and \
        (p[0] - c[0]) * (a[0] - c[0]) + (p[1] - c[1]) * (a[1] - c[1]) > 0


def _ohren(ring: list[int], uv: dict[int, tuple[float, float]]) -> list[tuple[int, int, int]]:
    """Ohrenschneiden eines einfachen Rings gegen den Uhrzeigersinn (Brückenpunkte dürfen doppelt vorkommen).

    Jede Kante des Rings bleibt Dreieckskante (live BFW 20.11.2026: Wände mit Zwischenpunkten auf geraden Kanten
    wurden sonst undicht — die Nachbarfläche nutzt den Zwischenpunkt, T-Fuge). Darum: eine Ecke auf gerader Strecke
    ist nie Ohrspitze und wird nie weggeworfen, sie wird von den Nachbarn aus mittrianguliert; kein Ohr, dessen
    Diagonale durch eine Ecke läuft. Nur Stachel (Hin und zurück auf derselben Linie) fallen weg."""
    v = list(ring)
    aus: list[tuple[int, int, int]] = []
    schutz = 0
    while len(v) > 3 and schutz < 10 * len(ring) + 10:
        schutz += 1
        n = len(v)
        geschnitten = False
        for i in range(n):
            a, b, c = v[i - 1], v[i], v[(i + 1) % n]
            pa, pb, pc = uv[a], uv[b], uv[c]
            k = _kreuz2(pa, pb, pc)
            if abs(k) <= 1e-18:
                if (pa[0] - pb[0]) * (pc[0] - pb[0]) + (pa[1] - pb[1]) * (pc[1] - pb[1]) > 0 or pa == pc:
                    del v[i]                                  # Stachel: kein Flächenanteil
                    geschnitten = True
                    break
                continue                                      # Ecke auf gerader Strecke: bleibt, keine Ohrspitze
            if k < 0:                                         # reflex
                continue
            frei = True
            for j in range(n):
                q = v[j]
                pq = uv[q]
                if q in (a, b, c) or pq in (pa, pb, pc):
                    continue
                if _auf_strecke(pq, pa, pc):                  # Diagonale träfe eine Ecke → T-Fuge
                    frei = False
                    break
                if _kreuz2(uv[v[j - 1]], pq, uv[v[(j + 1) % n]]) > 0:  # sonst können nur reflexe Ecken im Ohr liegen
                    continue
                if _im_dreieck(pq, pa, pb, pc):
                    frei = False
                    break
            if frei:
                aus.append((a, b, c))
                del v[i]
                geschnitten = True
                break
        if not geschnitten:                                   # numerisch verklemmt: erste konvexe Ecke erzwingen
            for i in range(n):
                if _kreuz2(uv[v[i - 1]], uv[v[i]], uv[v[(i + 1) % n]]) > 0:
                    aus.append((v[i - 1], v[i], v[(i + 1) % n]))
                    del v[i]
                    break
            else:                                             # nur noch eine Linie: flach auffächern, Kanten bleiben
                aus += [(v[0], v[i], v[i + 1]) for i in range(1, len(v) - 1)]
                return aus
    if len(v) == 3:
        aus.append((v[0], v[1], v[2]))
    return aus


# ─────────────────────────────────────────────────────────── Körper

def _ring(x: Any) -> list[int]:
    ids = x.get("vertexIds") if isinstance(x, dict) else x
    return [int(i) for i in ids or []]


def _faecher(punkte: list[Punkt], ring: list[int]) -> list[tuple[int, int, int]]:
    """Flacher Fächer über einen Ring ohne Fläche — hält seine Kanten, die die Nachbarflächen benutzen."""
    r: list[int] = []
    for i in ring:
        if not r or punkte[r[-1]] != punkte[i]:
            r.append(i)
    while len(r) > 1 and punkte[r[0]] == punkte[r[-1]]:
        r.pop()
    return [(r[0], r[i], r[i + 1]) for i in range(1, len(r) - 1)
            if len({punkte[r[0]], punkte[r[i]], punkte[r[i + 1]]}) == 3]


def polygon_dreiecke(punkte: list[Punkt], aussen: list[int], loecher: Iterable[list[int]] = ()) -> list[tuple[int, int, int]]:
    """Ein planares Polygon (Ring + Löcher, Indizes in ``punkte``) in Dreiecke mit dem Umlaufsinn des Rings.
    Jede Kante von Ring und Löchern bleibt Dreieckskante; Polygone/Löcher ohne Fläche werden flach aufgefächert."""
    loecher = [list(l) for l in loecher]
    n3 = newell(punkte, aussen)
    if max(abs(n3[0]), abs(n3[1]), abs(n3[2])) <= FLAECHE_MIN:
        return _faecher(punkte, aussen) + [t for l in loecher for t in _faecher(punkte, l)]
    k = max(range(3), key=lambda i: abs(n3[i]))               # Achse mit der größten Normalenkomponente wegfallen lassen
    a_, b_ = (k + 1) % 3, (k + 2) % 3                          # (y,z) / (z,x) / (x,y): Normale +k ↔ gegen Uhrzeiger
    s = 1.0 if n3[k] > 0 else -1.0                             # gespiegelt, damit der Ring in 2D gegen den Uhrzeiger läuft
    alle = set(aussen)
    for l in loecher:
        alle |= set(l)
    uv = {i: (s * punkte[i][a_], punkte[i][b_]) for i in alle}
    ring = _bereinigen(aussen, uv)
    if len(ring) < 3:
        return []
    if _flaeche2(ring, uv) < 0:
        ring.reverse()
    lr, flach = [], []
    for l in loecher:
        l = _bereinigen(l, uv)
        if len(l) < 3 or abs(_flaeche2(l, uv)) <= FLAECHE_MIN:
            flach += _faecher(punkte, l)
            continue
        if _flaeche2(l, uv) > 0:
            l.reverse()
        lr.append(l)
    for l in sorted(lr, key=lambda l: -max(uv[i][0] for i in l)):
        ring = _loch_einhaengen(ring, l, uv)
    aus = []
    for a, b, c in _ohren(ring, uv):
        # entartet = gleiche Punkte; flache Dreiecke bleiben, sonst fehlte eine Kante des Rings (T-Fuge)
        if a == b or b == c or a == c or punkte[a] == punkte[b] or punkte[b] == punkte[c] or punkte[a] == punkte[c]:
            continue
        n = _kreuz(_sub(punkte[b], punkte[a]), _sub(punkte[c], punkte[a]))
        if n[0] * n3[0] + n[1] * n3[1] + n[2] * n3[2] < -FLAECHE_MIN:  # Umlaufsinn des Polygons erhalten
            b, c = c, b
        aus.append((a, b, c))
    return aus + flach


def triangulieren(vertices: list, polygons: list) -> tuple[list[list[float]], list[list[int]]]:
    """Ein Körper (``Body3D``) → (punkte [[x,y,z]], dreiecke [[i,j,k]]), 0-basiert auf die Vertexliste des Körpers.
    Nichts wird zusammengelegt; ungültige Indizes → :class:`NetzFehler`."""
    punkte = [_punkt(v) for v in vertices or []]
    n = len(punkte)
    dreiecke: list[list[int]] = []
    for poly in polygons or []:
        aussen = _ring(poly)
        loecher = [_ring(h) for h in ((poly.get("holes") if isinstance(poly, dict) else None) or [])]
        for r in [aussen, *loecher]:
            if any(i < 0 or i >= n for i in r):
                raise NetzFehler(f"Polygon verweist auf Vertex außerhalb 0…{n - 1}")
        if len(aussen) < 3:
            continue
        dreiecke += [list(t) for t in polygon_dreiecke(punkte, aussen, loecher)]
    return [list(p) for p in punkte], dreiecke


def teilen(punkte: list, dreiecke: list, max_dreiecke: int = MAX_DREIECKE) -> list[dict]:
    """Körper → [{punkte, dreiecke}]; über ``max_dreiecke`` in Teile mit eigenen (nur benutzten) Punkten."""
    if len(dreiecke) <= max_dreiecke:
        return [{"punkte": punkte, "dreiecke": dreiecke}]
    teile = []
    for s in range(0, len(dreiecke), max_dreiecke):
        neu: dict[int, int] = {}
        pk, dr = [], []
        for t in dreiecke[s:s + max_dreiecke]:
            z = []
            for i in t:
                if i not in neu:
                    neu[i] = len(pk)
                    pk.append(punkte[i])
                z.append(neu[i])
            dr.append(z)
        teile.append({"punkte": pk, "dreiecke": dr})
    return teile


def box(koerper: list[dict]) -> Optional[list[float]]:
    """[xmin, ymin, zmin, xmax, ymax, zmax] über alle benutzten Punkte (Reihenfolge wie im Format, nicht wie diff)."""
    lo, hi = [float("inf")] * 3, [float("-inf")] * 3
    for k in koerper:
        benutzt = {i for t in k["dreiecke"] for i in t}
        for i in benutzt:
            p = k["punkte"][i]
            for a in range(3):
                lo[a], hi[a] = min(lo[a], p[a]), max(hi[a], p[a])
    return [round(x, 6) for x in lo + hi] if lo[0] != float("inf") else None


# ─────────────────────────────────────────────────────────── Auswahl, Metadaten

def auswahl(ergebnis: dict) -> list[dict]:
    """Welche Archicad-Bauteile mitgehen → [{archicadGuid, art, plattformGuid}].

    - ``neu``: jede GUID aus ``plattform.neu_importieren`` (nach ``entscheiden``/Diff; enthält Öffnungen ohne
      Partner), ``plattformGuid`` null.
    - ``geaendert``: jeder Eintrag aus ``diff.geometrie`` mit ``richtung`` „archicad→plattform“ (auch ASCII
      „archicad->plattform“) — der Diff setzt sie, wenn die Archicad-Box sich seit der Basis bewegt hat, die der
      Plattform nicht. Ältere ergebnis.json ohne ``richtung``: nur ``vergleich_mit == "basis"`` (dann war die
      Referenz schon die alte Archicad-Box). „beide“/„unklar“/„plattform→archicad“ gehen nicht mit.
      ``plattformGuid`` = Paarpartner (``plattform``).
    Doppelte GUIDs (Groß/Klein egal) zählen einmal, ``neu`` gewinnt."""
    aus: list[dict] = []
    gesehen: set[str] = set()
    for x in (ergebnis.get("plattform") or {}).get("neu_importieren") or []:
        g = x.get("archicad") if isinstance(x, dict) else x
        if g and ab.guid_norm(g) not in gesehen:
            gesehen.add(ab.guid_norm(g))
            aus.append({"archicadGuid": ab.guid_norm(g), "art": "neu", "plattformGuid": None})
    for x in (ergebnis.get("diff") or {}).get("geometrie") or []:
        r = x.get("richtung")
        mit = r in NACH_PLATTFORM if r is not None else x.get("vergleich_mit") == "basis"
        g = x.get("archicad")
        if mit and g and ab.guid_norm(g) not in gesehen:
            gesehen.add(ab.guid_norm(g))
            aus.append({"archicadGuid": ab.guid_norm(g), "art": "geaendert",
                        "plattformGuid": str(x["plattform"]) if x.get("plattform") else None})
    return aus


def metadaten(archicad: dict) -> dict[str, dict]:
    """GUID (groß) → {typ, ifc, name, geschoss, ebene, eigenschaften} — aus archicad.json von ``archicad-lesen``
    (``elemente``) oder aus den GetDiffDataELM-Seiten des Diffs (``seiten``). Mengen-Psets fliegen raus."""
    if archicad.get("seiten") is not None:
        elemente = dp.archicad_bauteile(archicad["seiten"])["elemente"]
    else:
        elemente = archicad.get("elemente") or []
    aus: dict[str, dict] = {}
    for e in elemente:
        eig = {k: v for k, v in (e.get("eigenschaften") or {}).items() if not dp.ist_menge(k)}
        klasse = e.get("klasse") or eig.get(dp.F_KLASSE)
        aus[ab.guid_norm(e.get("guid"))] = {
            "typ": e.get("typ"), "ifc": {"klasse": klasse, "globalId": e.get("ifcId")},
            "name": e.get("name"), "geschoss": e.get("geschoss"),
            "ebene": e.get("ebene") or (e.get("archicad") or {}).get("ebene"), "eigenschaften": eig}
    return aus


# ─────────────────────────────────────────────────────────── Lesen + Datei

def _fehlertext(x: dict) -> str:
    f = x.get("error") or {}
    return f"{f.get('code', '')} {f.get('message', '')}".strip() or json.dumps(f, ensure_ascii=False)[:200]


def bauteil(wahl: dict, meta: Optional[dict], geometrie: dict, max_dreiecke: int = MAX_DREIECKE) -> dict:
    """Ein Eintrag von ``bauteile`` aus Auswahl + Metadaten + ``geometries3D[i]``; :class:`NetzFehler`, wenn nichts
    Brauchbares kommt."""
    if not isinstance(geometrie, dict):
        raise NetzFehler("keine Antwort")
    if "error" in geometrie:
        raise NetzFehler(_fehlertext(geometrie))
    koerper = []
    for b in geometrie.get("bodies") or []:
        p, d = triangulieren(b.get("vertices") or [], b.get("polygons") or [])
        if d:
            koerper += teilen(p, d, max_dreiecke)
    if not koerper:
        raise NetzFehler(OHNE_KOERPER)
    m = meta or {}
    return {"archicadGuid": wahl["archicadGuid"], "art": wahl["art"], "plattformGuid": wahl.get("plattformGuid"),
            "typ": m.get("typ"), "ifc": m.get("ifc") or {"klasse": None, "globalId": None}, "name": m.get("name"),
            "geschoss": m.get("geschoss"), "ebene": m.get("ebene"), "eigenschaften": m.get("eigenschaften") or {},
            "box": box(koerper), "koerper": koerper}


def netze_lesen(holen: Callable[[list[str]], list[dict]], gewaehlt: list[dict], meta: dict[str, dict],
                paket: int = PAKET, max_dreiecke: int = MAX_DREIECKE) -> tuple[list[dict], list[dict]]:
    """``holen(guids)`` → ``geometries3D`` (gleiche Reihenfolge). Paketweise; ein Fehler am Element landet in
    ``fehler``, ein Fehler des ganzen Pakets bricht ab (der Aufrufer meldet ihn)."""
    bauteile, fehler = [], []
    for s in range(0, len(gewaehlt), max(1, paket)):
        teil = gewaehlt[s:s + paket]
        antw = holen([w["archicadGuid"] for w in teil]) or []
        for i, w in enumerate(teil):
            try:
                bauteile.append(bauteil(w, meta.get(w["archicadGuid"]), antw[i] if i < len(antw) else None,
                                        max_dreiecke))
            except NetzFehler as e:
                fehler.append({"archicadGuid": w["archicadGuid"], "art": w["art"],
                               "typ": (meta.get(w["archicadGuid"]) or {}).get("typ"), "grund": str(e)})
            if w["archicadGuid"] not in meta:
                fehler.append({"archicadGuid": w["archicadGuid"], "art": w["art"],
                               "grund": "nicht in archicad.json (Metadaten fehlen)", "nur_hinweis": True})
    return bauteile, fehler


def netze_datei(projekt: Optional[str], lauf: Optional[str], bauteile: list[dict], fehler: list[dict],
                erzeugt: Optional[str] = None) -> dict:
    """Das Dict der Datei ``archicad_netze.json.gz`` (Format :data:`FORMAT`)."""
    return {"format": FORMAT, "projekt": projekt, "lauf": lauf, "rahmen": {"system": "archicad", "einheit": "m"},
            "erzeugt": erzeugt or datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
            "bauteile": bauteile, "fehler": fehler,
            "zahlen": {"neu": sum(1 for b in bauteile if b["art"] == "neu"),
                       "geaendert": sum(1 for b in bauteile if b["art"] == "geaendert"),
                       "dreiecke": sum(len(k["dreiecke"]) for b in bauteile for k in b["koerper"]),
                       "fehler": sum(1 for f in fehler if not f.get("nur_hinweis")),
                       "ohne_koerper": ohne_koerper(fehler)}}


def ohne_koerper(fehler: list[dict]) -> dict[str, int]:
    """Je Archicad-Typ: wie viele Bauteile kamen ohne 3D-Körper (meist 3D-Ansicht gefiltert)."""
    aus: dict[str, int] = {}
    for f in fehler:
        if str(f.get("grund") or "").startswith("kein 3D-Körper"):
            t = str(f.get("typ") or "?")
            aus[t] = aus.get(t, 0) + 1
    return dict(sorted(aus.items()))


def gz_bytes(datei: dict) -> bytes:
    """gzip-JSON, UTF-8, kompakt; ``mtime=0`` → gleiche Daten, gleiche Bytes."""
    return gzip.compress(json.dumps(datei, ensure_ascii=False, separators=(",", ":")).encode("utf-8"), mtime=0)


def projekt_pruefen(soll: Optional[str], ist: Optional[str]) -> None:
    """Das offene Archicad-Projekt muss das des Diffs/Lesens sein — sonst wird nichts gelesen."""
    if not soll:
        raise NetzFehler("archicad.json nennt kein Projekt — Diff bzw. archicad-lesen neu laufen lassen")
    if soll != ist:
        raise NetzFehler(f"In Archicad ist „{ist}“ offen, gelesen wurde „{soll}“ — nichts gelesen.")


def _leer(v: Any) -> bool:
    return v is None or v == ""


def ifc_ergaenzen(meta: dict[str, dict], guids: list[str], *,
                  diff_daten: Optional[Callable[[list[str]], dict]] = None,
                  ifc_typen: Optional[Callable[[list[str]], list[dict]]] = None,
                  paket: int = 200) -> list[str]:
    """Fehlende IFC-Klasse (sonst wird in der Plattform alles Proxy), GlobalId, Ebene, Geschoss, Name, Typ für die
    gewählten GUIDs nachlesen — nur lesend. ``archicad.json`` aus ``archicad-lesen`` kennt keine IFC-Klasse.

    ``diff_daten(guids)`` = eine Antwortseite von ``ELM_SAB.GetDiffDataELM {elements: […]}`` (ab 0.11.13).
    Scheitert das (älteres Add-on), ``ifc_typen(guids)`` = ``elementIFCTypes`` von
    ``TapirCommand.GetIFCTypeOfElements`` (nur die Klasse). Füllt nur leere Felder. → Hinweise."""
    hinweise: list[str] = []
    fehlt = [g for g in guids if _leer(((meta.get(g) or {}).get("ifc") or {}).get("klasse"))]
    if not fehlt:
        return hinweise
    rest = list(fehlt)
    if diff_daten is not None:
        try:
            for s in range(0, len(fehlt), max(1, paket)):
                seite = diff_daten(fehlt[s:s + paket]) or {}
                for e in dp.archicad_bauteile([seite])["elemente"]:
                    g = ab.guid_norm(e.get("guid"))
                    m = meta.setdefault(g, {"typ": None, "ifc": {"klasse": None, "globalId": None}, "name": None,
                                            "geschoss": None, "ebene": None,
                                            "eigenschaften": {k: v for k, v in (e.get("eigenschaften") or {}).items()
                                                              if not dp.ist_menge(k)}})
                    ifc = m.setdefault("ifc", {"klasse": None, "globalId": None})
                    for feld, wert in (("klasse", e.get("klasse")), ("globalId", e.get("ifcId"))):
                        if _leer(ifc.get(feld)) and not _leer(wert):
                            ifc[feld] = wert
                    for feld in ("typ", "name", "geschoss", "ebene"):
                        if _leer(m.get(feld)) and not _leer(e.get(feld)):
                            m[feld] = e.get(feld)
        except Exception as e:                                  # noqa: BLE001 — Rückfall, Grund geht in die Hinweise
            hinweise.append(f"ELM_SAB.GetDiffDataELM mit elements scheitert ({str(e)[:120]}) — IFC-Klasse über "
                            "TapirCommand.GetIFCTypeOfElements (ohne GlobalId/Ebene).")
        rest = [g for g in fehlt if _leer(((meta.get(g) or {}).get("ifc") or {}).get("klasse"))]
    if rest and ifc_typen is not None:
        try:
            for s in range(0, len(rest), max(1, paket)):
                teil = rest[s:s + paket]
                for g, x in zip(teil, ifc_typen(teil) or []):
                    k = (x or {}).get("ifcType") if isinstance(x, dict) else None
                    if not _leer(k):
                        meta.setdefault(g, {}).setdefault("ifc", {"klasse": None, "globalId": None})["klasse"] = k
        except Exception as e:                                  # noqa: BLE001
            hinweise.append(f"TapirCommand.GetIFCTypeOfElements scheitert ({str(e)[:120]}).")
    rest = [g for g in fehlt if _leer(((meta.get(g) or {}).get("ifc") or {}).get("klasse"))]
    if rest:
        hinweise.append(f"{len(rest)} Bauteile ohne IFC-Klasse — die Plattform legt sie als Proxy an.")
    return hinweise


def erzeugen(ergebnis: dict, archicad: dict, holen: Callable[[list[str]], list[dict]], projekt_ist: Optional[str],
             paket: int = PAKET, erzeugt: Optional[str] = None, *,
             diff_daten: Optional[Callable[[list[str]], dict]] = None,
             ifc_typen: Optional[Callable[[list[str]], list[dict]]] = None,
             hinweise: Optional[list[str]] = None) -> dict:
    """Der ganze Schritt für Skript und MCP: Projekt prüfen (sonst nichts lesen), auswählen, fehlende IFC-Klassen
    nachlesen (:func:`ifc_ergaenzen`), Körper lesen, Datei-Dict. ``archicad`` = archicad.json (``archicad-lesen``:
    elemente; Diff: seiten) mit ``projekt``. ``hinweise`` (Liste) bekommt, was der Zeichner wissen muss."""
    projekt_pruefen(archicad.get("projekt"), projekt_ist)
    gewaehlt = auswahl(ergebnis)
    meta = metadaten(archicad)
    if gewaehlt:
        h = ifc_ergaenzen(meta, [w["archicadGuid"] for w in gewaehlt], diff_daten=diff_daten, ifc_typen=ifc_typen)
        if hinweise is not None:
            hinweise += h
    bauteile, fehler = netze_lesen(holen, gewaehlt, meta, paket) if gewaehlt else ([], [])
    return netze_datei(archicad.get("projekt"), ergebnis.get("lauf"), bauteile, fehler, erzeugt)


def elm_diff_daten(elm: Callable[[str, dict], dict]) -> Callable[[list[str]], dict]:
    """``elm(name, params)`` → ELM_SAB-Antwort; daraus ``diff_daten(guids)`` (ohne IFC-Eigenschaften/Oberflächen)."""
    def diff_daten(guids: list[str]) -> dict:
        return elm("GetDiffDataELM", {"elements": [{"elementId": {"guid": g}} for g in guids],
                                      "includeIfcProperties": False, "includeSurfaces": False})
    return diff_daten


def tapir_ifc_typen(befehl: Callable[[str, dict], dict]) -> Callable[[list[str]], list[dict]]:
    """``befehl(name, params)`` → TapirCommand-Antwort; daraus ``ifc_typen(guids)`` (GetIFCTypeOfElements)."""
    def ifc_typen(guids: list[str]) -> list[dict]:
        r = befehl("GetIFCTypeOfElements", {"elements": [{"elementId": {"guid": g}} for g in guids]})
        return r.get("elementIFCTypes") or []
    return ifc_typen


def tapir_holen(befehl: Callable[[str, dict], dict]) -> Callable[[list[str]], list[dict]]:
    """``befehl(name, params)`` → TapirCommand-Antwort; daraus ``holen(guids)`` für Get3DGeometryOfElements."""
    def holen(guids: list[str]) -> list[dict]:
        r = befehl("Get3DGeometryOfElements", {"elements": [{"elementId": {"guid": g}} for g in guids]})
        return r.get("geometries3D") or []
    return holen
