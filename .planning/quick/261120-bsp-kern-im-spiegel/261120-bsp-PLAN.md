---
phase: quick-261120-bsp
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
  - core/tests/test_plattform_abgleich_skript.py
  - .github/workflows/skill-spiegeln.yml
  - skills/archicad/recipes/plattform-abgleich.md
autonomous: true
requirements: [QUICK-261120-BSP]

must_haves:
  truths:
    - "Im Spiegel-Layout (skill/scripts/plattform_abgleich/plattform_abgleich.py + skill/scripts/_kern/elmonkey) läuft `python -I plattform_abgleich.py --help` mit Exit 0, ohne installiertes elmonkey-core und ohne PYTHONPATH"
    - "Ohne _kern und ohne installiertes elmonkey bricht das Skript mit einer deutschen Meldung ab, die elmonkey-core installieren oder Skill neu spiegeln nennt (kein nackter ModuleNotFoundError-Traceback, kein IndexError)"
    - "Im Repo (skills/archicad/scripts/...) funktioniert der Import weiter über core/ — bestehende Tests bleiben grün"
    - "Der Spiegel-Workflow schreibt core/elmonkey nach spiegel/scripts/_kern/elmonkey (ohne __pycache__) plus HERKUNFT.txt mit Quell-Commit"
  artifacts:
    - path: "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py"
      provides: "Import-Kaskade installiert → Repo-core → Spiegel-_kern"
      contains: "_kern"
    - path: ".github/workflows/skill-spiegeln.yml"
      provides: "Kern-Kopie in den Spiegel"
      contains: "scripts/_kern/elmonkey"
    - path: "core/tests/test_plattform_abgleich_skript.py"
      provides: "Spiegel-Layout-Tests (positiv + negativ)"
      contains: "_kern"
    - path: "skills/archicad/recipes/plattform-abgleich.md"
      provides: "Hinweis: Kern liegt im Spiegel unter scripts/_kern"
      contains: "<!-- 2026-11-20 -->"
  key_links:
    - from: "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py"
      to: "scripts/_kern/elmonkey"
      via: "sys.path-Kandidat Path(__file__).resolve().parents[1] / '_kern' (mit Längenprüfung)"
      pattern: "_kern"
    - from: ".github/workflows/skill-spiegeln.yml"
      to: "core/elmonkey"
      via: "rsync -a --delete --exclude __pycache__ quelle/core/elmonkey/ spiegel/scripts/_kern/elmonkey/"
      pattern: "rsync.*core/elmonkey"
---

<objective>
Der gespiegelte Archicad-Skill (mozzi86/-archicad) bringt den Kern `elmonkey` selbst mit, damit
`scripts/plattform_abgleich/plattform_abgleich.py` bei Kollegen ohne elmonkey-core nicht mehr mit
`ModuleNotFoundError: elmonkey` abbricht (verifiziert 20.11.2026 im installierten Skill).

Purpose: Der Spiegel enthält nur SKILL.md + recipes/reference/commands/scripts, kein core/. Der Kern ist
reine Standardbibliothek (core/pyproject.toml: dependencies = []; abgleich/diff_plattform importieren nur
re/typing; katalog mit ifcopenshell wird von `elmonkey/__init__.py` nicht importiert) und kann 1:1 mitkopiert werden.
Output: Import-Kaskade im Skript, Spiegel-Schritt im Workflow, Recipe-Hinweis, zwei neue Tests.

Arbeitsverzeichnis: /Users/ap/.scratch/elmonkey/diff-plattform-wt (Worktree, Zweig kern-im-spiegel — nicht wechseln).
Nur die vier Dateien aus files_modified anfassen. KEIN Hilfsskript für manuelles Spiegeln anlegen. Kein Push.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@./CLAUDE.md
@.github/workflows/skill-spiegeln.yml
@skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
@core/tests/test_plattform_abgleich_skript.py

<interfaces>
Aktueller Import in plattform_abgleich.py (Zeilen ~41-47), wird ersetzt:
  try: from elmonkey import abgleich as ab; from elmonkey import diff_plattform as dp
  except ImportError: sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "core")); erneuter Import
Problem: im Spiegel ist der Pfad <skill>/scripts/plattform_abgleich/plattform_abgleich.py → parents[4] zeigt
außerhalb des Skills bzw. wirft IndexError bei flachen Pfaden; core/ existiert dort nicht.
Das Skript nutzt danach auf Modulebene `ab` (z. B. ab.PLATTFORM_GUID in Konstanten) und `dp`.

Layout-Mapping:
  Repo:    <repo>/skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py → parents[4] = <repo>, Kern = <repo>/core/elmonkey
  Spiegel: <skill>/scripts/plattform_abgleich/plattform_abgleich.py → parents[1] = <skill>/scripts, Kern = <skill>/scripts/_kern/elmonkey

Testdatei: Skript wird per importlib.util.spec_from_file_location aus
SKRIPT = Path(__file__).resolve().parents[2] / "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py" geladen
(Modulobjekt `pa`); subprocess, pytest, Path sind bereits importiert.
Kern-Quelle für Tests: Path(__file__).resolve().parents[1] / "elmonkey" (= core/elmonkey).

Workflow-Schritt „Spiegeln“: Checkouts unter quelle/ und spiegel/; Schleife `for d in recipes reference commands scripts`
mit rsync -a --delete; danach cp SKILL.md. „Commit und Push“ macht git add -A -- SKILL.md recipes reference commands scripts
(deckt scripts/_kern ab). GITHUB_SHA wird nur als Shell-Variable genutzt (Injection-Schutz, siehe Kommentar dort).
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Import-Kaskade im Skript + Spiegel-Layout-Tests</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - test_spiegel_layout_mit_kern: kopiert das Skript nach tmp_path/skill/scripts/plattform_abgleich/plattform_abgleich.py und core/elmonkey (shutil.copytree mit ignore=shutil.ignore_patterns("__pycache__")) nach tmp_path/skill/scripts/_kern/elmonkey; führt [sys.executable, "-I", str(skript), "--help"] per subprocess.run aus (cwd=tmp_path, env = Kopie von os.environ ohne PYTHONPATH, capture_output=True, text=True, timeout=60) → returncode 0 und "archicad-lesen" in stdout.
    - test_spiegel_layout_ohne_kern: gleiches Layout ohne _kern → returncode != 0, stderr enthält "elmonkey-core" und "spiegeln", stderr enthält weder "IndexError" noch "Traceback".
    - Beide Tests: vorab pytest.skip, falls [sys.executable, "-I", "-c", "import elmonkey"] mit derselben sauberen env (cwd=tmp_path) Exit 0 liefert — dann ist elmonkey site-installiert und der Spiegelfall nicht prüfbar.
    - Bestehende Tests (Skript per importlib aus dem Repo geladen) bleiben grün.
  </behavior>
  <action>
RED: Die zwei Tests in core/tests/test_plattform_abgleich_skript.py ergänzen (os, shutil, sys oben importieren, falls nicht vorhanden; Hilfsfunktionen _saubere_env() und _spiegel_bauen(tmp_path, mit_kern) anlegen, die den Skriptpfad zurückgibt). Ausführen: der Positivtest muss mit dem alten Import scheitern (parents[4]/core existiert unter tmp nicht → ModuleNotFoundError).

GREEN: In plattform_abgleich.py den try/except-Import-Block durch eine robuste Kaskade ersetzen (Kommentar darüber nennt die drei Quellen: installiertes elmonkey-core / Repo core/ / Spiegel scripts/_kern, geschrieben vom Workflow skill-spiegeln):
1. Zuerst `from elmonkey import abgleich as ab` und `from elmonkey import diff_plattform as dp` (installiertes elmonkey-core).
2. Nur bei ImportError: Kandidatenliste aus eltern = Path(__file__).resolve().parents bauen — Repo-Kandidat eltern[4] / "core" nur wenn len(eltern) > 4, Spiegel-Kandidat eltern[1] / "_kern" nur wenn len(eltern) > 1; nur Kandidaten aufnehmen, deren Unterordner "elmonkey" ein Verzeichnis ist. Kein Indexzugriff ohne Längenprüfung.
3. Kandidaten der Reihe nach: Pfad vorn in sys.path einfügen, Import versuchen; bei ImportError Pfad wieder aus sys.path entfernen, halb geladene Einträge "elmonkey" und "elmonkey.*" aus sys.modules entfernen, nächsten Kandidaten probieren. Nur ImportError abfangen.
4. Klappt keiner: sys.exit(...) mit deutscher Meldung, sinngemäß „plattform_abgleich: Modul 'elmonkey' nicht gefunden. elmonkey-core installieren (aus mozzi86/ELMonkey core/) oder den Skill neu spiegeln bzw. neu installieren — im Spiegel muss scripts/_kern/elmonkey liegen (schreibt der Workflow skill-spiegeln). Geprüft: <Pfade>“. sys.exit mit String → Exit 1, Text auf stderr, kein Traceback.
Die Kaskade darf in einer kleinen Funktion (z. B. _kern_laden() → (ab, dp)) liegen; `ab` und `dp` müssen auf Modulebene unter diesen Namen bleiben (Rest des Skripts und Tests nutzen sie). Sonst nichts am Skript ändern.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q -rs</automated>
  </verify>
  <done>Beide neuen Tests grün und NICHT geskippt (Skip-Zusammenfassung von -rs prüfen), alle bestehenden Tests in core/tests grün; kein Zugriff auf parents[4] ohne Längenprüfung mehr im Skript.</done>
</task>

<task type="auto">
  <name>Task 2: Spiegel-Workflow kopiert Kern nach scripts/_kern + Recipe-Hinweis + Commit</name>
  <files>.github/workflows/skill-spiegeln.yml, skills/archicad/recipes/plattform-abgleich.md</files>
  <action>
Workflow .github/workflows/skill-spiegeln.yml:
- Kommentarblock oben ergänzen: Skripte im Skill brauchen den Kern elmonkey (nur Standardbibliothek); der Spiegel hat kein core/, darum legt dieser Workflow core/elmonkey unter scripts/_kern/elmonkey ab, plus scripts/_kern/HERKUNFT.txt; _kern im Spiegel nie von Hand ändern.
- on.push.paths um 'core/elmonkey/**' ergänzen, damit Kern-Änderungen ebenfalls gespiegelt werden.
- Schritt „Spiegeln“: NACH der rsync-Schleife (die Schleife löscht per --delete scripts/_kern, weil es in quelle/skills/archicad/scripts nicht existiert — darum muss die Kern-Kopie danach kommen; das als Kommentar im Schritt festhalten):
  mkdir -p spiegel/scripts/_kern/elmonkey und rsync -a --delete --exclude __pycache__ "quelle/core/elmonkey/" "spiegel/scripts/_kern/elmonkey/" (abschließende Schrägstriche).
  Dann spiegel/scripts/_kern/HERKUNFT.txt per printf/echo mit > und >> schreiben (kein Heredoc): Quelle https://github.com/mozzi86/ELMonkey, Pfad core/elmonkey, Commit ${GITHUB_SHA} (nur Shell-Variable, KEIN ${{ github.sha }} im run-Block), Zeile „Nicht von Hand ändern — kommt aus mozzi86/ELMonkey core/elmonkey und wird vom Workflow skill-spiegeln überschrieben.“
- „Commit und Push“ unverändert lassen. YAML-Einrückung wie Bestand.

Recipe skills/archicad/recipes/plattform-abgleich.md, Abschnitt „## Bekannt / offen“: einen Punkt anhängen mit Marker <!-- 2026-11-20 -->, sinngemäß: Das Skript braucht den Kern `elmonkey`; Reihenfolge installiertes elmonkey-core → im ELMonkey-Repo core/ → im Spiegel mozzi86/-archicad unter `scripts/_kern/elmonkey`. `_kern` schreibt der Workflow skill-spiegeln bei jedem Spiegeln (Herkunft in `scripts/_kern/HERKUNFT.txt`), nicht von Hand ändern. Meldung „Modul 'elmonkey' nicht gefunden“ → elmonkey-core installieren oder Skill neu spiegeln/installieren.

Danach alles committen (alle vier Dateien + diesen Plan-Ordner falls üblich), nur lokal:
git -c user.email=mozzi666@gmail.com commit mit deutscher Nachricht (z. B. „Skill-Spiegel bringt den Kern mit (scripts/_kern) — plattform_abgleich läuft ohne elmonkey-core“) und Trailer „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>“. Kein Push, Zweig nicht wechseln.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && uv run --no-project --with pyyaml python -c "import yaml;d=yaml.safe_load(open('.github/workflows/skill-spiegeln.yml'));s=[x for x in d['jobs']['spiegeln']['steps'] if x.get('name')=='Spiegeln'][0]['run'];assert 'scripts/_kern/elmonkey' in s and '__pycache__' in s and 'HERKUNFT.txt' in s and 'GITHUB_SHA' in s and '{{' not in s;assert s.index('for d in')<s.index('_kern/elmonkey');assert 'core/elmonkey/**' in d[True]['push']['paths'];print('ok')" && grep -n "2026-11-20" skills/archicad/recipes/plattform-abgleich.md | grep -q "_kern\|elmonkey" ; grep -n "scripts/_kern" skills/archicad/recipes/plattform-abgleich.md && git log -1 --format='%ae %s' && git log -1 --format=%B | grep -q "Co-Authored-By: Claude Opus 5.5"</automated>
  </verify>
  <done>YAML-Check gibt „ok“; Recipe enthält den Hinweis mit Marker 2026-11-20 und „scripts/_kern“; ein lokaler Commit mit Autor-Mail mozzi666@gmail.com und Co-Authored-By-Trailer existiert; Tests aus Task 1 weiterhin grün; nichts gepusht.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| GitHub-Workflow → Spiegel-Repo | Workflow schreibt mit PAT in mozzi86/-archicad |
| Skript → sys.path | Skript lädt Code aus Ordnern neben sich |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-bsp-01 | Tampering | skill-spiegeln.yml run-Block | mitigate | GITHUB_SHA nur als Shell-Variable, keine ${{ }}-Interpolation im run; kein neuer Trigger mit Fork-Zugriff (pull_request bleibt draußen) |
| T-bsp-02 | Tampering | rsync --delete im Spiegel | mitigate | --delete nur innerhalb spiegel/scripts/_kern/elmonkey/ mit abschließenden Schrägstrichen; Wurzel von -archicad bleibt unberührt |
| T-bsp-03 | Elevation | sys.path-Kandidaten im Skript | accept | Kandidaten liegen nur innerhalb des eigenen Skill-/Repo-Baums und werden erst nach installiertem elmonkey probiert; wer diese Ordner schreiben kann, kann auch das Skript ändern |
| T-bsp-04 | Information Disclosure | core/elmonkey im öffentlichen Spiegel | accept | core/elmonkey ist bereits im öffentlichen Repo mozzi86/ELMonkey; keine SAB-Daten, keine Secrets |
</threat_model>

<verification>
- PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q -rs → alles grün, neue Spiegel-Tests nicht geskippt
- YAML-Check aus Task 2 gibt „ok“
- git status sauber nach Commit; git log -1 zeigt den Commit; kein Push
</verification>

<success_criteria>
- Spiegel-Layout-Simulation (python -I, ohne PYTHONPATH) startet das Skript mit Exit 0
- Ohne Kern: deutsche Fehlermeldung mit Lösungsweg, kein Traceback/IndexError
- Workflow kopiert core/elmonkey nach scripts/_kern/elmonkey + HERKUNFT.txt, injection-sicher
- Recipe-Hinweis mit <!-- 2026-11-20 --> vorhanden
- Nur die vier Dateien geändert (plus Planungsartefakte), lokal committet
</success_criteria>

<output>
Create `.planning/quick/261120-bsp-kern-im-spiegel/261120-bsp-SUMMARY.md` when done
</output>
