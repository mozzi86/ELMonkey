# elmonkey-mcp

> **Drei Monkeys, funktionsgleich:** Archicad-Monkey (Add-On), Python-Monkey (Kern + MCP) und Plattform-Monkey (Schwarz Plattform) arbeiten gleich und werden immer angeglichen — eine Änderung an einem kommt im selben Zug in die beiden anderen. Siehe [README im Wurzelordner](../../README.md#drei-monkeys--funktionsgleich-immer-angeglichen).

MCP-Server für Archicad mit ELMonkey (Tapir eingebaut + ELM_SAB). Ersetzt `tapir-archicad-mcp`, gleiche drei Werkzeuge:

| Werkzeug | Zweck |
|---|---|
| `discovery_list_active_archicads` | Ports 19723–19744 abfragen: Projekt, Teamwork, Archicad- und ELMonkey-Version |
| `archicad_discover_tools(query, port?, max_results?)` | Stichwortsuche (auch deutsch) im Live-Katalog aus `ELM_SAB.ListCommands` (ab ELMonkey 0.9.30) + wichtige `API.*`-Befehle |
| `archicad_call_tool(name, arguments)` | `name` = `ELM_SAB.X`, `TapirCommand.X`, `API.X`, eindeutiger Kurzname oder alter tapir-archicad-mcp-Name; `arguments` = `{port, params}` oder Parameter direkt neben `port` |

Installation: `uv tool install "git+https://github.com/mozzi86/ELMonkey#subdirectory=mcp/elmonkey-mcp"`, Einrichtung je KI: [`clients/`](../../clients/).

Dazu (ab 0.3.0) `elmonkey_diff_plattform(stand, basis?, plattform_geometrie?, gebaut?, versatz?, ordner?)`: Diff-Checker
Archicad ↔ Plattform ohne IFC-Export — liest `ELM_SAB.GetDiffDataELM` (Add-on ≥ 0.11.13), vergleicht mit `stand.json`
(`elmonkey.diff_plattform`) und schreibt `ergebnis.json` (Format von `archicad_abgleich_melden` + `diff`) in den
Arbeitsordner. Siehe [`skills/archicad/reference/diff-plattform.md`](../../skills/archicad/reference/diff-plattform.md).

**Add-ons** (`ELMONKEY_ADDONS`, Standard `archicad,kern`): `archicad` = die Werkzeuge oben, `kern` = `elmonkey_finden` (Kern-Suche für Bauteillisten aus jeder Quelle).
Keine schweren Abhängigkeiten (nur `mcp`), HTTP per Standardbibliothek, Zeitgrenze 600 s je Aufruf.

Tests ohne Archicad: `python tests/test_catalog.py`.
