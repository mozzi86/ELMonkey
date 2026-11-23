"""Form einmal + Platzierung (Format ``sab-archicad-netze/2``) — reiner Kern, nur Standardbibliothek.

Weisung Mudi 24.11.2026 (BFW07: 619 Möbel = 4,9 Mio. Dreiecke, davon 445 Stühle): gleiche Körper stehen in der Datei
nur **einmal** (``formen``), jedes Bauteil trägt dann nur ``form`` + ``platzierung`` (Ursprung + Drehung um z).

    formen_bilden(datei)   ``sab-archicad-netze/1`` → ``/2``; Bauteile mit einer Form, die mindestens zweimal
                           vorkommt, verlieren ``koerper`` und bekommen ``form``/``platzierung``. Jede Zuordnung ist
                           nachgerechnet (alle Punkte ≤ :data:`TOLERANZ`), sonst bleibt das Bauteil wie es ist.
    bauteil_ausrollen(b, formen)   ein Bauteil zurück zu eigenen ``koerper``
    teilen(datei)          große ``/1``-Datei → Teile je ≤ 1 Mio. Dreiecke (Kopf ``teil {nr, von}``)
    ausrollen(datei)       ``/2`` → ``/1`` (jedes Bauteil wieder mit eigenen ``koerper``) — Gegenprobe und Vorlage
                           für die Plattform.

Erkannt wird über die Geometrie selbst, nicht über Bibliotheksnamen: gleiche Punktzahl und gleiche Dreiecke je Körper
(dasselbe Bibliothekselement liefert dieselbe Punktreihenfolge), dann Drehung um z + Verschiebung aus Schwerpunkt und
dem Punkt mit dem größten Abstand zur z-Achse. Gespiegelte Exemplare sind eigene Formen.

Welt = ursprung + Rz(winkel) · lokal, mit Rz(w) = [[cos w, −sin w, 0], [sin w, cos w, 0], [0, 0, 1]].
"""
from __future__ import annotations

import math
from typing import Any, Optional

FORMAT_1 = "sab-archicad-netze/1"
FORMAT_2 = "sab-archicad-netze/2"
DATEI = "archicad_netze_formen.json.gz"
TOLERANZ = 0.0005                          # m — größte Abweichung eines Punkts nach dem Ausrollen
STELLEN = 5                                # Nachkommastellen der lokalen Punkte (1/100 mm)
TEIL_MAX = 1_000_000                       # Dreiecke je Teil — Plattform-Grenze je Vorschlag (netze.MAX_DREIECKE)
TEIL_ORDNER = "netze_teile"
RADIUS_MIN = 0.001                         # m — darunter liegt alles auf der z-Achse, der Winkel ist dann frei


class FormenFehler(ValueError):
    pass


def _dreiecke(b: dict) -> int:
    return sum(len(k["dreiecke"]) for k in b.get("koerper") or [])


def _punkte(b: dict) -> list[list[float]]:
    return [p for k in b["koerper"] for p in k["punkte"]]


def _schwerpunkt(P: list) -> tuple[float, float, float]:
    n = len(P)
    return (sum(p[0] for p in P) / n, sum(p[1] for p in P) / n, sum(p[2] for p in P) / n)


def _schluessel(b: dict) -> tuple:
    """Grober Topf: Aufbau der Körper + Höhenverlauf (auf mm) — die Drehung um z ändert beides nicht."""
    P = _punkte(b)
    z0 = min(p[2] for p in P)
    return (tuple((len(k["punkte"]), hash(tuple(map(tuple, k["dreiecke"])))) for k in b["koerper"]),
            hash(tuple(round((p[2] - z0) * 1000) for p in P)))


def _lokal(b: dict) -> tuple[list[list[list[float]]], tuple[float, float, float]]:
    """Punkte je Körper um den Schwerpunkt aller Punkte, gerundet — so stehen sie in ``formen``."""
    c = _schwerpunkt(_punkte(b))
    return ([[[round(p[0] - c[0], STELLEN), round(p[1] - c[1], STELLEN), round(p[2] - c[2], STELLEN)]
              for p in k["punkte"]] for k in b["koerper"]], c)


def _platziert(lokal: list, ursprung, winkel: float) -> list[list[float]]:
    co, si = math.cos(winkel), math.sin(winkel)
    return [[ursprung[0] + co * p[0] - si * p[1], ursprung[1] + si * p[0] + co * p[1], ursprung[2] + p[2]]
            for p in lokal]


def passung(form_punkte: list[list[float]], anker: int, radius: float, Q: list[list[float]],
            toleranz: float = TOLERANZ) -> Optional[dict]:
    """Platzierung, mit der die lokale Form auf die Punkte Q fällt — oder None. ``anker`` = Index des Formpunkts mit
    dem größten Abstand zur z-Achse (``radius``)."""
    if len(form_punkte) != len(Q):
        return None
    c = _schwerpunkt(Q)
    if radius < RADIUS_MIN:
        winkel = 0.0
    else:
        p, q = form_punkte[anker], Q[anker]
        winkel = math.atan2(q[1] - c[1], q[0] - c[0]) - math.atan2(p[1], p[0])
        winkel = math.atan2(math.sin(winkel), math.cos(winkel))          # nach (−π, π]
    winkel = round(winkel, 9)
    ursprung = [round(c[0], 6), round(c[1], 6), round(c[2], 6)]
    t2 = toleranz * toleranz
    for a, q in zip(_platziert(form_punkte, ursprung, winkel), Q):
        if (a[0] - q[0]) ** 2 + (a[1] - q[1]) ** 2 + (a[2] - q[2]) ** 2 > t2:
            return None
    return {"ursprung": ursprung, "winkel": winkel}


def _zahlen(alt: dict, bauteile: list[dict], formen: dict) -> dict:
    einmalig = sum(f["dreiecke"] for f in formen.values()) + sum(_dreiecke(b) for b in bauteile if "koerper" in b)
    return {**(alt or {}), "formen": len(formen), "bauteile_mit_form": sum(1 for b in bauteile if "form" in b),
            "dreiecke_einmalig": einmalig}


def formen_bilden(datei: dict, toleranz: float = TOLERANZ) -> dict:
    """``/1`` → ``/2``. Reihenfolge und alle übrigen Felder der Bauteile bleiben."""
    if datei.get("format") != FORMAT_1:
        raise FormenFehler(f"erwartet {FORMAT_1}, ist {datei.get('format')!r}")
    toepfe: dict[tuple, list[dict]] = {}       # Schlüssel → Kandidaten {punkte, koerper, anker, radius, nutzer}
    zuordnung: list[Optional[tuple[dict, dict]]] = []
    for b in datei["bauteile"]:
        if not b.get("koerper") or not _punkte(b):
            zuordnung.append(None)
            continue
        Q = _punkte(b)
        kandidaten = toepfe.setdefault(_schluessel(b), [])
        treffer = None
        for k in kandidaten:
            pl = passung(k["punkte"], k["anker"], k["radius"], Q, toleranz)
            if pl:
                treffer = (k, pl)
                break
        if not treffer:
            koerper, _ = _lokal(b)
            flach = [p for kk in koerper for p in kk]
            anker = max(range(len(flach)), key=lambda i: flach[i][0] ** 2 + flach[i][1] ** 2)
            k = {"punkte": flach, "anker": anker, "radius": math.hypot(flach[anker][0], flach[anker][1]),
                 "koerper": [{"punkte": pk, "dreiecke": ko["dreiecke"]} for pk, ko in zip(koerper, b["koerper"])],
                 "nutzer": 0}
            pl = passung(k["punkte"], k["anker"], k["radius"], Q, toleranz)
            if not pl:                             # sollte nie eintreten (eigene Form) — dann bleibt das Bauteil
                zuordnung.append(None)
                continue
            kandidaten.append(k)
            treffer = (k, pl)
        treffer[0]["nutzer"] += 1
        zuordnung.append(treffer)
    formen: dict[str, dict] = {}
    bauteile = []
    for b, z in zip(datei["bauteile"], zuordnung):
        if not z or z[0]["nutzer"] < 2:
            bauteile.append(b)
            continue
        k, pl = z
        if "id" not in k:
            k["id"] = f"f{len(formen) + 1}"
            formen[k["id"]] = {"koerper": k["koerper"], "dreiecke": sum(len(x["dreiecke"]) for x in k["koerper"]),
                               "anzahl": k["nutzer"]}
        bauteile.append({**{f: v for f, v in b.items() if f != "koerper"}, "form": k["id"], "platzierung": pl})
    return {**datei, "format": FORMAT_2, "formen": formen, "bauteile": bauteile,
            "zahlen": _zahlen(datei.get("zahlen") or {}, bauteile, formen)}


def bauteil_ausrollen(b: dict, formen: dict) -> dict:
    """Ein Bauteil mit ``form``/``platzierung`` → dasselbe mit eigenen ``koerper`` (Archicad-Meter). Bauteile ohne
    ``form`` kommen unverändert zurück. Fehlende Form oder kaputte Platzierung: FormenFehler."""
    if not isinstance(b, dict) or "form" not in b:
        return b
    f = formen.get(b["form"]) if isinstance(formen, dict) and isinstance(b["form"], str) else None
    if not isinstance(f, dict) or not isinstance(f.get("koerper"), list):
        raise FormenFehler(f"{b.get('archicadGuid')}: Form {b['form']!r} fehlt in formen")
    pl = b.get("platzierung") if isinstance(b.get("platzierung"), dict) else {}
    try:
        ursprung = [float(pl["ursprung"][0]), float(pl["ursprung"][1]), float(pl["ursprung"][2])]
        winkel = float(pl["winkel"])
        if not all(math.isfinite(x) for x in (*ursprung, winkel)):
            raise ValueError
        koerper = [{"punkte": [[round(c, 6) for c in p] for p in _platziert(k["punkte"], ursprung, winkel)],
                    "dreiecke": k["dreiecke"]} for k in f["koerper"]]
    except (KeyError, IndexError, TypeError, ValueError) as e:
        raise FormenFehler(f"{b.get('archicadGuid')}: platzierung braucht ursprung [x, y, z] und winkel (rad); "
                           f"Form {b['form']!r} braucht koerper mit punkte/dreiecke") from e
    return {**{x: v for x, v in b.items() if x not in ("form", "platzierung")}, "koerper": koerper}


def ausrollen(datei: dict) -> dict:
    """``/2`` → ``/1``: jedes Bauteil wieder mit eigenen Körpern in Archicad-Metern."""
    if datei.get("format") != FORMAT_2:
        raise FormenFehler(f"erwartet {FORMAT_2}, ist {datei.get('format')!r}")
    formen = datei.get("formen") or {}
    bauteile = [bauteil_ausrollen(b, formen) for b in datei["bauteile"]]
    zahlen = {k: v for k, v in (datei.get("zahlen") or {}).items()
              if k not in ("formen", "bauteile_mit_form", "dreiecke_einmalig")}
    return {**{k: v for k, v in datei.items() if k != "formen"}, "format": FORMAT_1, "bauteile": bauteile,
            "zahlen": zahlen}


def teil_name(nr: int) -> str:
    return f"archicad_netze_teil_{nr:02d}.json.gz"


def teilen(datei: dict, max_dreiecke: int = TEIL_MAX) -> list[dict]:
    """Eine ``/1``-Datei in Teile zu je höchstens ``max_dreiecke`` (in Dateireihenfolge). Jeder Teil ist eine
    eigene, vollständige ``/1``-Datei mit Kopf ``teil {nr, von}`` — die Plattform macht aus jedem einen Vorschlag
    und hält so ihren Speicher je Aufruf. Passt alles in einen Teil, kommt die Datei unverändert zurück (ohne
    ``teil``). ``fehler`` stehen nur im ersten Teil."""
    if datei.get("format") != FORMAT_1:
        raise FormenFehler(f"erwartet {FORMAT_1}, ist {datei.get('format')!r}")
    gruppen: list[list[dict]] = [[]]
    summe = 0
    for b in datei["bauteile"]:
        n = _dreiecke(b)
        if gruppen[-1] and summe + n > max_dreiecke:
            gruppen.append([])
            summe = 0
        gruppen[-1].append(b)
        summe += n
    if len(gruppen) == 1:
        return [datei]
    alt = datei.get("zahlen") or {}
    teile = []
    for nr, g in enumerate(gruppen, 1):
        zahlen = {"neu": sum(1 for b in g if b.get("art") == "neu"),
                  "geaendert": sum(1 for b in g if b.get("art") == "geaendert"),
                  "dreiecke": sum(_dreiecke(b) for b in g)}
        if nr == 1:
            zahlen.update({k: v for k, v in alt.items() if k not in zahlen})
        teile.append({**datei, "teil": {"nr": nr, "von": len(gruppen)}, "bauteile": g,
                      "fehler": datei.get("fehler") or [] if nr == 1 else [], "zahlen": zahlen})
    return teile


def abweichung(a: dict, b: dict) -> dict[str, Any]:
    """Gegenprobe zweier ``/1``-Dateien: größte Punktabweichung (m) und ob Aufbau und Dreiecke gleich sind."""
    groesste, gleich = 0.0, len(a["bauteile"]) == len(b["bauteile"])
    for x, y in zip(a["bauteile"], b["bauteile"]):
        if x.get("archicadGuid") != y.get("archicadGuid") or len(x["koerper"]) != len(y["koerper"]):
            gleich = False
            continue
        for kx, ky in zip(x["koerper"], y["koerper"]):
            if kx["dreiecke"] != ky["dreiecke"] or len(kx["punkte"]) != len(ky["punkte"]):
                gleich = False
                continue
            for p, q in zip(kx["punkte"], ky["punkte"]):
                groesste = max(groesste, math.dist(p, q))
    return {"aufbau_gleich": gleich, "groesste_abweichung_m": groesste}
