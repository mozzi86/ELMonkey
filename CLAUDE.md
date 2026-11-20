<!-- GSD:project-start source:PROJECT.md -->
## Project

**ELMonkey**

ELMonkey besteht aus zwei Affen: dem **Python-Monkey** (Kern `elmonkey` in `core/` plus MCP-Server `elmonkey-mcp` in `mcp/`) und dem **Archicad-Monkey** (C++-Add-On ELM_SAB_Tapir in `archicad-addon/`). Dazu der Archicad-Skill unter `skills/archicad` (wird nach `mozzi86/-archicad` gespiegelt). Der Kern wird auch von der Schwarz Plattform als festes Release genutzt (`elmonkey.plattform`). Zielgruppe: ein Architekturbüro, das täglich in Archicad und der Plattform arbeitet und KIs als Assistenten einsetzt.

**Core Value:** **Jede KI arbeitet sicher in Archicad und der Plattform: finden, vorprüfen, schreiben, zurücklesen** — ohne dass Tool-Namen oder Parameter zur Laufzeit erfunden werden müssen. Discovery-zentriert, asymmetrisch sicher, selbst-verbessernd.

### Constraints

- **Tech Stack**: Kern Python, nur Standardbibliothek; MCP-Server Python; Add-On C++17, gebaut nur von der CI (AC27–30, Mac/Win), kein lokaler Build; Skill Markdown (Hub-and-Spoke: 1 SKILL.md + reference/* + recipes/*).
- **MCP-Abhängigkeit**: Skill ist nur funktional, wenn der Archicad-MCP-Server (`mcp__archicad__*`) konfiguriert und Archicad mit MCP-Plugin geöffnet ist.
- **Discovery-First**: Konkrete MCP-Tool-Namen werden nicht hartkodiert, sondern als „typischer Name (Hinweis)" dokumentiert; Validierung via Discovery zur Laufzeit. Begründung: Robustheit gegen Archicad-Versionswechsel.
- **Live-Verifikation Pflicht**: Jede Recipe-Datei wird gegen das laufende Archicad geprüft, bevor sie als „fertig" gilt. Worked Example dient als Smoke-Test.
- **Sicherheit asymmetrisch**: Create + Read frei; Update + Delete mit User-Confirm + Element-ID-Aufzählung. Keine Batch-Obergrenze (User-Präferenz).
- **Self-Improvement on**: Skill wächst pro Session durch Reflection-Trigger; Einträge mit Datums-Marker, Verifikation-Loop bei Wiederholfehlern.
<!-- GSD:project-end -->
