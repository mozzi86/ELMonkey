#!/usr/bin/env python3
"""Abgleich offenes Archicad-Projekt ↔ Schwarz Plattform (bidirektional). Nur Standardbibliothek.

Ablauf (Rezept: skills/archicad/recipes/plattform-abgleich.md). Die Links kommen aus dem Plattform-Konnektor
(`archicad_abgleich_lesen`); große Dateien laufen über diese Links, nicht durch den Chat.

    plattform_abgleich.py archicad-lesen  --ordner <dir> [--port 19723] [--typen Wall,Slab,…] [--alle-eigenschaften]
    plattform_abgleich.py planen          --ordner <dir> --stand-link "<stand_link>"
    plattform_abgleich.py entscheiden     --ordner <dir> [--antworten antworten.json]
    plattform_abgleich.py archicad-schreiben --ordner <dir> [--ausfuehren]      # Eigenschaften nach Archicad
    plattform_abgleich.py stempeln        --ordner <dir> --gebaut gebaut.json [--ausfuehren]
    plattform_abgleich.py bauen           --ordner <dir> --ifc plattform.ifc --bauliste bauliste.json --register register.json
                                          [--zuordnung z.json] [--nur GUID,..] [--ohne GUID,..] [--vorhanden v.json] [--ausfuehren]
    plattform_abgleich.py hochladen       --ordner <dir> --ablage "<ergebnis_ablage>"
    plattform_abgleich.py oberflaechen    --ordner <dir>                     # Export + ZIP für die Plattform
    plattform_abgleich.py ifc             --ordner <dir> [--translator "<Name>"]
    plattform_abgleich.py diff            --ordner <dir> --stand-link "<stand_link>" [--toleranz 0.003]
                                          # nur die Unterschiede, ohne IFC-Datei (ELMonkey ≥ 0.11.13)

Alles, was Archicad verändert, ist ohne `--ausfuehren` ein Trockenlauf. Gelöscht wird in Archicad nie.
Dateien im Arbeitsordner: archicad.json, stand.json, plan.json, fragen.md, antworten.json, ergebnis.json,
archicad_geschrieben.json, archicad_diff.json (Befehl diff); Befehl bauen: ausgelassen.json (immer),
angelegt.json / gebaut.json / ruecklese.json (je Paket fortgeschrieben, nur mit --ausfuehren).
"""
from __future__ import annotations

import argparse
import collections
import datetime
import json
import re
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid
import zipfile
from pathlib import Path
from typing import Any, Callable, Optional

try:
    from elmonkey import abgleich as ab
    from elmonkey import diff_plattform as dp
except ImportError:                                     # aus dem Repo heraus, ohne installiertes elmonkey-core
    sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "core"))
    from elmonkey import abgleich as ab
    from elmonkey import diff_plattform as dp

PORT = 19723
#: Elementtypen, die als Bauteil zählen (2D, Bemaßung, Zonen-Stempel usw. bleiben draußen).
TYPEN = ("Wall", "Column", "Beam", "Slab", "Roof", "Shell", "Morph", "Object", "Door", "Window", "Skylight",
         "Stair", "Railing", "CurtainWall", "Mesh", "Zone", "Opening", "Lamp")
PAKET = 400                                             # Elemente je Aufruf
STEMPEL_GRUPPE, STEMPEL_NAME, KI_NAME = "Allgemeine Werte", ab.PLATTFORM_GUID, "KI generiert"


class Fehler(RuntimeError):
    pass


# ─────────────────────────────────────────────────────────── Archicad (HTTP-JSON)

def _http(port: int) -> Callable[[dict], dict]:
    def senden(body: dict) -> dict:
        req = urllib.request.Request(f"http://127.0.0.1:{port}", data=json.dumps(body).encode(),
                                     headers={"Content-Type": "application/json"})
        try:
            return json.load(urllib.request.urlopen(req, timeout=900))
        except OSError as e:
            raise Fehler(f"Archicad antwortet nicht auf Port {port} — läuft Archicad mit ELMonkey? ({e})") from e
    return senden


class Archicad:
    """Dünne Hülle um die JSON-Schnittstelle. `senden` und `schlafen` sind austauschbar (Tests)."""

    SPERRE_VERSUCHE = 60                                    # 60 × 15 s = 15 min

    def __init__(self, senden: Callable[[dict], dict], schlafen: Callable[[float], Any] = time.sleep):
        self._senden = senden
        self._schlafen = schlafen

    def _rufen(self, name: str, body: dict) -> dict:
        """Sendet; bei Fehler 4001 (Archicad beschäftigt: Eingabe/Dialog) alle 15 s erneut, höchstens 15 min.
        „no open project“ ist kein Warten wert."""
        for _ in range(self.SPERRE_VERSUCHE):
            r = self._senden(body)
            if r.get("succeeded"):
                return r
            fehler = r.get("error") or {}
            if fehler.get("code") == 4001 and "open project" not in json.dumps(r, ensure_ascii=False):
                self._schlafen(15)
                continue
            raise Fehler(f"{name}: {json.dumps(fehler or r, ensure_ascii=False)[:500]}")
        raise Fehler(f"{name}: Archicad blieb 15 min gesperrt")

    def api(self, name: str, params: Optional[dict] = None) -> dict:
        """Eingebauten Befehl (API.*) direkt senden, gibt `result` zurück."""
        r = self._rufen(name, {"command": name, "parameters": params or {}})
        return r.get("result") or {}

    def befehl(self, name: str, params: Optional[dict] = None, ns: str = "TapirCommand") -> dict:
        r = self._rufen(f"{ns}.{name}", {"command": "API.ExecuteAddOnCommand", "parameters": {
            "addOnCommandId": {"commandNamespace": ns, "commandName": name},
            "addOnCommandParameters": params or {}}})
        antwort = (r.get("result") or {}).get("addOnCommandResponse") or {}
        if antwort.get("success") is False or ("error" in antwort and len(antwort) == 1):
            raise Fehler(f"{ns}.{name}: {json.dumps(antwort.get('error'), ensure_ascii=False)[:500]}")
        return antwort

    def elm(self, name: str, params: Optional[dict] = None) -> dict:
        return self.befehl(name, params, "ELM_SAB")


def _pakete(liste: list, n: int = PAKET):
    for i in range(0, len(liste), n):
        yield liste[i:i + n]


def _schluessel(p: dict) -> str:
    return f"{p.get('propertyGroupName')}.{p.get('propertyName')}"


def eigenschaften_katalog(ac: Archicad, alle: bool = False) -> dict[str, dict]:
    """{„Gruppe.Name“: Definition} — Vorgabe nur eigene (benutzerdefinierte) Eigenschaften, die sind der Fachinhalt."""
    aus: dict[str, dict] = {}
    for p in ac.befehl("GetAllProperties").get("properties") or []:
        # Tapir 1.7.2 meldet benutzerdefinierte Eigenschaften als „Custom“ (live BFW07 19.11.2026: 230 Custom,
        # 0 UserDefined — vorher 0 Eigenschaften und damit keine Paare); ältere Stände sagen „UserDefined“.
        if alle or p.get("propertyType") in ("UserDefined", "Custom"):
            aus.setdefault(_schluessel(p), p)
    return aus


def _ifc_kennung(zeile: Any) -> Optional[str]:
    """Kennung für die Paarung: `externalIFCId` (GlobalId eines per IFC gekommenen Bauteils) vor `ifcId`
    (Archicads eigene). Antwortform live geprüft mit 0.11.12 / AC29: `elementIFCIds`, je Eintrag ifcId +
    externalIFCId (übernommen aus dem Zweig elm-0.11.13-diff-checker, 16.11.2026)."""
    z = zeile if isinstance(zeile, dict) else {}
    return str(z.get("externalIFCId") or "").strip() or str(z.get("ifcId") or "").strip() or None


def archicad_lesen(ac: Archicad, typen=TYPEN, alle_eigenschaften: bool = False) -> dict:
    """Offenes Projekt als Abgleich-Stand: je Bauteil GUID, IFC-Id, Typ, Element-ID, Geschoss, Eigenschaften."""
    info = ac.befehl("GetProjectInfo")
    geschosse = {s.get("index"): s.get("name") for s in ac.befehl("GetStories").get("stories") or []}
    katalog = eigenschaften_katalog(ac, alle_eigenschaften)
    namen = sorted(katalog)
    prop_ids = [{"propertyId": katalog[n]["propertyId"]} for n in namen]
    elemente: list[dict] = []
    # Je Typ abfragen: GetDetailsOfElements.type heißt nicht wie die TYPEN-Namen (live wurden 89.823 Elemente
    # inkl. 2D gezählt) — der Typ ist deshalb der abgefragte.
    ids: list[tuple[str, dict]] = []
    for t in typen:
        ids += [(t, e["elementId"]) for e in ac.api("API.GetElementsByType", {"elementType": t}).get("elements") or []]
    for teil_t in _pakete(ids):
        teil = [i for _, i in teil_t]
        details = ac.befehl("GetDetailsOfElements", {"elements": [{"elementId": i} for i in teil]})
        behalten = [(i, {**(d if isinstance(d, dict) else {}), "type": t}) for (t, i), d
                    in zip(teil_t, details.get("detailsOfElements") or [])]
        if not behalten:
            continue
        el = [{"elementId": i} for i, _ in behalten]
        ifc = ac.befehl("GetIFCIdsOfElements", {"elements": el}).get("elementIFCIds") or []
        werte = (ac.befehl("GetPropertyValuesOfElements", {"elements": el, "properties": prop_ids})
                 .get("propertyValuesForElements") or []) if prop_ids else []
        for n, (i, d) in enumerate(behalten):
            eig = {}
            zeile = (werte[n] if n < len(werte) else {}).get("propertyValues") or []
            for name, w in zip(namen, zeile):
                pv = (w or {}).get("propertyValue")
                if pv is not None:                       # „nicht verfügbar“ = die Eigenschaft gilt für den Typ nicht
                    eig[name] = pv.get("value")
            elemente.append({"guid": i.get("guid"), "ifcId": _ifc_kennung(ifc[n] if n < len(ifc) else None),
                             "typ": d.get("type"), "name": d.get("id"),
                             "geschoss": geschosse.get(d.get("floorIndex")), "eigenschaften": eig})
    return {"format": ab.FORMAT, "projekt": info.get("projectName"), "teamwork": bool(info.get("isTeamwork")),
            "pfad": info.get("projectPath") or info.get("projectLocation"),
            "eigenschaften_katalog": {n: {"propertyId": katalog[n]["propertyId"],
                                          "editierbar": katalog[n].get("propertyIsEditable", True)} for n in namen},
            "elemente": elemente}


def eigenschaften_schreiben(ac: Archicad, eintraege: list[dict], katalog: dict[str, dict], *, teamwork: bool,
                            ausfuehren: bool, reservieren: bool = True) -> dict:
    """[{archicad, feld, wert}] nach Archicad. Was Archicad nicht als Eigenschaft kennt oder nicht editierbar ist,
    wird gemeldet, nicht geraten. Mit `ausfuehren`: reservieren (Teamwork), setzen, zurücklesen."""
    geht, geht_nicht = [], []
    for x in eintraege:
        d = katalog.get(x["feld"])
        if not d:
            geht_nicht.append({**x, "grund": "Eigenschaft gibt es in Archicad nicht"})
        elif d.get("editierbar") is False:
            geht_nicht.append({**x, "grund": "Eigenschaft ist in Archicad nicht editierbar (Ausdruck/gesperrt)"})
        else:
            geht.append(x)
    aus = {"ausgefuehrt": ausfuehren, "moeglich": len(geht), "nicht_moeglich": geht_nicht, "geschrieben": [],
           "fehler": []}
    if not ausfuehren or not geht:
        return aus
    if teamwork and reservieren:
        guids = sorted({x["archicad"] for x in geht})
        for teil in _pakete(guids):
            r = ac.befehl("ReserveElements", {"elements": [{"elementId": {"guid": g}} for g in teil]})
            if r.get("conflicts"):
                aus["fehler"].append({"grund": "Teamwork: von anderen reserviert", "anzahl": len(r["conflicts"])})
    for teil in _pakete(geht):
        # Natives API verlangt die volle Wertform {type,status,value}; die Kurzform liefert nur einen 4002
        ac.api("API.SetPropertyValuesOfElements", {"elementPropertyValues": [
            {"elementId": {"guid": x["archicad"]}, "propertyId": katalog[x["feld"]]["propertyId"],
             "propertyValue": {"type": "string", "status": "normal",
                               "value": "" if x["wert"] is None else str(x["wert"])}} for x in teil]})
    # Rücklese — die Antwort des Schreibens zählt nicht
    je_feld: dict[str, list[dict]] = {}
    for x in geht:
        je_feld.setdefault(x["feld"], []).append(x)
    for feld, liste in je_feld.items():
        for teil in _pakete(liste):
            werte = ac.befehl("GetPropertyValuesOfElements", {
                "elements": [{"elementId": {"guid": x["archicad"]}} for x in teil],
                "properties": [{"propertyId": katalog[feld]["propertyId"]}]}).get("propertyValuesForElements") or []
            for n, x in enumerate(teil):
                zeile = (werte[n] if n < len(werte) else {}).get("propertyValues") or [{}]
                ist = ((zeile[0] or {}).get("propertyValue") or {}).get("value")
                (aus["geschrieben"] if ab.gleich(ist, x["wert"]) else aus["fehler"]).append(
                    {**x, "ist": ist} if not ab.gleich(ist, x["wert"]) else x)
    return aus


# ─────────────────────────────────────────────────────────── Dateien, Links

def _lesen(pfad: Path) -> Any:
    if not pfad.is_file():
        raise Fehler(f"{pfad} fehlt — Schritt davor gelaufen?")
    return json.loads(pfad.read_text(encoding="utf-8"))


def _schreiben(pfad: Path, daten: Any) -> None:
    pfad.parent.mkdir(parents=True, exist_ok=True)
    pfad.write_text(json.dumps(daten, ensure_ascii=False, indent=1), encoding="utf-8")


def _nur_https(link: str) -> str:
    if not link.startswith("https://"):
        raise Fehler("Link muss mit https:// beginnen (aus archicad_abgleich_lesen)")
    return link


def herunterladen(link: str) -> Any:
    link = _nur_https(link)
    try:
        with urllib.request.urlopen(link, timeout=900) as r:
            return json.loads(r.read().decode("utf-8"))
    except (urllib.error.URLError, ssl.SSLError) as e:
        if "CERTIFICATE_VERIFY_FAILED" not in str(e):
            raise
    # python.org-Python auf macOS kennt die Systemzertifikate nicht → curl (Argumentliste, nie über die Shell)
    r = subprocess.run(["curl", "-sS", "--fail", link], capture_output=True, timeout=900)
    if r.returncode != 0:
        raise Fehler(f"curl scheitert ({r.returncode}): {r.stderr.decode('utf-8', 'replace')[:300]}")
    return json.loads(r.stdout.decode("utf-8"))


def ablegen(link: str, daten: bytes, typ: str) -> None:
    req = urllib.request.Request(_nur_https(link), data=daten, method="PUT",
                                 headers={"x-ms-blob-type": "BlockBlob", "Content-Type": typ})
    with urllib.request.urlopen(req, timeout=3600) as r:
        if r.status not in (200, 201):
            raise Fehler(f"Ablage antwortet mit {r.status}")


def fragen_text(plan: dict) -> str:
    z = plan["zahlen"]
    zeilen = [f"# Abgleich Archicad ↔ Plattform — {z['paare']} Paare", "",
              f"- nach Archicad: {z['nach_archicad_neu']} neue Bauteile, {z['nach_archicad_eigenschaften']} Eigenschaften",
              f"- nach Plattform: {z['nach_plattform_neu']} neue Bauteile (per IFC), "
              f"{z['nach_plattform_eigenschaften']} Eigenschaften",
              f"- unklar: {z['fragen']} Fragen zu {z['fragen_bauteile']} Bauteilen", ""]
    zeilen += [f"- Hinweis: {h}" for h in plan.get("hinweise") or []]
    for f in plan.get("fragen") or []:
        zeilen += ["", f"## {f['id']} — {f['anzahl']} Bauteile", f["text"],
                   "Beispiele: " + "; ".join(f["beispiele"]), "Antworten: " + " | ".join(f["optionen"])]
    return "\n".join(zeilen) + "\n"


# ─────────────────────────────────────────────────────────── IFC → Morph (Befehl bauen)

BAU_PAKET = 25
SAB_SYSTEM = "SAB_Klassifizierung_29"
STATUS = {"Abbruch": "ToBeDemolished", "Bestand": "Existing", "Neubau": "New"}
ZUORDNUNG = Path(__file__).with_name("sab_zuordnung.json")
_IFC_ZEICHEN = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$"
_STEP = re.compile(r"#(\d+)=\s*([A-Z0-9]+)\((.*)\);\s*$")
_REF = re.compile(r"#(\d+)")


def guid_lang(g22: str) -> str:
    """IFC-GlobalId (22 Zeichen, Base64-Alphabet von IFC) → UUID in Großbuchstaben."""
    n = 0
    for c in g22:
        n = n * 64 + _IFC_ZEICHEN.index(c)
    return str(uuid.UUID(int=n)).upper()


def ifc_netze(pfad) -> dict[str, list[tuple[list, list]]]:
    """{GUID (lang): [(Punkte, Dreiecke 0-basiert)]} aus den IFCTRIANGULATEDFACESET der Datei. Unterteile
    (IfcRelAggregates) werden dem Elternteil zugeschlagen. Nur GUIDs mit mindestens einem Netz."""
    zeilen: dict[int, tuple[str, str]] = {}
    with open(pfad, encoding="latin-1") as f:
        for z in f:
            m = _STEP.match(z)
            if m:
                zeilen[int(m.group(1))] = (m.group(2), m.group(3))

    def refs(a: str) -> list[int]:
        return [int(x) for x in _REF.findall(a)]

    nr2g: dict[int, str] = {}
    kinder: dict[int, list[int]] = collections.defaultdict(list)
    for i, (t, a) in zeilen.items():
        m = re.match(r"'([^']{22})',", a)
        if m and all(c in _IFC_ZEICHEN for c in m.group(1)) and not t.startswith("IFCREL") \
                and t not in ("IFCPROPERTYSET", "IFCELEMENTQUANTITY"):
            nr2g[i] = guid_lang(m.group(1))
        if t == "IFCRELAGGREGATES":
            r = refs(a)
            for kind in r[2:]:                          # r[0] = Besitzer, r[1] = Elternteil
                kinder[r[1]].append(kind)

    def netze(i: int) -> list[tuple[list, list]]:
        aus: list[tuple[list, list]] = []
        for pd in [x for x in refs(zeilen[i][1]) if zeilen.get(x, ("",))[0] == "IFCPRODUCTDEFINITIONSHAPE"]:
            for sr in refs(zeilen[pd][1]):
                for it in refs(zeilen.get(sr, ("", ""))[1]):
                    if zeilen.get(it, ("",))[0] == "IFCTRIANGULATEDFACESET":
                        fa = zeilen[it][1]
                        P = [[float(x) for x in p.split(",")]
                             for p in re.findall(r"\(([-0-9.Ee+,]+)\)", zeilen[refs(fa)[0]][1])]
                        F = [[int(x) - 1 for x in f.split(",")]
                             for f in re.findall(r"\((\d+,\d+,\d+)\)", fa[fa.index("(("):])]
                        aus.append((P, F))
        for k in kinder.get(i, []):
            aus += netze(k)
        return aus

    ergebnis = {g: netze(i) for i, g in nr2g.items()}
    return {g: n for g, n in ergebnis.items() if n}


def zerlegen(P: list, F: list) -> list[tuple[list, list]]:
    """Netz in Einzelkörper trennen: Dreiecke hängen nur über Kanten mit genau 2 Nachbarn zusammen. Verschweißte
    Teilstücke (Kante mit 4 Nachbarn) werden so getrennt und bekommen eigene Punkte — Archicad nimmt nur saubere
    Körper. Entartete Dreiecke (zwei gleiche Punkte) fallen weg."""
    F = [f for f in F if len({tuple(P[i]) for i in f}) == 3]
    kanten: dict[tuple, list[int]] = collections.defaultdict(list)
    for n, f in enumerate(F):
        for a in range(3):
            kanten[tuple(sorted((f[a], f[(a + 1) % 3])))].append(n)
    wurzel = list(range(len(F)))

    def finde(x: int) -> int:
        while wurzel[x] != x:
            wurzel[x] = wurzel[wurzel[x]]
            x = wurzel[x]
        return x

    for fs in kanten.values():
        if len(fs) == 2:
            wurzel[finde(fs[0])] = finde(fs[1])
    gruppen: dict[int, list] = collections.defaultdict(list)
    for n in range(len(F)):
        gruppen[finde(n)].append(F[n])
    aus = []
    for fs in gruppen.values():
        idx: dict[int, int] = {}
        Pk: list = []
        Fk: list = []
        for f in fs:
            for i in f:
                if i not in idx:
                    idx[i] = len(Pk)
                    Pk.append(P[i])
            Fk.append([idx[i] for i in f])
        aus.append((Pk, Fk))
    return aus


def volumen(P: list, F: list) -> float:
    """Vorzeichenbehaftetes Volumen (positiv = Flächen zeigen nach außen)."""
    return sum(P[a][0] * (P[b][1] * P[c][2] - P[b][2] * P[c][1])
               - P[a][1] * (P[b][0] * P[c][2] - P[b][2] * P[c][0])
               + P[a][2] * (P[b][0] * P[c][1] - P[b][1] * P[c][0]) for a, b, c in F) / 6


def morph_body(netze: list[tuple[list, list]]) -> tuple[dict, list]:
    """Archicad-Morph-Körper (Solid) aus Netzen; Körper mit negativem Volumen werden umgedreht.
    Rückgabe (body, box [xmin, ymin, zmin, xmax, ymax, zmax])."""
    V: list[dict] = []
    Pg: list[dict] = []
    for P, F in netze:
        for Pk, Fk in zerlegen(P, F):
            o = len(V)
            V += [{"x": p[0], "y": p[1], "z": p[2]} for p in Pk]
            if volumen(Pk, Fk) < 0:
                Fk = [[a, c, b] for a, b, c in Fk]
            Pg += [{"vertexIds": [a + o, b + o, c + o]} for a, b, c in Fk]
    xs, ys, zs = [v["x"] for v in V], [v["y"] for v in V], [v["z"] for v in V]
    box = [min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)] if V else []
    return {"bodyType": "Solid", "vertices": V, "polygons": Pg}, box


def zuordnung_laden(pfad=ZUORDNUNG) -> dict[str, str]:
    """IFC-Klasse → SAB-Klasse; „?“ am Schlüssel (Vorschlag) und „_“-Schlüssel (Hinweise) fallen weg."""
    roh = json.loads(Path(pfad).read_text(encoding="utf-8"))
    return {k.rstrip("?"): v for k, v in roh.items() if not k.startswith("_")}


def status_archicad(text: Optional[str]) -> Optional[str]:
    return STATUS.get(text or "")


def geschoss_index(stories: list[dict], z: float) -> int:
    """Höchstes Geschoss mit level ≤ Unterkante, sonst das unterste."""
    passend = [s for s in stories if s["level"] <= z + 1e-6] or [min(stories, key=lambda s: s["level"])]
    return max(passend, key=lambda s: s["level"])["index"]


def bauplan(bauliste: dict, netze: dict, register: dict, zuordnung: dict, klassen: dict, stories: list[dict],
            nur=None, ohne=()) -> tuple[list[dict], list[dict]]:
    """Rein, ohne Archicad: (plan, ausgelassen). klassen = {Klassen-id: Item-GUID}."""
    reg = {str(x["guid"]).upper(): x for x in register.get("neu") or []}
    netze = {k.upper(): v for k, v in netze.items()}
    nur_g = {g.upper() for g in nur} if nur else None
    ohne_g = {g.upper() for g in ohne}
    plan: list[dict] = []
    aus: list[dict] = []
    for b in bauliste.get("bauteile") or []:
        g = b["plattformGuid"]
        if (nur_g is not None and g.upper() not in nur_g) or g.upper() in ohne_g:
            continue
        ns = netze.get(g.upper()) or []
        klasse = zuordnung.get(b["ifc"]["klasse"])
        status_roh = (reg.get(g.upper()) or {}).get("status")
        status = status_archicad(status_roh)
        if not ns:
            aus.append({"plattformGuid": g, "name": b["name"], "grund": "keine Geometrie im IFC"})
            continue
        if klasse not in klassen or not status:
            aus.append({"plattformGuid": g, "name": b["name"],
                        "grund": f"Klasse/Status fehlt ({b['ifc']['klasse']}, {status_roh})"})
            continue
        body, box = morph_body(ns)
        plan.append({"g": g, "name": b["name"], "ebene": b["ebene"], "klasse": klasse, "status": status,
                     "soll": box, "morph": {"basePoint": {"x": 0, "y": 0, "z": 0}, "body": body,
                                            "floorIndex": geschoss_index(stories, box[2])}})
    return plan, aus


def sab_klassen(ac: Archicad, name: str = SAB_SYSTEM) -> tuple[str, dict[str, str]]:
    """(System-GUID, {Klassen-id: Item-GUID}) des Klassifizierungssystems `name`."""
    system = next((s for s in ac.api("API.GetAllClassificationSystems").get("classificationSystems") or []
                   if s.get("name") == name), None)
    if not system:
        raise Fehler(f"Klassifizierungssystem „{name}“ gibt es im Projekt nicht.")
    sid = system["classificationSystemId"]["guid"]
    baum = ac.api("API.GetAllClassificationsInSystem", {"classificationSystemId": {"guid": sid}})
    items: list[dict] = []

    def sammeln(knoten: list) -> None:
        for k in knoten or []:
            item = k.get("classificationItem") or k
            items.append(item)
            # live AC29/Tapir 1.7.2 (19.11.2026): Kinder hängen am Item, nicht am Hüllobjekt
            sammeln(item.get("children") or k.get("children"))

    sammeln(baum.get("classificationItems"))
    klassen: dict[str, str] = {}
    ohne_id: list[str] = []
    for item in items:
        g = (item.get("classificationItemId") or {}).get("guid")
        if g and item.get("id"):
            klassen[item["id"]] = g
        elif g:
            ohne_id.append(g)
    for teil in _pakete(ohne_id):
        r = ac.api("API.GetDetailsOfClassificationItems",
                   {"classificationItemIds": [{"classificationItemId": {"guid": g}} for g in teil]})
        # live: Antwortschlüssel ist „classificationItems“ (ältere Fassungen: „classificationItemDetails“)
        for g, d in zip(teil, r.get("classificationItems") or r.get("classificationItemDetails") or []):
            klasse_id = ((d or {}).get("classificationItem") or d or {}).get("id")
            if klasse_id:
                klassen[klasse_id] = g
    return sid, klassen


def _string(v: Any) -> dict:
    return {"type": "string", "status": "normal", "value": v}


def bauen(ac: Archicad, ordner, ifc, bauliste: dict, register: dict, zuordnung: dict, *, nur, ohne, vorhanden: dict,
          ausfuehren: bool, projekt_soll: Optional[str] = None) -> dict:
    """Plattform-Bauteile als Morphs (echte Geometrie) nach Archicad: Ebene, Geschoss, Klasse, Umbau-Status,
    Stempel, Rücklese. Ohne `ausfuehren` nur Trockenlauf — es wird nichts Schreibendes gesendet."""
    ordner = Path(ordner)
    projekt = ac.befehl("GetProjectInfo").get("projectName")
    stories = ac.befehl("GetStories").get("stories") or []
    system_id, klassen = sab_klassen(ac)
    plan, ausgelassen = bauplan(bauliste, ifc_netze(ifc), register, zuordnung, klassen, stories, nur, ohne)
    _schreiben(ordner / "ausgelassen.json", ausgelassen)
    zaehl = {"ebenen": dict(collections.Counter(p["ebene"] for p in plan)),
             "klassen": dict(collections.Counter(p["klasse"] for p in plan)),
             "status": dict(collections.Counter(p["status"] for p in plan)),
             "dreiecke": sum(len(p["morph"]["body"]["polygons"]) for p in plan)}
    if not ausfuehren:
        return {"trockenlauf": True, "projekt": projekt, "zu_bauen": len(plan), "ausgelassen": len(ausgelassen),
                **zaehl}
    if projekt_soll and projekt_soll != projekt:
        raise Fehler(f"In Archicad ist „{projekt}“ offen, gelesen wurde „{projekt_soll}“ — nichts gebaut.")
    ids = ac.api("API.GetPropertyIds", {"properties": [
        {"type": "UserDefined", "localizedName": [STEMPEL_GRUPPE, STEMPEL_NAME]},
        {"type": "UserDefined", "localizedName": [STEMPEL_GRUPPE, KI_NAME]},
        {"type": "BuiltIn", "nonLocalizedName": "Category_RenovationStatus"}]}).get("properties") or []
    if len(ids) < 3 or any("propertyId" not in x for x in ids):
        raise Fehler(f"Eigenschaften fehlen im Projekt (Plattform-GUID, KI generiert, Umbau-Status): {ids}")
    pg, ki, ps = (x["propertyId"] for x in ids)
    tag = "KI generiert " + datetime.date.today().isoformat()
    sys_id = {"guid": system_id}
    gebaut: list[dict] = []
    rueck: list[dict] = []
    fehler: list[dict] = []
    for ch in _pakete(plan, BAU_PAKET):
        neu_ = [p for p in ch if p["g"] not in vorhanden]
        antw = iter(ac.befehl("CreateMorphs", {"morphsData": [p["morph"] for p in neu_]}).get("elements") or []
                    if neu_ else [])
        r = [{"elementId": {"guid": vorhanden[p["g"]]}} if p["g"] in vorhanden else next(antw, {}) for p in ch]
        _schreiben(ordner / "angelegt.json", gebaut + [{"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"]}
                                                       for p, e in zip(ch, r) if "elementId" in e])
        ok = [(p, e) for p, e in zip(ch, r) if "elementId" in e]
        fehler += [{"plattformGuid": p["g"], "name": p["name"], "grund": str(e)[:200]}
                   for p, e in zip(ch, r) if "elementId" not in e]
        if not ok:
            continue
        for eb in sorted({p["ebene"] for p, _ in ok}):
            # reserve:false — sonst löst jedes Paket beim Nutzer in Teamwork einen Sendeversuch aus
            ac.elm("SetLayerOfElements", {"elements": [e for p, e in ok if p["ebene"] == eb],
                                          "layerName": eb, "reserve": False})
        ac.api("API.SetClassificationsOfElements", {"elementClassifications": [
            {"elementId": e["elementId"], "classificationId": {
                "classificationSystemId": sys_id, "classificationItemId": {"guid": klassen[p["klasse"]]}}}
            for p, e in ok]})
        ev: list[dict] = []
        for p, e in ok:
            ev += [{"elementId": e["elementId"], "propertyId": pg, "propertyValue": _string(p["g"])},
                   {"elementId": e["elementId"], "propertyId": ki, "propertyValue": _string(tag)},
                   {"elementId": e["elementId"], "propertyId": ps, "propertyValue": {
                       "type": "singleEnum", "status": "normal",
                       "value": {"type": "nonLocalizedValue", "nonLocalizedValue": p["status"]}}}]
        ac.api("API.SetPropertyValuesOfElements", {"elementPropertyValues": ev})
        els = [e for _, e in ok]
        boxen = ac.api("API.Get3DBoundingBoxes", {"elements": els}).get("boundingBoxes3D") or []
        pv = ac.api("API.GetPropertyValuesOfElements", {
            "elements": els, "properties": [{"propertyId": pg}, {"propertyId": ki}, {"propertyId": ps}]}
                    ).get("propertyValuesForElements") or []
        cl = ac.api("API.GetClassificationsOfElements", {
            "elements": els, "classificationSystemIds": [{"classificationSystemId": sys_id}]}
                    ).get("elementClassifications") or []
        det = ac.befehl("GetDetailsOfElements", {"elements": els}).get("detailsOfElements") or []
        id2klasse = {g: k for k, g in klassen.items()}
        for n, (p, e) in enumerate(ok):
            bx = ((boxen[n] if n < len(boxen) else {}) or {}).get("boundingBox3D")
            ist = [bx[k] for k in ("xMin", "yMin", "zMin", "xMax", "yMax", "zMax")] if bx else None
            abw = max(abs(a - b) for a, b in zip(ist, p["soll"])) if ist else None
            w = [(x or {}).get("propertyValue", {}).get("value")
                 for x in ((pv[n] if n < len(pv) else {}).get("propertyValues") or [])] + [None] * 3
            cids = (cl[n] if n < len(cl) else {}).get("classificationIds") or [{}]
            kg = ((cids[0] or {}).get("classificationId") or {}).get("classificationItemId", {}).get("guid")
            d = det[n] if n < len(det) else {}
            stimmt = (abw is not None and abw <= 0.002 and w[0] == p["g"] and w[1] == tag
                      and (w[2] or {}).get("nonLocalizedValue") == p["status"] and kg == klassen[p["klasse"]])
            gebaut.append({"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"]})
            rueck.append({"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"], "name": p["name"],
                          "stimmt": bool(stimmt), "abweichung_m": abw,
                          "geschlossen": ((d.get("details") or {}).get("body") or {}).get("isClosed"),
                          "geschoss": d.get("floorIndex"), "ebene": p["ebene"], "klasse": id2klasse.get(kg),
                          "status": (w[2] or {}).get("nonLocalizedValue")})
        _schreiben(ordner / "gebaut.json", gebaut)
        _schreiben(ordner / "ruecklese.json", {"rueck": rueck, "fehler": fehler})
    return {"trockenlauf": False, "projekt": projekt, "gebaut": len(gebaut),
            "stimmt": sum(1 for x in rueck if x["stimmt"]), "fehler": fehler[:10],
            "geschlossen": dict(collections.Counter(x["geschlossen"] for x in rueck)),
            "naechster_schritt": "Paare aus gebaut.json in ergebnis.json basis.paare ergänzen, hochladen, "
                                 "archicad_abgleich_melden {projekt, lauf, gebaut}"}


# ─────────────────────────────────────────────────────────── Befehle

def cmd_archicad_lesen(o) -> dict:
    ac = Archicad(_http(o.port))
    typen = tuple(t.strip() for t in o.typen.split(",")) if o.typen else TYPEN
    stand = archicad_lesen(ac, typen, o.alle_eigenschaften)
    _schreiben(Path(o.ordner) / "archicad.json", stand)
    return {"projekt": stand["projekt"], "teamwork": stand["teamwork"], "bauteile": len(stand["elemente"]),
            "eigenschaften": len(stand["eigenschaften_katalog"]),
            "hinweis": "Projektname dem Zeichner zeigen und bestätigen lassen, bevor es weitergeht."}


def cmd_planen(o) -> dict:
    ordner = Path(o.ordner)
    archicad = _lesen(ordner / "archicad.json")
    stand = herunterladen(o.stand_link) if o.stand_link else _lesen(ordner / "stand.json")
    if stand.get("format") != ab.FORMAT:
        raise Fehler(f"stand.json: Format „{stand.get('format')}“ — erwartet {ab.FORMAT}")
    _schreiben(ordner / "stand.json", stand)
    plan = ab.planen(archicad["elemente"], stand["elemente"], stand.get("basis"))
    plan["lauf"] = stand.get("lauf")
    _schreiben(ordner / "plan.json", plan)
    (ordner / "fragen.md").write_text(fragen_text(plan), encoding="utf-8")
    return {"lauf": plan["lauf"], "archicad_projekt": archicad.get("projekt"),
            "plattform_projekt": (stand.get("projekt") or {}).get("nummer"), **plan["zahlen"],
            "fragen": [{k: f[k] for k in ("id", "text", "anzahl", "beispiele", "optionen")}
                       for f in plan["fragen"][:40]],
            "hinweise": plan["hinweise"],
            "naechster_schritt": "Jede Frage dem Zeichner stellen (fragen.md), Antworten nach antworten.json "
                                 "{\"F1\": \"archicad\", …}, dann „entscheiden“."}


def cmd_entscheiden(o) -> dict:
    ordner = Path(o.ordner)
    plan = _lesen(ordner / "plan.json")
    datei = Path(o.antworten) if o.antworten else ordner / "antworten.json"
    antworten = _lesen(datei) if datei.is_file() else {}
    try:
        erg = ab.entscheiden(plan, antworten)
    except ab.AbgleichFehler as e:
        raise Fehler(str(e)) from e
    erg["lauf"] = plan.get("lauf")
    _schreiben(ordner / "ergebnis.json", erg)
    return {"lauf": erg["lauf"], **erg["zahlen"], "offen_fragen": erg["offen"]}


def cmd_archicad_schreiben(o) -> dict:
    ordner = Path(o.ordner)
    erg, archicad = _lesen(ordner / "ergebnis.json"), _lesen(ordner / "archicad.json")
    ac = Archicad(_http(o.port))
    if o.ausfuehren and ac.befehl("GetProjectInfo").get("projectName") != archicad.get("projekt"):
        raise Fehler("In Archicad ist ein anderes Projekt offen als beim Lesen — nichts geschrieben.")
    aus = eigenschaften_schreiben(ac, erg["archicad"]["eigenschaften"], archicad["eigenschaften_katalog"],
                                  teamwork=archicad.get("teamwork", False), ausfuehren=o.ausfuehren)
    _schreiben(ordner / "archicad_geschrieben.json", aus)
    return {"ausgefuehrt": aus["ausgefuehrt"], "moeglich": aus["moeglich"], "geschrieben": len(aus["geschrieben"]),
            "nicht_moeglich": len(aus["nicht_moeglich"]), "fehler": len(aus["fehler"]),
            "beispiele_nicht_moeglich": aus["nicht_moeglich"][:5],
            "loeschen_von_hand": erg["archicad"]["loeschen_von_hand"][:50],
            "hinweis": "Ohne --ausfuehren nur Trockenlauf. Nach dem Schreiben in Teamwork senden. Gelöscht wird nie "
                       "automatisch — die Liste „loeschen_von_hand“ geht an den Zeichner."}


def klassifiziert(ac: Archicad, guids: list[str]) -> set[str]:
    """GUIDs, die in irgendeinem Klassifizierungssystem eine Klasse tragen. Eigenschaften an unklassifizierten
    Elementen lehnt Archicad mit 6703 ab."""
    systeme = [{"classificationSystemId": s["classificationSystemId"]}
               for s in ac.api("API.GetAllClassificationSystems").get("classificationSystems") or []]
    aus: set[str] = set()
    if not systeme:
        return aus
    for teil in _pakete(list(guids)):
        r = ac.api("API.GetClassificationsOfElements", {
            "elements": [{"elementId": {"guid": g}} for g in teil], "classificationSystemIds": systeme})
        for g, c in zip(teil, r.get("elementClassifications") or []):
            if any(((x or {}).get("classificationId") or {}).get("classificationItemId", {}).get("guid")
                   for x in (c or {}).get("classificationIds") or []):
                aus.add(g)
    return aus


def cmd_stempeln(o) -> dict:
    """Frisch gebaute Bauteile mit „Plattform-GUID“ + „KI generiert“ versehen: gebaut.json =
    [{plattformGuid, archicadGuid}] — dieselbe Liste geht an archicad_abgleich_melden."""
    gebaut = _lesen(Path(o.gebaut))
    ac = Archicad(_http(o.port))
    katalog = {n: {"propertyId": p["propertyId"], "editierbar": p.get("propertyIsEditable", True)}
               for n, p in eigenschaften_katalog(ac, alle=False).items()}
    stempel = next((n for n in katalog if n.endswith("." + STEMPEL_NAME)), None)
    if not stempel:
        raise Fehler(f"Die Eigenschaft „{STEMPEL_NAME}“ gibt es im Projekt nicht — erst mit dem Zeichner anlegen "
                     f"(Gruppe „{STEMPEL_GRUPPE}“, Text).")
    ki = next((n for n in katalog if n.endswith("." + KI_NAME)), None)
    eintraege = [{"archicad": x["archicadGuid"], "feld": stempel, "wert": x["plattformGuid"]} for x in gebaut]
    if ki:
        eintraege += [{"archicad": x["archicadGuid"], "feld": ki,
                       "wert": "KI generiert " + datetime.date.today().isoformat()} for x in gebaut]
    klass = klassifiziert(ac, sorted({x["archicadGuid"] for x in gebaut}))
    ohne_klasse = [{"archicad": x["archicadGuid"], "plattformGuid": x["plattformGuid"],
                    "grund": "nicht klassifiziert (6703)"} for x in gebaut if x["archicadGuid"] not in klass]
    # Reservieren löst in Teamwork beim Nutzer einen Sendeversuch aus; frisch gebaute gehören uns ohnehin.
    aus = eigenschaften_schreiben(ac, [x for x in eintraege if x["archicad"] in klass], katalog,
                                  teamwork=bool(ac.befehl("GetProjectInfo").get("isTeamwork")),
                                  ausfuehren=o.ausfuehren, reservieren=False)
    nicht = ohne_klasse + aus["nicht_moeglich"]
    return {"ausgefuehrt": aus["ausgefuehrt"], "bauteile": len(gebaut), "geschrieben": len(aus["geschrieben"]),
            "fehler": aus["fehler"][:10], "nicht_moeglich": nicht[:10], "nicht_klassifiziert": len(ohne_klasse)}


def cmd_bauen(o) -> dict:
    ordner = Path(o.ordner)
    liste = lambda t: [x.strip() for x in t.split(",") if x.strip()]          # noqa: E731
    archicad = ordner / "archicad.json"
    soll = _lesen(archicad).get("projekt") if archicad.is_file() else None
    aus = bauen(Archicad(_http(o.port)), ordner, o.ifc, _lesen(Path(o.bauliste)), _lesen(Path(o.register)),
                zuordnung_laden(o.zuordnung or ZUORDNUNG), nur=liste(o.nur) or None, ohne=liste(o.ohne),
                vorhanden=_lesen(Path(o.vorhanden)) if o.vorhanden else {}, ausfuehren=o.ausfuehren,
                projekt_soll=soll)
    if aus.get("trockenlauf"):
        aus["hinweis"] = "Trockenlauf — nichts geschrieben. Nach dem Ja des Zeichners mit --ausfuehren wiederholen."
    return aus


def cmd_hochladen(o) -> dict:
    daten = (Path(o.ordner) / "ergebnis.json").read_bytes()
    ablegen(o.ablage, daten, "application/json")
    return {"hochgeladen": "ergebnis.json", "bytes": len(daten),
            "naechster_schritt": "archicad_abgleich_melden {projekt, lauf} — erst Vorschau, nach dem Ja bestaetigt=true"}


def cmd_oberflaechen(o) -> dict:
    ordner = Path(o.ordner).resolve()
    ziel = ordner / "oberflaechen"
    ziel.mkdir(parents=True, exist_ok=True)
    r = Archicad(_http(o.port)).elm("ExportSurfaces", {"targetFolder": str(ziel)})
    quelle = Path(r["folder"])
    paket = ordner / "oberflaechen.zip"
    with zipfile.ZipFile(paket, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(quelle.rglob("*")):
            if f.is_file():
                z.write(f, f.relative_to(quelle.parent))
    return {"oberflaechen": r.get("surfaces"), "mit_bild": r.get("withImage"), "bild_fehlt": r.get("imageMissing"),
            "paket": str(paket), "mb": round(paket.stat().st_size / 1e6, 1),
            "naechster_schritt": "Konnektor archicad_oberflaechen_hochladen {projekt} liefert den Ablage-Link (curl); "
                                 "die Antwort des Uploads ist der Prüfplan. Nach dem Ja "
                                 "archicad_oberflaechen_einpflegen {ablage_auftrag, bestaetigt: true}."}


def cmd_ifc(o) -> dict:
    ziel = (Path(o.ordner).resolve() / "archicad.ifc")
    params: dict = {"method": "save", "ifcFilePath": str(ziel)}
    if o.translator:
        params["translatorName"] = o.translator
    Archicad(_http(o.port)).befehl("IFCFileOperation", params)
    return {"ifc": str(ziel), "mb": round(ziel.stat().st_size / 1e6, 1) if ziel.is_file() else None,
            "naechster_schritt": "modell_hochladen {projekt, dateiname} liefert den Ablage-Link; nach dem Hochladen "
                                 "entsteht der Import-Vorschlag (neue Bauteile, Konflikte entscheidet die Freigabe)."}


def stempel_lesen(ac: Archicad, guids: list[str]) -> dict[str, dict]:
    """„…Plattform-GUID“ je Element aus den Archicad-Eigenschaften (der IFC-Übersetzer gibt ihn nicht immer aus)."""
    katalog = {n: p for n, p in eigenschaften_katalog(ac, alle=False).items() if n.endswith("." + ab.PLATTFORM_GUID)}
    aus: dict[str, dict] = {}
    for name, p in katalog.items():
        for teil in _pakete(guids):
            werte = ac.befehl("GetPropertyValuesOfElements", {
                "elements": [{"elementId": {"guid": g}} for g in teil],
                "properties": [{"propertyId": p["propertyId"]}]}).get("propertyValuesForElements") or []
            for g, zeile in zip(teil, werte):
                v = (((zeile or {}).get("propertyValues") or [{}])[0] or {}).get("propertyValue") or {}
                if str(v.get("value") or "").strip():
                    aus.setdefault(g, {})[name] = v.get("value")
    return aus


def cmd_diff(o) -> dict:
    """Nur die Unterschiede Archicad ↔ Plattform, ohne IFC-Datei — dünne Hülle um elmonkey.diff_plattform (dieselbe
    Logik wie das MCP-Werkzeug elmonkey_diff_plattform). Liest Archicad über ELM_SAB.GetDiffDataELM, schreibt
    archicad_diff.json + ergebnis.json. Ändert nichts, weder in Archicad noch in der Plattform."""
    ordner = Path(o.ordner)
    stand = herunterladen(o.stand_link) if o.stand_link else _lesen(ordner / "stand.json")
    if stand.get("format") != ab.FORMAT:
        raise Fehler(f"stand.json: Format „{stand.get('format')}“ — erwartet {ab.FORMAT}")
    _schreiben(ordner / "stand.json", stand)
    ac = Archicad(_http(o.port))
    seiten, offset = [], 0
    while True:
        try:
            s = ac.elm("GetDiffDataELM", {"offset": offset, "limit": PAKET})
        except Fehler as e:
            if not seiten:
                raise Fehler(f"ELM_SAB.GetDiffDataELM fehlt oder scheitert ({str(e)[:120]}) — ELMonkey ab 0.11.13 "
                             "installieren. Ohne ihn gibt es keinen Diff ohne IFC-Datei.") from e
            raise
        seiten.append(s)
        if s.get("nextOffset") is None:
            break
        if int(s["nextOffset"]) <= offset:
            raise Fehler(f"GetDiffDataELM: nextOffset {s['nextOffset']} geht nicht weiter")
        offset = int(s["nextOffset"])
    _schreiben(ordner / "archicad_diff.json", {"seiten": seiten})
    namen = {g.get("archicadName"): g.get("name") for g in stand.get("geschosse") or []
             if g.get("archicadName") and g.get("name")}
    archicad = dp.archicad_bauteile(seiten, geschoss_namen=namen)
    dp.stempel_eintragen(archicad, stempel_lesen(ac, [e["guid"] for e in archicad["elemente"]]))
    erg = dp.vergleichen(archicad, stand, toleranz=o.toleranz)
    _schreiben(ordner / "ergebnis.json", erg)
    d = erg["diff"]
    return {"lauf": erg.get("lauf"), **d["zahlen"], "hinweise": d["hinweise"],
            "beispiele": {"neu": d["neu"][:5], "weg": d["weg"][:5], "eigenschaften": d["eigenschaften"][:5],
                          "geometrie": d["geometrie"][:5]},
            "gebaut_vorschlag": d["gebaut_vorschlag"][:50],
            "naechster_schritt": "Zahlen dem Zeichner zeigen. ergebnis.json mit „hochladen“ ablegen, dann "
                                 "archicad_abgleich_melden {projekt, lauf, gebaut} — erst Vorschau, nach dem Ja "
                                 "bestaetigt=true (Geometrie geht als Vorschlag mit)."}


def main(argv=None) -> int:
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = a.add_subparsers(dest="befehl", required=True)

    def neu(name, fn, **extra):
        p = sub.add_parser(name)
        p.add_argument("--ordner", required=True, help="Arbeitsordner dieses Laufs")
        p.add_argument("--port", type=int, default=PORT)
        for k, v in extra.items():
            p.add_argument("--" + k.replace("_", "-"), **v)
        p.set_defaults(fn=fn)

    neu("archicad-lesen", cmd_archicad_lesen, typen={"default": ""},
        alle_eigenschaften={"action": "store_true", "help": "auch eingebaute Eigenschaften (groß, langsam)"})
    neu("planen", cmd_planen, stand_link={"default": ""})
    neu("entscheiden", cmd_entscheiden, antworten={"default": ""})
    neu("archicad-schreiben", cmd_archicad_schreiben, ausfuehren={"action": "store_true"})
    neu("stempeln", cmd_stempeln, gebaut={"required": True}, ausfuehren={"action": "store_true"})
    neu("bauen", cmd_bauen, ifc={"required": True}, bauliste={"required": True}, register={"required": True},
        zuordnung={"default": ""}, nur={"default": ""}, ohne={"default": ""}, vorhanden={"default": ""},
        ausfuehren={"action": "store_true"})
    neu("hochladen", cmd_hochladen, ablage={"required": True})
    neu("oberflaechen", cmd_oberflaechen)
    neu("ifc", cmd_ifc, translator={"default": ""})
    neu("diff", cmd_diff, stand_link={"default": ""},
        toleranz={"type": float, "default": dp.TOLERANZ, "help": "Meter, Vorgabe 0.003"})
    o = a.parse_args(argv)
    try:
        print(json.dumps(o.fn(o), ensure_ascii=False, indent=1))
        return 0
    except Fehler as e:
        print(json.dumps({"fehler": str(e)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
