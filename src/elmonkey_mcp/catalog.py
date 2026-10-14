"""Befehlskatalog aus dem geladenen Add-On (ELM_SAB.ListCommands) + Stichwortsuche."""
from __future__ import annotations

import json
import re
import time

from .archicad import ArchicadError, call

# Offizielle Graphisoft-JSON-API: kleine, geprueft genutzte Auswahl (Aufruf geht auch fuer andere API.*-Namen).
OFFICIAL = [
    ("IsAlive", "Prueft, ob Archicad auf dem Port antwortet."),
    ("GetProductInfo", "Archicad-Version, Build, Sprache."),
    ("GetElementsByType", "Element-IDs eines Typs (Wall, Slab, Zone, Door ...). Hatch kennt nur TapirCommand.GetElementsByType."),
    ("GetAllElements", "Alle Element-IDs des Projekts."),
    ("GetPropertyValuesOfElements", "Eigenschaftswerte (properties) von Elementen lesen."),
    ("SetPropertyValuesOfElements", "Eigenschaftswerte schreiben; Wert in voller Form {type,status,value}."),
    ("GetAllPropertyIds", "Alle Eigenschafts-IDs (properties) des Projekts."),
    ("GetDetailsOfProperties", "Name, Gruppe, Typ von Eigenschaften."),
    ("GetPropertyIds", "Eigenschafts-IDs per Name (Gruppe + Name)."),
    ("GetAttributesByType", "Attribute (Layer/Ebene, Fill/Schraffur, Pen, LayerCombination ...) eines Typs."),
    ("GetLayerAttributes", "Ebenen-Attribute: Name, isHidden, isLocked."),
    ("GetFillAttributes", "Schraffur-/Fuellungs-Attribute."),
    ("Get3DBoundingBoxes", "3D-Begrenzungsquader von Elementen."),
    ("Get2DBoundingBoxes", "2D-Begrenzungsrechtecke von Elementen."),
    ("GetClassificationsOfElements", "Klassifizierung von Elementen lesen."),
    ("SetClassificationsOfElements", "Klassifizierung von Elementen setzen."),
    ("GetAllClassificationSystems", "Alle Klassifizierungssysteme."),
    ("GetNavigatorItemTree", "Navigator-Baum (Projektmappe, Ansichten, Layoutbuch)."),
]

SYNONYMS = {
    "wand": "wall", "waende": "wall", "wände": "wall", "decke": "slab", "decken": "slab", "dach": "roof",
    "stuetze": "column", "stütze": "column", "tuer": "door", "tür": "door", "tueren": "door", "türen": "door",
    "fenster": "window", "zone": "zone", "raum": "zone", "räume": "zone", "raeume": "zone", "objekt": "object",
    "ebene": "layer", "ebenen": "layer", "geschoss": "story", "geschosse": "story", "eigenschaft": "property",
    "eigenschaften": "property", "schraffur": "hatch", "fuellung": "fill", "füllung": "fill", "linie": "line",
    "linien": "line", "polylinie": "polyline", "bogen": "arc", "kreis": "circle", "text": "text", "etikett": "label",
    "bemassung": "dimension", "bemaßung": "dimension", "masskette": "dimension", "maßkette": "dimension",
    "klassifizierung": "classification", "auswahl": "selection", "markierung": "selection", "loeschen": "delete",
    "löschen": "delete", "erstellen": "create", "anlegen": "create", "zeichnen": "create", "verschieben": "move",
    "drehen": "rotate", "lesen": "get", "setzen": "set", "aendern": "set", "ändern": "set", "suchen": "find",
    "finden": "find", "stift": "pen", "stifte": "pen", "farbe": "pen", "bild": "capture", "screenshot": "capture",
    "ansicht": "view", "schnitt": "section", "innenansicht": "interior", "reservieren": "reserve", "senden": "send",
    "teamwork": "teamwork", "oeffnung": "opening", "öffnung": "opening", "durchbruch": "opening", "fassade": "curtain",
    "vorhangfassade": "curtain", "geometrie": "geometry", "version": "version", "projekt": "project", "gruppe": "group",
}

_cache: dict = {}


def _norm(t: str) -> str:
    if t.endswith("ies") and len(t) > 4:
        return t[:-3] + "y"
    if t.endswith("es") and len(t) > 4 and t[-3] in "sxz":
        return t[:-2]
    if t.endswith("s") and not t.endswith("ss") and len(t) > 3:
        return t[:-1]
    return t


def _tokens(text: str) -> list[str]:
    text = text or ""
    text = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", text)          # getStories -> get Stories, 2DElements -> 2D Elements
    text = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", text)       # DElements -> D Elements
    text = re.sub(r"([A-Za-z])([0-9])", r"\1 \2", text)          # Create2D -> Create 2D
    out = []
    for t in re.split(r"[^0-9A-Za-zÄÖÜäöüß]+", text.lower()):
        if len(t) < 2:
            continue
        t = SYNONYMS.get(t, t)
        out.append(_norm(t))
    return out


def load(port: int) -> list[dict]:
    """Katalog je Port cachen (5 min). Ohne ELMonkey >= 0.9.30 nur die offizielle Auswahl."""
    hit = _cache.get(port)
    if hit and time.time() - hit[0] < 300:
        return hit[1]
    entries = [{"name": f"API.{n}", "namespace": "API", "command": n, "group": "Graphisoft JSON-API",
                "description": d, "version": "", "inputSchema": None} for n, d in OFFICIAL]
    note = None
    try:
        r = call(port, "ELM_SAB", "ListCommands", {"withSchemas": True}, timeout=30.0)
        for c in r.get("commands", []):
            schema = c.get("inputSchema")
            try:
                schema = json.loads(schema) if isinstance(schema, str) else schema
            except ValueError:
                pass
            entries.append({"name": f"{c['namespace']}.{c['name']}", "namespace": c["namespace"], "command": c["name"],
                            "group": c.get("group", ""), "description": c.get("description", ""),
                            "version": c.get("version", ""), "inputSchema": schema})
    except ArchicadError as e:
        note = (f"ELM_SAB.ListCommands nicht verfuegbar ({e}). ELMonkey >= 0.9.30 installieren "
                "(Installer: github.com/mozzi86/-archicad/releases/tag/elm-sab-tapir-latest).")
    _cache[port] = (time.time(), entries, note)
    return entries


def note_for(port: int) -> str | None:
    hit = _cache.get(port)
    return hit[2] if hit and len(hit) > 2 else None


def search(entries: list[dict], query: str, limit: int = 8) -> list[dict]:
    q = _tokens(query)
    if not q:
        return []
    scored = []
    for e in entries:
        name_t = set(_tokens(e["command"]))
        text_t = set(_tokens(e["description"] + " " + e["group"]))
        s = 0.0
        for t in q:
            if t in name_t:
                s += 3
            elif any(t in n or n in t for n in name_t if len(n) > 3 and len(t) > 3):
                s += 1.5
            if t in text_t:
                s += 1
        if e["namespace"] == "ELM_SAB":
            s *= 1.1  # SAB-Befehle haben Ruecklese - bei Gleichstand bevorzugen
        if s > 0:
            scored.append((s, e))
    scored.sort(key=lambda x: -x[0])
    return [e for _, e in scored[:limit]]


def resolve(entries: list[dict], name: str) -> tuple[str, str]:
    """'NS.Cmd' | 'Cmd' (eindeutig) | alter tapir-archicad-mcp-Name 'elements_get_details_of_elements'."""
    if "." in name:
        ns, cmd = name.split(".", 1)
        return ns, cmd
    by_lower: dict[str, list[dict]] = {}
    for e in entries:
        by_lower.setdefault(e["command"].lower(), []).append(e)
    cands = by_lower.get(name.lower(), [])
    if not cands and "_" in name:
        parts = name.lower().split("_")
        for i in range(len(parts)):
            key = "".join(parts[i:])
            if key in by_lower:
                cands = by_lower[key]
                break
    if not cands:
        raise ValueError(f"Befehl '{name}' unbekannt. Mit archicad_discover_tools suchen oder 'Namensraum.Befehl' angeben "
                         "(z. B. 'ELM_SAB.FindElements', 'TapirCommand.GetDetailsOfElements', 'API.GetElementsByType').")
    if len(cands) > 1:
        pref = [c for c in cands if c["namespace"] == "TapirCommand"]
        names = ", ".join(c["name"] for c in cands)
        raise ValueError(f"'{name}' ist mehrdeutig: {names}. Bitte mit Namensraum angeben."
                         + (f" (Vanilla-Tapir-Variante: {pref[0]['name']})" if pref else ""))
    return cands[0]["namespace"], cands[0]["command"]
