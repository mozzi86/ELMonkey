"""Diff-Checker Archicad ↔ Plattform ohne IFC-Export — reiner Kern, nur Standardbibliothek.

Eingänge:

    archicad   Antwortseiten von ``ELM_SAB.GetDiffDataELM`` (Format ``elm-diff-daten/1``): je Element GUID,
               IFC-GlobalId, IFC-Klasse, IFC-Attribute/-Eigenschaften, Geschoss, Umbau-Status, Ebene, Aufbau,
               Oberflächen, Klassifizierung, Zone, Wirt, Geometrie-Box mit Quelle (3dModel|calcBounds|none).
    stand      ``stand.json`` aus dem Plattform-Werkzeug ``archicad_abgleich_lesen`` (Format
               ``sab-archicad-abgleich/1``: elemente, geschosse, basis, lauf).

Ausgang: ``ergebnis.json`` im Format ``sab-archicad-abgleich/1`` (wie ``elmonkey.abgleich.entscheiden`` — das liest
``archicad_abgleich_melden``) plus Erweiterung ``diff`` (neu / weg / Eigenschaften / Geometrie je Seite mit Delta,
Zonen, Öffnungen, Bauteilteile, ungeprüfte Geometrie, Hinweise). Die Plattform ignoriert unbekannte Schlüssel.

Fallen aus dem ersten Live-Lauf (je eine Regel hier, Tests in tests/test_diff_plattform.py):

- rohe Box statt getrimmter Geometrie → nur Archicad-Boxen mit Quelle ``3dModel`` gelten als Beleg
- Mehrschicht-Bauteile mit Geometrie nur in ``IfcBuildingElementPart`` → Teile in ihr Bauteil zusammenfassen
- Umbau-Filter blendet Abbruch aus → Archicad-Seite liest ohne Filter; fehlen Abbruch-Bauteile trotzdem: Hinweis
- ``IfcOpeningElement``-GUIDs sind je Export neu → Öffnungen über Tür/Fenster bzw. Lage abgleichen, nie „weg"
- unvollständige Klassenliste → jede IFC-Klasse zählt, nur Nicht-Bauteile (Annotation, Raster …) bleiben draußen
- ``IfcSpace`` → Zonen getrennt, nie „neu"
- Schreibweisen (``.T.``/„Ja", ``IFCLABEL('X')``/``['X']``, ``\\X\\E4``) → normalisieren
- Mengen ändern sich ohne Geometrie → Mengen-Psets nie als Änderung
- Plattform-Bauteile, in Archicad nachgebaut → Paare über Stempel „Plattform-GUID" bzw. ``gebaut``
"""
from __future__ import annotations

import re
from typing import Any, Iterable, Optional

from . import abgleich as ab

FORMAT = ab.FORMAT
FORMAT_ARCHICAD = "elm-diff-daten/1"
TOLERANZ = 0.003                     # m, Geometrie je Seite
SEITEN = ("x-", "x+", "y-", "y+", "z-", "z+")

#: IFC-Klassen, die kein Bauteil sind (räumliche Struktur, Annotation, Raster …).
KEINE_BAUTEILE = {"ifcannotation", "ifcgrid", "ifcgridaxis", "ifcsite", "ifcbuilding", "ifcbuildingstorey",
                  "ifcproject", "ifcvirtualelement", "ifcstructuralanalysismodel"}
ZONE = "ifcspace"
OEFFNUNG = "ifcopeningelement"
TEIL = "ifcbuildingelementpart"
TUER_FENSTER = {"ifcdoor", "ifcwindow", "ifcdoorstandardcase", "ifcwindowstandardcase"}

#: Psets, die Mengen tragen — ändern sich mit Öffnungen/Anschlüssen, ohne dass das Bauteil geändert wurde.
MENGEN_PSET = re.compile(r"^(qto_|basequantities|archicadquantities|.*quantit|.*mengen)", re.IGNORECASE)

#: Feldnamen der Plattform für IFC-Attribute und Archicad-Kopfdaten (aus core/plattform_monkey/abgleich.py `_KOPF`).
ATTRIBUT_FELD = {"Name": "IFC.Name", "Description": "IFC.Beschreibung", "PredefinedType": "IFC.Vordefinierter Typ",
                 "ObjectType": "IFC.Objekttyp", "Tag": "IFC.Tag"}
F_KLASSE, F_GESCHOSS, F_STATUS, F_GLOBALID = "IFC.Klasse", "Verortung.Geschoss", "SAB_Planung.Umbau-Status", "IFC.GlobalId"

STATUS_ARCHICAD = {"Existing": "Bestand", "New": "Neubau", "ToBeDemolished": "Abbruch"}
_STATUS_KANON = {"bestand": "bestand", "existing": "bestand", "vorhanden": "bestand",
                 "neu": "neu", "neubau": "neu", "new": "neu",
                 "abbruch": "abbruch", "abriss": "abbruch", "tobedemolished": "abbruch", "demolished": "abbruch",
                 "abzubrechen": "abbruch"}


# ─────────────────────────────────────────────────────────── Werte normalisieren

def step_dekodieren(s: str) -> str:
    r"""IFC/STEP-Zeichenkodierung auflösen: ``\X2\00E4\X0\`` (UTF-16), ``\X\E4`` (Latin-1), ``\S\d`` und ``''``."""
    def x2(m: re.Match) -> str:
        h = m.group(1)
        return "".join(chr(int(h[i:i + 4], 16)) for i in range(0, len(h) - len(h) % 4, 4))
    s = re.sub(r"\\X2\\([0-9A-Fa-f]+)\\X0\\", x2, s)
    s = re.sub(r"\\X\\([0-9A-Fa-f]{2})", lambda m: chr(int(m.group(1), 16)), s)
    s = re.sub(r"\\S\\(.)", lambda m: chr(ord(m.group(1)) + 128), s)
    return s.replace("''", "'")


_WAHR = {".t.", "t", "true", ".true.", "ja", "wahr", "yes"}
_FALSCH = {".f.", "f", "false", ".false.", "nein", "falsch", "no"}
_LISTE = re.compile(r"^\s*('(?:[^']|'')*'|\"[^\"]*\")(\s*,\s*('(?:[^']|'')*'|\"[^\"]*\"))+\s*$")


def wert_norm(v: Any) -> Any:
    """Wert beider Seiten vergleichbar machen.

    ``IFCLABEL('X')``/``IFCBOOLEAN(.T.)`` → Inhalt, ``['X']``/``("X")`` → X, ``.T.``/„Ja"/True → True,
    ``.ENUM.`` → ENUM, STEP-Kodierung aufgelöst, Zahl mit Komma → float, Leerraum zusammengefasst, leer → None."""
    if v is None:
        return None
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, (list, tuple)):
        teile = [wert_norm(x) for x in v]
        teile = [t for t in teile if t is not None]
        if not teile:
            return None
        return teile[0] if len(teile) == 1 else tuple(teile)
    s = step_dekodieren(str(v)).strip()
    for _ in range(3):                                   # verschachtelt: (IFCLABEL('X')), ['X'] …
        alt = s
        if len(s) >= 2 and s[0] + s[-1] in ("[]", "()"):
            innen = s[1:-1].strip()
            if "," not in innen or re.fullmatch(r"[+-]?\d+,\d+", innen):
                s = innen                                # ['X'], (IFCLABEL('X')), [0,24]
            elif _LISTE.match(innen):                    # ['A','B'] → ("A", "B"), wie eine echte Liste
                return wert_norm([a or b for a, b in re.findall(r"'((?:[^']|'')*)'|\"([^\"]*)\"", innen)])
        m = re.match(r"^IFC[A-Z0-9_]*\s*\((.*)\)$", s, re.IGNORECASE)
        if m:
            s = m.group(1).strip()
        if len(s) >= 2 and s[0] == s[-1] and s[0] in "'\"":
            s = s[1:-1]
        if s == alt:
            break
    s = re.sub(r"\s+", " ", s).strip()
    if not s or s in ("$", "*"):                         # STEP: $ = nicht gesetzt, * = abgeleitet
        return None
    k = s.lower()
    if k in _WAHR:
        return True
    if k in _FALSCH:
        return False
    if len(s) > 2 and s[0] == s[-1] == ".":
        return s[1:-1]
    if re.fullmatch(r"[+-]?(\d+([.,]\d*)?|[.,]\d+)([eE][+-]?\d+)?", s):
        try:
            return float(s.replace(",", "."))
        except ValueError:
            pass
    return s


def status_norm(v: Any) -> Optional[str]:
    w = wert_norm(v)
    if w is None:
        return None
    return _STATUS_KANON.get(re.sub(r"[\s_-]", "", str(w)).lower(), str(w).lower())


def klasse_norm(k: Any) -> str:
    """IfcWallStandardCase ~ IfcWall; Groß-/Kleinschreibung egal."""
    s = str(wert_norm(k) or "").strip().lower()
    return re.sub(r"(standardcase|elementedcase)$", "", s)


def gleich(a: Any, b: Any, feld: str = "") -> bool:
    """Gleich nach Normalisierung. Zahlen: |a−b| ≤ max(0,002; 1e-4·|a|). Felder Klasse/Umbau-Status mit Synonymen."""
    if feld == F_KLASSE:
        return klasse_norm(a) == klasse_norm(b)
    if feld == F_STATUS or feld.endswith(".Umbau-Status") or feld.endswith(".Renovierungsstatus"):
        return status_norm(a) == status_norm(b)
    na, nb = wert_norm(a), wert_norm(b)
    if isinstance(na, float) and isinstance(nb, float):
        return abs(na - nb) <= max(0.002, 1e-4 * abs(na))
    if isinstance(na, str) and isinstance(nb, str):
        return na.casefold() == nb.casefold()
    return na == nb


def ist_menge(feld: str) -> bool:
    """Feld aus einem Mengen-Pset (Qto_*, BaseQuantities, …) — wird nie als Änderung gewertet."""
    pset = feld.split(".", 1)[0] if "." in feld else ""
    return bool(pset) and bool(MENGEN_PSET.match(pset))


# ─────────────────────────────────────────────────────────── Archicad-Seite

def _ifc22(v: Any) -> Optional[str]:
    s = str(v or "").strip().strip("{}")
    if not s:
        return None
    if len(s) == 22 and "-" not in s:
        return s
    return ab.ifc_guid(s) or s                              # AC27: unkomprimierte GUID; Unbekanntes bleibt, wie es ist


def _box(v: Any) -> Optional[list[float]]:
    """Box als [xmin, xmax, ymin, ymax, zmin, zmax] aus Liste oder {xMin…}/{xmin…}."""
    if isinstance(v, dict):
        try:
            return [float(v[k]) for k in ("xMin", "xMax", "yMin", "yMax", "zMin", "zMax")]
        except (KeyError, TypeError, ValueError):
            try:
                return [float(v[k]) for k in ("xmin", "xmax", "ymin", "ymax", "zmin", "zmax")]
            except (KeyError, TypeError, ValueError):
                return None
    if isinstance(v, (list, tuple)) and len(v) == 6:
        try:
            b = [float(x) for x in v]
        except (TypeError, ValueError):
            return None
        return b if b[0] <= b[1] and b[2] <= b[3] and b[4] <= b[5] else None
    return None


def ifc_kennungen(extern: Optional[str], eigen: Optional[str], guid: Any = None) -> list[str]:
    """Kennungen zum Paaren mit der Plattform, in dieser Reihenfolge: externe GlobalId (Bauteil kam per IFC ins
    Archicad-Projekt — so kennt es die Plattform), dann Archicads eigene, dann die komprimierte Element-GUID.
    Antwortform von GetIFCIdsOfElements live geprüft (0.11.12 / AC29): je Eintrag ifcId + externalIFCId."""
    aus: list[str] = []
    for k in (extern, eigen, ab.ifc_guid(guid) if guid else None):
        if k and k not in aus:
            aus.append(k)
    return aus


def archicad_bauteile(seiten: Iterable[dict], *, geschoss_namen: Optional[dict] = None) -> dict:
    """Antwortseiten von GetDiffDataELM → {"elemente": [Abgleich-Bauteil], "kontext": {...}}.

    Abgleich-Bauteil = Format von ``elmonkey.abgleich`` (guid, ifcId, typ, name, geschoss, eigenschaften) plus
    ``klasse``, ``box``/``geometrie_quelle``, ``wirt``, ``archicad`` (Ebene, Element-ID, Aufbau, Oberflächen,
    Klassifizierung, Zone, Umbau-Status). ``geschoss_namen``: Archicad-Geschossname → Name in der Plattform."""
    geschoss_namen = geschoss_namen or {}
    elemente: list[dict] = []
    kontext: dict = {"seiten": 0, "uebersprungen": {}, "datenbank_umgeschaltet": False, "datenbank_zurueck": True}
    for seite in seiten:
        if seite.get("format") not in (None, FORMAT_ARCHICAD):
            raise ab.AbgleichFehler(f"Archicad-Daten: Format „{seite.get('format')}“ — erwartet {FORMAT_ARCHICAD}")
        kontext["seiten"] += 1
        k = seite.get("context") or {}
        for f in ("elmSabVersion", "archicadVersion", "cutPlanesActive"):
            if f in k:
                kontext[f] = k[f]
        kontext["datenbank_umgeschaltet"] |= bool(k.get("databaseSwitched"))
        for f, z in (("geometry3DModel", "geometrie_3d_modell"), ("geometryCalcBounds", "geometrie_roh"),
                     ("geometryNone", "geometrie_keine")):
            kontext[z] = kontext.get(z, 0) + int(k.get(f) or 0)
        for c in k.get("no3DModelCodes") or []:
            codes = kontext.setdefault("ohne_3d_modell_codes", {})
            codes[str(c.get("code"))] = codes.get(str(c.get("code")), 0) + int(c.get("count") or 0)
        if k.get("databaseSwitched") and not k.get("databaseRestored", True):
            kontext["datenbank_zurueck"] = False
        for f, n in (seite.get("skipped") or {}).items():
            kontext["uebersprungen"][f] = kontext["uebersprungen"].get(f, 0) + int(n or 0)
        for x in seite.get("elements") or []:
            elemente.append(_archicad_bauteil(x, geschoss_namen))
    return {"elemente": elemente, "kontext": kontext}


def _archicad_bauteil(x: dict, geschoss_namen: dict) -> dict:
    guid = str(((x.get("elementId") or {}).get("guid")) or "")
    eig: dict[str, Any] = {}
    for p in x.get("ifcProperties") or []:
        pset, name = str(p.get("pset") or "").strip(), str(p.get("name") or "").strip()
        if name:
            eig[f"{pset}.{name}" if pset else name] = p.get("value")
    attribute = {str(a.get("name")): a.get("value") for a in x.get("ifcAttributes") or []}
    for a, feld in ATTRIBUT_FELD.items():
        if a in attribute:
            eig.setdefault(feld, attribute[a])
    if x.get("ifcType"):
        eig.setdefault(F_KLASSE, x["ifcType"])
    geschoss = x.get("storyName") or None
    if geschoss is not None:
        eig.setdefault(F_GESCHOSS, geschoss_namen.get(geschoss, geschoss))
    status = STATUS_ARCHICAD.get(str(x.get("renovationStatus") or ""))
    if status:
        eig.setdefault(F_STATUS, status)
    geo = x.get("geometry") or {}
    quelle = geo.get("source") or "none"
    box = _box(geo.get("box"))
    extern, eigen = _ifc22(x.get("externalIfcGlobalId")), _ifc22(x.get("ifcGlobalId")) or ab.ifc_guid(guid)
    return {"guid": guid, "ifcId": extern or eigen, "ifcIds": ifc_kennungen(extern, eigen, guid),
            "typ": x.get("type"), "klasse": x.get("ifcType"),
            "name": attribute.get("Name") or x.get("elementIdText") or None,
            "geschoss": geschoss_namen.get(geschoss, geschoss) if geschoss else None,
            "ebene": x.get("layer"), "eigenschaften": eig,
            "box": box if quelle == "3dModel" else None, "box_roh": box if quelle == "calcBounds" else None,
            "geometrie_quelle": quelle if box else "none",
            "wirt": str(((x.get("hostElementId") or {}).get("guid")) or "") or None,
            "archicad": {"elementId": x.get("elementIdText"), "ebene": x.get("layer"),
                         "ebene_ausgeblendet": bool(x.get("layerHidden")), "umbau": x.get("renovationStatus"),
                         "aufbau": x.get("structure"), "oberflaechen": x.get("surfaces") or [],
                         "klassifizierung": x.get("classifications") or [], "zone": x.get("zone"),
                         "koerper": geo.get("bodies")}}


# ─────────────────────────────────────────────────────────── Plattform-Seite

def plattform_box(e: dict) -> Optional[list[float]]:
    """Box eines Plattform-Bauteils, wenn der Stand eine mitbringt (``bbox``/``box``/``geometrie.box``)."""
    for v in (e.get("bbox"), e.get("box"), (e.get("geometrie") or {}).get("box") if isinstance(e.get("geometrie"), dict)
              else None):
        b = _box(v)
        if b:
            return b
    return None


def _teil_von(e: dict) -> Optional[str]:
    for v in (e.get("teil_von"), e.get("parent"), (e.get("eigenschaften") or {}).get("IFC.Teil von")):
        if v:
            return str(v)
    return None


def _kl(e: dict) -> str:
    return klasse_norm(e.get("klasse") or (e.get("eigenschaften") or {}).get(F_KLASSE) or "")


def sortieren(elemente: list[dict]) -> dict[str, list[dict]]:
    """Nach Rolle trennen: bauteile, zonen, oeffnungen, teile, sonstige (keine Bauteile)."""
    aus: dict[str, list[dict]] = {"bauteile": [], "zonen": [], "oeffnungen": [], "teile": [], "sonstige": []}
    for e in elemente:
        k = _kl(e)
        if k == ZONE or e.get("typ") == "Zone":
            aus["zonen"].append(e)
        elif k == OEFFNUNG:
            aus["oeffnungen"].append(e)
        elif k == TEIL:
            aus["teile"].append(e)
        elif k in KEINE_BAUTEILE or (k and not k.startswith("ifc")):
            aus["sonstige"].append(e)
        else:
            aus["bauteile"].append(e)                       # jede andere Klasse zählt (keine feste Liste)
    return aus


def teile_zusammenfassen(bauteile: list[dict], teile: list[dict], geometrie: dict[str, list[float]]) -> dict:
    """Box der Teile in ihr Bauteil, wenn das Bauteil selbst keine Box hat. Teile = IfcBuildingElementPart und
    jedes Element mit ``teil_von`` (Plattform-Gruppen: Kopf ohne eigenes Netz, ``bbox_fehlt`` „Gruppe“). Seit der
    Plattform vom 16.11.2026 sind Schichten aus IFC-Importen schon beim Import in ihr Bauteil eingerechnet —
    dann kommt die Box direkt aus dem Stand und hier passiert nichts.
    → {"zugeordnet": n, "ohne_bauteil": [guid]}; ``geometrie`` (Plattform-GUID → Box) wird ergänzt."""
    je_guid = {ab.guid_norm(b.get("guid")): b for b in bauteile}
    je_ifc = {str((b.get("eigenschaften") or {}).get(F_GLOBALID) or ""): b for b in bauteile}
    zusammen: dict[str, list[float]] = {}
    ohne: list[str] = []
    n = 0
    for t in teile:
        vg = _teil_von(t)
        b = je_guid.get(ab.guid_norm(vg)) if vg else None
        b = b or (je_ifc.get(vg) if vg else None)
        if b is None:
            ohne.append(str(t.get("guid")))
            continue
        n += 1
        tb = plattform_box(t) or geometrie.get(str(t.get("guid")))
        if tb:
            g = str(b.get("guid"))
            alt = zusammen.get(g)
            zusammen[g] = tb if alt is None else [min(alt[0], tb[0]), max(alt[1], tb[1]), min(alt[2], tb[2]),
                                                  max(alt[3], tb[3]), min(alt[4], tb[4]), max(alt[5], tb[5])]
    for g, box in zusammen.items():
        geometrie.setdefault(g, box)
    return {"zugeordnet": n, "ohne_bauteil": ohne}


# ─────────────────────────────────────────────────────────── Geometrie

def seiten_delta(ac: list[float], pf: list[float], toleranz: float = TOLERANZ) -> dict[str, float]:
    """Je Seite (x−, x+, y−, y+, z−, z+) Archicad minus Plattform, nur was über der Toleranz liegt."""
    return {s: round(a - p, 4) for s, a, p in zip(SEITEN, ac, pf) if abs(a - p) > toleranz}


def _versetzt(b: list[float], versatz: tuple[float, float, float]) -> list[float]:
    dx, dy, dz = versatz
    return [b[0] + dx, b[1] + dx, b[2] + dy, b[3] + dy, b[4] + dz, b[5] + dz]


def _ueberlappt(a: list[float], b: list[float], rand: float) -> bool:
    return all(a[2 * i] - rand <= b[2 * i + 1] and b[2 * i] - rand <= a[2 * i + 1] for i in range(3))


def _mitte_nah(a: list[float], b: list[float], rand: float) -> bool:
    return all(abs((a[2 * i] + a[2 * i + 1]) - (b[2 * i] + b[2 * i + 1])) / 2 <= rand for i in range(3))


# ─────────────────────────────────────────────────────────── Öffnungen

def oeffnungen_abgleichen(ac_oeff: list[dict], pf_oeff: list[dict], ac_tuer_fenster: list[dict],
                          pf_geo: dict[str, list[float]], basis_paare: dict) -> dict:
    """IfcOpeningElement: GUIDs sind je Export neu. Reihenfolge: (1) gleiche GUID/GlobalId/Basis-Paar,
    (2) Lage gleich (Mitte ±5 cm) wie eine Archicad-Öffnung → Paar, (3) Lage an einer Tür/einem Fenster →
    gehört zu ihr/ihm (der Vergleich läuft über die Tür/das Fenster). Was bleibt, ist „ohne Partner", nie „weg"."""
    ac_je = {}
    for a in ac_oeff:
        for k in (ab.guid_norm(a.get("guid")), a.get("ifcId")):
            if k:
                ac_je.setdefault(str(k), a)
    vergeben: set[str] = set()
    paare, tuer_fenster, ohne = [], [], []
    alt = {ab.guid_norm(p): ab.guid_norm(a) for p, a in (basis_paare or {}).items()}
    for p in pf_oeff:
        pg = str(p.get("guid"))
        gid = str((p.get("eigenschaften") or {}).get(F_GLOBALID) or "")
        a = next((ac_je.get(k) for k in (alt.get(ab.guid_norm(pg), ""), ab.guid_norm(pg), pg, gid)
                  if k and ac_je.get(k) is not None and ab.guid_norm(ac_je[k].get("guid")) not in vergeben), None)
        pb = plattform_box(p) or pf_geo.get(pg)
        if a is None and pb:
            a = next((x for x in ac_oeff if ab.guid_norm(x.get("guid")) not in vergeben
                      and (x.get("box") or x.get("box_roh")) and _mitte_nah(pb, x.get("box") or x.get("box_roh"), 0.05)),
                     None)
        if a is not None:
            vergeben.add(ab.guid_norm(a.get("guid")))
            paare.append((p, a))
            continue
        tf = next((x for x in ac_tuer_fenster if pb and (x.get("box") or x.get("box_roh"))
                   and _ueberlappt(pb, x.get("box") or x.get("box_roh"), 0.05)), None) if pb else None
        if tf is not None:
            tuer_fenster.append({"plattform": pg, "archicad": tf.get("guid")})
        else:
            ohne.append(pg)
    nur_ac = [a for a in ac_oeff if ab.guid_norm(a.get("guid")) not in vergeben]
    return {"paare": paare, "tuer_fenster": tuer_fenster, "ohne_partner": ohne, "nur_archicad": nur_ac}


# ─────────────────────────────────────────────────────────── Vergleich

def _ohne_mengen(eig: dict) -> dict:
    return {k: v for k, v in (eig or {}).items() if not ist_menge(k)}


def _angleichen(p: dict, a: dict) -> dict:
    """Archicad-Bauteil mit den Plattform-Schreibweisen: Felder, die nach Normalisierung gleich sind, bekommen den
    Plattform-Wert (sonst sähe ``elmonkey.abgleich`` „.T." ≠ True). Echte Unterschiede behalten den Archicad-Wert."""
    pe, ae = p.get("eigenschaften") or {}, dict(a.get("eigenschaften") or {})
    for f in set(pe) & set(ae):
        if ae[f] != pe[f] and gleich(ae[f], pe[f], f):
            ae[f] = pe[f]
    return {**a, "eigenschaften": ae}


def stempel_eintragen(archicad: dict, stempel: dict[str, dict]) -> int:
    """{Archicad-GUID: {„Gruppe.Plattform-GUID“: wert}} in die Bauteile von :func:`archicad_bauteile` eintragen
    (die Eigenschaft gibt der IFC-Übersetzer nicht immer aus). → Anzahl ergänzter Bauteile."""
    je = {ab.guid_norm(e.get("guid")): e for e in archicad.get("elemente") or []}
    n = 0
    for g, eig in (stempel or {}).items():
        e = je.get(ab.guid_norm(g))
        if e is not None and eig:
            e.setdefault("eigenschaften", {}).update(eig)
            n += 1
    return n


def stempel_paare(archicad: list[dict], basis_paare: dict) -> list[dict]:
    """Archicad-Bauteile mit Stempel „Plattform-GUID“, die noch nicht als Paar in der Basis stehen →
    [{plattformGuid, archicadGuid}] für ``archicad_abgleich_melden`` (gebaut)."""
    bekannt = {ab.guid_norm(p): ab.guid_norm(a) for p, a in (basis_paare or {}).items()}
    aus = []
    for a in archicad:
        st = ab.plattform_guid_von(a)
        if st and bekannt.get(st) != ab.guid_norm(a.get("guid")):
            aus.append({"plattformGuid": st, "archicadGuid": a.get("guid")})
    return aus


def geometrie_richtung(ac_box: list[float], ref: list[float], quelle: str, basis_box: Optional[list[float]],
                       toleranz: float = TOLERANZ) -> str:
    """Welche Seite hat die Geometrie seit der Basis (Archicad-Box des letzten gemerkten Laufs) geändert?

    ``quelle == "basis"``: die Referenz IST die alte Archicad-Box → Archicad hat geändert. Sonst gegen die Basis:
    nur Archicad weg → „archicad→plattform“, nur Plattform weg → „plattform→archicad“, beide → „beide“, ohne Basis
    oder keine Seite über der Toleranz → „unklar“. Die Netz-Übergabe (elmonkey.netze) nimmt nur „archicad→plattform“."""
    if quelle == "basis":
        return "archicad→plattform"
    if not basis_box:
        return "unklar"
    ac, pf = bool(seiten_delta(ac_box, basis_box, toleranz)), bool(seiten_delta(ref, basis_box, toleranz))
    return {(True, False): "archicad→plattform", (False, True): "plattform→archicad",
            (True, True): "beide"}.get((ac, pf), "unklar")


def vergleichen(archicad: dict, stand: dict, *, gebaut: Optional[list[dict]] = None,
                plattform_geometrie: Optional[dict] = None, basis: Optional[dict] = None,
                antworten: Optional[dict] = None, toleranz: float = TOLERANZ,
                versatz: tuple[float, float, float] = (0.0, 0.0, 0.0)) -> dict:
    """Der ganze Diff. ``archicad`` = Ergebnis von :func:`archicad_bauteile`; ``stand`` = stand.json.

    ``plattform_geometrie``: Plattform-GUID → Box (falls der Stand keine Boxen trägt, z. B. aus dem IFC der
    Plattform). ``basis``: ersetzt ``stand["basis"]``. ``versatz``: wird auf die Archicad-Boxen addiert.
    → ergebnis.json (Format sab-archicad-abgleich/1 + ``diff``)."""
    if stand.get("format") not in (None, FORMAT):
        raise ab.AbgleichFehler(f"stand.json: Format „{stand.get('format')}“ — erwartet {FORMAT}")
    basis = dict(basis if basis is not None else (stand.get("basis") or {}))
    paare_basis = dict(basis.get("paare") or {})
    for x in gebaut or []:
        if x.get("plattformGuid") and x.get("archicadGuid"):
            paare_basis[str(x["plattformGuid"])] = str(x["archicadGuid"])
    hinweise: list[str] = []

    ac_alle = archicad.get("elemente") or []
    ac = sortieren(ac_alle)
    pf = sortieren(stand.get("elemente") or [])
    pf_geo: dict[str, list[float]] = {str(g): b for g, b in ((plattform_geometrie or {}).items()) if _box(b)}
    for e in stand.get("elemente") or []:
        b = plattform_box(e)
        if b:
            pf_geo.setdefault(str(e.get("guid")), b)
    gruppen_teile = [e for e in pf["bauteile"] if _teil_von(e)]       # bleiben Bauteile, geben nur ihre Box ab
    teile = teile_zusammenfassen(pf["bauteile"], pf["teile"] + gruppen_teile, pf_geo)

    # Paare über die IFC-GlobalId der Plattform (Bauteile kamen per IFC aus Archicad)
    # Paare über die IFC-Kennung (Bauteile kamen per IFC aus Archicad): je Archicad-Bauteil zuerst die externe
    # GlobalId, dann die eigene — eine externe Kennung schlägt die eigene eines anderen Bauteils.
    ac_je_ifc: dict[str, dict] = {}
    for rang in range(3):
        for a in ac["bauteile"]:
            ids = a.get("ifcIds") or [a.get("ifcId")]
            if rang < len(ids) and ids[rang]:
                ac_je_ifc.setdefault(str(ids[rang]), a)
    vergeben_ifc = {ab.guid_norm(a) for a in paare_basis.values()}
    for p in pf["bauteile"]:
        if str(p.get("guid")) in paare_basis:
            continue
        for k in (str(p.get("guid") or ""), str((p.get("eigenschaften") or {}).get(F_GLOBALID) or "")):
            a = ac_je_ifc.get(k) if k else None
            if a is not None and ab.guid_norm(a.get("guid")) not in vergeben_ifc:
                paare_basis[str(p.get("guid"))] = a.get("guid")
                vergeben_ifc.add(ab.guid_norm(a.get("guid")))
                break
    neu_gestempelt = stempel_paare(ac["bauteile"], paare_basis)
    for x in neu_gestempelt:
        paare_basis.setdefault(x["plattformGuid"], x["archicadGuid"])

    # Werte: Mengen raus (beide Seiten + Basis), Schreibweisen angleichen
    pf_b = [{**p, "eigenschaften": _ohne_mengen(p.get("eigenschaften"))} for p in pf["bauteile"]]
    ac_b = [{**a, "eigenschaften": _ohne_mengen(a.get("eigenschaften"))} for a in ac["bauteile"]]
    werte = {g: _ohne_mengen(w) for g, w in (basis.get("werte") or {}).items() if isinstance(w, dict)}
    basis_neu = {**basis, "paare": paare_basis, "werte": werte}
    z = ab.paaren(ac_b, pf_b, basis_neu)
    angeglichen = {ab.guid_norm(a.get("guid")): _angleichen(p, a) for p, a in z["paare"]}
    ac_b = [angeglichen.get(ab.guid_norm(a.get("guid")), a) for a in ac_b]
    # Basiswerte in Plattform-Schreibweise, damit Drei-Wege nicht an „.T.“ ↔ True scheitert
    for p, a in z["paare"]:
        bw = werte.get(ab.guid_norm(p.get("guid"))) or werte.get(str(p.get("guid")))
        if bw:
            for f in list(bw):
                pv = (p.get("eigenschaften") or {}).get(f, ab._FEHLT)
                if pv is not ab._FEHLT and bw[f] != pv and gleich(bw[f], pv, f):
                    bw[f] = pv

    plan = ab.planen(ac_b, pf_b, basis_neu)
    plan["lauf"] = stand.get("lauf")
    erg = ab.entscheiden(plan, antworten or {})
    erg["lauf"] = stand.get("lauf")

    # Geometrie je Paar
    ac_je_guid = {ab.guid_norm(a.get("guid")): a for a in ac["bauteile"]}
    geo_basis = {str(g): b for g, b in (basis.get("geometrie") or {}).items() if _box(b)}
    pf_je_guid = {ab.guid_norm(p.get("guid")): p for p in pf["bauteile"]}
    geometrie, ungeprueft = [], []
    for pg, ag in plan["paare"].items():
        a = ac_je_guid.get(ab.guid_norm(ag))
        if a is None:
            continue
        if not a.get("box"):
            grund = ("Archicad: nur rohe Box (kein 3D-Modell, z. B. Ebene ausgeblendet)" if a.get("box_roh")
                     else "Archicad: keine Geometrie")
            ungeprueft.append({"plattform": pg, "archicad": ag, "grund": grund})
            continue
        ab_box = _versetzt(a["box"], versatz)
        ref, quelle = pf_geo.get(str(pg)), "plattform"
        if ref is None and str(pg) in geo_basis:
            ref, quelle = geo_basis[str(pg)], "basis"
        if ref is None:
            p_e = pf_je_guid.get(ab.guid_norm(pg)) or {}
            grund = p_e.get("bbox_fehlt") or "keine Geometrie im Stand (Plattform vor 16.11.2026?)"
            ungeprueft.append({"plattform": pg, "archicad": ag, "grund": f"Plattform: {grund}"})
            continue
        d = seiten_delta(ab_box, ref, toleranz)
        if d:
            geometrie.append({"plattform": pg, "archicad": ag, "seiten": d, "vergleich_mit": quelle,
                              "richtung": geometrie_richtung(ab_box, ref, quelle, geo_basis.get(str(pg)), toleranz),
                              "archicad_box": [round(x, 4) for x in ab_box], "plattform_box": ref,
                              "bauteil": ab._kurz(a)})

    # Öffnungen
    tf = [a for a in ac["bauteile"] if _kl(a) in TUER_FENSTER]
    oe = oeffnungen_abgleichen(ac["oeffnungen"], pf["oeffnungen"], tf, pf_geo, paare_basis)
    for p, a in oe["paare"]:
        erg["basis"]["paare"][p.get("guid")] = a.get("guid")
    for a in oe["nur_archicad"]:
        erg["plattform"]["neu_importieren"].append(a.get("guid"))

    # Zonen: getrennt, nie neu/weg
    pf_z = {ab.guid_norm(p.get("guid")) for p in pf["zonen"]} | {
        str((p.get("eigenschaften") or {}).get(F_GLOBALID) or "") for p in pf["zonen"]}
    zonen = {"archicad": len(ac["zonen"]), "plattform": len(pf["zonen"]),
             "nur_archicad": [{"archicad": a.get("guid"), "name": ((a.get("archicad") or {}).get("zone") or {}).get("name")
                               or a.get("name"), "nummer": ((a.get("archicad") or {}).get("zone") or {}).get("number"),
                               "geschoss": a.get("geschoss")}
                              for a in ac["zonen"]
                              if ab.guid_norm(a.get("guid")) not in pf_z and str(a.get("ifcId")) not in pf_z]}

    # weg / neu als Liste mit Bauteilangabe
    pf_je = {ab.guid_norm(p.get("guid")): p for p in pf["bauteile"]}
    weg, nur_plattform = [], []
    for f in plan["fragen"]:
        if f["art"] != "fehlt_in_archicad":
            continue
        for b in f["bauteile"]:
            p = pf_je.get(ab.guid_norm(b.get("plattform"))) or {}
            weg.append({"plattform": b.get("plattform"), "bauteil": ab._kurz(p),
                        "umbau": (p.get("eigenschaften") or {}).get(F_STATUS)})
    for x in plan["nach_archicad"]["neu_bauen"]:
        nur_plattform.append({"plattform": x["plattform"], "bauteil": x["bauteil"]})
    neu = [{"archicad": x["archicad"], "bauteil": x["bauteil"]} for x in plan["nach_plattform"]["neu_importieren"]]
    neu += [{"archicad": a.get("guid"), "bauteil": ab._kurz(a), "oeffnung": True} for a in oe["nur_archicad"]]

    abbruch_weg = [w for w in weg if status_norm(w.get("umbau")) == "abbruch"]
    if abbruch_weg:
        hinweise.append(f"{len(abbruch_weg)} Abbruch-Bauteile der Plattform fehlen in Archicad. Der Lesebefehl liest "
                        "ohne Umbau-Filter — prüfen, ob sie wirklich gelöscht sind (oder in einem anderen Teilmodell liegen).")
    if not archicad.get("kontext", {}).get("datenbank_zurueck", True):
        hinweise.append("Archicad: die aktuelle Datenbank konnte nach dem Lesen nicht zurückgestellt werden.")
    if neu_gestempelt:
        hinweise.append(f"{len(neu_gestempelt)} Archicad-Bauteile tragen den Stempel „Plattform-GUID“ und sind noch "
                        "kein gemerktes Paar — mit archicad_abgleich_melden als gebaut melden.")
    hinweise += plan.get("hinweise") or []                         # z. B. „Plattform-GUID“ an mehreren Bauteilen
    if ungeprueft:
        hinweise.append(f"Geometrie von {len(ungeprueft)} Paaren nicht geprüft (Gründe in diff.geometrie_ungeprueft).")
    k = archicad.get("kontext") or {}
    if k.get("geometrie_roh", 0) + k.get("geometrie_keine", 0) and not k.get("geometrie_3d_modell", 0):
        hinweise.append("Archicad hat für kein Element einen 3D-Körper geliefert — 3D-Fenster einmal öffnen (alle "
                        "Ebenen, Umbau-Filter „alles zeigen“) und erneut lesen.")

    eigenschaften = []
    for x in erg["plattform"]["eigenschaften"]:
        eigenschaften.append({**x, "richtung": "archicad→plattform"})
    for x in erg["archicad"]["eigenschaften"]:
        eigenschaften.append({**x, "richtung": "plattform→archicad"})
    for f in plan["fragen"]:
        if f["art"] == "eigenschaft":
            for b in f["bauteile"]:
                eigenschaften.append({"plattform": b.get("plattform"), "archicad": b.get("archicad"), "feld": f["feld"],
                                      "archicad_wert": f["archicad"], "plattform_wert": f["plattform"],
                                      "richtung": "unklar", "frage": f["id"]})

    erg["basis"]["geometrie"] = {pg: [round(x, 4) for x in _versetzt(ac_je_guid[ab.guid_norm(ag)]["box"], versatz)]
                                 for pg, ag in erg["basis"]["paare"].items()
                                 if ab.guid_norm(ag) in ac_je_guid and ac_je_guid[ab.guid_norm(ag)].get("box")}
    erg["diff"] = {
        "toleranz_m": toleranz,
        "neu": neu, "weg": weg, "nur_plattform": nur_plattform,
        "eigenschaften": eigenschaften, "geometrie": geometrie, "geometrie_ungeprueft": ungeprueft,
        "zonen": zonen,
        "oeffnungen": {"paare": len(oe["paare"]), "an_tuer_fenster": oe["tuer_fenster"],
                       "ohne_partner": oe["ohne_partner"], "nur_archicad": [a.get("guid") for a in oe["nur_archicad"]]},
        "bauteilteile": {"plattform": len(pf["teile"]), **teile},
        "nicht_verglichen": {"archicad": len(ac["sonstige"]), "plattform": len(pf["sonstige"])},
        "gebaut_vorschlag": neu_gestempelt,
        "ausgeblendet_in_plattform": [b for f in plan["fragen"] if f["art"] == "in_plattform_ausgeblendet"
                                      for b in f["bauteile"]],
        "plattform_geometrie_kopf": stand.get("geometrie") if isinstance(stand.get("geometrie"), dict) else None,
        "kontext": archicad.get("kontext") or {},
        "hinweise": hinweise,
    }
    erg["diff"]["zahlen"] = zusammenfassung(erg)
    return erg


def zusammenfassung(erg: dict) -> dict:
    d = erg.get("diff") or {}
    return {"neu": len(d.get("neu") or []), "weg": len(d.get("weg") or []),
            "nur_plattform": len(d.get("nur_plattform") or []),
            "eigenschaften": len(d.get("eigenschaften") or []),
            "eigenschaften_bauteile": len({(x.get("plattform"), x.get("archicad")) for x in d.get("eigenschaften") or []}),
            "geometrie": len(d.get("geometrie") or []),
            "geometrie_ungeprueft": len(d.get("geometrie_ungeprueft") or []),
            "zonen_nur_archicad": len((d.get("zonen") or {}).get("nur_archicad") or []),
            "oeffnungen_an_tuer_fenster": len((d.get("oeffnungen") or {}).get("an_tuer_fenster") or []),
            "offen": sum(o.get("anzahl", 0) for o in erg.get("offen") or [])}


# ─────────────────────────────────────────────────────────── Plattform-IFC (Rückfall für Geometrie)

def boxen_aus_ifc(text: str) -> dict[str, list[float]]:
    """Boxen je GlobalId aus einem IFC-Text (STEP), nur Standardbibliothek: Punkte aus IfcCartesianPointList3D unter
    der Darstellung (ohne Platzierung — passt für Dateien mit Weltkoordinaten, wie der Plattform-Export).
    Teile (IfcRelAggregates) werden in ihr Bauteil zusammengefasst, wenn es selbst keine Punkte hat."""
    e = {int(m.group(1)): (m.group(2), m.group(3))
         for m in re.finditer(r"^#(\d+)\s*=\s*([A-Z0-9]+)\((.*)\);\s*$", text, re.M)}

    def refs(s: str) -> list[int]:
        return [int(x) for x in re.findall(r"#(\d+)", s)]

    boxen: dict[str, list[float]] = {}
    gid_von: dict[int, str] = {}
    for k, (t, a) in e.items():
        m = re.match(r"'([0-9A-Za-z_$]{22})',", a)
        if not m:
            continue
        gid_von[k] = m.group(1)
        r = refs(a)
        form = next((x for x in reversed(r) if e.get(x, ("",))[0] == "IFCPRODUCTDEFINITIONSHAPE"), None)
        if form is None:
            continue
        pts: list[tuple[float, ...]] = []
        stapel, gesehen = refs(e[form][1]), set()
        while stapel:
            x = stapel.pop()
            if x in gesehen or x not in e:
                continue
            gesehen.add(x)
            tt, aa = e[x]
            if tt == "IFCCARTESIANPOINTLIST3D":
                pts += [tuple(float(c) for c in p.split(",")) for p in re.findall(r"\(([-\d.E+,]+)\)", aa)]
            elif tt in ("IFCSHAPEREPRESENTATION", "IFCTRIANGULATEDFACESET", "IFCPOLYGONALFACESET", "IFCMAPPEDITEM",
                        "IFCREPRESENTATIONMAP"):
                stapel += refs(aa)
        pts = [p for p in pts if len(p) == 3]
        if pts:
            xs, ys, zs = zip(*pts)
            boxen[m.group(1)] = [min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)]
    for k, (t, a) in e.items():
        if t != "IFCRELAGGREGATES":
            continue
        m = re.search(r",\s*#(\d+)\s*,\s*\(([^()]*)\)\s*$", a)   # …, RelatingObject, (RelatedObjects)
        if not m:
            continue
        ganz = gid_von.get(int(m.group(1)))
        if not ganz or ganz in boxen:
            continue
        teile = [boxen[gid_von[x]] for x in refs(m.group(2)) if gid_von.get(x) in boxen]
        if teile:
            boxen[ganz] = [min(b[0] for b in teile), max(b[1] for b in teile), min(b[2] for b in teile),
                           max(b[3] for b in teile), min(b[4] for b in teile), max(b[5] for b in teile)]
    return boxen
