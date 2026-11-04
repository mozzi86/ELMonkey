<p align="center"><img src="docs/logo/elmonkey-logo.svg" width="220" alt="ELMonkey"></p>

# ELMonkey

**EN** — ELMonkey is two monkeys. The **Python monkey** is the core `elmonkey` (find, pre-check, read back) plus the MCP server `elmonkey-mcp`; it works with any AI and is also used by the Schwarz Platform. The **Archicad monkey** is the C++ add-on ELM_SAB_Tapir (Tapir built in, AC27–30, Mac/Win). An Archicad skill with the working knowledge belongs to it.

**DE** — ELMonkey sind zwei Affen. Der **Python-Monkey** ist der Kern `elmonkey` (Finden, Vorprüfung, Rücklese) plus der MCP-Server `elmonkey-mcp`; er läuft mit jeder KI und wird auch von der Schwarz Plattform genutzt. Der **Archicad-Monkey** ist das C++-Add-On ELM_SAB_Tapir (Tapir eingebaut, AC27–30, Mac/Win). Dazu gehört der Archicad-Skill mit dem Arbeitswissen.

| Teil | Ordner | Stand |
|---|---|---|
| **Kern** `elmonkey` – Finden (und/oder/nicht), Vorprüfung mit Codes, Rücklese, Plattform-Klebe | [`core/`](core/) | ✅ 0.2.0, 19 Tests |
| **Schwarz Plattform** – Plattform bindet `elmonkey-core` als festes Release ein (`elmonkey.plattform`) | Plattform (privat) | ✅ Klebe-Logik im Kern, Einbau nach Abstimmung |
| **MCP-Server** `elmonkey-mcp` 0.2.0 – ein Server, Add-ons `archicad` + `kern`, für jede KI | [`mcp/`](mcp/) | ✅ 7 Tests, live gegen Archicad gelesen |
| **Add-on Archicad** – C++-Add-On ELM_SAB_Tapir 0.10.4, CI (AC27–30 × Mac/Win), Installer | [`archicad-addon/`](archicad-addon/) | ✅ umgezogen (mit Historie) |
| **Skill Archicad** – Arbeitswissen für Claude | [`skills/archicad/`](skills/archicad/) | ✅ umgezogen, wird nach `mozzi86/-archicad` gespiegelt |
| **Client-Konfigurationen** (Claude Desktop/Code, Cursor, VS Code, Codex) | [`clients/`](clients/) | ✅ |
| **Claude-Plugin** | [`plugin/`](plugin/) | ⏳ |

Kollegen installieren den Skill weiter aus [`mozzi86/-archicad`](https://github.com/mozzi86/-archicad); das ist jetzt ein
automatischer Spiegel von `skills/archicad`. Umbauplan: [`docs/UMBAU.md`](docs/UMBAU.md).

## Installation

- **Kern:** `pip install "elmonkey-core @ https://github.com/mozzi86/ELMonkey/archive/refs/tags/core-v0.2.0.tar.gz#subdirectory=core"`
- **MCP-Server:** `uv tool install "git+https://github.com/mozzi86/ELMonkey#subdirectory=mcp/elmonkey-mcp"`
- **Add-On:** Release [`elm-sab-tapir-latest`](https://github.com/mozzi86/ELMonkey/releases/tag/elm-sab-tapir-latest), siehe [`archicad-addon/ELM_SAB_Tapir/README.md`](archicad-addon/ELM_SAB_Tapir/README.md)

## Gelerntes einbauen

Gelerntes kommt nur nach ELMonkey: Skill-Wissen nach `skills/archicad/`, Code nach `core/` (Python-Monkey)
bzw. `archicad-addon/` (Archicad-Monkey). `skills/archicad` wird automatisch nach `mozzi86/-archicad`
gespiegelt (Workflow `skill-spiegeln`); die Plattform holt neue Kern-Releases per Update-PR
(Tag `core-vX.Y.Z` → Release).

## Die drei Arbeitsweisen

1. **Finden wie „Suchen & aktivieren“** – Kriterien beliebig tief verschachtelt:
   `{"und": [{"typ": "IfcDoor"}, {"geschoss": "OG"}, {"nicht": {"status": "Bestand"}}]}`,
   dazu `eigenschaft` mit `wert` (`gleich`, `enthaelt`, `beginnt`, `regex`, `min`/`max`, Dezimalkomma geht).
2. **Vorprüfung mit Code je Bauteil**, bevor geschrieben wird. `NOT_FOUND`, `GELOESCHT`, `UNGUELTIG`,
   `ZU_VIELE` halten den ganzen Satz an; `NO_EFFECT`, `AUSGEBLENDET`, `VORSCHLAG_OFFEN` sind Hinweise.
3. **Rücklese** – nach dem Schreiben frisch lesen und je Bauteil `ok`, `abweichend` oder `ungeprueft`
   melden. Ein `success` ist kein Beleg.

## Kern benutzen

```bash
pip install "elmonkey-core @ https://github.com/mozzi86/ELMonkey/archive/refs/tags/core-v0.2.0.tar.gz#subdirectory=core"
```

```python
import elmonkey as em

treffer = em.finden(bauteile, {"typ": "IfcWall", "eigenschaft": "Pset_WallCommon.FireRating", "wert": "F30"})
befunde = em.vorpruefen(ops, wirksam, alle)
if em.blockiert(befunde):
    ...                                # nichts schreiben
bericht = em.ruecklesen(vorschau, vorher, nachher)

# Schwarz Plattform: ihr Modul hineingeben, gleiche Namen
from elmonkey.plattform import Plattform
p = Plattform(modell_stand)           # p.finden, p.vorpruefen, p.stand_nach, …
```

Andere Quelle, andere Feldnamen: `em.Regeln(f_geschoss="Geschoss", status_felder=("Renovierungsstatus",))`.

Tests: `cd core && python -m pytest -q tests`

## Für KIs

Einstieg: [`AGENTS.md`](AGENTS.md).

---

**Third-party licenses · Fremdlizenzen:** ELMonkey contains MIT-licensed code from [Tapir](https://github.com/ENZYME-APD/tapir-archicad-automation) (© 2024 Enzyme APD), [archicad-additional-json-commands](https://github.com/tlorantfy/archicad-additional-json-commands) (© 2023 Tibor Lorántfy) and the Graphisoft add-on template (© 2020 GRAPHISOFT, basis of the old [ELM_SAB_Add-On](https://github.com/mozzi86/-archicad/tree/main/ELM_SAB_Add-On)) — license texts: [`archicad-addon/ELM_SAB_Tapir/README.md`](archicad-addon/ELM_SAB_Tapir/README.md#lizenzen--licenses). · ELMonkey enthält MIT-lizenzierten Code aus Tapir (© 2024 Enzyme APD), archicad-additional-json-commands (© 2023 Tibor Lorántfy) und der Graphisoft-Add-On-Vorlage (© 2020 GRAPHISOFT, Basis des alten ELM_SAB_Add-On) — Lizenztexte siehe oben.
