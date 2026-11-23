#!/usr/bin/env python3
"""Abgleich offenes Archicad-Projekt ↔ Schwarz Plattform (bidirektional). Nur Standardbibliothek.

Ablauf (Rezept: skills/archicad/recipes/plattform-abgleich.md). Die Links kommen aus dem Plattform-Konnektor
(`archicad_abgleich_lesen`); große Dateien laufen über diese Links, nicht durch den Chat.

    plattform_abgleich.py archicad-lesen  --ordner <dir> [--port 19723] [--typen Wall,Slab,…] [--alle-eigenschaften]
    plattform_abgleich.py planen          --ordner <dir> --stand-link "<stand_link>"
    plattform_abgleich.py entscheiden     --ordner <dir> [--antworten antworten.json]
    plattform_abgleich.py archicad-schreiben --ordner <dir> [--ausfuehren]      # Eigenschaften nach Archicad
    plattform_abgleich.py stempeln        --ordner <dir> --gebaut gebaut.json [--ausfuehren]
    plattform_abgleich.py bauen           --ordner <dir> --bauliste <datei|link> [weitere …] --register register.json
                                          [--ifc plattform.ifc] [--trotzdem] [--zuordnung z.json] [--nur GUID,..] [--ohne GUID,..] [--vorhanden v.json] [--verbund einzeln|zusammen] [--ausfuehren]
    plattform_abgleich.py hochladen       --ordner <dir> --ablage "<ergebnis_ablage>" [--netze-ablage "<netze_ablage>"]
    plattform_abgleich.py netze           --ordner <dir> [--paket 50]   # neue + in Archicad geänderte Bauteile mit
                                          # Körpern → archicad_netze.json.gz (sab-archicad-netze/1), statt IFC
    plattform_abgleich.py formen          --ordner <dir>                     # Teile je ≤ 1 Mio. Dreiecke, gleiche Körper
                                          # je Teil einmal: Form + Platzierung (sab-archicad-netze/2) → netze_teile/;
                                          # danach je Teil: hochladen --netze-teil <nr>, dann melden
    plattform_abgleich.py oberflaechen    --ordner <dir>                     # Export + ZIP für die Plattform
    plattform_abgleich.py ifc             --ordner <dir> [--translator "<Name>"]
    plattform_abgleich.py diff            --ordner <dir> --stand-link "<stand_link>" [--toleranz 0.003]
                                          # nur die Unterschiede, ohne IFC-Datei (ELMonkey ≥ 0.11.13)

Alles, was Archicad verändert, ist ohne `--ausfuehren` ein Trockenlauf. Gelöscht wird in Archicad nie.
Dateien im Arbeitsordner: archicad.json, stand.json, plan.json, fragen.md, antworten.json, ergebnis.json,
archicad_geschrieben.json, archicad_diff.json (Befehl diff), archicad_netze.json.gz (Befehl netze); Befehl bauen: ausgelassen.json + netz_pruefen.json (immer),
angelegt.json / gebaut.json / ruecklese.json (je Paket fortgeschrieben, nur mit --ausfuehren).
"""
from __future__ import annotations

import argparse
import collections
import datetime
import gzip
import json
import re
import ssl
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid
import zipfile
from pathlib import Path
from typing import Any, Callable, Optional

# Der Kern `elmonkey` kommt aus drei Quellen, in dieser Reihenfolge: installiertes elmonkey-core,
# core/ im ELMonkey-Repo, scripts/_kern im gespiegelten Skill (schreibt der Workflow skill-spiegeln).
def _kern_laden():
    try:
        from elmonkey import abgleich as _ab
        from elmonkey import diff_plattform as _dp
        from elmonkey import netze as _nz
        return _ab, _dp, _nz
    except ImportError:
        pass
    eltern = Path(__file__).resolve().parents
    kandidaten = []
    if len(eltern) > 4:
        kandidaten.append(eltern[4] / "core")
    if len(eltern) > 1:
        kandidaten.append(eltern[1] / "_kern")
    kandidaten = [k for k in kandidaten if (k / "elmonkey").is_dir()]
    for k in kandidaten:
        sys.path.insert(0, str(k))
        try:
            from elmonkey import abgleich as _ab
            from elmonkey import diff_plattform as _dp
            from elmonkey import netze as _nz
            return _ab, _dp, _nz
        except ImportError:
            sys.path.remove(str(k))
            for name in [n for n in sys.modules if n == "elmonkey" or n.startswith("elmonkey.")]:
                del sys.modules[name]
    sys.exit("plattform_abgleich: Modul 'elmonkey' nicht gefunden. elmonkey-core installieren "
             "(aus mozzi86/ELMonkey core/) oder den Skill neu spiegeln bzw. neu installieren - im Spiegel muss "
             "scripts/_kern/elmonkey liegen (schreibt der Workflow skill-spiegeln). Geprüft: "
             + (", ".join(str(k) for k in kandidaten) or "keine Kandidaten"))


ab, dp, nz = _kern_laden()

PORT = 19723
#: Elementtypen, die als Bauteil zählen (2D, Bemaßung, Zonen-Stempel usw. bleiben draußen).
TYPEN = ("Wall", "Column", "Beam", "Slab", "Roof", "Shell", "Morph", "Object", "Door", "Window", "Skylight",
         "Stair", "Railing", "CurtainWall", "Mesh", "Zone", "Opening", "Lamp")
PAKET = 400                                             # Elemente je Aufruf
STEMPEL_GRUPPE, STEMPEL_NAME, KI_NAME = "Allgemeine Werte", ab.PLATTFORM_GUID, "KI generiert"
VERBUND_NAME = "Plattform-Verbund"


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
        # Tapir 1.7.2 meldet benutzerdefinierte Eigenschaften als „Custom“ (live Projekt-E 19.11.2026: 230 Custom,
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


def _ist_zertifikatsfehler(e: BaseException) -> bool:
    return "CERTIFICATE_VERIFY_FAILED" in str(e)


def herunterladen(link: str) -> Any:
    link = _nur_https(link)
    try:
        with urllib.request.urlopen(link, timeout=900) as r:
            return json.loads(r.read().decode("utf-8"))
    except (urllib.error.URLError, ssl.SSLError) as e:
        if not _ist_zertifikatsfehler(e):
            raise
    # python.org-Python auf macOS kennt die Systemzertifikate nicht → curl (Argumentliste, nie über die Shell)
    r = subprocess.run(["curl", "-sS", "--fail", link], capture_output=True, timeout=900)
    if r.returncode != 0:
        raise Fehler(f"curl scheitert ({r.returncode}): {r.stderr.decode('utf-8', 'replace')[:300]}")
    return json.loads(r.stdout.decode("utf-8"))


def bauliste_laden(quellen) -> dict:
    """Eine oder mehrere Baulisten (Datei oder https-Link) zu einer zusammenführen. Format „sab-bauliste/…“ oder das
    alte (ohne format-Feld). Gleiche plattformGuid (Groß/Klein egal): die spätere Quelle gewinnt, Reihenfolge =
    erstes Auftreten. GUIDs, die irgendwo als Bauteil vorliegen, gelten nicht mehr als nicht bearbeitet."""
    if isinstance(quellen, (str, Path)):
        quellen = [quellen]
    if not quellen:
        raise Fehler("Keine Bauliste angegeben.")
    bauteile: dict[str, dict] = {}
    nicht: dict[str, str] = {}
    nicht_wirksam: dict[str, Any] = {}
    letzte: dict = {}
    for q in quellen:
        q = str(q)
        d = herunterladen(q) if q.startswith("http") else _lesen(Path(q))
        if not isinstance(d, dict):
            raise Fehler(f"Bauliste {q[:60]}: kein JSON-Objekt.")
        fmt = d.get("format")
        if fmt is not None and not str(fmt).startswith("sab-bauliste/"):
            raise Fehler(f"Bauliste {q[:60]}: Format „{fmt}“ unbekannt (erwartet sab-bauliste/…).")
        letzte = d
        for b in d.get("bauteile") or []:
            bauteile[str(b["plattformGuid"]).upper()] = b
        gr = d.get("nicht_bearbeitet_grund") or {}
        for g in d.get("nicht_bearbeitet") or []:
            nicht.setdefault(str(g).upper(), gr.get(g) or gr.get(str(g).upper()) or "")
        for g in d.get("nicht_im_wirksamen_stand") or []:
            nicht_wirksam.setdefault(str(g).upper(), (d.get("nicht_im_wirksamen_stand_grund") or {}).get(g, ""))
    for g in list(nicht):
        if g in bauteile:
            del nicht[g]
    return {"format": letzte.get("format"), "projekt": letzte.get("projekt"), "rahmen": letzte.get("rahmen"),
            "erzeugt": letzte.get("erzeugt"), "bauteile": list(bauteile.values()),
            "nicht_bearbeitet": list(nicht), "nicht_bearbeitet_grund": nicht,
            "nicht_im_wirksamen_stand": list(nicht_wirksam), "nicht_im_wirksamen_stand_grund": nicht_wirksam,
            "quellen": len(quellen)}


def bauliste_netze(bauliste: dict) -> dict[str, list[tuple[list, list]]]:
    """{GUID groß: [(punkte, dreiecke), …]} aus parameter.koerper — nur Bauteile mit echten Netzen."""
    aus: dict[str, list[tuple[list, list]]] = {}
    for b in bauliste.get("bauteile") or []:
        ks = (b.get("parameter") or {}).get("koerper") or []
        if ks:
            aus[str(b["plattformGuid"]).upper()] = [(k["punkte"], k["dreiecke"]) for k in ks]
    return aus


def nicht_bearbeitet_pruefen(bauliste: dict, trotzdem: bool = False) -> None:
    """Abbruch (vor jedem Archicad-Aufruf), wenn die Plattform GUIDs der Anfrage nicht bearbeitet hat."""
    nicht = bauliste.get("nicht_bearbeitet") or []
    if not nicht or trotzdem:
        return
    gr = bauliste.get("nicht_bearbeitet_grund") or {}
    liste = ", ".join(f"{g} ({gr.get(g) or gr.get(str(g).upper()) or 'ohne Grund'})" for g in nicht)
    raise Fehler(f"{len(nicht)} Bauteile hat die Plattform nicht bearbeitet – Bauliste für diese GUIDs neu abrufen "
                 f"(archicad_abgleich_bauliste) oder --trotzdem: {liste}")


def ablegen(link: str, daten: bytes, typ: str) -> None:
    link = _nur_https(link)
    req = urllib.request.Request(link, data=daten, method="PUT",
                                 headers={"x-ms-blob-type": "BlockBlob", "Content-Type": typ})
    try:
        with urllib.request.urlopen(req, timeout=3600) as r:
            if r.status not in (200, 201):
                raise Fehler(f"Ablage antwortet mit {r.status}")
        return
    except (urllib.error.URLError, ssl.SSLError) as e:
        if not _ist_zertifikatsfehler(e):
            raise
    # Zertifikatsfehler (python.org-Python auf macOS) → curl, Argumentliste, nie über die Shell
    kurz = link.split("?", 1)[0]
    tmp = tempfile.NamedTemporaryFile(delete=False)
    try:
        with tmp:
            tmp.write(daten)
        r = subprocess.run(["curl", "-sS", "--fail", "-X", "PUT", "-H", "x-ms-blob-type: BlockBlob",
                            "-H", f"Content-Type: {typ}", "--data-binary", f"@{tmp.name}",
                            "-o", "/dev/null", "-w", "%{http_code}", link],
                           capture_output=True, timeout=3600)
    finally:
        Path(tmp.name).unlink(missing_ok=True)
    code = r.stdout.decode("utf-8", "replace").strip()
    if r.returncode != 0 or code not in ("200", "201"):
        err = r.stderr.decode("utf-8", "replace").replace(link, kurz)[:300]
        raise Fehler(f"curl-Ablage scheitert (Rückgabe {r.returncode}, HTTP {code or '?'}) bei {kurz}: {err}")


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
# Standard ist „Graphisoft Klassifizierung“: Archicads IFC-Typzuordnungen finden ihr Quellsystem über den
# NAMEN. Unter einem Eigennamen wie „SAB_Klassifizierung_29“ fällt beim IFC-Export alles auf
# IfcBuildingElementProxy. Das alte SAB-System bleibt nur Rückfall für Altprojekte (mit Hinweis).
SAB_SYSTEME = ("Graphisoft Klassifizierung", "SAB_Klassifizierung_29")
SAB_SYSTEM = SAB_SYSTEME[0]
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


def _attribute(s: str) -> list[str]:
    """STEP-Attribute der obersten Ebene trennen (Klammern und Zeichenketten beachten)."""
    aus, cur, tiefe, q = [], [], 0, False
    for ch in s:
        if q:
            cur.append(ch)
            if ch == "'":
                q = False
            continue
        if ch == "'":
            q = True
        elif ch == "(":
            tiefe += 1
        elif ch == ")":
            tiefe -= 1
        elif ch == "," and tiefe == 0:
            aus.append("".join(cur)); cur = []
            continue
        cur.append(ch)
    aus.append("".join(cur))
    return aus


_X2 = re.compile(r"\\X2\\((?:[0-9A-Fa-f]{4})+)\\X0\\")
_IFC_NAMEN = ("IfcPipeSegment", "IfcCovering", "IfcPlate", "IfcDuctSegment", "IfcBuildingElementProxy", "IfcSlab",
              "IfcWall", "IfcPipeFitting", "IfcDuctFitting", "IfcFlowSegment")


def step_text(s: str) -> str:
    """STEP-Zeichenkette lesen: Anführungszeichen weg, \\X2\\…\\X0\\ → Unicode, '' → '; „$“ → leer."""
    s = s.strip()
    if s in ("$", "*", ""):
        return ""
    if len(s) >= 2 and s[0] == s[-1] == "'":
        s = s[1:-1]
    s = _X2.sub(lambda m: "".join(chr(int(m.group(1)[i:i + 4], 16)) for i in range(0, len(m.group(1)), 4)), s)
    return s.replace("''", "'")


def ifc_klasse(step_typ: str) -> str:
    """STEP-Typ (IFCPIPESEGMENT) → IFC-Schreibweise (IfcPipeSegment) über sab_zuordnung.json + feste Liste."""
    try:
        bekannt = list(zuordnung_laden())
    except Exception:  # noqa: BLE001 — Zuordnung ist hier nur Schreibhilfe
        bekannt = []
    for k in bekannt + list(_IFC_NAMEN):
        if k.upper() == step_typ.upper():
            return k
    return "Ifc" + step_typ[3:].capitalize() if step_typ.upper().startswith("IFC") else step_typ


def ifc_teile(pfad) -> tuple[dict, dict, dict]:
    """(eigene, kinder, info): eigene = {GUID (lang): [(Punkte, Dreiecke 0-basiert)]} nur eigene Netze der
    IFCTRIANGULATEDFACESET (nur GUIDs mit Netz); kinder = {Kopf-GUID: [Unterteil-GUIDs]} aus IfcRelAggregates;
    info = {GUID: {"name", "klasse"}}."""
    zeilen: dict[int, tuple[str, str]] = {}
    with open(pfad, encoding="latin-1") as f:
        for z in f:
            m = _STEP.match(z)
            if m:
                zeilen[int(m.group(1))] = (m.group(2), m.group(3))

    def refs(a: str) -> list[int]:
        return [int(x) for x in _REF.findall(a)]

    nr2g: dict[int, str] = {}
    kinder_nr: dict[int, list[int]] = collections.defaultdict(list)
    for i, (t, a) in zeilen.items():
        m = re.match(r"'([^']{22})',", a)
        if m and all(c in _IFC_ZEICHEN for c in m.group(1)) and not t.startswith("IFCREL") \
                and t not in ("IFCPROPERTYSET", "IFCELEMENTQUANTITY"):
            nr2g[i] = guid_lang(m.group(1))
        if t == "IFCRELAGGREGATES":
            # IfcRelAggregates(GlobalId, OwnerHistory, Name, Description, RelatingObject, RelatedObjects):
            # nach Attributen trennen, nicht nach Verweis-Reihenfolge — OwnerHistory ist im Plattform-IFC „$“
            # (live Projekt-E 19.11.2026: sonst hingen die Kinder am ersten Geschwister, 35 Verbünde ohne Körper).
            attr = _attribute(a)
            eltern = refs(attr[4]) if len(attr) > 5 else []
            if eltern:
                kinder_nr[eltern[0]].extend(refs(attr[5]))

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
        return aus

    eigene: dict[str, list] = {}
    info: dict[str, dict] = {}
    for i, g in nr2g.items():
        n = netze(i)
        if n:
            eigene[g] = n
        t, a = zeilen[i]
        at = _attribute(a)
        info[g] = {"name": step_text(at[2]) if len(at) > 2 else "", "klasse": ifc_klasse(t)}
    kinder: dict[str, list[str]] = {}
    for i, ks in kinder_nr.items():
        if i in nr2g:
            kinder[nr2g[i]] = [nr2g[k] for k in ks if k in nr2g]
    return eigene, kinder, info


def _zusammen(eigene: dict, kinder: dict) -> dict[str, list]:
    """Eigene Netze + die aller Nachfahren (Zyklen-sicher). Nur GUIDs mit mindestens einem Netz."""
    def sammeln(g: str, besucht: set) -> list:
        if g in besucht:
            return []
        besucht.add(g)
        aus = list(eigene.get(g, []))
        for k in kinder.get(g, []):
            aus += sammeln(k, besucht)
        return aus

    alle = set(eigene) | set(kinder)
    erg = {g: sammeln(g, set()) for g in alle}
    return {g: n for g, n in erg.items() if n}


def ifc_netze(pfad) -> dict[str, list[tuple[list, list]]]:
    """{GUID (lang): [(Punkte, Dreiecke 0-basiert)]} — Unterteile (IfcRelAggregates) werden dem Elternteil
    zugeschlagen. Nur GUIDs mit mindestens einem Netz."""
    eigene, kinder, _ = ifc_teile(pfad)
    return _zusammen(eigene, kinder)


def _komponenten(P: list, F: list) -> list[tuple[list, list]]:
    """Netz in zusammenhängende Teile trennen: Dreiecke hängen nur über Kanten mit genau 2 Nachbarn zusammen.
    Jeder Teil bekommt eigene, neu nummerierte Punkte. Entartete Dreiecke (zwei gleiche Punkte) fallen weg."""
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


def mannigfaltig(P: list, F: list) -> tuple[list, list]:
    """Mehrfachkanten (Kante mit mehr als 2 Dreiecken) auflösen, indem der Dreiecksfächer je Eckpunkt getrennt wird.

    2026-11-19 live Projekt-E: 6 Plattform-Teile (Zuluftstutzen Ø160, Steigrohr Ø315, Kondensat-Fallrohr …) scheiterten
    mit "Failed to create morph" (Solid und Surface), weil nach dem Trennen noch eine Kante mit 4 Dreiecken übrig
    blieb (Rohrprofil läuft zweimal durch einen Punkt). Fächer je Eckpunkt trennen löst es: live alle 6 gebaut,
    Rücklese exakt.

    Je Eckpunkt werden die anliegenden Dreiecke über Kanten mit genau 2 Dreiecken verbunden; jede weitere
    Fächergruppe bekommt eine Punktkopie. Alle Nachbarschaftsabfragen laufen auf den unveränderten Originalflächen
    (F0), Ersetzungen gehen in eine getrennte Ausgabeliste — so können frühere Ersetzungen spätere Kantenabfragen
    nicht verfehlen lassen. Eingabelisten werden nicht verändert."""
    F0 = [list(f) for f in F]
    kanten: dict[tuple, list[int]] = {}
    an_punkt: dict[int, list[int]] = {}
    for n, f in enumerate(F0):
        for a in range(3):
            kanten.setdefault(tuple(sorted((f[a], f[(a + 1) % 3]))), []).append(n)
            an_punkt.setdefault(f[a], []).append(n)
    P2 = [list(p) for p in P]
    F2 = [list(f) for f in F0]
    for v, fl in an_punkt.items():
        wurzel = {n: n for n in fl}

        def finde(x: int) -> int:
            while wurzel[x] != x:
                wurzel[x] = wurzel[wurzel[x]]
                x = wurzel[x]
            return x

        for n in fl:
            for x in F0[n]:
                if x == v:
                    continue
                paar = kanten.get(tuple(sorted((v, x))), ())
                if len(paar) == 2:
                    wurzel[finde(paar[0])] = finde(paar[1])
        neu: dict[int, int] = {}
        for n in fl:
            w = finde(n)
            if not neu:
                neu[w] = v
            elif w not in neu:
                neu[w] = len(P2)
                P2.append(list(P[v]))
            if neu[w] != v:
                F2[n] = [neu[w] if i == v else i for i in F2[n]]
    return P2, F2


def zerlegen(P: list, F: list) -> list[tuple[list, list]]:
    """Netz in Einzelkörper trennen: Dreiecke hängen nur über Kanten mit genau 2 Nachbarn zusammen. Verschweißte
    Teilstücke (Kante mit 4 Nachbarn) werden so getrennt und bekommen eigene Punkte — Archicad nimmt nur saubere
    Körper. Entartete Dreiecke (zwei gleiche Punkte) fallen weg. Bleibt in einem Teil noch eine Mehrfachkante
    (Teil hängt über andere Kanten zusammen), wird der Fächer je Eckpunkt getrennt (mannigfaltig) und neu zerlegt."""
    aus: list[tuple[list, list]] = []
    for Pk, Fk in _komponenten(P, F):
        P2, F2 = mannigfaltig(Pk, Fk)
        if len(P2) != len(Pk):
            aus.extend(_komponenten(P2, F2))
        else:
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


def bauplan(bauliste: dict, netze, register: dict, zuordnung: dict, klassen: dict, stories: list[dict],
            nur=None, ohne=(), verbund: str = "einzeln") -> tuple[list[dict], list[dict]]:
    """Rein, ohne Archicad: (plan, ausgelassen). klassen = {Klassen-id: Item-GUID}. netze = Ergebnis von
    ifc_teile() (Tupel), ein Dict {GUID: Netze} (= nur eigene Netze, keine Unterteile) oder None (kein IFC).
    Quellen je Bauteil in dieser Reihenfolge: status „fehler“ → ausgelassen; (a) IFC mit verbund „einzeln“ und
    Unterteilen mit Geometrie → Kopf + Unterteile mit eigener GUID (Entscheidung Mudi 19.11.2026); (b) Netze der
    Bauliste (parameter.koerper) → EIN Morph über alle Körper; (c) IFC-Netz des Kopfes; (d) sonst ausgelassen
    („nur Hülle“). verbund „einzeln“: Kopf + jedes Unterteil als eigener Morph; „zusammen“: ein Morph je Verbund."""
    if verbund not in ("einzeln", "zusammen"):
        raise Fehler(f"verbund muss „einzeln“ oder „zusammen“ sein, nicht „{verbund}“.")
    mit_ifc = netze is not None
    if netze is None:
        eigene, kinder, info = {}, {}, {}
    elif isinstance(netze, tuple):
        eigene, kinder, info = netze
    else:
        eigene, kinder, info = netze, {}, {}
    eigene = {k.upper(): v for k, v in eigene.items()}
    kinder = {k.upper(): [x.upper() for x in v] for k, v in kinder.items()}
    info = {k.upper(): v for k, v in info.items()}
    reg = {str(x["guid"]).upper(): x for x in register.get("neu") or []}
    zu_ci = {k.lower(): v for k, v in zuordnung.items()}
    nur_g = {g.upper() for g in nur} if nur else None
    ohne_g = {g.upper() for g in ohne}
    kopf_g = {b["plattformGuid"].upper() for b in bauliste.get("bauteile") or []}
    b_netze = bauliste_netze(bauliste)
    if verbund == "zusammen":
        eigene, kinder = _zusammen(eigene, kinder), {}

    def nachfahren(g: str) -> list[str]:
        aus: list[str] = []
        besucht = {g}

        def lauf(x: str) -> None:
            for k in kinder.get(x, []):
                if k in besucht or k in kopf_g:
                    continue
                besucht.add(k)
                aus.append(k)
                lauf(k)

        lauf(g)
        return aus

    plan: list[dict] = []
    aus: list[dict] = []

    def eintrag(g, name, ebene, klasse, status, ns, kopf, quelle="ifc", kennzahlen=None):
        body, box = morph_body(ns)
        if not box:
            aus.append({"plattformGuid": g, "name": name, "grund": "Netz ohne Dreiecke"})
            return
        plan.append({"g": g, "name": name, "ebene": ebene, "klasse": klasse, "status": status, "kopf": kopf,
                     "quelle": quelle, "netz_kennzahlen": kennzahlen or {},
                     "soll": box, "morph": {"basePoint": {"x": 0, "y": 0, "z": 0}, "body": body,
                                            "floorIndex": geschoss_index(stories, box[2])}})

    for b in bauliste.get("bauteile") or []:
        g = b["plattformGuid"]
        gu = g.upper()
        subs = nachfahren(gu) if verbund == "einzeln" else []
        if gu in ohne_g:
            continue
        if nur_g is not None and gu not in nur_g and not any(s in nur_g for s in subs):
            continue
        klasse = zuordnung.get(b["ifc"]["klasse"])
        status_roh = (reg.get(gu) or {}).get("status")
        status = status_archicad(status_roh)
        kopf_ok = gu in nur_g if nur_g is not None else True
        gebaut_vorher = len(plan)
        ausg_vorher = len(aus)
        if b.get("status") == "fehler":
            aus.append({"plattformGuid": g, "name": b["name"],
                        "grund": "status fehler: " + (b.get("meldung") or "ohne Meldung")})
            continue
        ifc_unter = any(eigene.get(s) for s in subs)
        if not ifc_unter and gu in b_netze and kopf_ok:
            if klasse not in klassen or not status:
                aus.append({"plattformGuid": g, "name": b["name"],
                            "grund": f"Klasse/Status fehlt ({b['ifc']['klasse']}, {status_roh})"})
            else:
                eintrag(g, b["name"], b["ebene"], klasse, status, b_netze[gu], None, "bauliste",
                        b.get("netz_kennzahlen"))
            continue
        ns = eigene.get(gu) or []
        if ns and kopf_ok:
            if klasse not in klassen or not status:
                aus.append({"plattformGuid": g, "name": b["name"],
                            "grund": f"Klasse/Status fehlt ({b['ifc']['klasse']}, {status_roh})"})
            else:
                eintrag(g, b["name"], b["ebene"], klasse, status, ns, None)
        for s in subs:
            if s in ohne_g or (nur_g is not None and not kopf_ok and s not in nur_g):
                continue
            sns = eigene.get(s) or []
            if not sns:
                continue
            si = info.get(s) or {}
            sname = f"{b['name']} / {si.get('name') or s}"
            sk = zu_ci.get((si.get("klasse") or "").lower())
            sk = sk if sk in klassen else klasse
            if sk not in klassen or not status:
                aus.append({"plattformGuid": s, "kopf": g, "name": sname,
                            "grund": f"Klasse/Status fehlt ({si.get('klasse') or b['ifc']['klasse']}, {status_roh})"})
                continue
            eintrag(s, sname, b["ebene"], sk, status, sns, g)
        if len(plan) == gebaut_vorher and len(aus) == ausg_vorher:
            aus.append({"plattformGuid": g, "name": b["name"],
                        "grund": "keine Geometrie im IFC" if mit_ifc
                        else f"nur Hülle (geometrie={b.get('geometrie') or '?'})"})
    return plan, aus


def netz_pruefung(plan: list[dict]) -> dict:
    """Quellen-Zählung und Netz-Kennzahlen (nur Bauliste-Netze) für Trockenlauf/Ergebnis."""
    q = collections.Counter(p.get("quelle") or "ifc" for p in plan)
    nm = of = 0
    pruefen: list[dict] = []
    for p in plan:
        if p.get("quelle") != "bauliste":
            continue
        k = p.get("netz_kennzahlen") or {}
        a, o = int(k.get("nicht_mannigfaltige_kanten") or 0), int(k.get("offene_kanten") or 0)
        nm, of = nm + a, of + o
        if a > 0 or o > 0:
            pruefen.append({"plattformGuid": p["g"], "name": p["name"],
                            "nicht_mannigfaltige_kanten": a, "offene_kanten": o})
    return {"quelle": {"bauliste": q.get("bauliste", 0), "ifc": q.get("ifc", 0)},
            "netz_kennzahlen": {"nicht_mannigfaltige_kanten": nm, "offene_kanten": of},
            "netz_pruefen": pruefen}


def klassifizierung_hinweis(sys_name: str) -> dict:
    """Warnung, wenn nicht das Standardsystem benutzt wird (IFC-Export ordnet dann falsch zu)."""
    if sys_name == SAB_SYSTEM:
        return {}
    return {"klassifizierung_hinweis": f"„{sys_name}“ statt „{SAB_SYSTEM}“: Beim IFC-Export greifen Archicads "
            f"Typzuordnungen nur über den Namen „{SAB_SYSTEM}“, sonst wird alles IfcBuildingElementProxy. "
            "System im Klassifizierungs-Manager umbenennen (nicht neu importieren)."}


def sab_klassen(ac: Archicad, name: Optional[str] = None) -> tuple[str, dict[str, str], str]:
    """(System-GUID, {Klassen-id: Item-GUID}, Systemname). Ohne `name` das erste vorhandene aus SAB_SYSTEME,
    also „Graphisoft Klassifizierung“ vor dem alten SAB-System."""
    vorhanden = ac.api("API.GetAllClassificationSystems").get("classificationSystems") or []
    gesucht = (name,) if name else SAB_SYSTEME
    system = next((s for g in gesucht for s in vorhanden if s.get("name") == g), None)
    if not system:
        raise Fehler("Klassifizierungssystem " + " / ".join(f"„{g}“" for g in gesucht)
                     + " gibt es im Projekt nicht. Vorhanden: "
                     + (", ".join(f"„{s.get('name')}“" for s in vorhanden) or "keine"))
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
    return sid, klassen, system["name"]


def _string(v: Any) -> dict:
    return {"type": "string", "status": "normal", "value": v}


def bauen(ac: Archicad, ordner, ifc, bauliste: dict, register: dict, zuordnung: dict, *, nur, ohne, vorhanden: dict,
          ausfuehren: bool, projekt_soll: Optional[str] = None, verbund: str = "einzeln",
          system: Optional[str] = None) -> dict:
    """Plattform-Bauteile als Morphs (echte Geometrie) nach Archicad: Ebene, Geschoss, Klasse, Umbau-Status,
    Stempel, Rücklese. Ohne `ausfuehren` nur Trockenlauf — es wird nichts Schreibendes gesendet."""
    ordner = Path(ordner)
    projekt = ac.befehl("GetProjectInfo").get("projectName")
    stories = ac.befehl("GetStories").get("stories") or []
    system_id, klassen, sys_name = sab_klassen(ac, system)
    plan, ausgelassen = bauplan(bauliste, ifc_teile(ifc) if ifc else None, register, zuordnung, klassen, stories, nur, ohne,
                                verbund=verbund)
    _schreiben(ordner / "ausgelassen.json", ausgelassen)
    pruef = netz_pruefung(plan)
    _schreiben(ordner / "netz_pruefen.json", pruef["netz_pruefen"])
    zaehl = {**pruef, "klassifizierung": sys_name, **klassifizierung_hinweis(sys_name), "ebenen": dict(collections.Counter(p["ebene"] for p in plan)),
             "klassen": dict(collections.Counter(p["klasse"] for p in plan)),
             "status": dict(collections.Counter(p["status"] for p in plan)),
             "dreiecke": sum(len(p["morph"]["body"]["polygons"]) for p in plan),
             "verbuende": len({p["kopf"] for p in plan if p.get("kopf")}),
             "unterteile": sum(1 for p in plan if p.get("kopf"))}
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
    try:
        vid = ac.api("API.GetPropertyIds", {"properties": [
            {"type": "UserDefined", "localizedName": [STEMPEL_GRUPPE, VERBUND_NAME]}]}).get("properties") or []
    except Fehler:
        vid = []
    pv_id = (vid[0] or {}).get("propertyId") if vid else None
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
        _schreiben(ordner / "angelegt.json", gebaut + [{"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"],
                                                        "kopf": p.get("kopf")}
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
                   *([{"elementId": e["elementId"], "propertyId": pv_id, "propertyValue": _string(p["kopf"])}]
                     if pv_id and p.get("kopf") else []),
                   {"elementId": e["elementId"], "propertyId": ps, "propertyValue": {
                       "type": "singleEnum", "status": "normal",
                       "value": {"type": "nonLocalizedValue", "nonLocalizedValue": p["status"]}}}]
        ac.api("API.SetPropertyValuesOfElements", {"elementPropertyValues": ev})
        els = [e for _, e in ok]
        boxen = ac.api("API.Get3DBoundingBoxes", {"elements": els}).get("boundingBoxes3D") or []
        pv = ac.api("API.GetPropertyValuesOfElements", {
            "elements": els, "properties": [{"propertyId": pg}, {"propertyId": ki}, {"propertyId": ps}]
                          + ([{"propertyId": pv_id}] if pv_id else [])}
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
                 for x in ((pv[n] if n < len(pv) else {}).get("propertyValues") or [])] + [None] * 4
            cids = (cl[n] if n < len(cl) else {}).get("classificationIds") or [{}]
            kg = ((cids[0] or {}).get("classificationId") or {}).get("classificationItemId", {}).get("guid")
            d = det[n] if n < len(det) else {}
            stimmt = (abw is not None and abw <= 0.002 and w[0] == p["g"] and w[1] == tag
                      and (w[2] or {}).get("nonLocalizedValue") == p["status"] and kg == klassen[p["klasse"]]
                      and (not (pv_id and p.get("kopf")) or w[3] == p["kopf"]))
            gebaut.append({"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"],
                           "kopf": p.get("kopf")})
            rueck.append({"plattformGuid": p["g"], "archicadGuid": e["elementId"]["guid"], "name": p["name"],
                          "kopf": p.get("kopf"),
                          "stimmt": bool(stimmt), "abweichung_m": abw,
                          "geschlossen": ((d.get("details") or {}).get("body") or {}).get("isClosed"),
                          "geschoss": d.get("floorIndex"), "ebene": p["ebene"], "klasse": id2klasse.get(kg),
                          "status": (w[2] or {}).get("nonLocalizedValue")})
        _schreiben(ordner / "gebaut.json", gebaut)
        _schreiben(ordner / "ruecklese.json", {"rueck": rueck, "fehler": fehler})
    hinweis = ({"hinweis": "Plattform-Verbund fehlt – Kopf nur in gebaut.json"}
               if not pv_id and any(p.get("kopf") for p in plan) else {})
    return {"trockenlauf": False, "projekt": projekt, "klassifizierung": sys_name, "gebaut": len(gebaut),
            "verbuende": len({x["kopf"] for x in gebaut if x.get("kopf")}),
            "unterteile": sum(1 for x in gebaut if x.get("kopf")), **hinweis,
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
    bauliste = bauliste_laden(o.bauliste)
    nicht_bearbeitet_pruefen(bauliste, o.trotzdem)           # vor jedem Archicad-Aufruf
    aus = bauen(Archicad(_http(o.port)), ordner, o.ifc or None, bauliste, _lesen(Path(o.register)),
                zuordnung_laden(o.zuordnung or ZUORDNUNG), nur=liste(o.nur) or None, ohne=liste(o.ohne),
                vorhanden=_lesen(Path(o.vorhanden)) if o.vorhanden else {}, ausfuehren=o.ausfuehren,
                projekt_soll=soll, verbund=o.verbund, system=o.system or None)
    if aus.get("trockenlauf"):
        aus["hinweis"] = "Trockenlauf — nichts geschrieben. Nach dem Ja des Zeichners mit --ausfuehren wiederholen."
    return aus


def cmd_hochladen(o) -> dict:
    ordner = Path(o.ordner)
    daten = (ordner / "ergebnis.json").read_bytes()
    netze = ordner / nz.DATEI
    teil = int(getattr(o, "netze_teil", 0) or 0)
    if teil:
        from elmonkey import formen as fo
        netze = ordner / fo.TEIL_ORDNER / fo.teil_name(teil)
        if not o.netze_ablage:
            raise Fehler("--netze-teil braucht --netze-ablage")
    if o.netze_ablage:
        _nur_https(o.netze_ablage)                         # vor dem ersten Upload prüfen
        if not netze.is_file():
            raise Fehler(f"{netze.name} fehlt — erst „{'formen' if teil else 'netze'}“ laufen lassen "
                         "(oder --netze-ablage weglassen)")
    ablegen(o.ablage, daten, "application/json")
    aus = {"hochgeladen": ["ergebnis.json"], "bytes": {"ergebnis.json": len(daten)}}
    if o.netze_ablage:
        n = netze.read_bytes()
        ablegen(o.netze_ablage, n, "application/gzip")
        aus["hochgeladen"].append(netze.name)
        aus["bytes"][netze.name] = len(n)
        if teil:
            aus["netze_teil"] = teil
    elif netze.is_file():
        aus["hinweis"] = (f"{nz.DATEI} liegt im Ordner, ist aber nicht hochgeladen — --netze-ablage "
                          "<netze_ablage aus archicad_abgleich_lesen> mitgeben.")
    aus["naechster_schritt"] = ("archicad_abgleich_melden {projekt, lauf} — erst Vorschau, nach dem Ja bestaetigt=true"
                                + (" (liest archicad_netze.json.gz: neue Bauteile + Netz-Ersatz)" if o.netze_ablage
                                   else ""))
    return aus


def cmd_formen(o) -> dict:
    """archicad_netze.json.gz (sab-archicad-netze/1) → netze_teile/archicad_netze_teil_NN.json.gz (/2): je Teil
    höchstens 1 Mio. Dreiecke (Plattform-Grenze je Vorschlag), gleiche Körper stehen je Teil einmal unter `formen`,
    die Bauteile tragen `form` + `platzierung`. Rein rechnerisch, kein Archicad; die Ausgangsdatei bleibt.
    Geschrieben wird nur, wenn das Zurückrollen jedes Bauteil wieder trifft (≤ 0,5 mm, gleiche Dreiecke)."""
    from elmonkey import formen as fo
    ordner = Path(o.ordner)
    quelle = ordner / nz.DATEI
    if not quelle.is_file():
        raise Fehler(f"{nz.DATEI} fehlt — erst „netze“ laufen lassen")
    datei = json.loads(gzip.decompress(quelle.read_bytes()))
    try:
        fertig, groesste = [], 0.0
        for t in fo.teilen(datei):
            neu = fo.formen_bilden(t)
            probe = fo.abweichung(t, fo.ausrollen(neu))
            if not probe["aufbau_gleich"] or probe["groesste_abweichung_m"] > fo.TOLERANZ:
                raise Fehler(f"Gegenprobe gescheitert ({probe}) — nichts geschrieben")
            groesste = max(groesste, probe["groesste_abweichung_m"])
            fertig.append(neu)
    except fo.FormenFehler as e:
        raise Fehler(str(e)) from e
    ziel = ordner / fo.TEIL_ORDNER
    ziel.mkdir(exist_ok=True)
    for alt in ziel.glob("archicad_netze_teil_*.json.gz"):
        alt.unlink()
    teile = []
    for nr, neu in enumerate(fertig, 1):
        pfad = ziel / fo.teil_name(nr)
        pfad.write_bytes(nz.gz_bytes(neu))
        z = neu["zahlen"]
        teile.append({"teil": nr, "datei": pfad.name, "bauteile": len(neu["bauteile"]), "dreiecke": z.get("dreiecke"),
                      "dreiecke_einmalig": z["dreiecke_einmalig"], "formen": z["formen"],
                      "mb": round(pfad.stat().st_size / 1e6, 2)})
    return {"ordner": str(ziel), "format": fo.FORMAT_2, "teile": teile, "bauteile": len(datei["bauteile"]),
            "dreiecke": sum(t["dreiecke"] or 0 for t in teile),
            "dreiecke_einmalig": sum(t["dreiecke_einmalig"] for t in teile),
            "mb_vorher": round(quelle.stat().st_size / 1e6, 2), "mb": round(sum(t["mb"] for t in teile), 2),
            "gegenprobe_groesste_abweichung_mm": round(groesste * 1000, 3),
            "naechster_schritt": "Je Teil der Reihe nach: hochladen --ordner <Ordner> --ablage <ergebnis_ablage> "
                                 "--netze-ablage <netze_ablage> --netze-teil <nr>, dann archicad_abgleich_melden "
                                 "{projekt, lauf} (Vorschau, nach dem Ja bestaetigt=true) — je Teil ein Vorschlag."}


def archicad_datei(ordner: Path) -> Path:
    """Die jüngste von archicad.json (archicad-lesen bzw. MCP-Diff) und archicad_diff.json (Befehl diff)."""
    da = [p for p in (ordner / "archicad.json", ordner / "archicad_diff.json") if p.is_file()]
    if not da:
        raise Fehler(f"{ordner}/archicad.json bzw. archicad_diff.json fehlt — erst diff oder archicad-lesen")
    return max(da, key=lambda p: p.stat().st_mtime)


def cmd_netze(o) -> dict:
    """Neue und in Archicad geänderte Bauteile mit echten Körpern → archicad_netze.json.gz (sab-archicad-netze/1).
    Liest Archicad (GetProjectInfo, fehlende IFC-Klassen/Ebenen über GetDiffDataELM {elements} bzw.
    GetIFCTypeOfElements, Get3DGeometryOfElements, Segmente, Design-Option); anderes Projekt offen → nichts gelesen.
    Einzige Änderung: ausgeblendete Ebenen der gewählten Bauteile kurz einblenden (ELM_SAB.SetLayerAttributes) und
    danach genau diese wieder ausblenden + zurücklesen — nicht mit --ebenen-nicht-einblenden; Teamwork: nur wenn
    die Ebenen-Einstellungen niemand anderes reserviert hat."""
    ordner = Path(o.ordner)
    erg = _lesen(ordner / "ergebnis.json")
    quelle = archicad_datei(ordner)
    archicad = _lesen(quelle)
    ac = Archicad(_http(o.port))
    hinweise: list[str] = []
    bericht: dict = {}
    info = ac.befehl("GetProjectInfo")
    ebenen = None if o.ebenen_nicht_einblenden else nz.Ebenen(ac.elm, ac.befehl, bool(info.get("isTeamwork")))
    try:
        datei = nz.erzeugen(erg, archicad, nz.tapir_holen(ac.befehl), info.get("projectName"), paket=o.paket,
                            diff_daten=nz.elm_diff_daten(ac.elm), ifc_typen=nz.tapir_ifc_typen(ac.befehl),
                            unterelemente=nz.tapir_unterelemente(ac.befehl),
                            design_option=nz.tapir_design_option(ac.befehl),
                            umbau_lesen=nz.api_umbau_lesen(ac.api), ebenen=ebenen,
                            hinweise=hinweise, bericht=bericht)
    except (nz.NetzFehler, nz.EbenenFehler) as e:
        raise Fehler(str(e)) from e
    except Fehler as e:
        notizen = getattr(e, "__notes__", None)
        raise Fehler(str(e) + (" — " + " ".join(notizen) if notizen else "")) from e
    ziel = ordner / nz.DATEI
    ziel.write_bytes(nz.gz_bytes(datei))
    return {"datei": str(ziel), "quelle": quelle.name, "projekt": datei["projekt"], "lauf": datei["lauf"],
            **datei["zahlen"], "mb": round(ziel.stat().st_size / 1e6, 2),
            "fehler_beispiele": [f for f in datei["fehler"] if not f.get("nur_hinweis")][:10],
            **bericht, "hinweise": hinweise,
            "naechster_schritt": "hochladen --ordner <Ordner> --ablage <ergebnis_ablage> --netze-ablage <netze_ablage> "
                                 "(beide Links aus archicad_abgleich_lesen), dann archicad_abgleich_melden."}


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
    _schreiben(ordner / "archicad_diff.json", {"projekt": ac.befehl("GetProjectInfo").get("projectName"),
                                               "seiten": seiten})
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
            "naechster_schritt": "Zahlen dem Zeichner zeigen. Dann „netze“ (neue + in Archicad geänderte Bauteile "
                                 "mit Körpern, keine Gesamt-IFC), ergebnis.json + archicad_netze.json.gz mit "
                                 "„hochladen --netze-ablage“ ablegen, dann archicad_abgleich_melden {projekt, lauf, "
                                 "gebaut} — erst Vorschau, nach dem Ja bestaetigt=true."}


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
    neu("bauen", cmd_bauen, ifc={"default": ""}, bauliste={"required": True, "nargs": "+"},
        trotzdem={"action": "store_true", "help": "trotz nicht bearbeiteter GUIDs bauen"},
        register={"required": True},
        zuordnung={"default": ""}, nur={"default": ""}, ohne={"default": ""}, vorhanden={"default": ""},
        verbund={"choices": ["einzeln", "zusammen"], "default": "einzeln"},
        system={"default": "", "help": "Klassifizierungssystem erzwingen (sonst automatisch)"},
        ausfuehren={"action": "store_true"})
    neu("hochladen", cmd_hochladen, ablage={"required": True},
        netze_ablage={"default": "", "help": "Link netze_ablage aus archicad_abgleich_lesen (archicad_netze.json.gz)"},
        netze_teil={"type": int, "default": 0,
                    "help": "statt archicad_netze.json.gz den Teil <nr> aus netze_teile/ hochladen (nach „formen“)"})
    neu("netze", cmd_netze, paket={"type": int, "default": nz.PAKET, "help": "GUIDs je Get3DGeometryOfElements"},
        ebenen_nicht_einblenden={"action": "store_true",
                                 "help": "ausgeblendete Ebenen nicht kurz einblenden (Bauteile darauf ohne Körper)"})
    neu("formen", cmd_formen)
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
