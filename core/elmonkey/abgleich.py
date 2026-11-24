"""Abgleich Archicad ↔ Schwarz Plattform (bidirektional) — reiner Kern, nur Standardbibliothek.

Zwei Stände desselben Gebäudes: das offene Archicad-Projekt (BIMcloud/Teamwork) und der wirksame Stand der
Plattform. Dieser Kern paart die Bauteile, vergleicht ihre Informationen mit dem Stand des letzten Abgleichs
(Drei-Wege) und macht daraus einen Plan:

    nach_archicad   Bauteile, die nur die Plattform kennt → in Archicad NEU bauen (neue Archicad-GUID,
                    gestempelt mit „Plattform-GUID“); geänderte Eigenschaften der Plattform → nach Archicad
    nach_plattform  Bauteile, die nur Archicad kennt → per IFC in die Plattform (Import-Vorschlag);
                    geänderte Eigenschaften aus Archicad → als Vorschlag in die Plattform
    fragen          alles Unklare, gebündelt: beide Seiten haben geändert, erster Abgleich mit verschiedenen
                    Werten, auf einer Seite gelöscht. Nichts davon wird ohne Antwort geschrieben.

Formate (alles JSON, `FORMAT`):

    Archicad-Bauteil   {"guid", "ifcId"?, "typ"?, "name"?, "geschoss"?, "ebene"?, "eigenschaften": {"Gruppe.Name": wert}}
    Plattform-Bauteil  {"guid", "name"?, "klasse"?, "geschoss"?, "herkunft": "archicad" | "plattform",
                        "ausgeblendet"?: bool, "eigenschaften": {"Pset.Name": wert}}
    Basis              {"paare": {plattformGuid: archicadGuid}, "werte": {plattformGuid: {feld: wert}}, "stand"?}

Der Kern schreibt nirgends. `planen` → `entscheiden` (mit den Antworten) → Ergebnis mit den Operationen je Seite
und der neuen Basis; ausgeführt wird es vom Skript (Archicad) und von der Plattform (Konnektor).
"""
from __future__ import annotations

from typing import Any, Iterable, Optional

FORMAT = "sab-archicad-abgleich/1"

#: Eigenschaft, die jedes aus der Plattform gebaute Archicad-Element trägt (Endung zählt, die Gruppe ist frei).
PLATTFORM_GUID = "Plattform-GUID"

#: Felder, die nie verglichen werden: Stempel und Technik, kein Fachinhalt.
NIE = ("Plattform-GUID", "KI generiert", "SAB_Plattform.Herkunft", "IFC.GlobalId")

#: Antworten je Frageart; die erste ist „nichts tun“.
OPTIONEN = {
    "eigenschaft": ("ueberspringen", "archicad", "plattform"),
    "fehlt_in_archicad": ("lassen", "plattform_ausblenden", "in_archicad_bauen"),
    "fehlt_in_plattform": ("lassen", "in_plattform_importieren", "in_archicad_loeschen"),
    "in_plattform_ausgeblendet": ("lassen", "in_archicad_loeschen", "plattform_einblenden"),
}

_FEHLT = object()
_B64 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_$"


class AbgleichFehler(ValueError):
    pass


# ─────────────────────────────────────────────────────────── GUIDs

def guid_norm(g: Any) -> str:
    """GUID vergleichbar machen: ohne Klammern, groß. IFC-GlobalIds (22 Zeichen) bleiben, wie sie sind."""
    s = str(g or "").strip().strip("{}")
    return s if len(s) == 22 and "-" not in s else s.upper()


def ifc_guid(guid: Any) -> Optional[str]:
    """Archicad-GUID (8-4-4-4-12) → IFC-GlobalId (22 Zeichen, Standard-Kompression). None, wenn es keine ist."""
    h = str(guid or "").strip().strip("{}").replace("-", "")
    if len(h) != 32:
        return None
    try:
        zahl = int(h, 16)
    except ValueError:
        return None
    aus = []
    for _ in range(22):
        aus.append(_B64[zahl % 64])
        zahl //= 64
    return "".join(reversed(aus))


# ─────────────────────────────────────────────────────────── Werte

def wert_norm(v: Any) -> Any:
    """Werte beider Seiten vergleichbar machen: leer = None, Zahl = gerundet, Wahrheitswert = bool, Text getrimmt."""
    if v is None:
        return None
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return round(float(v), 6)
    if isinstance(v, (list, tuple)):
        return tuple(wert_norm(x) for x in v)
    if isinstance(v, dict):
        return tuple(sorted((str(k), wert_norm(x)) for k, x in v.items()))
    s = str(v).strip()
    if not s:
        return None
    klein = s.lower()
    if klein in ("true", "ja", "wahr"):
        return True
    if klein in ("false", "nein", "falsch"):
        return False
    try:
        return round(float(s.replace(",", ".")), 6) if any(c.isdigit() for c in s) else s
    except ValueError:
        return s


def gleich(a: Any, b: Any) -> bool:
    return wert_norm(a) == wert_norm(b)


def _vergleichbar(feld: str, nie: Iterable[str]) -> bool:
    return not any(feld == n or feld.endswith("." + n) for n in nie)


def plattform_guid_von(e: dict) -> Optional[str]:
    """„Plattform-GUID“ eines Archicad-Bauteils (Stempel aus einem früheren Bau), sonst None."""
    for k, v in (e.get("eigenschaften") or {}).items():
        if (k == PLATTFORM_GUID or k.endswith("." + PLATTFORM_GUID)) and str(v or "").strip():
            return guid_norm(v)
    return None


# ─────────────────────────────────────────────────────────── paaren

def paaren(archicad: list[dict], plattform: list[dict], basis: Optional[dict] = None) -> dict:
    """Bauteile beider Seiten einander zuordnen.

    Reihenfolge je Plattform-Bauteil: (1) Paar aus dem letzten Abgleich, (2) Stempel „Plattform-GUID“ am
    Archicad-Bauteil, (3) gleiche GUID bzw. IFC-GlobalId (Bauteil kam per IFC aus Archicad).
    → {"paare": [(p, a)], "nur_plattform": [p], "nur_archicad": [a], "doppelt": [...]}"""
    basis = basis or {}
    ac_je_guid = {guid_norm(a.get("guid")): a for a in archicad}
    ac_je_ifc: dict[str, dict] = {}
    ac_je_stempel: dict[str, dict] = {}
    doppelt: list[dict] = []
    for a in archicad:
        for ifc in (a.get("ifcId"), ifc_guid(a.get("guid"))):
            if ifc:
                ac_je_ifc.setdefault(str(ifc), a)
        st = plattform_guid_von(a)
        if st:
            if st in ac_je_stempel:
                doppelt.append({"plattformGuid": st, "archicad": [ac_je_stempel[st].get("guid"), a.get("guid")]})
            else:
                ac_je_stempel[st] = a
    alt = {guid_norm(p): guid_norm(a) for p, a in (basis.get("paare") or {}).items()}
    vergeben: set[str] = set()
    paare, nur_p = [], []
    for p in plattform:
        pg = guid_norm(p.get("guid"))
        kandidaten = (ac_je_guid.get(alt.get(pg, "")), ac_je_stempel.get(pg), ac_je_guid.get(pg),
                      ac_je_ifc.get(str(p.get("guid") or "")))
        a = next((x for x in kandidaten if x is not None and guid_norm(x.get("guid")) not in vergeben), None)
        if a is None:
            nur_p.append(p)
        else:
            vergeben.add(guid_norm(a.get("guid")))
            paare.append((p, a))
    nur_a = [a for a in archicad if guid_norm(a.get("guid")) not in vergeben]
    return {"paare": paare, "nur_plattform": nur_p, "nur_archicad": nur_a, "doppelt": doppelt}


# ─────────────────────────────────────────────────────────── vergleichen (Drei-Wege)

def vergleichen(p: dict, a: dict, basis_werte: Optional[dict] = None, *, nie: Iterable[str] = NIE) -> dict:
    """Eigenschaften eines Paars vergleichen.

    Verglichen wird, was beide Seiten kennen, dazu jedes Feld aus der Basis. Mit Basiswert entscheidet, wer sich
    davon entfernt hat; ohne Basiswert füllt ein vorhandener Wert die leere Seite, zwei verschiedene Werte sind
    eine Frage. → {"nach_archicad": [...], "nach_plattform": [...], "konflikte": [...], "gleich": {feld: wert}}"""
    pe, ae = p.get("eigenschaften") or {}, a.get("eigenschaften") or {}
    bw = basis_werte or {}
    felder = sorted(f for f in (set(pe) & set(ae)) | (set(bw) & (set(pe) | set(ae))) if _vergleichbar(f, nie))
    aus: dict = {"nach_archicad": [], "nach_plattform": [], "konflikte": [], "gleich": {}}
    for f in felder:
        wp, wa, wb = pe.get(f), ae.get(f), bw.get(f, _FEHLT)
        if gleich(wp, wa):
            aus["gleich"][f] = wp
            continue
        eintrag = {"feld": f, "archicad": wa, "plattform": wp}
        if wb is not _FEHLT:
            eintrag["basis"] = wb
            if gleich(wa, wb):
                aus["nach_archicad"].append(eintrag)        # nur die Plattform hat geändert
            elif gleich(wp, wb):
                aus["nach_plattform"].append(eintrag)       # nur Archicad hat geändert
            else:
                aus["konflikte"].append(eintrag)            # beide haben geändert, verschieden
        elif wert_norm(wa) is None and f in ae:
            aus["nach_archicad"].append(eintrag)            # Archicad leer, Plattform hat einen Wert
        elif wert_norm(wp) is None and f in pe:
            aus["nach_plattform"].append(eintrag)
        else:
            aus["konflikte"].append(eintrag)                # erster Abgleich, zwei verschiedene Werte
    return aus


# ─────────────────────────────────────────────────────────── planen

def _kurz(e: dict) -> str:
    teile = [str(e.get("name") or "").strip(), str(e.get("klasse") or e.get("typ") or "").strip(),
             str(e.get("geschoss") or "").strip()]
    return " · ".join(t for t in teile if t) or str(e.get("guid"))


class _Fragen:
    """Gleichartige Unklarheiten zu EINER Frage bündeln — eine Antwort gilt für alle, Ausnahmen je Bauteil."""

    def __init__(self) -> None:
        self._je: dict[tuple, dict] = {}

    def dazu(self, art: str, schluessel: tuple, text: str, p: Optional[dict], a: Optional[dict], **extra) -> None:
        f = self._je.get((art, *schluessel))
        if f is None:
            f = self._je[(art, *schluessel)] = {"art": art, "text": text, "optionen": list(OPTIONEN[art]),
                                               "anzahl": 0, "beispiele": [], "bauteile": [], **extra}
        f["anzahl"] += 1
        if len(f["beispiele"]) < 5:
            f["beispiele"].append(_kurz(p or a or {}))
        f["bauteile"].append({"plattform": (p or {}).get("guid"), "archicad": (a or {}).get("guid")})

    def liste(self) -> list[dict]:
        fragen = sorted(self._je.values(), key=lambda f: (-f["anzahl"], f["art"], f["text"]))
        for i, f in enumerate(fragen, 1):
            f["id"] = f"F{i}"
        return fragen


def planen(archicad: list[dict], plattform: list[dict], basis: Optional[dict] = None, *,
           nie: Iterable[str] = NIE) -> dict:
    """Der ganze Abgleich als Plan (schreibt nichts). Siehe Modulkopf."""
    basis = basis or {}
    nie = tuple(nie)
    z = paaren(archicad, plattform, basis)
    alt_p = {guid_norm(g) for g in (basis.get("paare") or {})}
    alt_a = {guid_norm(g) for g in (basis.get("paare") or {}).values()}
    werte = {guid_norm(g): w for g, w in (basis.get("werte") or {}).items()}
    fragen = _Fragen()
    plan: dict = {"format": FORMAT, "paare": {}, "werte": {},
                  "nach_archicad": {"neu_bauen": [], "eigenschaften": []},
                  "nach_plattform": {"neu_importieren": [], "eigenschaften": []},
                  "hinweise": []}
    for d in z["doppelt"]:
        plan["hinweise"].append(f"„Plattform-GUID“ {d['plattformGuid']} steht an mehreren Archicad-Bauteilen "
                                f"({', '.join(str(g) for g in d['archicad'])}) — nur das erste zählt als Partner.")

    for p, a in z["paare"]:
        pg, ag = p.get("guid"), a.get("guid")
        plan["paare"][pg] = ag
        if p.get("ausgeblendet"):
            fragen.dazu("in_plattform_ausgeblendet", (), "In der Plattform gelöscht/ausgeblendet, in Archicad noch da.",
                        p, a)
            continue
        v = vergleichen(p, a, werte.get(guid_norm(pg)), nie=nie)
        plan["werte"][pg] = dict(v["gleich"])
        for e in v["nach_archicad"]:
            plan["nach_archicad"]["eigenschaften"].append(
                {"plattform": pg, "archicad": ag, "feld": e["feld"], "wert": e["plattform"], "vorher": e["archicad"]})
        for e in v["nach_plattform"]:
            plan["nach_plattform"]["eigenschaften"].append(
                {"plattform": pg, "archicad": ag, "feld": e["feld"], "wert": e["archicad"], "vorher": e["plattform"]})
        for e in v["konflikte"]:
            grund = "beide Seiten geändert" if "basis" in e else "erster Abgleich, Werte verschieden"
            fragen.dazu("eigenschaft", (e["feld"], repr(wert_norm(e["archicad"])), repr(wert_norm(e["plattform"]))),
                        f"{e['feld']}: Archicad „{e['archicad']}“ ↔ Plattform „{e['plattform']}“ ({grund})",
                        p, a, feld=e["feld"], archicad=e["archicad"], plattform=e["plattform"])

    # Verbünde und Ebenen-Bauteile (24.11.2026, BFW07): Archicad-Bauteile mit „Plattform-GUID“ stammen aus der
    # Plattform — auch wenn der Stand ihre GUID nicht als eigenes Bauteil führt (Unterteil eines Verbunds, Bauteil
    # einer Plattform-Ebene). Sie sind nie „neu aus Archicad“; ein Kopf, dessen Unterteile gestempelt in Archicad
    # stehen, fehlt dort nicht.
    gestempelt = {g for g in (plattform_guid_von(a) for a in z["nur_archicad"]) if g}
    plan["nach_plattform"]["plattform_herkunft"] = []
    plan["verbuende_ueber_unterteile"] = []

    for p in z["nur_plattform"]:
        if p.get("ausgeblendet"):
            continue                                          # auf beiden Seiten weg
        teile = [guid_norm((u or {}).get("guid") if isinstance(u, dict) else u) for u in p.get("unterteile") or []]
        da = [t for t in teile if t in gestempelt]
        if da:
            plan["verbuende_ueber_unterteile"].append({"plattform": p.get("guid"), "unterteile": len(teile),
                                                       "in_archicad": len(da), "bauteil": _kurz(p)})
            continue
        if guid_norm(p.get("guid")) in alt_p:
            fragen.dazu("fehlt_in_archicad", ("geloescht",), "War abgeglichen, in Archicad jetzt gelöscht.", p, None)
        elif p.get("herkunft") == "plattform":
            plan["nach_archicad"]["neu_bauen"].append({"plattform": p.get("guid"), "bauteil": _kurz(p)})
        else:
            fragen.dazu("fehlt_in_archicad", ("alt",), "Stammt aus einem früheren Archicad-Stand, fehlt im offenen "
                        "Archicad-Projekt (gelöscht, ausgeblendete Ebene oder anderes Teilmodell?).", p, None)

    for a in z["nur_archicad"]:
        if guid_norm(a.get("guid")) in alt_a:                 # zuerst: war gepaart, Plattform-Partner ist weg
            fragen.dazu("fehlt_in_plattform", (), "War abgeglichen, in der Plattform jetzt nicht mehr vorhanden.",
                        None, a)
        elif plattform_guid_von(a):
            plan["nach_plattform"]["plattform_herkunft"].append(
                {"archicad": a.get("guid"), "plattformGuid": plattform_guid_von(a), "bauteil": _kurz(a)})
        else:
            plan["nach_plattform"]["neu_importieren"].append({"archicad": a.get("guid"), "bauteil": _kurz(a)})

    n = len(plan["nach_plattform"]["plattform_herkunft"])
    if n:
        plan["hinweise"].append(f"{n} Archicad-Bauteile tragen eine „Plattform-GUID“, die der Stand nicht als eigenes "
                                "Bauteil führt (Verbund-Unterteile, Bauteile einer Plattform-Ebene) — sie kommen aus "
                                "der Plattform und werden nicht als neu gemeldet.")
    unvollst = [v for v in plan["verbuende_ueber_unterteile"] if v["in_archicad"] < v["unterteile"]]
    if unvollst:
        plan["hinweise"].append(f"{len(unvollst)} Verbünde stehen nur teilweise in Archicad (Unterteile fehlen): "
                                + ", ".join(str(v["plattform"]) for v in unvollst[:10]))
    plan["fragen"] = fragen.liste()
    plan["zahlen"] = zahlen(plan)
    return plan


def zahlen(plan: dict) -> dict:
    na, np_ = plan["nach_archicad"], plan["nach_plattform"]
    return {"paare": len(plan["paare"]),
            "nach_archicad_neu": len(na["neu_bauen"]), "nach_archicad_eigenschaften": len(na["eigenschaften"]),
            "nach_plattform_neu": len(np_["neu_importieren"]),
            "nach_plattform_eigenschaften": len(np_["eigenschaften"]),
            "plattform_herkunft_in_archicad": len(np_.get("plattform_herkunft") or []),
            "verbuende_ueber_unterteile": len(plan.get("verbuende_ueber_unterteile") or []),
            "fragen": len(plan.get("fragen") or []),
            "fragen_bauteile": sum(f["anzahl"] for f in plan.get("fragen") or [])}


# ─────────────────────────────────────────────────────────── entscheiden

def _wahl(antwort: Any, bauteil: dict) -> Optional[str]:
    if antwort is None:
        return None
    if isinstance(antwort, str):
        return antwort
    aus = antwort.get("ausnahmen") or {}
    for g in (bauteil.get("plattform"), bauteil.get("archicad")):
        if g in aus:
            return aus[g]
    return antwort.get("wahl")


def entscheiden(plan: dict, antworten: Optional[dict] = None) -> dict:
    """Plan + Antworten → Ergebnis: die Operationen je Seite, was offen bleibt, die neue Basis.

    `antworten`: {"F1": "archicad"} oder {"F1": {"wahl": "archicad", "ausnahmen": {guid: "plattform"}}}.
    Unbeantwortete Fragen bleiben offen — dazu wird nichts geschrieben, und sie kommen beim nächsten Abgleich
    wieder (ihr Feld geht nicht in die Basis)."""
    antworten = antworten or {}
    bekannt = {f["id"] for f in plan.get("fragen") or []}
    fremd = sorted(set(antworten) - bekannt)
    if fremd:
        raise AbgleichFehler(f"Antworten zu unbekannten Fragen: {', '.join(fremd)}")
    erg: dict = {
        "format": FORMAT,
        "archicad": {"neu_bauen": [x["plattform"] for x in plan["nach_archicad"]["neu_bauen"]],
                     "eigenschaften": [dict(x) for x in plan["nach_archicad"]["eigenschaften"]],
                     "loeschen_von_hand": []},
        "plattform": {"neu_importieren": [x["archicad"] for x in plan["nach_plattform"]["neu_importieren"]],
                      "eigenschaften": [dict(x) for x in plan["nach_plattform"]["eigenschaften"]],
                      "ausblenden": [], "einblenden": []},
        "offen": [],
    }
    paare = dict(plan["paare"])
    werte = {g: dict(w) for g, w in plan["werte"].items()}
    # Basis eines übertragenen Felds = Wert der EMPFANGENDEN Seite vor der Übertragung. Kommt die Übertragung an,
    # sind beide Seiten beim nächsten Lauf gleich; kommt sie nicht an (Vorschlag in der Plattform noch nicht
    # freigegeben, Schreiben in Archicad gescheitert), wird sie ohne neue Frage wiederholt — nie umgekehrt.
    for x in erg["archicad"]["eigenschaften"] + erg["plattform"]["eigenschaften"]:
        werte.setdefault(x["plattform"], {})[x["feld"]] = x["vorher"]

    for f in plan.get("fragen") or []:
        offen = 0
        for b in f["bauteile"]:
            w = _wahl(antworten.get(f["id"]), b)
            if w is None:
                offen += 1
                continue
            if w not in f["optionen"]:
                raise AbgleichFehler(f"{f['id']}: Antwort „{w}“ gibt es nicht — möglich: {', '.join(f['optionen'])}")
            pg, ag = b.get("plattform"), b.get("archicad")
            if f["art"] == "eigenschaft":
                if w == "archicad":
                    erg["plattform"]["eigenschaften"].append({"plattform": pg, "archicad": ag, "feld": f["feld"],
                                                               "wert": f["archicad"], "vorher": f["plattform"]})
                    werte.setdefault(pg, {})[f["feld"]] = f["plattform"]
                elif w == "plattform":
                    erg["archicad"]["eigenschaften"].append({"plattform": pg, "archicad": ag, "feld": f["feld"],
                                                              "wert": f["plattform"], "vorher": f["archicad"]})
                    werte.setdefault(pg, {})[f["feld"]] = f["archicad"]
            elif w == "plattform_ausblenden":
                erg["plattform"]["ausblenden"].append(pg)
                paare.pop(pg, None)
                werte.pop(pg, None)
            elif w == "in_archicad_bauen":
                erg["archicad"]["neu_bauen"].append(pg)
            elif w == "in_plattform_importieren":
                erg["plattform"]["neu_importieren"].append(ag)
            elif w == "in_archicad_loeschen":
                erg["archicad"]["loeschen_von_hand"].append(ag)
            elif w == "plattform_einblenden":
                erg["plattform"]["einblenden"].append(pg)
        if offen:
            erg["offen"].append({"id": f["id"], "text": f["text"], "anzahl": offen})
    erg["basis"] = {"paare": paare, "werte": werte}
    erg["zahlen"] = {
        "archicad_neu_bauen": len(erg["archicad"]["neu_bauen"]),
        "archicad_eigenschaften": len(erg["archicad"]["eigenschaften"]),
        "archicad_loeschen_von_hand": len(erg["archicad"]["loeschen_von_hand"]),
        "plattform_neu_importieren": len(erg["plattform"]["neu_importieren"]),
        "plattform_eigenschaften": len(erg["plattform"]["eigenschaften"]),
        "plattform_ausblenden": len(erg["plattform"]["ausblenden"]),
        "plattform_einblenden": len(erg["plattform"]["einblenden"]),
        "offen": sum(o["anzahl"] for o in erg["offen"]),
    }
    return erg


def gebaut_eintragen(basis: dict, gebaut: dict[str, str]) -> dict:
    """Nach dem Bau in Archicad: {plattformGuid: neue Archicad-GUID} in die Basis — ab jetzt ein Paar."""
    paare = dict(basis.get("paare") or {})
    paare.update({p: a for p, a in gebaut.items() if p and a})
    return {**basis, "paare": paare}
