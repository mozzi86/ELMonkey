# ELMonkey als eigenes Repo – Umbauplan (03.11.2026)

**Stand:** Schritte 1–5 erledigt (04.11.2026). Kern 0.3.0 + MCP 0.2.2; Archicad-Add-On 0.11.12 (RK-Linie 0.10.1–0.10.8 + Polygonwand-Fix, 0.11.12 Oberflächen-Export) umgezogen, CI grün, Release `elm-sab-tapir-latest`; Skill nach `skills/archicad` umgezogen; Plattform-Klebe (`elmonkey.plattform`) im Kern. Kollegen installieren den Skill weiter aus -archicad (Spiegel).

**Ziel:** Ein neues privates Repo `mozzi86/ELMonkey`: ein MCP-Plugin mit gemeinsamem Python-Kern
und zwei Add-ons (Archicad, Schwarz Plattform). Jede KI soll sich wohlfühlen:
- als **MCP-Server** für jeden Client (Claude, ChatGPT, Cursor, Codex, …),
- als **Claude-Plugin** (MCP-Server + Skill + Befehle in einem Schritt).

`mozzi86/-archicad` bleibt vorerst unverändert bestehen. Erst wenn ELMonkey läuft, zeigen
Installer und Skill auf das neue Repo, und `-archicad` wird zum reinen Archiv bzw. Weiterleitung.

## Zielstruktur

```
ELMonkey/
├─ README.md                 Was es ist, Installation je KI (Claude, ChatGPT, Cursor, Codex, …)
├─ AGENTS.md / LLM.txt       Einstieg für jede KI: Werkzeuge, Sicherheitsregeln, Rücklese-Pflicht
├─ core/                     Python-Paket `elmonkey` (rein, ohne Archicad und ohne Cosmos)
│   └─ elmonkey/             finden (und/oder/nicht), Vorprüfung mit Codes, Rücklese-Abgleich
├─ mcp/                      Python-MCP-Server `elmonkey-mcp` (heute: -archicad/mcp/elmonkey-mcp)
│   └─ addons/
│       ├─ archicad/         JSON-API + ELMonkey-Add-On (Discovery, call_tool, Finden auf Archicad-Daten)
│       └─ schwarzplattform/ Anbindung an den Plattform-Konnektor (später, braucht Zugang)
├─ archicad-addon/           C++-Add-On ELM_SAB_Tapir (heute: -archicad/ELM_SAB_Tapir) + CI + Installer
├─ skills/archicad/          der heutige Archicad-Skill (SKILL.md, reference/, recipes/)
├─ plugin/                   .claude-plugin/plugin.json, .mcp.json, Befehle
└─ clients/                  fertige MCP-Konfigurationen für Claude Desktop, Cursor, Codex, ChatGPT
```

## Schritte

1. **Gerüst** – privates Repo anlegen, Struktur, README, AGENTS.md, Lizenzhinweise
   (Tapir MIT + Archicad-Additional-JSON-Commands übernehmen).
2. **Kern** – `core/elmonkey` aus PR #95 (`backend/app/core/elmonkey.py`) herauslösen und
   neutral machen: Bauteil = `{guid, klasse, geschoss, name, beschreibung, eigenschaften, status}`;
   die Plattform-Faltung (`modell_stand`) bleibt im Plattform-Adapter. Die 16 Tests ziehen mit.
3. **MCP** – `elmonkey-mcp` umziehen, Add-on-Mechanik (Archicad zuerst), Werkzeuge
   `elmonkey_finden` / `elmonkey_aendern` auch gegen Archicad (Finden über Properties/Klassen,
   Vorprüfung, Rücklese).
4. **Archicad-Add-On + CI** – `ELM_SAB_Tapir` mit Historie umziehen (`git filter-repo --path`),
   CI-Workflows, `ci-status`, Release, Installer-URLs. **Vorsicht:** Kollegen installieren heute aus
   `-archicad` → Umstellung erst, wenn das neue Release grün ist.
5. **Skill + Plugin** – Skill nach `skills/archicad`, Symlink `~/.claude/skills/archicad` umhängen,
   Claude-Plugin und Client-Konfigurationen, `/gsd-quick`-Regel und `.planning/` mitnehmen.
6. **Plattform** – (offen) SchwarzPlatform pinnt ein Kern-Release (Tag `core-vX.Y.Z`, Tarball-URL) und ersetzt ihre `core/elmonkey.py` durch `Plattform(modell_stand)`. Abstimmung mit dem Plattform-Maintainer; PR #95 bleibt bis dahin die Plattform-Version.
7. **Abschluss** – `-archicad` auf Archiv/Weiterleitung, Memory und Handoff aktualisieren.

## Offene Entscheidungen

- Repo-Name genau `ELMonkey`, privat (Vorschlag).
- Historie mitnehmen (filter-repo) oder frisch starten (Vorschlag: mitnehmen für Add-On und Skill).
- Plattform-Add-on im MCP-Server: über den Plattform-Konnektor (Token nötig) oder nur als Kern-Adapter.

**Entscheidung 03.11.2026:** ELMonkey wird **öffentlich** (kostenlose CI, Installer ohne Anmeldung).
Der Plattform-Adapter gehört in die private Schwarz Plattform und ist hier aus der Historie entfernt.

## Abgleich (entschieden 04.11.2026)

Eine Quelle = ELMonkey. `skills/archicad` wird per Workflow `skill-spiegeln` in die Wurzel von
`mozzi86/-archicad` gespiegelt (nur `SKILL.md`, `recipes/`, `reference/`, `commands/`, `scripts/`;
Secret `ARCHICAD_SPIEGEL_TOKEN`). Die Plattform pinnt Kern-Releases (Tag `core-vX.Y.Z`, Tarball-URL)
und zieht Updates per PR; der Maintainer entscheidet.
