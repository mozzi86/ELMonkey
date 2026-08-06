#!/usr/bin/env python3
"""
arc — Direct HTTP CLI for Archicad's JSON API.

Umgeht MCP-Discovery und Pagination-Context-Explosion. Für schnelle Read-only-
Queries und für Bulk-Operationen (>100 Elemente), wo die MCP-Pagination den
Claude-Context sprengt. Siehe reference/mcp-conventions.md § "Direkter HTTP-Zugriff".

Verwendung:
  arc ports                          # aktive Archicad-Instanzen (19723..19730)
  arc call <Cmd> [json]              # offizieller API.<Cmd>
  arc tapir <TapirCmd> [json]        # Tapir-Add-On-Command via API.ExecuteAddOnCommand
  arc selected                       # aktuelle Selektion (GetSelectedElements)
  arc zones                          # alle Zonen (Kurzform von tapir GetElementsByType)
  arc layers [regex]                 # Layer, optional per Regex gefiltert
  arc info                           # Projekt-Info + aktive Story + Einheit
  arc doctor                         # Self-Check (Ports, Tapir-Verfügbarkeit)
  arc launch [datei.pln]             # Archicad kontrolliert starten + auf JSON-API warten
  arc view-elements                  # Elemente im aktiven Fenster (2D: VisLayer+ActFloor, 3D: In3D)
  arc splash <Property> [--clear]    # Elemente nach Property-Wert einfärben + Legende

Port-Auswahl:
  --port N        Expliziter Port (Default: 19723, dann Auto-Scan bei Fehler)
  --project TEXT  Match per Projektname (Substring, case-insensitive)

Format:
  --json          Rohe JSON-Response ausgeben (Default: kompakte Zusammenfassung)

Name-Historie: früher `ac`, umbenannt zu `arc` wegen Kollision mit macOS-
System-Binary /usr/sbin/ac (BSD process accounting).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.error
import urllib.request
from typing import Any

DEFAULT_PORT = 19723
SCAN_PORTS = range(19723, 19731)  # 19723..19730
HTTP_TIMEOUT = 10  # Sekunden für kleine Queries; Bulk kann länger dauern


def post(port: int, command: str, parameters: dict | None = None, *, timeout: int = HTTP_TIMEOUT) -> dict:
    body = json.dumps({"command": command, "parameters": parameters or {}}).encode("utf-8")
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}/",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def tapir(port: int, command: str, parameters: dict | None = None, *, timeout: int = HTTP_TIMEOUT) -> dict:
    envelope = {
        "addOnCommandId": {"commandNamespace": "TapirCommand", "commandName": command},
        "addOnCommandParameters": parameters or {},
    }
    resp = post(port, "API.ExecuteAddOnCommand", envelope, timeout=timeout)
    return resp.get("result", {}).get("addOnCommandResponse", resp)


def scan_ports() -> list[dict]:
    """Return list of {port, projectName, projectPath} for reachable Archicads."""
    alive = []
    for p in SCAN_PORTS:
        try:
            info = post(p, "API.IsAlive", timeout=1)
            if info.get("succeeded"):
                try:
                    project = tapir(p, "GetProjectInfo", timeout=5)
                    alive.append({
                        "port": p,
                        "projectName": project.get("projectName", "?"),
                        "projectPath": project.get("projectLocation", ""),
                        "isTeamwork": project.get("isTeamwork", False),
                    })
                except Exception:
                    alive.append({"port": p, "projectName": "?", "projectPath": "", "isTeamwork": False})
        except Exception:
            continue
    return alive


def resolve_port(explicit: int | None, project_match: str | None) -> int:
    """Pick a port. Priority: --port > --project match > single active > error."""
    if explicit is not None:
        return explicit
    ports = scan_ports()
    if not ports:
        die("Keine aktive Archicad-Instanz gefunden auf Ports 19723..19730. Läuft Archicad? Tapir Add-On aktiv?")
    if project_match:
        m = [p for p in ports if project_match.lower() in p["projectName"].lower()]
        if len(m) == 1:
            return m[0]["port"]
        if not m:
            die(f"Keine Instanz mit Projektname enthält '{project_match}'. Aktiv: {[p['projectName'] for p in ports]}")
        die(f"Mehrere Instanzen passen zu '{project_match}': {[(p['port'], p['projectName']) for p in m]}")
    if len(ports) == 1:
        return ports[0]["port"]
    die(f"Mehrere aktive Instanzen — bitte --port oder --project angeben. Aktiv: {[(p['port'], p['projectName']) for p in ports]}")


def die(msg: str, code: int = 1) -> None:
    print(f"arc: {msg}", file=sys.stderr)
    sys.exit(code)


def parse_json_arg(arg: str | None) -> dict:
    if not arg:
        return {}
    try:
        return json.loads(arg)
    except json.JSONDecodeError as e:
        die(f"Ungültiges JSON-Argument: {e}")


def print_result(data: Any, *, raw: bool) -> None:
    if raw or not isinstance(data, (dict, list)):
        print(json.dumps(data, indent=2, ensure_ascii=False))
        return
    print(json.dumps(data, indent=2, ensure_ascii=False))


# --- Subcommands -------------------------------------------------------------


def cmd_ports(args) -> None:
    ports = scan_ports()
    if not ports:
        print("Keine aktive Archicad-Instanz gefunden.")
        sys.exit(1)
    for p in ports:
        tw = " [TW]" if p["isTeamwork"] else ""
        print(f"{p['port']}  {p['projectName']}{tw}")


def cmd_call(args) -> None:
    port = resolve_port(args.port, args.project)
    params = parse_json_arg(args.params)
    result = post(port, args.cmd, params, timeout=args.timeout)
    print_result(result, raw=args.json)


def cmd_tapir(args) -> None:
    port = resolve_port(args.port, args.project)
    params = parse_json_arg(args.params)
    result = tapir(port, args.cmd, params, timeout=args.timeout)
    print_result(result, raw=args.json)


def cmd_selected(args) -> None:
    port = resolve_port(args.port, args.project)
    result = post(port, "API.GetSelectedElements")
    ids = [e["elementId"]["guid"] for e in result.get("elements", [])]
    if args.json:
        print(json.dumps(ids, indent=2))
    else:
        for i in ids:
            print(i)
        print(f"({len(ids)} selected)", file=sys.stderr)


def cmd_zones(args) -> None:
    port = resolve_port(args.port, args.project)
    result = tapir(port, "GetElementsByType", {"elementType": "Zone"})
    elements = result.get("elements", [])
    if args.json:
        print(json.dumps(elements, indent=2))
    else:
        for e in elements:
            print(e.get("elementId", {}).get("guid", "?"))
        print(f"({len(elements)} zones)", file=sys.stderr)


def cmd_layers(args) -> None:
    port = resolve_port(args.port, args.project)
    result = post(port, "API.GetAttributesByType", {"attributeType": "Layer"})
    layers = result.get("attributes", []) or result.get("layers", [])
    pattern = re.compile(args.regex, re.IGNORECASE) if args.regex else None
    for a in layers:
        name = a.get("name", "?")
        if pattern and not pattern.search(name):
            continue
        aid = a.get("attributeId", {}).get("guid", "?")
        idx = a.get("index", "")
        print(f"{aid}\t{idx}\t{name}")


def cmd_info(args) -> None:
    port = resolve_port(args.port, args.project)
    project = tapir(port, "GetProjectInfo")
    path = project.get("projectLocation", "") or ""
    # Teamwork-Pfade sind mit URL-encodetem Auth-Blob unlesbar — kürze auf
    # "@<server>/<project>" wenn --json nicht gesetzt ist.
    if not args.json and path.startswith("teamwork://") and "@" in path:
        try:
            server_and_proj = path.rsplit("@", 1)[-1]
            path = f"teamwork://…@{server_and_proj}"
        except Exception:
            pass
    print(f"Port:        {port}")
    print(f"Project:     {project.get('projectName', '?')}")
    print(f"Path:        {path}")
    print(f"Teamwork:    {project.get('isTeamwork', False)}")
    # Story: try Tapir GetStories, fall back gracefully
    try:
        stories_resp = tapir(port, "GetStories")
        stories = stories_resp.get("stories", [])
        current = next((s for s in stories if s.get("isCurrent")), None) or (stories[0] if stories else None)
        if current:
            idx = current.get("index", current.get("floorNumber", "?"))
            name = current.get("name", current.get("storyName", "?"))
            print(f"Story:       {name} (index {idx})  [{len(stories)} stories total]")
        else:
            print(f"Story:       — (Response: {json.dumps(stories_resp)[:200]})")
    except Exception as e:
        print(f"Story:       — ({e})")


def cmd_launch(args) -> None:
    """Archicad kontrolliert starten und warten, bis die JSON-API antwortet.

    Übernommen aus revit-mcp-python (launch_revit): Start + Readiness-Polling in
    einem Schritt. Umgeht nebenbei den Doppelklick-Start-Bug (taub bleibende
    Instanz), weil der Erfolg hier verifiziert statt angenommen wird.
    """
    import subprocess
    import time
    from pathlib import Path

    if sys.platform != "darwin":
        die("arc launch ist macOS-only (nutzt 'open -na').")

    project_file = None
    if args.file:
        project_file = Path(args.file).expanduser()
        if not project_file.exists():
            die(f"Projektdatei nicht gefunden: {project_file}")
        if project_file.suffix.lower() not in (".pln", ".pla"):
            die(f"Nur .pln/.pla starten sinnvoll — '{project_file.suffix}' nicht "
                "(.tpl kann die JSON-API nicht öffnen, Vorlagen öffnet der Nutzer selbst).")

    before = {p["port"] for p in scan_ports()}
    cmd = ["open", "-na", args.app]
    if project_file:
        cmd.append(str(project_file))
    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True)
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        stderr = getattr(e, "stderr", "") or ""
        die(f"Start fehlgeschlagen ({args.app}): {stderr.strip() or e}")

    print(f"{args.app} gestartet — warte auf JSON-API (max. {args.wait}s) …", file=sys.stderr)
    expected = project_file.stem.lower() if project_file else None
    deadline = time.time() + args.wait
    seen_new = None
    while time.time() < deadline:
        time.sleep(3)
        for p in scan_ports():
            if p["port"] in before:
                continue
            seen_new = p
            if expected is None:
                print(f"Port {p['port']}: bereit ({p['projectName']})")
                return
            if expected in p["projectName"].lower() or expected in (p["projectPath"] or "").lower():
                print(f"Port {p['port']}: {p['projectName']} geladen")
                return
    if seen_new:
        die(f"Instanz antwortet auf Port {seen_new['port']}, aber das Projekt '{expected}' ist "
            f"nach {args.wait}s nicht geladen (aktuell: {seen_new['projectName']}). Großes Projekt? "
            f"--wait erhöhen oder mit 'arc ports' weiterbeobachten.")
    die(f"Keine neue Instanz nach {args.wait}s erreichbar. Mögliche Ursachen: JSON-API in den "
        "Add-On-Einstellungen deaktiviert, Instanz außerhalb 19723..19730, oder der bekannte "
        "Start-Bug (Instanz dauerhaft taub) — dann Archicad beenden und erneut per 'arc launch' starten.")


def _current_view_filters(port: int) -> "tuple[list[str], str]":
    """Filter-Satz für „im aktiven Fenster sichtbar" je Fenstertyp."""
    win = tapir(port, "GetCurrentWindowType")
    wtype = win.get("currentWindowType", "?")
    if wtype == "3DModel":
        return ["In3D"], wtype
    if wtype == "FloorPlan":
        return ["OnVisLayer", "OnActFloor"], wtype
    # Schnitt/Ansicht/Arbeitsblatt & Co.: sichtbare Ebenen als bestmögliche Näherung
    return ["OnVisLayer"], wtype


def cmd_view_elements(args) -> None:
    """Kontext-Primitive aus revit-mcp-python (get_current_view_elements) —
    per Tapir-Kombi GetCurrentWindowType + GetAllElements(filters), kein C++ nötig."""
    port = resolve_port(args.port, args.project)
    filters, wtype = _current_view_filters(port)
    result = tapir(port, "GetAllElements", {"filters": filters})
    elements = result.get("elements", [])
    guids = [e.get("elementId", {}).get("guid", "?") for e in elements]
    if args.json:
        print(json.dumps({"windowType": wtype, "filters": filters, "guids": guids}, indent=2))
        return
    for g in guids:
        print(g)
    print(f"({len(guids)} Elemente im aktiven Fenster [{wtype}], Filter: {'+'.join(filters)})", file=sys.stderr)


# RGBA 0–255, angelehnt an Okabe-Ito (farbenblind-tauglich); letzter Wert = Sammelfarbe.
SPLASH_PALETTE = [
    [230, 159, 0, 255], [86, 180, 233, 255], [0, 158, 115, 255],
    [240, 228, 66, 255], [0, 114, 178, 255], [213, 94, 0, 255],
    [204, 121, 167, 255], [153, 153, 153, 255], [146, 0, 0, 255],
    [0, 68, 27, 255], [64, 0, 128, 255],
]
SPLASH_REST = [60, 60, 60, 255]


def cmd_splash(args) -> None:
    """color_splash aus revit-mcp-python: Elemente nach Property-Wert einfärben.

    Reine Bildschirm-Überlagerung (Tapir HighlightElements) — kein Projektinhalt,
    kein Teamwork-Sync. PFLICHT-CLEANUP danach: `arc splash --clear`, sonst sieht
    das Modell für den Nutzer nach „kaputten Farben" aus.
    """
    port = resolve_port(args.port, args.project)

    if args.clear:
        tapir(port, "HighlightElements", {"elements": [], "highlightedColors": []})
        print("Highlights entfernt.")
        return

    if not args.prop:
        die("Property-Name fehlt (oder --clear zum Aufräumen).")

    # 1) Property per Name auflösen (exakter Treffer vor Teilstring)
    props = tapir(port, "GetAllProperties").get("properties", [])
    needle = args.prop.lower()
    exact = [p for p in props if p.get("propertyName", "").lower() == needle]
    matches = exact or [p for p in props if needle in p.get("propertyName", "").lower()]
    if not matches:
        die(f"Keine Property passt zu '{args.prop}'. Tipp: arc tapir GetAllProperties | grep -i …")
    if len(matches) > 1:
        namelist = [f"{m.get('propertyGroupName','?')} / {m.get('propertyName','?')}" for m in matches[:10]]
        die(f"{len(matches)} Properties passen zu '{args.prop}' — bitte genauer: " + "; ".join(namelist))
    prop = matches[0]
    prop_label = f"{prop.get('propertyGroupName','?')} / {prop.get('propertyName','?')}"

    # 2) Elemente bestimmen: Selektion > Typ > aktives Fenster
    if args.selected:
        elements = post(port, "API.GetSelectedElements").get("elements", [])
        scope = "Selektion"
    elif args.eltype:
        elements = tapir(port, "GetElementsByType", {"elementType": args.eltype}).get("elements", [])
        scope = f"Typ {args.eltype}"
    else:
        filters, wtype = _current_view_filters(port)
        elements = tapir(port, "GetAllElements", {"filters": filters}).get("elements", [])
        scope = f"aktives Fenster [{wtype}]"
    if not elements:
        die(f"Keine Elemente gefunden ({scope}).")

    # 3) Werte lesen (lokalisierte Strings, siehe Memory Tapir-Property-Bulk-Read)
    values_resp = tapir(port, "GetPropertyValuesOfElements", {
        "elements": elements,
        "properties": [{"propertyId": prop["propertyId"]}],
    }, timeout=max(args.timeout, 60))
    per_element = values_resp.get("propertyValuesForElements", [])

    def value_of(item) -> str:
        try:
            return str(item["propertyValues"][0]["propertyValue"]["value"])
        except (KeyError, IndexError, TypeError):
            return "— (kein Wert)"

    values = [value_of(v) for v in per_element]
    if len(values) != len(elements):
        die(f"Antwortlänge passt nicht ({len(values)} Werte für {len(elements)} Elemente) — Abbruch statt falscher Färbung.")

    # 4) Werte → Farben (häufigste Werte zuerst; Überhang wird zusammengefasst)
    from collections import Counter
    counts = Counter(values)
    ranked = [v for v, _ in counts.most_common()]
    color_of = {}
    for i, v in enumerate(ranked):
        color_of[v] = SPLASH_PALETTE[i] if i < len(SPLASH_PALETTE) else SPLASH_REST
    highlighted = [color_of[v] for v in values]

    tapir(port, "HighlightElements", {
        "elements": elements,
        "highlightedColors": highlighted,
        "nonHighlightedColor": [180, 180, 180, 120],
    }, timeout=max(args.timeout, 60))

    # 5) Legende
    print(f"Splash: {prop_label} — {len(elements)} Elemente ({scope})")
    for i, v in enumerate(ranked):
        rgba = color_of[v]
        tag = "" if i < len(SPLASH_PALETTE) else "  [Sammelfarbe]"
        print(f"  RGB {rgba[0]:>3},{rgba[1]:>3},{rgba[2]:>3}  {counts[v]:>5}×  {v}{tag}")
    print("Aufräumen nicht vergessen: arc splash --clear", file=sys.stderr)


def cmd_doctor(args) -> None:
    print("== arc doctor ==")
    ports = scan_ports()
    if not ports:
        print("[FAIL] Keine aktive Archicad-Instanz auf 19723..19730")
        print("        - Läuft Archicad?")
        print("        - Ist das Tapir Add-On aktiv (Optionen → Add-On Manager)?")
        sys.exit(1)
    print(f"[OK]   {len(ports)} aktive Instanz(en):")
    for p in ports:
        tw = " [Teamwork]" if p["isTeamwork"] else ""
        print(f"        Port {p['port']}: {p['projectName']}{tw}")
    # Tapir-Check am ersten Port
    p = ports[0]["port"]
    try:
        tapir(p, "GetProjectInfo", timeout=8)
        print(f"[OK]   Tapir Add-On antwortet auf Port {p}")
    except Exception as e:
        print(f"[WARN] Tapir Add-On antwortete nicht in 8s auf Port {p}: {e}")
        print("        Modell möglicherweise beschäftigt — versuch's gleich nochmal, wenn Archicad idle ist.")
    # MCP registrierung (heuristisch)
    from pathlib import Path
    mcp_json = Path.home() / ".claude.json"
    if mcp_json.exists() and "archicad" in mcp_json.read_text():
        print("[OK]   MCP-Server 'archicad' in ~/.claude.json referenziert")
    else:
        print("[WARN] MCP-Server 'archicad' nicht in ~/.claude.json gefunden — regulärer Skill-Path funktioniert evtl. nicht")
    print("Fertig. Fast-Path via 'arc' einsatzbereit.")


# --- Main --------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="arc", description=__doc__.strip().split("\n", 1)[0])
    p.add_argument("--port", type=int, help="Expliziter Port")
    p.add_argument("--project", help="Match per Projektname (Substring)")
    p.add_argument("--json", action="store_true", help="Rohe JSON-Response")
    p.add_argument("--timeout", type=int, default=HTTP_TIMEOUT, help=f"HTTP-Timeout (Default {HTTP_TIMEOUT}s)")

    sub = p.add_subparsers(dest="sub", required=True)

    sub.add_parser("ports", help="Aktive Archicad-Instanzen").set_defaults(func=cmd_ports)

    call = sub.add_parser("call", help="Offizieller API.<Cmd>")
    call.add_argument("cmd", help="Command ohne 'API.'-Prefix, z.B. GetProjectInfo")
    call.add_argument("params", nargs="?", help="JSON-Params")
    call.set_defaults(func=cmd_call)

    tap = sub.add_parser("tapir", help="Tapir-Add-On-Command via API.ExecuteAddOnCommand")
    tap.add_argument("cmd", help="Tapir-Command, z.B. GetElementsByType")
    tap.add_argument("params", nargs="?", help="JSON-Params für addOnCommandParameters")
    tap.set_defaults(func=cmd_tapir)

    sub.add_parser("selected", help="GUIDs der aktuellen Selektion").set_defaults(func=cmd_selected)
    sub.add_parser("zones", help="Alle Zonen-GUIDs (Kurzform)").set_defaults(func=cmd_zones)

    lay = sub.add_parser("layers", help="Layer-Liste, optional Regex-gefiltert")
    lay.add_argument("regex", nargs="?", help="Filter (case-insensitive)")
    lay.set_defaults(func=cmd_layers)

    sub.add_parser("info", help="Projekt + Story + Teamwork-Info").set_defaults(func=cmd_info)
    sub.add_parser("doctor", help="Self-Check: Instanzen, Tapir, MCP-Config").set_defaults(func=cmd_doctor)

    la = sub.add_parser("launch", help="Archicad kontrolliert starten (optional mit Projekt) + auf JSON-API warten")
    la.add_argument("file", nargs="?", help="Projektdatei .pln/.pla")
    la.add_argument("--app", default="Archicad 29", help="App-Name (Default: Archicad 29)")
    la.add_argument("--wait", type=int, default=180, help="Wartezeit in Sekunden (Default 180)")
    la.set_defaults(func=cmd_launch)

    ve = sub.add_parser("view-elements", help="Alle Elemente im aktiven Fenster (2D: VisLayer+ActFloor, 3D: In3D)")
    ve.set_defaults(func=cmd_view_elements)

    sp = sub.add_parser("splash", help="Elemente nach Property-Wert einfärben (Highlight-Overlay) + Legende")
    sp.add_argument("prop", nargs="?", help="Property-Name (exakt oder Teilstring, case-insensitive)")
    sp.add_argument("--type", dest="eltype", help="Elementtyp statt aktivem Fenster (z.B. Wall, Zone)")
    sp.add_argument("--selected", action="store_true", help="Nur aktuelle Selektion einfärben")
    sp.add_argument("--clear", action="store_true", help="Highlights entfernen (Pflicht-Cleanup)")
    sp.set_defaults(func=cmd_splash)

    # Prefixiere alle Subparser mit den globalen Optionen — vererbt sich via parent
    return p


def main() -> None:
    parser = build_parser()
    # argparse benötigt globale Flags vor dem Subcommand-Namen. Wir tolerieren
    # aber auch die Reihenfolge `arc ports --json` durch Pre-Parse-Umsortierung
    # nicht — bewusst simpel gehalten. Nutzung ist trotzdem intuitiv:
    # `arc --port 19724 call GetProjectInfo`
    args = parser.parse_args()
    try:
        args.func(args)
    except urllib.error.URLError as e:
        die(f"HTTP-Fehler: {e}")
    except KeyboardInterrupt:
        die("abgebrochen", code=130)


if __name__ == "__main__":
    main()
