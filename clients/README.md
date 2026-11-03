# ELMonkey in jeder KI einrichten

Einmal installieren (macOS/Windows, braucht [uv](https://docs.astral.sh/uv/)):

```bash
uv tool install "git+https://github.com/mozzi86/ELMonkey#subdirectory=mcp/elmonkey-mcp"
```

Das legt den Befehl `elmonkey-mcp` an (macOS: `~/.local/bin/elmonkey-mcp`). Der Server spricht MCP über
stdio. Add-ons wählt `ELMONKEY_ADDONS` (Standard `archicad,kern`).

| KI / Programm | Eintrag |
|---|---|
| **Claude Desktop** | `~/Library/Application Support/Claude/claude_desktop_config.json` → [`claude_desktop.json`](claude_desktop.json) |
| **Claude Code** | `claude mcp add archicad -s user -- ~/.local/bin/elmonkey-mcp` |
| **Cursor** | `~/.cursor/mcp.json` → [`cursor.json`](cursor.json) |
| **VS Code (Copilot)** | `.vscode/mcp.json` → [`vscode.json`](vscode.json) |
| **OpenAI Codex CLI** | `~/.codex/config.toml` → [`codex.toml`](codex.toml) |
| **ChatGPT** | ChatGPT bindet nur entfernte MCP-Server (https) an, keinen lokalen stdio-Server. Archicad läuft lokal → vorerst über Claude, Cursor, Codex oder VS Code. |

Der Servername heißt überall **`archicad`**: So bleiben die Werkzeugnamen (`mcp__archicad__…`) und der
Archicad-Skill gültig.

Prüfen: Werkzeug `discovery_list_active_archicads` aufrufen. Leere Liste trotz laufendem Archicad =
meist ein offener Dialog in Archicad.
