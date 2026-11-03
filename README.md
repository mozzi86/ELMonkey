# ELMonkey

**ELMonkey ist ein MCP-Plugin des Schwarz Architekturbüros.** Ein gemeinsamer Python-Kern, dazu
Add-ons für die Quellen, in denen gearbeitet wird:

| Teil | Ordner | Stand |
|---|---|---|
| **Kern** `elmonkey` – Finden (und/oder/nicht), Vorprüfung mit Codes, Rücklese | [`core/`](core/) | ✅ 0.1.0, 11 Tests |
| **Add-on Schwarz Plattform** – Kern mit den Plattform-Regeln, Grundmodell + Änderungsschicht | [`platform-addon/`](platform-addon/) | ✅ gegen die 16 Plattform-Tests aus PR #95 geprüft, noch nicht eingebaut |
| **MCP-Server** `elmonkey-mcp` 0.2.0 – ein Server, Add-ons `archicad` + `kern`, für jede KI | [`mcp/`](mcp/) | ✅ umgezogen (mit Historie), 7 Tests, live gegen Archicad gelesen |
| **Add-on Archicad** – C++-Add-On ELM_SAB_Tapir, CI, Installer | [`archicad-addon/`](archicad-addon/) | ⏳ zieht aus `mozzi86/-archicad` um |
| **Skill Archicad** – Arbeitswissen für Claude | [`skills/`](skills/) | ⏳ zieht aus `mozzi86/-archicad` um |
| **Client-Konfigurationen** (Claude Desktop/Code, Cursor, VS Code, Codex) | [`clients/`](clients/) | ✅ |
| **Claude-Plugin** | [`plugin/`](plugin/) | ⏳ |

Bis alles umgezogen ist und ein grünes Release hat, bleibt **`mozzi86/-archicad` die Quelle für
Installationen**. Umbauplan: siehe [`docs/UMBAU.md`](docs/UMBAU.md).

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
uv pip install "git+ssh://git@github.com/mozzi86/ELMonkey.git#subdirectory=core"
```

Das Repo ist privat: über `git+ssh` mit hinterlegtem SSH-Schlüssel (geprüft 03.11.2026). `git+https`
braucht einen Zugangstoken.

```python
import elmonkey as em

treffer = em.finden(bauteile, {"typ": "IfcWall", "eigenschaft": "Pset_WallCommon.FireRating", "wert": "F30"})
befunde = em.vorpruefen(ops, wirksam, alle)
if em.blockiert(befunde):
    ...                                # nichts schreiben
bericht = em.ruecklesen(vorschau, vorher, nachher)
```

Andere Quelle, andere Feldnamen: `em.Regeln(f_geschoss="Geschoss", status_felder=("Renovierungsstatus",))`.

Tests: `cd core && python -m pytest -q tests`

## Für KIs

Einstieg: [`AGENTS.md`](AGENTS.md).
