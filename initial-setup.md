# Initial-Setup — MCP-Server + Tapir installieren

Einmalige Installation, damit Claude mit Archicad sprechen kann. Ohne diese drei Bausteine läuft **kein einziges Rezept** dieses Skills. Zielgruppe: Kollege richtet den Skill auf einem neuen Rechner ein.

<!-- live-verifiziert 2026-07-10: macOS, Archicad 29, Tapir 1.5.3, tapir-archicad-mcp 0.3.2 (multiconn_archicad 0.5.1) -->

## Inhaltsverzeichnis

1. [Die drei Bausteine](#die-drei-bausteine)
2. [Schritt 1 — Tapir-Add-On in Archicad installieren](#schritt-1--tapir-add-on-in-archicad-installieren)
3. [Schritt 2 — MCP-Server installieren (uv)](#schritt-2--mcp-server-installieren-uv)
4. [Schritt 3 — MCP-Server bei Claude registrieren](#schritt-3--mcp-server-bei-claude-registrieren)
5. [Schritt 4 — Smoke-Test](#schritt-4--smoke-test)
6. [Troubleshooting](#troubleshooting)

## Die drei Bausteine

| Baustein | Was es ist | Woher |
|---|---|---|
| **Archicad JSON-API** | HTTP-Schnittstelle, die Archicad selbst mitbringt. Lauscht pro Instanz auf `localhost:19723`, jede weitere Instanz bekommt den nächsten Port (19724, …). | Eingebaut, nichts zu installieren. |
| **Tapir-Add-On** | Archicad-Add-On, das die JSON-API um die `TapirCommand`-Familie erweitert (GetProjectInfo, CreateWalls, Klassifizierung, …). Ohne Tapir kann die JSON-API fast nichts Praktisches. | [github.com/ENZYME-APD/tapir-archicad-automation](https://github.com/ENZYME-APD/tapir-archicad-automation) → Releases |
| **MCP-Server `tapir-archicad-mcp`** | Python-Brücke zwischen Claude (MCP-Protokoll) und der JSON-API. Stellt `mcp__archicad__*`-Tools bereit (Discovery, call_tool, Instanz-Liste). | PyPI, Installation via `uv` |

Reihenfolge ist egal — funktionieren tut es erst, wenn alle drei stehen **und** Archicad mit geladenem Tapir läuft.

## Schritt 1 — Tapir-Add-On in Archicad installieren

1. Release-Seite öffnen: `https://github.com/ENZYME-APD/tapir-archicad-automation/releases/latest` — Assets gibt es für **AC25 bis AC29**, je Mac (`.zip` mit `.bundle`) und Windows (`.apx`). <!-- 2026-07-10, Release 1.5.3 -->
2. Passende Datei zur Archicad-Version laden, z. B. `TapirAddOn_AC29_Mac.zip`.
3. **macOS:** ZIP entpacken, das `TapirAddOn_AC29_Mac.bundle` in den Add-Ons-Ordner legen:
   `/Applications/GRAPHISOFT/Archicad 29/Add-Ons/` <!-- live-verifiziert 2026-07-10 -->
   **Windows:** `.apx` nach `C:\Program Files\GRAPHISOFT\Archicad 29\Add-Ons\` legen.
4. Falls macOS das Bundle blockiert („kann nicht überprüft werden"): Quarantäne-Flag entfernen: `xattr -dr com.apple.quarantine "/Applications/GRAPHISOFT/Archicad 29/Add-Ons/TapirAddOn_AC29_Mac.bundle"` <!-- verifiziert 2026-07-14 --> (mehrfach bei ELM_SAB-Bundle-Tausch angewandt)
5. Archicad starten → **Optionen → Add-On-Manager**: Tapir muss in der Liste stehen und geladen sein. Bei Bedarf über „Add-On suchen" manuell auf das Bundle zeigen.

## Schritt 2 — MCP-Server installieren (uv)

Voraussetzung: [`uv`](https://docs.astral.sh/uv/) (Python-Tool-Manager). Falls nicht vorhanden:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Dann den Server als isoliertes Tool installieren:

```bash
uv tool install tapir-archicad-mcp
```

Das legt das Binary **`~/.local/bin/archicad-server`** an. <!-- live-verifiziert 2026-07-10, v0.3.2 --> Prüfen:

```bash
ls ~/.local/bin/archicad-server
```

Updates später: `uv tool upgrade tapir-archicad-mcp`.

## Schritt 3 — MCP-Server bei Claude registrieren

**Claude Code (Terminal/Desktop):**

```bash
claude mcp add --scope user archicad -- ~/.local/bin/archicad-server
```

**Claude Desktop (App):** in `~/Library/Application Support/Claude/claude_desktop_config.json` (Win: `%APPDATA%\Claude\claude_desktop_config.json`) ergänzen: <!-- live-verifiziert 2026-07-10 -->

```json
{
  "mcpServers": {
    "archicad": { "command": "/Users/DEIN-USER/.local/bin/archicad-server" }
  }
}
```

Danach Claude neu starten. Es müssen die Tools `mcp__archicad__discovery_list_active_archicads`, `mcp__archicad__archicad_discover_tools` und `mcp__archicad__archicad_call_tool` auftauchen.

## Schritt 4 — Smoke-Test

Archicad öffnen (beliebiges Projekt), dann in dieser Reihenfolge testen — so isoliert man sofort, welcher Baustein fehlt:

1. **JSON-API lebt?**
   ```bash
   curl -s -m 5 localhost:19723 -H 'Content-Type: application/json' \
     -d '{"command":"API.IsAlive"}'
   ```
   Erwartung: `{"succeeded":true,...}`. Scheitert das → Archicad läuft nicht, oder die Instanz sitzt auf einem anderen Port (19724, 19725, … durchprobieren — Ports sind volatil, siehe [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md)).

2. **Tapir geladen?**
   ```bash
   curl -s -m 5 localhost:19723 -H 'Content-Type: application/json' \
     -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"TapirCommand","commandName":"GetProjectInfo"},"addOnCommandParameters":{}}}'
   ```
   Erwartung: Projektname im Ergebnis. Fehler wie „add-on command not found" → Schritt 1 prüfen (Add-On-Manager).

3. **MCP-Brücke steht?** In Claude: `mcp__archicad__discovery_list_active_archicads` aufrufen → muss die laufende Instanz mit Port und Projektname listen.

Alle drei grün → Setup fertig, ab hier gelten die normalen Rezepte inkl. Warm-up ([`../reference/workflow-context.md`](../reference/workflow-context.md)).

## Troubleshooting

| Symptom | Ursache | Fix |
|---|---|---|
| `curl` auf 19723 verweigert Verbindung | Archicad läuft nicht / anderer Port | Archicad starten; Ports 19723–19730 durchprobieren; **nie** Port ≠ Datei-Zuordnung aus alter Session annehmen |
| `API.IsAlive` ok, Tapir-Kommando schlägt fehl | Add-On nicht geladen | Add-On-Manager prüfen, Archicad-Version ↔ Bundle-Version muss passen (AC29-Bundle lädt nicht in AC28) |
| `archicad-server: command not found` | `~/.local/bin` nicht im PATH | Vollen Pfad in der Registrierung verwenden (so wie oben) |
| MCP-Tools tauchen in Claude nicht auf | Registrierung greift nicht | Claude komplett neu starten; `claude mcp list` prüfen |
| `Port not active` aus MCP-Tool | Instanz geschlossen/gewechselt | `discovery_list_active_archicads` neu aufrufen, nie alte Ports weiterverwenden |

Versions-Stand der Verifikation: Archicad 29 (Mac ARM), Tapir-Add-On aus Release 1.5.3, `tapir-archicad-mcp` 0.3.2, `multiconn_archicad` 0.5.1. <!-- 2026-07-10 -->
