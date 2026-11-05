"""Bauen aus dem Katalog: Katalogtyp + Wandachse → Bauplan (Körper in Wandkoordinaten).

Wandkoordinaten (wie IFC, LayerSetDirection AXIS2, DirectionSense POSITIVE, Offset 0):
  x entlang der Achse 0…L, y quer zur Achse 0…D (Schicht 1 liegt an der Referenzlinie, y wächst nach links
  in Laufrichtung), z 0…H über Unterkante.

Zwei Arten (Spec §2 Nr. 5/6):
- `schichten`: ein Körper (L × D × H), Schichtfolge über IfcMaterialLayerSetUsage.
- `teile`:     Ständerwand — die Wand hat keinen eigenen Körper; Teile sind Beplankung (IfcPlate SHEET),
               Ständer (IfcMember STUD), UW (IfcMember PLATE), Querriegel (IfcMember MEMBER),
               Dämmfelder (IfcBuildingElementPart INSULATION) und Verstärkungen
               (IfcMember USERDEFINED „Wandverstärkung“). Luftschichten haben keinen Körper.
Der Bauplan ist reine Geometrie + Klasse — `ifc_bibliothek` schreibt daraus IFC, die Plattform ein Netz.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field, asdict
from typing import Optional

from .modell import Katalog, KatalogFehler, gesamtdicke

UW_HOEHE = 0.04          # Flanschhöhe UW nach DIN 18182-1 (Nennmaß)
EPS = 1e-6


@dataclass
class Koerper:
    name: str
    klasse: str
    predefined_type: str
    baustoff: Optional[str]
    x: tuple[float, float]
    y: tuple[float, float]
    z: tuple[float, float]
    object_type: Optional[str] = None
    eigenschaften: dict = field(default_factory=dict)
    schicht: Optional[int] = None

    @property
    def volumen(self) -> float:
        return (self.x[1] - self.x[0]) * (self.y[1] - self.y[0]) * (self.z[1] - self.z[0])


@dataclass
class Bauplan:
    typ: dict
    laenge: float
    hoehe: float
    dicke: float
    art: str                                  # schichten | teile
    koerper: list[Koerper]
    hinweise: list[str]
    #: Option „Ständerwand als Schichtwand“: was die Teile gewesen wären (für Eigenschaften am Bauteil)
    staenderwerk: dict = field(default_factory=dict)

    def zusammenfassung(self) -> dict:
        from collections import Counter
        z = Counter(f"{k.klasse} {k.object_type or k.predefined_type}" for k in self.koerper)
        return {"typ": f"{self.typ['schluessel']}@{self.typ.get('version', 1)}", "art": self.art,
                "laenge_m": round(self.laenge, 3), "hoehe_m": round(self.hoehe, 3), "dicke_m": round(self.dicke, 4),
                "teile": dict(z), "hinweise": self.hinweise}

    def als_dict(self) -> dict:
        d = self.zusammenfassung()
        d["koerper"] = [asdict(k) for k in self.koerper]
        return d


def staenderraster(laenge: float, achsabstand: float, flansch: float) -> list[float]:
    """Ständermitten: an beiden Enden (bündig), dazwischen im Achsraster ab Wandanfang."""
    if laenge < 2 * flansch:
        raise KatalogFehler(f"Wand {laenge:.3f} m ist kürzer als zwei Ständer")
    p = [flansch / 2]
    x = achsabstand
    while x <= laenge - 2 * flansch + EPS:
        if x - p[-1] >= flansch - EPS:
            p.append(x)
        x += achsabstand
    p.append(laenge - flansch / 2)
    return p


AUSGABEN = ("teile", "schichten")


def wand(k: Katalog, typ_ref: str, laenge: float, hoehe: float,
         verstaerkungen: Optional[list[dict]] = None, staenderwand_als: str = "teile") -> Bauplan:
    """staenderwand_als: „teile“ (Standard, Spec §2 Nr. 6) oder „schichten“ — Option für Zielsysteme ohne zerlegte Wände
    Ständerwand wie eine Schichtwand (Extrusion + LayerSetUsage), Ständerraster und
    Verstärkungen nur als Eigenschaften. Archicad legt eine zerlegte Wand ohne Körper sonst nicht an."""
    if staenderwand_als not in AUSGABEN:
        raise KatalogFehler(f"staenderwand_als muss {' oder '.join(AUSGABEN)} sein")
    t = k.typ(typ_ref)
    if t["ifc"]["klasse"] != "IfcWall":
        raise KatalogFehler(f"{t['schluessel']} ist keine Wand ({t['ifc']['klasse']})")
    if t.get("rohling"):
        pass                                   # Rohling darf gebaut werden — er bekommt erst beim Bearbeiten Namen
    if not (0.05 <= laenge <= 200):
        raise KatalogFehler("laenge 0,05…200 m")
    if not (0.1 <= hoehe <= 30):
        raise KatalogFehler("hoehe 0,1…30 m")
    D = gesamtdicke(t)
    sw_schichten = [i for i, s in enumerate(t["schichten"]) if s.get("staenderwerk")]
    if not sw_schichten:
        if verstaerkungen:
            raise KatalogFehler("verstaerkungen gibt es nur in Ständerwänden (Schicht mit staenderwerk)")
        kz = [Koerper(t.get("kurzname_fest") or t["schluessel"], "IfcWall", t["ifc"]["predefined_type"], None,
                      (0.0, laenge), (0.0, D), (0.0, hoehe))]
        return Bauplan(t, laenge, hoehe, D, "schichten", kz, [])
    teile = _staenderwand(k, t, laenge, hoehe, D, verstaerkungen or [])   # prüft auch die Lage der Verstärkungen
    if staenderwand_als == "teile":
        return teile
    from collections import Counter
    z = Counter(kp.predefined_type if kp.object_type is None else kp.object_type for kp in teile.koerper)
    sw = {"Staender": z.get("STUD", 0), "UW": z.get("PLATE", 0), "Querriegel": z.get("MEMBER", 0),
          "Verstaerkungen": "; ".join(f"{kp.name} @ {kp.eigenschaften.get('Abstand_m')} m / h {kp.eigenschaften.get('Hoehe_m')} m "
                                      f"({kp.eigenschaften.get('Seite')})" for kp in teile.koerper
                                      if kp.object_type == "Wandverstärkung") or None}
    kz = [Koerper(t.get("kurzname_fest") or t["schluessel"], "IfcWall", t["ifc"]["predefined_type"], None,
                  (0.0, laenge), (0.0, D), (0.0, hoehe))]
    return Bauplan(t, laenge, hoehe, D, "schichten", kz,
                   teile.hinweise + ["Ständerwand als Schichtwand ausgegeben (Option): Ständer/Verstärkungen nur als "
                                     "Eigenschaften (Pset <Präfix>_Staenderwerk_Bauteil)"], sw)


def _intervalle(z0: float, z1: float, schnitte: list[tuple[float, float]]) -> list[tuple[float, float]]:
    aus = [(z0, z1)]
    for a, b in sorted(schnitte):
        neu = []
        for u, v in aus:
            if b <= u + EPS or a >= v - EPS:
                neu.append((u, v))
                continue
            if a > u + EPS:
                neu.append((u, a))
            if b < v - EPS:
                neu.append((b, v))
        aus = neu
    return [(u, v) for u, v in aus if v - u > 0.005]


def _staenderwand(k: Katalog, t: dict, L: float, H: float, D: float, verst_roh: list[dict]) -> Bauplan:
    kz: list[Koerper] = []
    hinweise: list[str] = []
    y = 0.0
    lagen = []
    for i, s in enumerate(t["schichten"]):
        lagen.append((i, s, y, y + s["dicke_m"]))
        y += s["dicke_m"]
    sw_lagen = [la for la in lagen if la[1].get("staenderwerk")]

    # Verstärkungen auflösen (Lage am Bauteil): {art, abstand_m (Mitte ab Wandanfang), hoehe_m (Mitte), seite}
    verst = []
    for n, v in enumerate(verst_roh, 1):
        art = k.verstaerkungen.get(v.get("art"))
        if not art:
            raise KatalogFehler(f"verstaerkung {n}: art {v.get('art')!r} unbekannt ({', '.join(k.verstaerkungen)})")
        cx = float(v.get("abstand_m", -1))
        cz = float(v.get("hoehe_m", art["hoehe_standard_m"]))
        if not (0 <= cx <= L):
            raise KatalogFehler(f"verstaerkung {n}: abstand_m 0…{L:.3f}")
        if not (art["hoehe_m"] / 2 <= cz <= H - art["hoehe_m"] / 2):
            raise KatalogFehler(f"verstaerkung {n}: hoehe_m liegt außerhalb der Wand")
        seite = v.get("seite", "innen")
        if seite not in ("innen", "aussen"):
            raise KatalogFehler(f"verstaerkung {n}: seite innen|aussen")
        verst.append((n, art, cx, cz, seite))

    for i, s, y0, y1 in lagen:
        b = k.baustoff(s["baustoff"])
        kl = b["werkstoffklasse"]
        if s.get("staenderwerk"):
            sw = s["staenderwerk"]
            prof = k.profile[sw["profil"]]
            f = prof["flansch_m"]
            uw_key = None
            if sw.get("uw_anschluss", True):
                uw_key = "UW" + sw["profil"][2:] if sw["profil"].startswith("CW") else None
            z0, z1 = (UW_HOEHE, H - UW_HOEHE) if uw_key else (0.0, H)
            if uw_key:
                for nm, zz in (("UW unten", (0.0, UW_HOEHE)), ("UW oben", (H - UW_HOEHE, H))):
                    kz.append(Koerper(f"{nm} {uw_key}", "IfcMember", "PLATE", k.profile[uw_key]["baustoff"],
                                      (0.0, L), (y0, y1), zz, eigenschaften={"Profil": uw_key, "Norm": prof["norm"]}, schicht=i))
            mitten = staenderraster(L, sw.get("achsabstand_m", 0.625), f)
            for j, m in enumerate(mitten, 1):
                kz.append(Koerper(f"Ständer {sw['profil']} {j}", "IfcMember", "STUD", prof["baustoff"],
                                  (m - f / 2, m + f / 2), (y0 + (s["dicke_m"] - prof["steg_m"]) / 2,
                                                           y0 + (s["dicke_m"] + prof["steg_m"]) / 2), (z0, z1),
                                  eigenschaften={"Profil": sw["profil"], "Achsabstand": sw.get("achsabstand_m", 0.625),
                                                 "Norm": prof["norm"]}, schicht=i))
            riegel = [float(r["hoehe_m"]) for r in sw.get("querriegel") or []]
            # Gefache zwischen den Ständern
            for a, c in zip(mitten, mitten[1:]):
                gx = (a + f / 2, c - f / 2)
                if gx[1] - gx[0] < 0.005:
                    continue
                schnitte = []
                for r in riegel:
                    kz.append(Koerper(f"Querriegel {sw['profil']} h={r:.2f}", "IfcMember", "MEMBER", prof["baustoff"],
                                      gx, (y0, y1), (r - f / 2, r + f / 2), eigenschaften={"Profil": sw["profil"]}, schicht=i))
                    schnitte.append((r - f / 2, r + f / 2))
                v_hier = [v for v in verst if gx[0] - EPS <= v[2] <= gx[1] + EPS]
                band = []
                for n, art, cx, cz, seite in v_hier:
                    b0, b1 = cz - art["hoehe_m"] / 2, cz + art["hoehe_m"] / 2
                    vx = (max(gx[0], cx - art["breite_m"] / 2), min(gx[1], cx + art["breite_m"] / 2))
                    if vx[1] - vx[0] < art["breite_m"] - 0.005:
                        hinweise.append(f"Verstärkung {n} ({art['name']}): auf Gefachbreite {vx[1] - vx[0]:.3f} m begrenzt")
                    vy = (y1 - art["dicke_m"], y1) if seite == "innen" else (y0, y0 + art["dicke_m"])
                    kz.append(Koerper(art["name"], "IfcMember", "USERDEFINED", art["baustoff"], vx, vy, (b0, b1),
                                      object_type="Wandverstärkung",
                                      eigenschaften={"Art": art["schluessel"], "Zweck": art["zweck"],
                                                     "Last_kN": art["last_kN"], "Last_Hinweis": art.get("last_kN_hinweis"),
                                                     "Norm": art.get("norm"), "Seite": seite,
                                                     "Abstand_m": round(cx, 3), "Hoehe_m": round(cz, 3)}, schicht=i))
                    band.append((b0, b1, vy))
                if kl.startswith("daemmung"):
                    # Dämmfelder: zwischen Riegeln; im Band einer Verstärkung um deren Dicke dünner
                    grenzen = sorted({z0, z1, *[b for bb in band for b in bb[:2]]})
                    for u, w in _intervalle(z0, z1, schnitte):
                        punkte = sorted({u, w, *[g for g in grenzen if u < g < w]})
                        for p, q in zip(punkte, punkte[1:]):
                            if q - p < 0.005:
                                continue
                            yy = (y0, y1)
                            for b0, b1, vy in band:
                                if b0 <= p + EPS and q <= b1 + EPS:
                                    yy = (y0, vy[0]) if vy[1] >= y1 - EPS else (vy[1], y1)
                            kz.append(Koerper(f"Dämmung {b['name']}", "IfcBuildingElementPart", "INSULATION", s["baustoff"],
                                              gx, yy, (p, q), schicht=i))
            for n, art, cx, cz, seite in verst:
                if not any(a + f / 2 - EPS <= cx <= c - f / 2 + EPS for a, c in zip(mitten, mitten[1:])):
                    raise KatalogFehler(f"verstaerkung {n}: Mitte {cx:.3f} m liegt auf einem Ständer — verschieben")
        elif kl == "luft":
            continue
        elif kl.startswith("daemmung"):
            kz.append(Koerper(f"Dämmung {b['name']}", "IfcBuildingElementPart", "INSULATION", s["baustoff"],
                              (0.0, L), (y0, y1), (0.0, H), schicht=i))
        else:
            seite = "außen" if y1 <= sw_lagen[0][2] + EPS else "innen"
            kz.append(Koerper(f"Beplankung {b.get('kurz') or b['name']} {seite}", "IfcPlate", "SHEET", s["baustoff"],
                              (0.0, L), (y0, y1), (0.0, H), schicht=i))
    if verst and not sw_lagen:
        raise KatalogFehler("verstaerkungen gibt es nur in Ständerwänden")
    return Bauplan(t, L, H, D, "teile", kz, hinweise)


def achse(von: list[float], bis: list[float]) -> tuple[float, float]:
    """Länge und Drehwinkel (rad) einer Wandachse in der Ebene."""
    dx, dy = bis[0] - von[0], bis[1] - von[1]
    L = math.hypot(dx, dy)
    if L < 0.05:
        raise KatalogFehler("von/bis liegen zu dicht beieinander (< 5 cm)")
    return L, math.atan2(dy, dx)


def in_welt(p: tuple[float, float, float], von: list[float], winkel: float, z0: float) -> tuple[float, float, float]:
    c, s = math.cos(winkel), math.sin(winkel)
    return (von[0] + p[0] * c - p[1] * s, von[1] + p[0] * s + p[1] * c, z0 + p[2])
