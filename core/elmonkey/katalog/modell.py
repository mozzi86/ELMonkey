"""Bauteilkatalog — Datenmodell (kein IFC, keine Plattform-Abhängigkeit).

Quelle ist ein Ordner des Aufrufers (baustoffe.json + typen/*.json = Grundbestand). Profile und Verstärkungen
bringt das Paket als herstellerneutrale Daten mit (überschreibbar). Laufzeit-Typen kommen vom Aufrufer dazu und
überdecken den Grundbestand nie (eigener Schlüssel, `basiert_auf`).

Regeln:
- Schichtbauteil: mindestens eine Schicht, Dicke ≥ 0, Luft ist eine eigene Schicht.
- Name: Kurzname + Langname aus allen Schichten außen→innen.
- Gleicher Aufbau (Fingerabdruck) = derselbe Typ (E3).
- Typ-GUID fest aus Schlüssel + Version (E4).
"""
from __future__ import annotations

import copy
import hashlib
import json
import uuid
from importlib import resources
from pathlib import Path
from typing import Any, Iterable, Optional

ROLLEN = ("LoadBearing", "Insulation", "Inner finish", "Outer finish")   # IFC-Schlüsselwörter (Category)
SCHICHT_KLASSEN = {"IfcWall": "AXIS2", "IfcSlab": "AXIS3", "IfcCovering": "AXIS3", "IfcRoof": "AXIS3"}
OEFFNUNG_KLASSEN = ("IfcDoor", "IfcWindow")
_NS = uuid.UUID("6f1c2a52-6d2b-4f0e-9a3e-5ab000000001")       # Namensraum Bauteilkatalog (unverändert lassen: GUID-Stabilität)


class KatalogFehler(ValueError):
    """Ungültige Katalogangabe — die Meldung nennt das Feld."""


def paketdaten(name: str) -> dict:
    """Herstellerneutrale Paketdaten (`profile.json`, `verstaerkungen.json`) aus elmonkey/katalog/daten."""
    return json.loads((resources.files("elmonkey.katalog") / "daten" / name).read_text(encoding="utf-8"))


# ── Laden

class Katalog:
    def __init__(self, baustoffe: dict[str, dict], typen: dict[str, dict], verstaerkungen: dict[str, dict],
                 profile: dict[str, dict]):
        self.baustoffe = baustoffe
        self.typen = typen                    # schluessel → neueste Version
        self.verstaerkungen = verstaerkungen
        self.profile = profile
        self.versionen: dict[str, dict[int, dict]] = {}
        for t in typen.values():
            self.versionen.setdefault(t["schluessel"], {})[int(t.get("version", 1))] = t

    @classmethod
    def laden(cls, ordner: "Path | str", laufzeit: Optional[Iterable[dict]] = None,
              profile: Optional[list[dict]] = None, verstaerkungen: Optional[list[dict]] = None) -> "Katalog":
        """`ordner`: baustoffe.json + typen/*.json. Profile/Verstärkungen: Parameter, sonst Datei im `ordner`,
        sonst Paketdaten."""
        ordner = Path(ordner)

        def lies(name):
            return json.loads((ordner / name).read_text(encoding="utf-8"))

        def optional(name, param, schluessel):
            if param is not None:
                return param
            return (lies(name) if (ordner / name).is_file() else paketdaten(name))[schluessel]
        baustoffe = {b["schluessel"]: b for b in lies("baustoffe.json")["baustoffe"]}
        verst = {v["schluessel"]: v for v in optional("verstaerkungen.json", verstaerkungen, "verstaerkungen")}
        profile = {p["schluessel"]: p for p in optional("profile.json", profile, "profile")}
        typen: dict[str, dict] = {}
        for datei in sorted((ordner / "typen").glob("*.json")):
            for t in json.loads(datei.read_text(encoding="utf-8"))["typen"]:
                t.setdefault("ebene", "grundbestand")
                typen[t["schluessel"]] = t
        k = cls(baustoffe, typen, verst, profile)
        for t in laufzeit or []:
            k.aufnehmen(t, laufzeit=True)
        for t in list(k.typen.values()):
            pruefen(t, k)
        return k

    def aufnehmen(self, typ: dict, laufzeit: bool = False) -> None:
        t = copy.deepcopy(typ)
        if laufzeit:
            t["ebene"] = "laufzeit"
            alt = self.typen.get(t["schluessel"])
            if alt and alt.get("ebene") == "grundbestand":
                raise KatalogFehler(f"schluessel {t['schluessel']}: Laufzeit-Typ darf den Grundbestand nicht überdecken")
        v = int(t.get("version", 1))
        self.versionen.setdefault(t["schluessel"], {})[v] = t
        if v >= int(self.typen.get(t["schluessel"], {}).get("version", 0)):
            self.typen[t["schluessel"]] = t

    def typ(self, ref: str) -> dict:
        """`schluessel` oder `schluessel@version`."""
        s, _, v = str(ref).partition("@")
        if s not in self.versionen:
            raise KatalogFehler(f"katalog_typ {s!r} unbekannt")
        if not v:
            return self.typen[s]
        try:
            return self.versionen[s][int(v)]
        except (KeyError, ValueError):
            raise KatalogFehler(f"katalog_typ {s}: Version {v} unbekannt (vorhanden: {sorted(self.versionen[s])})")

    def baustoff(self, schluessel: str) -> dict:
        try:
            return self.baustoffe[schluessel]
        except KeyError:
            raise KatalogFehler(f"baustoff {schluessel!r} unbekannt")


# ── Prüfen

def pruefen(t: dict, k: Katalog) -> None:
    s = t.get("schluessel") or "?"
    for feld in ("schluessel", "ifc"):
        if not t.get(feld):
            raise KatalogFehler(f"{s}: {feld} fehlt")
    ifc = t["ifc"]
    if not ifc.get("klasse") or not ifc.get("predefined_type"):
        raise KatalogFehler(f"{s}: ifc.klasse und ifc.predefined_type sind Pflicht (E1)")
    if ifc["predefined_type"] == "USERDEFINED" and not ifc.get("object_type"):
        raise KatalogFehler(f"{s}: USERDEFINED braucht ifc.object_type")
    if ifc["klasse"] in SCHICHT_KLASSEN:
        schichten = t.get("schichten")
        if not isinstance(schichten, list) or not schichten:
            raise KatalogFehler(f"{s}: schichten — mindestens eine Schicht (IfcMaterialLayerSet LIST [1:?])")
        for i, sch in enumerate(schichten, 1):
            k.baustoff(sch.get("baustoff"))
            d = sch.get("dicke_m")
            if not isinstance(d, (int, float)) or d < 0 or d > 5:
                raise KatalogFehler(f"{s}: schicht {i} dicke_m 0…5 m (war {d!r})")
            if sch.get("staenderwerk"):
                sw = sch["staenderwerk"]
                if sw.get("profil") not in k.profile:
                    raise KatalogFehler(f"{s}: schicht {i} staenderwerk.profil {sw.get('profil')!r} unbekannt")
                a = sw.get("achsabstand_m", 0.625)
                if not 0.2 <= a <= 1.25:
                    raise KatalogFehler(f"{s}: schicht {i} achsabstand_m 0,2…1,25 m (war {a})")
                if k.profile[sw["profil"]]["steg_m"] > d + 1e-6:
                    raise KatalogFehler(f"{s}: schicht {i} Profil {sw['profil']} ist breiter als die Schicht")
    elif ifc["klasse"] in OEFFNUNG_KLASSEN:
        for f in ("breite_m", "hoehe_m"):
            if not isinstance(t.get(f), (int, float)) or t[f] <= 0:
                raise KatalogFehler(f"{s}: {f} > 0 fehlt")


# ── Namen, Fingerabdruck, GUID

def _cm(m: float) -> str:
    z = round(m * 100, 1)
    return (f"{z:.1f}".rstrip("0").rstrip(".")).replace(".", ",")


def _mm(m: float) -> str:
    return f"{round(m * 1000, 1):g}".replace(".", ",")


def gesamtdicke(t: dict) -> float:
    return round(sum(s["dicke_m"] for s in t.get("schichten") or []), 4)


def kern(t: dict) -> Optional[dict]:
    sch = t.get("schichten") or []
    return next((s for s in sch if s.get("rolle") == "LoadBearing"), None) or \
        next((s for s in sch if s.get("staenderwerk")), None) or (max(sch, key=lambda s: s["dicke_m"]) if sch else None)


def kurzname(t: dict, k: Katalog) -> str:
    """Kurzname-Stil (Kern + wesentliche Zusatzschicht + Gesamtdicke) („KS 24 + WDVS 16 – 42 cm“)."""
    if t["ifc"]["klasse"] in OEFFNUNG_KLASSEN:
        pre = "T" if t["ifc"]["klasse"] == "IfcDoor" else "F"
        return f"{pre} {t.get('bezeichnung') or ''} {_cm(t['breite_m'])}x{_cm(t['hoehe_m'])}".replace("  ", " ").strip()
    sch = t["schichten"]
    kr = kern(t)
    teile = []
    if kr:
        if kr.get("staenderwerk"):
            sw = kr["staenderwerk"]
            bepl = [s for s in sch if k.baustoff(s["baustoff"]).get("werkstoffklasse") == "gipsplatte"]
            lagen = max(1, len(bepl) // 2) if bepl else 0
            teile.append(f"{k.profile[sw['profil']]['kurz']}" + (f" {lagen}×{_cm(bepl[0]['dicke_m'])}" if bepl else ""))
        else:
            teile.append(f"{k.baustoff(kr['baustoff']).get('kurz') or k.baustoff(kr['baustoff'])['name']} {_cm(kr['dicke_m'])}")
    zusatz = [s for s in sch if s is not kr and s.get("rolle") == "Insulation" and s["dicke_m"] >= 0.04
              and not (kr and kr.get("staenderwerk"))]
    for z in zusatz[:1]:
        teile.append(f"{k.baustoff(z['baustoff']).get('kurz') or 'WD'} {_cm(z['dicke_m'])}")
    return f"{' + '.join(teile)} – {_cm(gesamtdicke(t))} cm"


def langname(t: dict, k: Katalog) -> str:
    if t["ifc"]["klasse"] in OEFFNUNG_KLASSEN:
        return kurzname(t, k)
    out = []
    for s in t["schichten"]:
        b = k.baustoff(s["baustoff"])
        txt = f"{b.get('kurz') or b['name']} {_mm(s['dicke_m'])} mm"
        if s.get("staenderwerk"):
            sw = s["staenderwerk"]
            txt += f" ({k.profile[sw['profil']]['kurz']} e={_cm(sw.get('achsabstand_m', 0.625))} cm)"
        out.append(txt)
    return " · ".join(out) + f" | {_mm(gesamtdicke(t))} mm"


def fingerabdruck(t: dict) -> str:
    kern_ = {"ifc": [t["ifc"].get("klasse"), t["ifc"].get("predefined_type"), t["ifc"].get("object_type")],
             "schichten": [[s["baustoff"], round(s["dicke_m"], 4), s.get("rolle"), s.get("staenderwerk")]
                           for s in t.get("schichten") or []],
             "oeffnung": [t.get(f) for f in ("breite_m", "hoehe_m", "oeffnungsart", "zarge", "blatt", "verglasung")]}
    return hashlib.sha1(json.dumps(kern_, sort_keys=True, ensure_ascii=False).encode()).hexdigest()[:16]


_B64 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$"


def ifc_guid_komprimieren(hex32: str) -> str:
    """32 Hex-Zeichen (128 Bit) → IFC-GlobalId (22 Zeichen, Base64-Alphabet von IFC); wie ifcopenshell.guid.compress."""
    n = int(hex32, 16)
    if not 0 <= n < 1 << 128:
        raise ValueError("128 Bit erwartet")
    out = []
    for _ in range(21):
        out.append(_B64[n & 63])
        n >>= 6
    out.append(_B64[n])                      # die verbleibenden 2 Bit
    return "".join(reversed(out))


def typ_guid(t: dict) -> str:
    """Feste IFC-GlobalId (22 Zeichen) aus Schlüssel + Version — gleich über alle Exporte (E4)."""
    u = uuid.uuid5(_NS, f"{t['schluessel']}@{int(t.get('version', 1))}")
    return ifc_guid_komprimieren(u.hex)


def benennen(t: dict, k: Katalog) -> dict:
    t = copy.deepcopy(t)
    t["kurzname"] = t.get("kurzname_fest") or kurzname(t, k)
    t["langname"] = langname(t, k)
    t["fingerabdruck"] = fingerabdruck(t)
    return t


def doppelt(t: dict, k: Katalog) -> Optional[str]:
    """Schlüssel eines vorhandenen Typs mit gleichem Aufbau (E3), sonst None."""
    fp = fingerabdruck(t)
    for s, v in k.typen.items():
        if s != t.get("schluessel") and not v.get("rohling") and fingerabdruck(v) == fp:
            return s
    return None
