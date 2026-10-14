# Initial-Setup — ELMonkey + MCP-Server installieren

<!-- 2026-10-14: umgestellt auf ELMonkey (Tapir ist eingebaut) + eigenen MCP-Server elmonkey-mcp.
     Vanilla-Tapir und tapir-archicad-mcp werden NICHT mehr installiert. Live-Stand siehe Fußzeile. -->

Einmalige Einrichtung für einen neuen Rechner oder Kollegen. Drei Bausteine:

| Baustein | Was | Woher |
|---|---|---|
| **ELMonkey-Add-On** | Archicad-Add-On mit **Tapir vollständig eingebaut** (Namensraum `TapirCommand`) plus den SAB-Befehlen (`ELM_SAB`). Ein separates Tapir-Add-On ist **nicht nötig und stört** (zwei Add-Ons mit demselben Namensraum). | Release `elm-sab-tapir-latest` auf github.com/mozzi86/-archicad — per Installer |
| **MCP-Server `elmonkey-mcp`** | Brücke Claude ↔ Archicad. Stellt `mcp__archicad__*` bereit (gleiche drei Werkzeugnamen wie früher). Kennt alle ELMonkey-Befehle live aus `ELM_SAB.ListCommands`, sucht auch deutsch. | Dieses Repo, `mcp/elmonkey-mcp`, Installation per `uv` |
| **Claude-Registrierung** | Server unter dem Namen `archicad` eintragen. | `claude mcp add` bzw. Desktop-Config |

## Schritt 1 — ELMonkey installieren (Installer)

1. **Archicad komplett beenden** (Mac: ⌘Q; Windows: alle Archicad-Fenster schließen).
2. Release-Seite öffnen: <https://github.com/mozzi86/-archicad/releases/tag/elm-sab-tapir-latest>
3. **Mac:** `ELMonkey-Installer-Mac.command` laden und doppelklicken. Meldet macOS „kann nicht geöffnet werden": Rechtsklick → Öffnen. Alternativ im Terminal:
   ```bash
   curl -fsSL https://raw.githubusercontent.com/mozzi86/-archicad/main/installer/ELMonkey-Installer-Mac.command | bash
   ```
   **Windows:** `ELMonkey-Installer-Windows.cmd` laden und doppelklicken (fragt nach Administratorrechten).
4. Der Installer findet alle Archicad 27/28/29, lädt je das passende Bundle, **sichert vorhandene ELMonkey- und Tapir-Add-Ons** nach `~/ELM_SAB_Backups/<Zeitstempel>/` (Windows: `%USERPROFILE%\ELM_SAB_Backups\…`) und entfernt die Quarantäne.
5. Archicad starten → Menü **ELMonkey SAB → Über**: zeigt `ELM_SAB <Version> · Tapir 1.5.4 · Port …`.

**Updates später:** im selben Dialog **„Nach Updates suchen"** → „Jetzt aktualisieren" tauscht das Add-On der laufenden Archicad-Version aus (altes wird gesichert), wirksam nach dem nächsten Neustart. Oder den Installer erneut ausführen.

## Schritt 2 — MCP-Server installieren (uv)

Voraussetzung [`uv`](https://docs.astral.sh/uv/). Falls nicht vorhanden:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Server installieren:

```bash
uv tool install "git+https://github.com/mozzi86/-archicad#subdirectory=mcp/elmonkey-mcp"
```

Das legt **`~/.local/bin/elmonkey-mcp`** an. Updates: `uv tool upgrade elmonkey-mcp`.
Wer vorher `tapir-archicad-mcp` hatte: `uv tool uninstall tapir-archicad-mcp` (der neue Server ersetzt ihn; Werkzeugnamen bleiben gleich).

## Schritt 3 — Bei Claude registrieren

**Claude Code:**

```bash
claude mcp remove archicad --scope user
claude mcp add --scope user archicad -- ~/.local/bin/elmonkey-mcp
```

**Claude Desktop:** `~/Library/Application Support/Claude/claude_desktop_config.json` (Windows: `%APPDATA%\Claude\claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "archicad": { "command": "/Users/DEIN-USER/.local/bin/elmonkey-mcp" }
  }
}
```

Windows: `"command": "C:\\Users\\DEIN-USER\\.local\\bin\\elmonkey-mcp.exe"`. Danach Claude neu starten.
Es müssen `mcp__archicad__discovery_list_active_archicads`, `mcp__archicad__archicad_discover_tools` und `mcp__archicad__archicad_call_tool` erscheinen.

## Schritt 4 — Smoke-Test

Archicad mit einem Projekt öffnen, dann:

1. **JSON-API lebt?**
   ```bash
   curl -s -m 5 localhost:19723 -H 'Content-Type: application/json' -d '{"command":"API.IsAlive"}'
   ```
   Erwartung `{"succeeded":true,…}`. Sonst Port 19724, 19725 … probieren (Ports wechseln bei jedem Start).
2. **ELMonkey geladen?**
   ```bash
   curl -s -m 5 localhost:19723 -H 'Content-Type: application/json' \
     -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"ELM_SAB","commandName":"GetAddOnVersion"},"addOnCommandParameters":{}}}'
   ```
   Erwartung: `version` (ELMonkey) und `tapirBaseVersion`.
3. **MCP-Brücke?** In Claude `mcp__archicad__discovery_list_active_archicads` → Port, Projekt, `elmonkeyVersion`. Dann `archicad_discover_tools` mit „Schraffur anlegen" → `ELM_SAB.Create2DElements` oben.

## Troubleshooting

| Symptom | Ursache | Fix |
|---|---|---|
| Archicad meldet „Einige Add-Ons konnten nicht geladen werden" | altes Tapir- oder Backup-Bundle im Add-Ons-Ordner | Installer erneut ausführen (räumt Tapir/ELMonkey-Altbestände weg); nie `.bak`-Bundles im Add-Ons-Ordner lassen |
| Tapir-Befehle doppelt / seltsames Verhalten | Vanilla-Tapir UND ELMonkey geladen | Tapir-Bundle entfernen (macht der Installer) |
| `archicad_discover_tools` liefert nur `API.*` mit Hinweis | ELMonkey < 0.9.30 (kein `ListCommands`) | ELMonkey aktualisieren; `archicad_call_tool` mit `Namensraum.Befehl` geht trotzdem |
| „Mehrere Archicad-Instanzen offen" | kein `port` angegeben | `discovery_list_active_archicads` → `port` mitgeben |
| `curl` auf 19723 verweigert | Archicad läuft nicht / anderer Port / modaler Dialog offen | Ports 19723–19744 durchprobieren; offenen Dialog schließen |
| Windows: Installer startet nicht | Ausführungsrichtlinie / Download blockiert | `.cmd` verwenden (setzt `-ExecutionPolicy Bypass`), Datei-Eigenschaften → „Zulassen" |

Stand: ELMonkey 0.9.30 (Installer, Update-Knopf, `ListCommands`), `elmonkey-mcp` 0.1.0. <!-- 2026-10-14 -->
Live verifiziert: Mac-Installer gegen Test-Ordner (Tapir + altes ELMonkey gesichert, AC28/29 installiert, Quarantäne weg); MCP-Server per stdio-Client (3 Werkzeuge, Fehlertexte). Windows-Installer und Selbst-Update unter Windows: **noch nicht live geprüft**.
