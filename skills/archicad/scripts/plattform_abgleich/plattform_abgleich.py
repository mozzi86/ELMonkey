#!/usr/bin/env python3
"""Abgleich offenes Archicad-Projekt ↔ Schwarz Plattform (bidirektional). Nur Standardbibliothek.

Ablauf (Rezept: skills/archicad/recipes/plattform-abgleich.md). Die Links kommen aus dem Plattform-Konnektor
(`archicad_abgleich_lesen`); große Dateien laufen über diese Links, nicht durch den Chat.

    plattform_abgleich.py archicad-lesen  --ordner <dir> [--port 19723] [--typen Wall,Slab,…] [--alle-eigenschaften]
    plattform_abgleich.py planen          --ordner <dir> --stand-link "<stand_link>"
    plattform_abgleich.py entscheiden     --ordner <dir> [--antworten antworten.json]
    plattform_abgleich.py archicad-schreiben --ordner <dir> [--ausfuehren]      # Eigenschaften nach Archicad
    plattform_abgleich.py stempeln        --ordner <dir> --gebaut gebaut.json [--ausfuehren]
    plattform_abgleich.py hochladen       --ordner <dir> --ablage "<ergebnis_ablage>"
    plattform_abgleich.py oberflaechen    --ordner <dir>                     # Export + ZIP für die Plattform
    plattform_abgleich.py ifc             --ordner <dir> [--translator "<Name>"]
    plattform_abgleich.py diff            --ordner <dir> --stand-link "<stand_link>" [--toleranz 0.003]
                                          # nur die Unterschiede, ohne IFC-Datei (ELMonkey ≥ 0.11.13)

Alles, was Archicad verändert, ist ohne `--ausfuehren` ein Trockenlauf. Gelöscht wird in Archicad nie.
Dateien im Arbeitsordner: archicad.json, stand.json, plan.json, fragen.md, antworten.json, ergebnis.json,
archicad_geschrieben.json, archicad_diff.json (Befehl diff).
"""
from __future__ import annotations

import argparse
import json
import sys
import urllib.request
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
    """Dünne Hülle um die JSON-Schnittstelle. `senden` ist austauschbar (Tests)."""

    def __init__(self, senden: Callable[[dict], dict]):
        self._senden = senden

    def befehl(self, name: str, params: Optional[dict] = None, ns: str = "TapirCommand") -> dict:
        r = self._senden({"command": "API.ExecuteAddOnCommand", "parameters": {
            "addOnCommandId": {"commandNamespace": ns, "commandName": name},
            "addOnCommandParameters": params or {}}})
        if not r.get("succeeded"):
            raise Fehler(f"{ns}.{name}: {json.dumps(r.get('error') or r, ensure_ascii=False)[:500]}")
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
        if alle or p.get("propertyType") == "UserDefined":
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
    ids = [e["elementId"] for e in ac.befehl("GetAllElements").get("elements") or []]
    katalog = eigenschaften_katalog(ac, alle_eigenschaften)
    namen = sorted(katalog)
    prop_ids = [{"propertyId": katalog[n]["propertyId"]} for n in namen]
    elemente: list[dict] = []
    for teil in _pakete(ids):
        details = ac.befehl("GetDetailsOfElements", {"elements": [{"elementId": i} for i in teil]})
        behalten = [(i, d) for i, d in zip(teil, details.get("detailsOfElements") or [])
                    if isinstance(d, dict) and d.get("type") in typen]
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
                            ausfuehren: bool) -> dict:
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
    if teamwork:
        guids = sorted({x["archicad"] for x in geht})
        for teil in _pakete(guids):
            r = ac.befehl("ReserveElements", {"elements": [{"elementId": {"guid": g}} for g in teil]})
            if r.get("conflicts"):
                aus["fehler"].append({"grund": "Teamwork: von anderen reserviert", "anzahl": len(r["conflicts"])})
    for teil in _pakete(geht):
        ac.befehl("SetPropertyValuesOfElements", {"elementPropertyValues": [
            {"elementId": {"guid": x["archicad"]}, "propertyId": katalog[x["feld"]]["propertyId"],
             "propertyValue": {"value": "" if x["wert"] is None else str(x["wert"])}} for x in teil]})
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
    with urllib.request.urlopen(_nur_https(link), timeout=900) as r:
        return json.loads(r.read().decode("utf-8"))


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
        eintraege += [{"archicad": x["archicadGuid"], "feld": ki, "wert": "ja"} for x in gebaut]
    aus = eigenschaften_schreiben(ac, eintraege, katalog,
                                  teamwork=bool(ac.befehl("GetProjectInfo").get("isTeamwork")),
                                  ausfuehren=o.ausfuehren)
    return {"ausgefuehrt": aus["ausgefuehrt"], "bauteile": len(gebaut), "geschrieben": len(aus["geschrieben"]),
            "fehler": aus["fehler"][:10], "nicht_moeglich": aus["nicht_moeglich"][:10]}


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
