<p align="center"><img src="docs/logo/elmonkey-logo.svg" width="220" alt="ELMonkey"></p>

# ELMonkey

**EN** — ELMonkey is two monkeys. The **Python monkey** is the core `elmonkey` (find, pre-check, read back) plus the MCP server `elmonkey-mcp`; it works with any AI and is also used by the Schwarz Platform. The **Archicad monkey** is the C++ add-on ELM_SAB_Tapir (Tapir built in, AC27–30, Mac/Win). An Archicad skill with the working knowledge belongs to it.

**DE** — ELMonkey sind zwei Affen. Der **Python-Monkey** ist der Kern `elmonkey` (Finden, Vorprüfung, Rücklese) plus der MCP-Server `elmonkey-mcp`; er läuft mit jeder KI und wird auch von der Schwarz Plattform genutzt. Der **Archicad-Monkey** ist das C++-Add-On ELM_SAB_Tapir (Tapir eingebaut, AC27–30, Mac/Win). Dazu gehört der Archicad-Skill mit dem Arbeitswissen.

| Teil | Ordner | Stand |
|---|---|---|
| **Kern** `elmonkey` – Finden (und/oder/nicht), Vorprüfung mit Codes, Rücklese, Plattform-Klebe | [`core/`](core/) | ✅ 0.3.0, 54 Tests |
| **Plattform-Monkey** – eigene, funktionsgleiche Kopie des Kerns in der Schwarz Plattform (`core/plattform_monkey`) | Plattform (privat) | ✅ gleichauf mit Kern 0.3.0 (abgeglichen 09.11.2026) |
| **MCP-Server** `elmonkey-mcp` 0.2.3 – ein Server, Add-ons `archicad` + `kern`, für jede KI | [`mcp/`](mcp/) | ✅ 9 Tests, live gegen Archicad gelesen |
| **Add-on Archicad** – C++-Add-On ELM_SAB_Tapir 0.11.12.2 (Zwischenstand liegende Fassaden), CI (AC27–30 × Mac/Win), Installer | [`archicad-addon/`](archicad-addon/) | ✅ umgezogen (mit Historie) |
| **Skill Archicad** – Arbeitswissen für Claude | [`skills/archicad/`](skills/archicad/) | ✅ umgezogen, wird nach `mozzi86/-archicad` gespiegelt |
| **Client-Konfigurationen** (Claude Desktop/Code, Cursor, VS Code, Codex) | [`clients/`](clients/) | ✅ |
| **Claude-Plugin** | [`plugin/`](plugin/) | ⏳ |

## Drei Monkeys – funktionsgleich, immer angeglichen

| Monkey | Wo | Was |
|---|---|---|
| **Archicad-Monkey** | Add-On ELM_SAB_Tapir ([`archicad-addon/`](archicad-addon/)) | schreibt in Archicad |
| **Python-Monkey** | Kern `elmonkey` + MCP-Server ([`core/`](core/), [`mcp/`](mcp/)) | jede KI, außerhalb von Archicad |
| **Plattform-Monkey** | `core/plattform_monkey` in der Schwarz Plattform (privat) | am Grundmodell der Plattform, ohne Archicad |

**DE** — Die drei Monkeys sind **funktionsgleich**: dieselben drei Arbeitsweisen (Finden mit und/oder/nicht wie
„Suchen & aktivieren“, Vorprüfung mit denselben Codes vor jedem Schreiben, Rücklese nach jedem Schreiben), dieselben
Begriffe und dasselbe Austauschformat (z. B. `sab-oberflaechen/1`). Sie werden **immer angeglichen**: Eine Änderung an
einem Monkey kommt im selben Zug in die beiden anderen — Kern und Plattform-Monkey bleiben Zeile für Zeile gleich
(`kern`, `regeln`, `plattform`), neue Archicad-Befehle bekommen ihr Gegenstück bzw. ihren Suchbegriff im
Python-Monkey und ihren Weg in die Plattform. Abgleich zuletzt: 10.11.2026 (Add-On 0.11.12.2 · Kern 0.3.0 ·
MCP 0.2.3 · Plattform-Monkey = Kern 0.3.0).

**EN** — The three monkeys are **functionally identical** (same find / pre-check / read-back, same codes, same
exchange formats) and are **always kept in sync**: a change to one goes into the other two in the same step.

Kollegen installieren den Skill weiter aus [`mozzi86/-archicad`](https://github.com/mozzi86/-archicad); das ist jetzt ein
automatischer Spiegel von `skills/archicad`. Umbauplan: [`docs/UMBAU.md`](docs/UMBAU.md).

## Installation

- **Kern:** `pip install "elmonkey-core @ https://github.com/mozzi86/ELMonkey/releases/download/core-v0.3.0/elmonkey_core-0.3.0-py3-none-any.whl"`
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
pip install "elmonkey-core @ https://github.com/mozzi86/ELMonkey/releases/download/core-v0.3.0/elmonkey_core-0.3.0-py3-none-any.whl"
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
