"""ELMonkey-Kern — Suchen, Vorprüfen und Rücklesen, gleich für jede Quelle (Archicad, Schwarz Plattform).

Drei Arbeitsweisen aus dem Archicad-Add-on ELMonkey (ELM_SAB), hier ohne Archicad und ohne Datenbank:

1. **Finden wie „Suchen & aktivieren“**: Kriterien beliebig tief mit und/oder/nicht verschachtelt —
   Klasse, Geschoss, Name, Status, jede Eigenschaft, GUIDs, Zustände.
2. **Vorprüfung mit Code je Bauteil**, bevor etwas geschrieben wird: NOT_FOUND, GELOESCHT, UNGUELTIG und
   ZU_VIELE halten den ganzen Satz an; NO_EFFECT, AUSGEBLENDET und VORSCHLAG_OFFEN sind Hinweise.
3. **Rücklese**: nach dem Schreiben den Stand frisch lesen und je Bauteil prüfen, ob drin steht, was die
   Vorschau versprochen hat — nicht der Rückgabe des Schreibens glauben.

Geschrieben wird nie hier, sondern im Add-on der jeweiligen Quelle. Alles hier ist rein: Listen rein,
Listen raus — nur Standardbibliothek. Was je Quelle anders ist, steht in `regeln.Regeln`.

Herkunft: Schwarz Plattform, PR #95 (core/elmonkey.py, 30.10.2026), hier von der Plattform gelöst.
"""
from __future__ import annotations

import re
from collections import Counter
from typing import Any, Iterable, Optional

from .regeln import STANDARD, ElmFehler, Regeln

# Vorprüfung: diese Codes halten den Satz an, alle anderen sind Hinweise
BLOCKIEREND = {"NOT_FOUND", "GELOESCHT", "UNGUELTIG", "ZU_VIELE"}
# Ops, deren Ziel über `finden` statt über GUIDs bestimmt werden darf
MIT_FINDEN = {"loeschen", "wiederherstellen", "ausblenden", "einblenden", "eigenschaften_setzen"}
HOECHSTENS = 500

_BLAETTER = ("typ", "geschoss", "name", "beschreibung", "status", "eigenschaft", "guids",
             "ausgeblendet", "geaendert", "plattform")


# ─────────────────────────────────────────────────────────── Finden

def _zahl(w: Any) -> Optional[float]:
    if isinstance(w, (int, float)) and not isinstance(w, bool):
        return float(w)
    try:
        return float(str(w).strip().replace(".", "").replace(",", ".")) if "," in str(w) \
            else float(str(w).strip())
    except (TypeError, ValueError):
        return None


def _text_passt(wert: Any, regel: Any) -> bool:
    """regel: Text (enthält, Groß/klein egal) oder {gleich|enthaelt|beginnt|regex|min|max}."""
    if wert is None:
        return False
    s = str(wert)
    if not isinstance(regel, dict):
        return str(regel).lower() in s.lower()
    if "gleich" in regel and s.lower() != str(regel["gleich"]).lower():
        return False
    if "enthaelt" in regel and str(regel["enthaelt"]).lower() not in s.lower():
        return False
    if "beginnt" in regel and not s.lower().startswith(str(regel["beginnt"]).lower()):
        return False
    if "regex" in regel:
        try:
            if not re.search(str(regel["regex"]), s, re.IGNORECASE):
                return False
        except re.error as e:
            raise ElmFehler(f"regex „{regel['regex']}“: {e}") from e
    if "min" in regel or "max" in regel:
        z = _zahl(s)
        if z is None:
            return False
        if "min" in regel and z < float(_zahl(regel["min"]) or 0):
            return False
        if "max" in regel and z > float(_zahl(regel["max"]) or 0):
            return False
    return True


def _status_soll(w: Any, regeln: Regeln) -> str:
    s = regeln.status_werte.get(str(w or "").strip().lower())
    if not s:
        raise ElmFehler("status: Bestand | Neubau | Abbruch | Temporär")
    return s


def passt(e: dict, k: Any, regeln: Regeln = STANDARD) -> bool:
    """Ein Bauteil gegen einen Kriterienbaum. Mehrere Schlüssel in EINEM Knoten gelten als „und“."""
    if not isinstance(k, dict) or not k:
        raise ElmFehler("Kriterium muss ein Objekt sein, z. B. {\"typ\": \"IfcWall\"}")
    props = e.get("properties") or {}
    for schl, regel in k.items():
        if schl == "und":
            if not all(passt(e, x, regeln) for x in _liste(regel, "und")):
                return False
        elif schl == "oder":
            if not any(passt(e, x, regeln) for x in _liste(regel, "oder")):
                return False
        elif schl == "nicht":
            if passt(e, regel, regeln):
                return False
        elif schl == "typ":
            soll = {str(t).lower() for t in (regel if isinstance(regel, list) else [regel])}
            if str(e.get("type") or "").lower() not in soll:
                return False
        elif schl == "geschoss":
            if not _text_passt(props.get(regeln.f_geschoss), regel):
                return False
        elif schl == "name":
            if not _text_passt(e.get("name"), regel):
                return False
        elif schl == "beschreibung":
            if not _text_passt(props.get(regeln.f_beschreibung), regel):
                return False
        elif schl == "status":
            soll = {_status_soll(s, regeln) for s in (regel if isinstance(regel, list) else [regel])}
            if regeln.status(e) not in soll:
                return False
        elif schl == "eigenschaft":
            if not _eigenschaft_passt(props, regel):
                return False
        elif schl == "guids":
            soll = {str(g).upper() for g in (regel if isinstance(regel, list) else [regel])}
            if str(e.get("guid") or "").upper() not in soll:
                return False
        elif schl in ("ausgeblendet", "geaendert", "plattform"):
            if bool(e.get(schl)) is not bool(regel):
                return False
        elif schl in ("wert", "vorhanden"):
            continue                    # gehören zu „eigenschaft“ im selben Knoten
        else:
            raise ElmFehler(f"unbekanntes Kriterium „{schl}“ — erlaubt: und, oder, nicht, {', '.join(_BLAETTER)}")
    if "eigenschaft" not in k and ("wert" in k or "vorhanden" in k):
        raise ElmFehler("wert/vorhanden nur zusammen mit eigenschaft")
    return True


def _liste(regel: Any, schl: str) -> list:
    if not isinstance(regel, list) or not regel:
        raise ElmFehler(f"{schl}: Liste von Kriterien")
    return regel


def _eigenschaft_passt(props: dict, regel: Any) -> bool:
    """{eigenschaft: "Pset_DoorCommon.FireRating", wert?: Text|{…}, vorhanden?: bool} — als eigener Knoten
    oder kurz {eigenschaft: "Pset.Feld"} (= vorhanden)."""
    if isinstance(regel, str):
        regel = {"name": regel}
    if not isinstance(regel, dict) or not regel.get("name"):
        raise ElmFehler("eigenschaft: \"Pset.Feld\" oder {name: \"Pset.Feld\", wert?, vorhanden?}")
    w = props.get(str(regel["name"]))
    if "vorhanden" in regel and (w not in (None, "")) is not bool(regel["vorhanden"]):
        return False
    if "wert" in regel and not _text_passt(w, regel["wert"]):
        return False
    if "vorhanden" not in regel and "wert" not in regel:
        return w not in (None, "")
    return True


def _normal(k: Any) -> Any:
    """{eigenschaft: "X", wert: …} in einem Knoten → {eigenschaft: {name: "X", wert: …}}."""
    if isinstance(k, dict):
        k = {s: (_normal(v) if s in ("und", "oder", "nicht") and not isinstance(v, list)
                 else [_normal(x) for x in v] if s in ("und", "oder") and isinstance(v, list) else v)
             for s, v in k.items()}
        if isinstance(k.get("eigenschaft"), str) and ("wert" in k or "vorhanden" in k):
            k["eigenschaft"] = {"name": k["eigenschaft"], **{f: k.pop(f) for f in ("wert", "vorhanden") if f in k}}
    return k


def finden(elemente: Iterable[dict], kriterium: Any, regeln: Regeln = STANDARD) -> list[dict]:
    k = _normal(kriterium)
    return [e for e in elemente if passt(e, k, regeln)]


def kurz(e: dict, felder: Iterable[str] = (), regeln: Regeln = STANDARD) -> dict:
    props = e.get("properties") or {}
    out = {"guid": e.get("guid"), "name": e.get("name"), "typ": e.get("type"),
           "geschoss": props.get(regeln.f_geschoss), "status": regeln.status(e)}
    for f in ("ausgeblendet", "geaendert", "plattform"):
        if e.get(f):
            out[f] = True
    for f in felder:
        out[f] = props.get(f)
    return out


def schluessel(elemente: list[dict], n: int = 60) -> list[dict]:
    """Die häufigsten Eigenschaften der Treffer — damit Claude weiß, wonach es weiter filtern kann."""
    z = Counter(s for e in elemente for s, w in (e.get("properties") or {}).items() if w not in (None, ""))
    return [{"eigenschaft": s, "bauteile": c} for s, c in z.most_common(n)]


# ─────────────────────────────────────────────────────────── Vorprüfung

def ops_ausrollen(ops: list[dict], wirksam: dict[str, dict], alle: dict[str, dict],
                  hoechstens: int = HOECHSTENS, regeln: Regeln = STANDARD) -> tuple[list[dict], list[dict], list[dict]]:
    """`finden` in einer Op → GUIDs. Gibt (ops, auswahl je Op, Befunde) zurück.
    eigenschaften_setzen gilt je Bauteil — es wird in eine Op je GUID aufgeteilt."""
    raus: list[dict] = []
    auswahl: list[dict] = []
    befunde: list[dict] = []
    for i, op in enumerate(ops):
        if not isinstance(op, dict) or "finden" not in op:
            raus.append(op)
            continue
        art = op.get("op")
        if art not in MIT_FINDEN:
            raise ElmFehler(f"Op {i + 1} ({art}): finden geht nur bei {', '.join(sorted(MIT_FINDEN))}")
        if any(op.get(f) for f in ("guid", "guids", "ids")):
            raise ElmFehler(f"Op {i + 1}: entweder finden oder guids, nicht beides")
        menge = [alle[g] for g in alle if g not in wirksam] if art == "wiederherstellen" else wirksam.values()
        treffer = [e["guid"] for e in finden(menge, op["finden"], regeln)]
        rest = {s: v for s, v in op.items() if s != "finden"}
        auswahl.append({"op_nr": i + 1, "op": art, "kriterium": op["finden"], "anzahl": len(treffer)})
        if not treffer:
            befunde.append({"op_nr": i + 1, "code": "NOT_FOUND", "text": "finden ergibt kein Bauteil"})
            continue
        if len(treffer) > hoechstens:
            befunde.append({"op_nr": i + 1, "code": "ZU_VIELE",
                            "text": f"finden ergibt {len(treffer)} Bauteile, erlaubt sind {hoechstens} — enger "
                                    "fassen oder hoechstens bewusst erhöhen"})
            continue
        if art == "eigenschaften_setzen":
            raus += [{**rest, "guid": g} for g in treffer]
        else:
            raus.append({**rest, "guids": treffer})
    return raus, auswahl, befunde


def _ziele(op: dict) -> list[str]:
    out: list[str] = []
    for f in ("guid", "alt", "haupt"):
        if op.get(f):
            out.append(str(op[f]))
    for f in ("guids", "teile"):
        roh = op.get(f)
        if isinstance(roh, str):
            roh = [roh]
        out += [str(g) for g in roh or []]
    return list(dict.fromkeys(out))


def vorpruefen(ops: list[dict], wirksam: dict[str, dict], alle: dict[str, dict], *,
               skizzen: Iterable[str] = (), offene_vorschlaege: Iterable[dict] = (),
               regeln: Regeln = STANDARD) -> list[dict]:
    """Ein Befund je (Op, Bauteil), wie ELM_SAB vor dem Schreiben: Code + Klartext."""
    skizzen = set(skizzen)
    offen: dict[str, int] = {}
    for s in offene_vorschlaege:
        for g in regeln.guids_von(s):
            offen.setdefault(g, s.get("nr"))
    befunde: list[dict] = []

    def melden(i: int, op: dict, g: str, code: str, text: str) -> None:
        befunde.append({"op_nr": i + 1, "op": op.get("op"), "guid": g, "code": code, "text": text})

    for i, op in enumerate(ops):
        if not isinstance(op, dict):
            continue
        art = op.get("op")
        for g in _ziele(op):
            if g in skizzen or "/" in g:
                continue                                  # Skizzen-Bauteil — prüft der Ebenen-Schreibweg
            if g not in alle:
                melden(i, op, g, "NOT_FOUND", "gibt es im Modell nicht")
                continue
            e = wirksam.get(g)
            if art == "wiederherstellen":
                if e is not None:
                    melden(i, op, g, "NO_EFFECT", "ist nicht gelöscht")
                continue
            if e is None:
                melden(i, op, g, "GELOESCHT", "ist gelöscht — erst wiederherstellen")
                continue
            if art == "ausblenden" and e.get("ausgeblendet"):
                melden(i, op, g, "NO_EFFECT", "ist schon ausgeblendet")
            elif art == "einblenden" and not e.get("ausgeblendet"):
                melden(i, op, g, "NO_EFFECT", "ist schon sichtbar")
            elif art == "eigenschaften_setzen":
                try:
                    patch, _ = regeln.patch(op, e)
                except ValueError as x:
                    melden(i, op, g, "UNGUELTIG", str(x))
                    continue
                if not patch:
                    melden(i, op, g, "NO_EFFECT", "alle Werte stehen schon so")
            if e.get("ausgeblendet") and art not in ("einblenden", "ausblenden", "loeschen"):
                melden(i, op, g, "AUSGEBLENDET", "ist ausgeblendet — Änderung ist in der Ansicht nicht zu sehen")
            if g in offen:
                melden(i, op, g, "VORSCHLAG_OFFEN", f"steckt schon in offenem Vorschlag #{offen[g]}")
    return befunde


def blockiert(befunde: list[dict]) -> list[dict]:
    return [b for b in befunde if b["code"] in BLOCKIEREND]


# ─────────────────────────────────────────────────────────── Rücklese

def _erwartungen(vorschau: list[dict]) -> list[dict]:
    """Aus der Vorschau des Schreibwegs: was je Bauteil hinterher gelten muss."""
    out: list[dict] = []
    for v in vorschau:
        art = v.get("op")
        if v.get("skizze") and v.get("skizze") == v.get("guid"):
            out.append({"op": art, "guid": v["guid"], "soll": "skizze"})
            continue
        if art == "loeschen":
            if v.get("guid"):
                out.append({"op": art, "guid": v["guid"], "soll": "weg"})
        elif art in ("wiederherstellen", "hinzufuegen", "uebernehmen", "gruppieren"):
            out.append({"op": art, "guid": v.get("guid"), "soll": "da"})
        elif art in ("ausblenden", "einblenden"):
            out.append({"op": art, "guid": v.get("guid"), "soll": art})
        elif art == "eigenschaften_setzen":
            out.append({"op": art, "guid": v.get("guid"), "soll": "werte",
                        "werte": [(d["feld"], d.get("nachher")) for d in v.get("aenderungen") or []]})
        elif art == "ersetzen":
            alt, neu = (v.get("alt") or {}).get("guid"), (v.get("neu") or {}).get("guid")
            if alt and alt != neu:
                out.append({"op": art, "guid": alt, "soll": "weg"})
            if neu:
                out.append({"op": art, "guid": neu, "soll": "da"})
        elif art == "geometrie":
            out.append({"op": art, "guid": v.get("guid"),
                        "soll": "da" if v.get("kopie_von") else "bewegt"})
        elif v.get("guid"):
            out.append({"op": art, "guid": v["guid"], "soll": "da"})
    return [x for x in out if x.get("guid")]


def _wert_ist(e: dict, feld: str) -> Any:
    if feld == "name":
        return e.get("name")
    if feld == "IFC-Klasse":
        return e.get("type")
    return (e.get("properties") or {}).get(feld)


def ruecklesen(vorschau: list[dict], vorher: dict[str, dict], nachher: dict[str, dict]) -> dict:
    """Vorschau gegen den frisch gelesenen Stand. Je Bauteil: ok | abweichend | ungeprueft."""
    zeilen: list[dict] = []
    for x in _erwartungen(vorschau):
        g, soll = x["guid"], x["soll"]
        e = nachher.get(g)
        z = {"op": x["op"], "guid": g, "soll": soll}
        if soll == "skizze":
            z.update(ergebnis="ungeprueft", text="Skizzen-Bauteil — liegt in seiner Ebene, nicht im Modell")
        elif soll == "weg":
            z.update(ergebnis="ok" if e is None else "abweichend",
                     **({} if e is None else {"text": "ist noch im wirksamen Stand"}))
        elif e is None:
            z.update(ergebnis="abweichend", text="fehlt im wirksamen Stand")
        elif soll == "ausblenden":
            z.update(ergebnis="ok" if e.get("ausgeblendet") else "abweichend",
                     **({} if e.get("ausgeblendet") else {"text": "ist sichtbar"}))
        elif soll == "einblenden":
            z.update(ergebnis="abweichend" if e.get("ausgeblendet") else "ok",
                     **({"text": "ist ausgeblendet"} if e.get("ausgeblendet") else {}))
        elif soll == "werte":
            falsch = [{"feld": f, "soll": w, "ist": _wert_ist(e, f)} for f, w in x["werte"]
                      if (_wert_ist(e, f) or None) != w]
            z.update(ergebnis="abweichend" if falsch else "ok", **({"felder": falsch} if falsch else {}),
                     geprueft=len(x["werte"]))
        elif soll == "bewegt":
            alt = (vorher.get(g) or {}).get("vertices")
            neu = e.get("vertices")
            if alt is None or neu is None:
                z.update(ergebnis="ungeprueft", text="keine Geometrie zum Vergleichen")
            else:
                z.update(ergebnis="abweichend" if list(alt) == list(neu) else "ok",
                         **({"text": "Geometrie unverändert"} if list(alt) == list(neu) else {}))
        else:
            z["ergebnis"] = "ok"
        zeilen.append(z)
    zahl = Counter(z["ergebnis"] for z in zeilen)
    return {"verifiziert": zahl["abweichend"] == 0 and bool(zeilen), "ok": zahl["ok"],
            "abweichend": zahl["abweichend"], "ungeprueft": zahl["ungeprueft"],
            "abweichungen": [z for z in zeilen if z["ergebnis"] == "abweichend"],
            "je_bauteil": zeilen[:300]}
