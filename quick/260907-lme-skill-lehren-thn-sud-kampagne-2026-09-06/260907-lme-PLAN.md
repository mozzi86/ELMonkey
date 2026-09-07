---
phase: quick-260907-lme
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - recipes/oeffnungen-aus-konturen.md
  - reference/mcp-conventions.md
autonomous: true
requirements: [QUICK-260907-LME]
must_haves:
  truths:
    - "Ein Claude, der HL-/Trassenpläne liest, findet die Nachlese-Regeln (Fund-Definition, „/"-Falle, Maß-Fallback, Einheiten, Steigschacht-Abgrenzung, Entdopplung, Gewerk-Normalisierung, Klappen, Rohrhülsen, Nebenzeichnung, Registrierung ohne Achsraster) im Öffnungs-Rezept."
    - "Ein Claude, der in Teamwork schreibt, weiß, dass ReserveElements trotz success Konflikte melden kann und dass Folge-Writes dann still no-op sind."
    - "Ein Claude, der Hilfsskripte gegen Archicad baut, findet die Werkzeug-Hygiene-Regeln (Datum zur Laufzeit, enge Globs, Exit-Code+Dateialter, zsh-echo-Falle, ein Schreiblauf)."
    - "Kein neuer Eintrag wiederholt Inhalt, der schon durch 9b8215d oder c6100f7 im Repo steht."
  artifacts:
    - path: "recipes/oeffnungen-aus-konturen.md"
      provides: "Nachlese-Regeln HL-/Trassenpläne mit Marker 2026-09-07"
      contains: "<!-- 2026-09-07 -->"
    - path: "reference/mcp-conventions.md"
      provides: "Teamwork-Reserve-Konflikte + Werkzeug-Hygiene mit Marker 2026-09-07"
      contains: "<!-- 2026-09-07 -->"
  key_links:
    - from: "reference/mcp-conventions.md"
      to: "Inhaltsverzeichnis"
      via: "neue Abschnitte als TOC-Einträge verlinkt"
      pattern: "Werkzeug-Hygiene"
---

<objective>
Die Erkenntnisse der THN-SuD-Kampagne 2026-09-06/07 als kurze, dublettenfreie
Einträge mit Datums-Marker `<!-- 2026-09-07 -->` in zwei Skill-Dateien eintragen.

Purpose: Der Skill soll die teuren Lehren der Kampagne (verlorene Funde durch
falsche Ausschlussregel, stille Teamwork-No-Ops, Werkzeug-Fallen) beim nächsten
Trassenplan-Lauf sofort parat haben.
Output: Ergänzungen in `recipes/oeffnungen-aus-konturen.md` und
`reference/mcp-conventions.md`, ein atomarer Commit, Push auf `origin main`.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@CLAUDE.md
@recipes/oeffnungen-aus-konturen.md
@reference/mcp-conventions.md

**Schon vorhanden — NICHT wiederholen** (Commits `c6100f7` + `9b8215d` vom 2026-09-06):
- `oeffnungen-aus-konturen.md`: „Host-Deckung messen" (Mitte statt `origin`),
  „Mittig in der Wand" (clearance-Kriterium, Reihenfolge Drehen→Tiefe→Zentrieren),
  „Parser-Falle Bauteil-Kürzel", „Geschosslogik + Beschriftung der Durchbruch-Symbole"
  (inkl. `iSymbUse`-Tabelle, `bShowPrefix`, Kombi-Gewerke via `iSymbUse=0`),
  Beschriftungsgrammatik `[Gewerk] WD|BD B/H` und cm/m-Regel in der Pipeline (Schritt 3).
- `mcp-conventions.md`: Fehlerklassen-Zeile „Reserve meldet success, Write scheitert
  mit 6001" (Layer-Lock/Fremdvorbehalt), Abschnitt „Betriebs-Fallen (THN-Live-Sitzung)
  <!-- 2026-09-06 -->" (600-s-Zeitgrenze, Aktionsbefehle nie leer testen, nie `kill`,
  Python-Pufferung, Modell-Durchgang cachen, nur ein Prozess am Modell).

Vor jeder Einfügung die Zieldatei lesen und prüfen, ob die Aussage dort in anderer
Formulierung schon steht. Bei Überschneidung: den bestehenden Satz präzisieren statt
einen zweiten Abschnitt anzulegen.

Stil der Datei beibehalten: deutsche Fließtext-Bullets, Fettung der Kernregel am
Bullet-Anfang, Datums-Marker `<!-- 2026-09-07 -->` an Überschrift bzw. Bullet,
Live-Belege in Zahlen. Kurz halten — keine Wiederholung des Kontexts.
</context>

<tasks>

<task type="auto">
  <name>Task 1: Nachlese-Regeln für HL-/Trassenpläne im Öffnungs-Rezept ergänzen</name>
  <files>recipes/oeffnungen-aus-konturen.md</files>
  <action>
Neuen Abschnitt `## Nachlese in HL-/Trassenplänen: was ein Fund ist <!-- 2026-09-07 -->`
nach dem Abschnitt „Geschosslogik + Beschriftung der Durchbruch-Symbole" (Dateiende)
anhängen. Inhalt als kompakte Bullets, jeder Punkt ein Satz bis zwei:

- Fund-Definition: jede Wandkreuzung einer Trasse, die ein Öffnungssymbol trägt —
  gekreuzter Kasten, WD-Marker auch ohne Maßangabe, Klappe, Bowtie.
- ⚠️ Kernfalle: Maßangaben mit „/" sind KEIN Ausschlusskriterium; der WG-Pilotlauf
  filterte sie heraus und verlor dadurch rund 40 % der Funde.
- Maß-Kaskade: Maß aus dem Label; sonst Kanalquerschnitt + 10 cm, dann als unsicher
  markieren; sonst `MASS?`-Platzhalter — nie stillschweigend raten oder verwerfen.
- Einheit mm/cm pro Blatt am Kanalmaß verifizieren (Blätter mischen die Einheiten).
- Steigschächte = Rechteck mit Diagonalkreuz (BD/DD). KEIN Fund: Treppen-Bruchlinien,
  Rundstützen (Kreis mit Kreuz), kreuzschraffierte 60/60-Stützen.
- Entdopplung: Symbol und Fahne liegen bis 2 m auseinander → über Label-Signatur bzw.
  Klappen-ID zusammenführen, nicht über Abstand allein.
- Gewerk normalisieren: Kombi-Angabe „H/L/S" → „HLS" (Anschluss an die bestehende
  Kombi-Gewerk-Regel `iSymbUse=0` + `symb_cust_text`, per Querverweis, nicht neu erklären).
- Klappen: FSK/SK/ESK werden als BSK geführt, Maße in mm.
- Rohrhülsen `ROHRH D=…` in Wänden werden runde Wanddurchführung HD mit DN — nicht
  als 20/20-WD abbilden.
- Nebenzeichnungen mit eigenem Lineal per Skalierungsfaktor ins Hauptlineal überführen.
- Blätter ohne Achsraster (1:20-Schachtpläne) über Wandflächen gegen die
  3D-BoundingBoxen der Modellwände registrieren (`API.Get3DBoundingBoxes`);
  am THN Residuen ≤ 8 mm erreicht.

Wenn eine Aussage inhaltlich einen bestehenden Satz im Rezept doppelt (z. B.
Bowtie-Clustering in Pipeline-Schritt 2, cm/m-Regel in Schritt 3), stattdessen dort
präzisieren und im neuen Abschnitt nur verweisen.
  </action>
  <verify>
    <automated>grep -q '2026-09-07' recipes/oeffnungen-aus-konturen.md && grep -qi 'ROHRH' recipes/oeffnungen-aus-konturen.md && grep -qi 'MASS?' recipes/oeffnungen-aus-konturen.md && grep -qi 'Bruchlinien' recipes/oeffnungen-aus-konturen.md && echo OK</automated>
  </verify>
  <done>Abschnitt mit Marker 2026-09-07 vorhanden, alle 11 Punkte abgedeckt, keine Dublette zu bestehenden Abschnitten.</done>
</task>

<task type="auto">
  <name>Task 2: Teamwork-Reserve-Konflikte und Werkzeug-Hygiene in mcp-conventions.md</name>
  <files>reference/mcp-conventions.md</files>
  <action>
Zwei neue Abschnitte am Dateiende anlegen, beide mit `<!-- 2026-09-07 -->`, und
das Inhaltsverzeichnis (Zeilen 5–16) um die neuen Einträge erweitern.

**A) `## Teamwork: Konflikt trotz success, stille No-Ops <!-- 2026-09-07 -->`**
(Bezug auf die bestehende Fehlerklassen-Zeile „Reserve meldet success, Write scheitert
mit 6001" herstellen — hier ist der Fall ANDERS: der Write scheitert NICHT, er tut nur nichts.)
- Tapir `ReserveElements` antwortet mit `executionResult.success:true` UND
  `conflicts: [{elementId, user}]`, wenn ein anderer Nutzer das Element reserviert hat.
  Folge-Aufrufe (`MoveElements`, `RotateElements`, `SetGDLParametersOfElements`)
  melden dann ebenfalls `success` und ändern trotzdem nichts. → `conflicts` immer
  auswerten, betroffene Elemente überspringen, jede Schreibung rücklesen.
- `SetGDLParametersOfElements` ignoriert Bool-Parameter stillschweigend, wenn `0`/`1`
  statt `true`/`false` übergeben wird.
- Die Archicad-Warnung „Teamwork-Operation nicht erfolgreich / Verbindung zum Server
  fehlgeschlagen" blockiert die API bis zum OK — per System Events auslesen und
  bestätigen (Anschluss an den bestehenden Modal-Dialog-Abschnitt, per Querverweis).
- `GetDetailsOfElements` liefert für Beams nur die Achse (`begCoordinate`/
  `endCoordinate`) — Breite aus `API.Get3DBoundingBoxes` herleiten.
- Objekte ohne Wandwirt zusätzlich in Unterzügen (Beams) suchen, bevor man sie als
  wirtlos einstuft (Querverweis auf „Host-Deckung messen" im Öffnungs-Rezept).

**B) `## Werkzeug-Hygiene für Hilfsskripte <!-- 2026-09-07 -->`**
- Hilfsdateinamen mit Datum brechen am Datumswechsel → Datum zur Laufzeit bilden,
  nicht im Namen festschreiben.
- Globs eng fassen: `wirt_suche_*.json` las die eigene Ausgabe
  `wirt_suche_unterzug_….json` wieder ein.
- Subprozess-Hilfsläufe auf Exit-Code UND Dateialter der Ausgabe prüfen, nicht auf
  Vorhandensein der Datei.
- In zsh-Subshells kein `echo "== …"` (`=`-Expansion schlägt fehl).
- Nur EIN Schreiblauf gegen Archicad gleichzeitig; vorher per `ps` prüfen, dass kein
  zweiter läuft (verschärft die bestehende Regel „nur ein Prozess am Modell").
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && grep -c '2026-09-07' reference/mcp-conventions.md | grep -qv '^0$' && grep -qi 'conflicts' reference/mcp-conventions.md && grep -qi 'Werkzeug-Hygiene' reference/mcp-conventions.md && grep -qi 'begCoordinate' reference/mcp-conventions.md && grep -A14 'Inhaltsverzeichnis' reference/mcp-conventions.md | grep -qi 'Werkzeug-Hygiene' && echo OK</automated>
  </verify>
  <done>Beide Abschnitte vorhanden und im Inhaltsverzeichnis verlinkt; alle 9 Punkte abgedeckt; keine Dublette zum 2026-09-06-Abschnitt.</done>
</task>

<task type="auto">
  <name>Task 3: Atomarer Commit + Push</name>
  <files>.planning/STATE.md</files>
  <action>
STATE.md: Zeile in „Quick Tasks Completed" ergänzen
(`| 2026-09-07 | lme-skill-lehren-thn-sud-kampagne | … |`) und `last_activity`
aktualisieren.

Dann EIN atomarer Commit nach Konvention von `c648b83`:
`docs(quick-260907-lme): THN-SuD-Kampagne-Lehren in Öffnungs-Rezept + MCP-Conventions`
mit den geänderten Dateien (2 Skill-Dateien + .planning/), Trailer
`Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
Anschließend `git push origin main`.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && git log -1 --format=%s | grep -q 'docs(quick-260907-lme)' && git status --porcelain | grep -qv . && git log origin/main -1 --format=%H | grep -q "$(git rev-parse HEAD)" && echo OK</automated>
  </verify>
  <done>Ein Commit auf main, Working Tree clean, origin/main zeigt auf denselben Commit.</done>
</task>

</tasks>

<verification>
- `grep -n '2026-09-07' recipes/oeffnungen-aus-konturen.md reference/mcp-conventions.md` zeigt die neuen Marker.
- `git show --stat HEAD` listet genau die beiden Skill-Dateien plus `.planning/`.
- Sichtprüfung: kein neuer Abschnitt wiederholt Aussagen aus den 2026-09-06-Abschnitten.
</verification>

<success_criteria>
Alle 11 Öffnungs-Punkte und alle 9 MCP-Punkte des Auftrags stehen kurz und
dublettenfrei mit Marker `<!-- 2026-09-07 -->` in den Zieldateien, in einem
Commit, gepusht auf origin main.
</success_criteria>

<output>
Create `.planning/quick/260907-lme-skill-lehren-thn-sud-kampagne-2026-09-06/260907-lme-SUMMARY.md` when done
</output>
