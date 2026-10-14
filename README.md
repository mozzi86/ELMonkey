# elmonkey-mcp

MCP-Server für Archicad mit ELMonkey (Tapir eingebaut + ELM_SAB). Ersetzt `tapir-archicad-mcp`, gleiche drei Werkzeuge:

| Werkzeug | Zweck |
|---|---|
| `discovery_list_active_archicads` | Ports 19723–19744 abfragen: Projekt, Teamwork, Archicad- und ELMonkey-Version |
| `archicad_discover_tools(query, port?, max_results?)` | Stichwortsuche (auch deutsch) im Live-Katalog aus `ELM_SAB.ListCommands` (ab ELMonkey 0.9.30) + wichtige `API.*`-Befehle |
| `archicad_call_tool(name, arguments)` | `name` = `ELM_SAB.X`, `TapirCommand.X`, `API.X`, eindeutiger Kurzname oder alter tapir-archicad-mcp-Name; `arguments` = `{port, params}` oder Parameter direkt neben `port` |

Installation: `uv tool install "git+https://github.com/mozzi86/-archicad#subdirectory=mcp/elmonkey-mcp"`, Registrierung siehe `recipes/initial-setup.md`.
Keine schweren Abhängigkeiten (nur `mcp`), HTTP per Standardbibliothek, Zeitgrenze 600 s je Aufruf.

Tests ohne Archicad: `python tests/test_catalog.py`.
