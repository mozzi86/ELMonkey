---
phase: quick-261120-dbc
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
  - core/tests/test_plattform_abgleich_skript.py
  - skills/archicad/recipes/plattform-abgleich.md
autonomous: true
requirements: [QUICK-261120-dbc]

must_haves:
  truths:
    - "`bauen` baut Plattform-Bauteile aus den echten Netzen der neuen Bauliste (sab-bauliste/1, parameter.koerper) — ohne IFC-Datei"
    - "Mehrere Baulisten (Pfade und https-Links) werden zusammengeführt, Dubletten per plattformGuid (spätere gewinnt)"
    - "Bauteile mit status \"fehler\" werden nie gebaut, sondern mit Meldung ausgelassen"
    - "Hülle ohne IFC wird ausgelassen (\"nur Hülle (geometrie=…)\"), nie geraten"
    - "Mit --ifc und verbund einzeln gewinnen IFC-Unterteile mit eigener GUID (Kopf + Unterteile, Entscheidung Mudi 19.11.2026)"
    - "Nicht bearbeitete GUIDs in der Bauliste brechen `bauen` vor dem ersten Archicad-Aufruf mit deutscher Meldung ab, außer --trotzdem"
    - "Trockenlauf/Ergebnis zeigen quelle-Zählung, Summen nicht_mannigfaltige_kanten/offene_kanten und netz_pruefen"
    - "Alte Aufrufe (eine --bauliste-Datei, --ifc, altes Bauliste-Format) laufen unverändert; alle bestehenden Tests grün"
  artifacts:
    - path: "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py"
      provides: "bauliste_laden, bauliste_netze, nicht_bearbeitet_pruefen, netz_pruefung, bauplan mit Quellen-Priorität, CLI bauen --bauliste N --trotzdem, --ifc optional"
      contains: "def bauliste_laden"
    - path: "core/tests/test_plattform_abgleich_skript.py"
      provides: "Tests für neue Bauliste (ohne Netz, ohne Archicad)"
      contains: "sab-bauliste/1"
    - path: "skills/archicad/recipes/plattform-abgleich.md"
      provides: "Neuer Bau-Ablauf mit bauliste_link"
      contains: "<!-- 2026-11-20 -->"
  key_links:
    - from: "cmd_bauen"
      to: "bauliste_laden"
      via: "o.bauliste (Liste) → bauliste_laden → nicht_bearbeitet_pruefen vor Archicad(_http(...))"
      pattern: "bauliste_laden\\(o\\.bauliste"
    - from: "bauplan"
      to: "bauliste_netze / parameter.koerper"
      via: "Quelle bauliste → morph_body über alle Körper"
      pattern: "quelle"
---

<objective>
`bauen` in plattform_abgleich.py auf die neue Plattform-Bauliste mit echten Netzen umstellen (SchwarzPlatform PR 404,
Staging seit 20.11.2026). IFC wird optional und nur noch für Verbund-Unterteile mit eigener GUID gebraucht.

Purpose: Die alte Bauliste lieferte nur die konvexe Hülle (395/432 unbrauchbar am 19.11.), deshalb lief jeder Bau über
einen Komplett-IFC-Export. Die neue Bauliste trägt die echte Geometrie je Bauteil.
Output: geändertes Skript, neue Tests, aktualisiertes Rezept, Commits (kein Push).
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@./CLAUDE.md
@skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
@core/tests/test_plattform_abgleich_skript.py
@skills/archicad/recipes/plattform-abgleich.md

Arbeitsverzeichnis: /Users/ap/.scratch/elmonkey/diff-plattform-wt (git worktree, Branch `bauliste-netze` von main
242c226 — NICHT wechseln). Nur die drei Dateien oben ändern. Nur Standardbibliothek. Kommentare/Meldungen deutsch,
Stil wie im Bestand (knapp, Datumsvermerk bei Live-Erkenntnissen).

Beispieldatei (echte BFW07-Daten, groß — NICHT ganz lesen, nur per kurzem `python3 -I -c` inspizieren, NICHT als
Test-Fixture ins Repo kopieren): /Users/ap/.scratch/bfw07/abgleich/bauliste-neu-2026-11-20/probe.json

<interfaces>
Neues Bauliste-Format (sab-bauliste/1), Top-Level: format ("sab-bauliste/1"), projekt, rahmen, erzeugt, koerper_form
(Text), bauteile (Liste), nicht_bearbeitet (Liste GUIDs), nicht_bearbeitet_grund ({GUID: Text}),
nicht_im_wirksamen_stand (Liste), nicht_im_wirksamen_stand_grund (dict).
Je Bauteil: plattformGuid, name, ifc {klasse, predefined}, werkzeug ("Morph"), geschoss, geschossOkff, ebene,
parameter {umriss, unterkante, oberkante, koerper: [{punkte: [[x,y,z],…], dreiecke: [[i,j,k],…]}]} (0-basiert je
Körper, Archicad-Meter), eigenschaften, vereinfacht (bool), geometrie ("netz"|"huelle"|"werkzeug"),
status ("offen"|"fehler"|…), archicadGuid, meldung, netz_kennzahlen {koerper, punkte, dreiecke,
nicht_mannigfaltige_kanten, offene_kanten}.
Probe: Kopf „Trinkwasser warm … Verteilleitungen“ (IfcPipeSegment) mit 84 Körpern, 0/0 Kanten; „Zuluftstutzen Ø160“
(IfcDuctSegment) 1 Körper, nicht_mannigfaltige_kanten=1. Verbund-Köpfe kommen als EIN Bauteil mit vielen Körpern,
OHNE Unterteil-GUIDs. Hinweis: koerper_form der Plattform sagt „je Körper ein Morph“ — der Auftrag verlangt EIN Morph
je Bauteil über alle Körper (morph_body); Auftrag gilt.

Bestehende Funktionen im Skript (wiederverwenden, nicht neu bauen):
- Fehler (Exception, deutsche Meldung; main() gibt sie als {"fehler": …} auf stderr, Rückgabe 1)
- _lesen(pfad: Path) -> Any ; _schreiben(pfad, daten) ; _nur_https(link) ; herunterladen(link) -> Any (urllib, bei
  CERTIFICATE_VERIFY_FAILED curl-Fallback als Argumentliste)
- ifc_teile(pfad) -> (eigene {GUID: [(P,F)]}, kinder {Kopf: [GUIDs]}, info {GUID: {name, klasse}})
- _zusammen(eigene, kinder) -> {GUID: Netze inkl. Nachfahren}
- zerlegen(P, F), morph_body(netze: list[(P,F)]) -> (body, box) — wendet zerlegen inkl. mannigfaltig an
- bauplan(bauliste, netze, register, zuordnung, klassen, stories, nur=None, ohne=(), verbund="einzeln") -> (plan, aus);
  netze = Tupel aus ifc_teile oder Dict {GUID: Netze}; Plan-Eintrag {g, name, ebene, klasse, status, kopf, soll, morph}
- bauen(ac, ordner, ifc, bauliste, register, zuordnung, *, nur, ohne, vorhanden, ausfuehren, projekt_soll=None,
  verbund="einzeln") -> dict (Trockenlauf: trockenlauf, projekt, zu_bauen, ausgelassen + zaehl)
- cmd_bauen(o); main(): neu("bauen", cmd_bauen, ifc={"required": True}, bauliste={"required": True}, …)
Tests: Modul als `pa` geladen; Hilfen _wuerfel(versatz, innen), _bauteil(g, klasse, name, ebene), STORIES,
FakeSenden({...}), _mini_ifc, _verbund_ifc/_verbund_setup(tmp_path), pa.guid_lang.
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Bauliste laden (Pfade + Links, Zusammenführen), Netze, Abbruch bei nicht_bearbeitet</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - bauliste_laden([pfad1, pfad2]) mit je einem sab-bauliste/1-Dict: bauteile zusammengeführt, gleiche plattformGuid (Groß/Klein egal) nur einmal, der Eintrag aus der späteren Datei gewinnt, Reihenfolge = erstes Auftreten
    - nicht_bearbeitet aus allen Dateien gesammelt (ohne Dubletten) samt nicht_bearbeitet_grund; GUIDs, die in einer Datei als Bauteil vorkommen, fallen aus nicht_bearbeitet heraus (nachgeholtes Paket)
    - "https://…"-Quelle wird über pa.herunterladen geladen (im Test per monkeypatch ersetzt, kein Netz); "http://…" → Fehler; einzelner String statt Liste geht auch
    - Datei im alten Format (kein format-Feld, Bauteile ohne parameter.koerper) lädt; unbekanntes format (nicht "sab-bauliste/…") → Fehler
    - bauliste_netze(bauliste) liefert {GUID groß: [(punkte, dreiecke), …]} nur für Bauteile mit nicht-leerem parameter.koerper
    - nicht_bearbeitet_pruefen(bauliste, trotzdem=False) wirft Fehler mit deutscher Meldung, die jede GUID (+ Grund) nennt und „neu abrufen“ (archicad_abgleich_bauliste) verlangt; mit trotzdem=True kein Fehler; leere Liste kein Fehler
  </behavior>
  <action>
    Im Abschnitt „Dateien, Links“ bzw. direkt vor bauplan neue reine Funktionen ergänzen:
    bauliste_laden(quellen) — quellen = str oder Liste von str. Je Quelle: beginnt mit "http" → herunterladen(quelle)
    (dessen _nur_https weist http:// ab; curl-Fallback bleibt), sonst _lesen(Path(quelle)). Format prüfen: fehlendes
    format = altes Format, ok; format beginnt nicht mit "sab-bauliste/" → Fehler mit Quelle im Text. Ergebnis-Dict:
    format/projekt/rahmen/erzeugt aus der letzten Quelle, "bauteile" zusammengeführt (Schlüssel plattformGuid.upper(),
    spätere gewinnt, Reihenfolge erstes Auftreten), "nicht_bearbeitet" + "nicht_bearbeitet_grund" sowie
    "nicht_im_wirksamen_stand" + "_grund" über alle Quellen gesammelt; GUIDs, die als Bauteil vorliegen, aus
    nicht_bearbeitet entfernen; "quellen": Anzahl.
    bauliste_netze(bauliste) — {plattformGuid.upper(): [(k["punkte"], k["dreiecke"]) for k in parameter.koerper]} nur
    bei nicht-leerem koerper.
    nicht_bearbeitet_pruefen(bauliste, trotzdem=False) — bei nicht leerer nicht_bearbeitet-Liste und nicht trotzdem:
    Fehler(„N Bauteile hat die Plattform nicht bearbeitet – Bauliste für diese GUIDs neu abrufen
    (archicad_abgleich_bauliste) oder --trotzdem: GUID (Grund), …“). Tests zuerst (RED), dann implementieren (GREEN).
    Test-Hilfe in der Testdatei: _neu_bauteil(g, koerper=…, geometrie="netz", status="offen", meldung=None,
    kennzahlen=…) baut ein synthetisches sab-bauliste/1-Bauteil (Würfel aus _ECKEN/_WUERFEL als koerper); keine
    probe.json im Test.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests/test_plattform_abgleich_skript.py -q -k "bauliste or nicht_bearbeitet"</automated>
  </verify>
  <done>Neue Tests für Laden/Zusammenführen/Link/altes Format/Netze/Abbruch grün; Commit `feat(quick-261120-dbc): Bauliste mit Netzen laden und zusammenführen`.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: bauplan/bauen mit Quellen-Priorität, Kennzahlen, CLI --bauliste N / --trotzdem / --ifc optional</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - bauplan mit netze=None und einem Bauteil mit koerper (2 Würfel-Körper) → 1 Plan-Eintrag, quelle "bauliste", kopf None, Polygone beider Körper im Morph, soll-Box über beide, netz_kennzahlen übernommen
    - status "fehler" → ausgelassen mit grund "status fehler: <meldung>", auch wenn koerper oder IFC-Netz vorhanden
    - geometrie "huelle" ohne koerper und ohne IFC → ausgelassen, grund "nur Hülle (geometrie=huelle)"
    - mit IFC (_verbund_setup) + verbund einzeln + Kopf hat Unterteile mit Geometrie, Bauteil trägt zusätzlich koerper → IFC-Unterteile gewinnen (eigene GUIDs, kopf gesetzt, quelle "ifc"); mit verbund "zusammen" gewinnt koerper (quelle "bauliste")
    - mit IFC, Bauteil ohne koerper, ohne Unterteile → IFC-Netz wie bisher (quelle "ifc"); fehlt es auch im IFC → grund bleibt "keine Geometrie im IFC" (bestehende Tests unverändert grün)
    - netz_pruefung(plan) → {"quelle": {"bauliste": n, "ifc": n}, "netz_kennzahlen": {"nicht_mannigfaltige_kanten": Summe, "offene_kanten": Summe}, "netz_pruefen": [{plattformGuid, name, nicht_mannigfaltige_kanten, offene_kanten}] nur für Einträge mit >0}
    - bauen(ac, ordner, None, …) Trockenlauf mit FakeSenden ohne IFC: zu_bauen aus der Bauliste, Rückgabe enthält quelle, netz_kennzahlen, netz_pruefen
    - main(["bauen", "--ordner", tmp, "--bauliste", a.json, b.json, "--register", r.json]) mit nicht_bearbeitet in b → Rückgabe 1, stderr nennt die GUID, kein HTTP-Aufruf (Port egal); alter Aufruf mit einer --bauliste-Datei wird weiter geparst
  </behavior>
  <action>
    bauplan: Parameter netze darf None sein (= kein IFC); Netze der Bauliste per bauliste_netze(bauliste) holen.
    Je Bauteil nach den bestehenden nur/ohne-Filtern, Klasse/Status-Ermittlung wie bisher, Reihenfolge der Quellen:
    (0) status == "fehler" → aus {plattformGuid, name, grund: "status fehler: " + (meldung or "ohne Meldung")}, weiter;
    (a) netze gegeben UND verbund == "einzeln" UND unter nachfahren(gu) gibt es mindestens ein Unterteil mit eigenen
    Netzen → bestehender Kopf+Unterteile-Pfad, Einträge quelle "ifc";
    (b) Bauliste-Netz vorhanden → ein Eintrag über alle Körper (eintrag(..., ns=bauliste_netze[gu], kopf=None),
    morph_body wendet zerlegen inkl. mannigfaltig an), quelle "bauliste", "netz_kennzahlen": b.get("netz_kennzahlen") or {};
    (c) netze gegeben → bestehender Pfad (eigene des Kopfes; bei "zusammen" bereits zusammengefasst), quelle "ifc",
    am Ende wie bisher "keine Geometrie im IFC" falls nichts entstand;
    (d) sonst aus mit grund "nur Hülle (geometrie=" + (b.get("geometrie") or "?") + ")".
    Klasse/Status-fehlt-Ausgelassen gilt für alle Quellen gleich. eintrag() bekommt quelle (+ optional kennzahlen) als
    Argument. Docstring ergänzen (Priorität, „Kopf + Unterteile“ Entscheidung Mudi 19.11.2026).
    Neue reine Funktion netz_pruefung(plan) wie im Verhalten beschrieben (nur quelle "bauliste" zählt für Kennzahlen).
    bauen: ifc optional — ifc_teile(ifc) nur wenn ifc, sonst None an bauplan; zaehl um **netz_pruefung(plan) ergänzen,
    so dass Trockenlauf UND Ausführungs-Ergebnis quelle/netz_kennzahlen/netz_pruefen enthalten; zusätzlich
    netz_pruefen nach ordner/"netz_pruefen.json" schreiben (wie ausgelassen.json, immer).
    cmd_bauen: bauliste = bauliste_laden(o.bauliste); nicht_bearbeitet_pruefen(bauliste, o.trotzdem) VOR
    Archicad(_http(o.port)) und vor jedem Lesen weiterer Dateien außer Bauliste; o.ifc or None übergeben.
    main: neu("bauen", …, ifc={"default": ""}, bauliste={"required": True, "nargs": "+"}, trotzdem={"action":
    "store_true", "help": "trotz nicht bearbeiteter GUIDs bauen"}, …). Modul-Docstring-Zeile für bauen anpassen
    (--bauliste <datei|link> [weitere …] [--ifc plattform.ifc] [--trotzdem]) und in der Dateiliste netz_pruefen.json
    nennen. Tests zuerst (RED), dann implementieren (GREEN); bestehende Tests nicht ändern außer wo eine Signatur es
    zwingend verlangt (sollte nicht nötig sein).
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q</automated>
  </verify>
  <done>Alle Tests in core/tests grün (bestehende + neue für Quelle bauliste, status fehler, Hülle ohne IFC, IFC-Unterteile gewinnen, netz_pruefung, Trockenlauf ohne IFC, CLI-Abbruch); Commit `feat(quick-261120-dbc): bauen nutzt Bauliste-Netze, IFC nur für Verbund-Unterteile`.</done>
</task>

<task type="auto">
  <name>Task 3: Rezept auf den neuen Bau-Ablauf umstellen, Endprüfung, Commit</name>
  <files>skills/archicad/recipes/plattform-abgleich.md</files>
  <action>
    Schritt 7 („Archicad: neue Bauteile bauen“) neu fassen mit Marker <!-- 2026-11-20 -->: Konnektor
    `archicad_abgleich_bauliste {projekt, guids}` in Paketen zu höchstens 200 GUIDs (netz=link ist Vorgabe), Antwort
    enthält nur Kennzahlen + `bauliste_link` (https, 2 h gültig); alle Links innerhalb von 2 h an
    `plattform_abgleich.py bauen --ordner $O --bauliste <link1> <link2> … --register $O/register.json` übergeben
    (lokale Dateien gehen auch). Nicht bearbeitete GUIDs → Abbruch, Bauliste für diese neu abrufen (`--trotzdem` nur
    nach Rücksprache). IFC (`--ifc`, Konnektor `modell_exportieren {modellarten: ["plattform"]}`) nur nötig für
    Verbund-Unterteile mit eigener GUID (Entscheidung Mudi 19.11.2026: Kopf + Unterteile); ohne IFC wird ein Verbund
    ein Morph mit allen Körpern. Vor dem Bauen `netz_pruefen` (nicht mannigfaltige / offene Kanten) im Trockenlauf
    dem Zeichner zeigen; `status fehler` und reine Hüllen stehen in ausgelassen.json mit Grund. Rest des Schritts
    (Trockenlauf → Ja → --ausfuehren, Stempel, Rücklese, Plattform-GUID-Eigenschaft) beibehalten.
    Im Abschnitt „Live-Lauf BFW07 19.11.2026“ die Zeilen „Bauliste liefert nur die konvexe Hülle … → modell_exportieren
    + bauen“ und „nur ~20 GUIDs je Aufruf“ als überholt kennzeichnen (Verweis auf neue Bauliste, PR 404, Staging
    20.11.2026, bis 200 GUIDs, Marker <!-- 2026-11-20 -->); den Rat „konvexe Hülle → IFC“ nirgends mehr als aktuellen
    Weg stehen lassen. Unter „Bekannt / offen“ ergänzen: neuer Weg noch nicht live gegen Archicad gelaufen.
    Danach Endprüfung und Commit: `git -c user.email=mozzi666@gmail.com commit` mit Trailer
    „Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>“ (gilt auch für Commits aus Task 1/2). Kein Push,
    kein Branchwechsel.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q && grep -c "2026-11-20" skills/archicad/recipes/plattform-abgleich.md && grep -n "bauliste_link" skills/archicad/recipes/plattform-abgleich.md && python3 -I skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py bauen --help | grep -E "trotzdem|bauliste" && git status --short && git log --oneline -4</automated>
  </verify>
  <done>Rezept beschreibt den Link-Ablauf, netz_pruefen und IFC nur für Unterteile; alter Hüllen-Rat als überholt markiert; Tests grün; Arbeitsbaum sauber, Commits mit Mail mozzi666@gmail.com und Co-Authored-By-Trailer, nichts gepusht.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Plattform-Link → Skript | heruntergeladene Bauliste-JSON (2 h gültiger SAS-Link) ist fremde Eingabe |
| Skript → Archicad | Morphs werden aus fremden Netzen erzeugt |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-dbc-01 | Spoofing/Tampering | bauliste_laden Links | mitigate | nur https (bestehendes _nur_https, http:// → Fehler); curl nur als Argumentliste, nie über die Shell |
| T-dbc-02 | Tampering | Bauliste-Inhalt | mitigate | nur json.loads, kein eval; status "fehler"/Hülle werden nie gebaut; Format-Prüfung sab-bauliste/ |
| T-dbc-03 | Tampering | unvollständige Pakete | mitigate | nicht_bearbeitet bricht vor jedem Archicad-Aufruf ab (außer --trotzdem) |
| T-dbc-04 | Elevation/Integrity | Archicad-Modell | mitigate | unverändert: ohne --ausfuehren nur Trockenlauf, Projektname-Abgleich, Rücklese |
| T-dbc-05 | Information Disclosure | Links in Logs/Rezept | accept | Links laufen nach 2 h ab; Rezept enthält keine echten Links oder Projektdaten |
</threat_model>

<verification>
- `PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q` grün
- `git diff --stat main` zeigt nur die drei Dateien (plus Planungsdateien)
- Kein Netzwerk- oder Archicad-Zugriff in Tests (herunterladen per monkeypatch, FakeSenden)
</verification>

<success_criteria>
- Anforderungen (1)–(6) des Auftrags umgesetzt: bauliste_laden, bauliste_netze, Quellen-Priorität a–d mit quelle, CLI --bauliste N + Abbruch/--trotzdem, Kennzahlen/netz_pruefen, Rezept mit Marker 2026-11-20
- Alter Aufruf (eine Datei, --ifc) funktioniert unverändert
- Commits lokal auf Branch bauliste-netze, nicht gepusht
</success_criteria>

<output>
Create `.planning/quick/261120-dbc-bauliste-netze/261120-dbc-SUMMARY.md` when done
</output>
