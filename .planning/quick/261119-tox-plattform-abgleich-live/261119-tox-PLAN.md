---
phase: quick-261119-tox
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
  - skills/archicad/scripts/plattform_abgleich/sab_zuordnung.json
  - core/tests/test_plattform_abgleich_skript.py
  - skills/archicad/recipes/plattform-abgleich.md
  - skills/archicad/SKILL.md
autonomous: true
requirements: [QUICK-261119-tox]

must_haves:
  truths:
    - "archicad-lesen findet benutzerdefinierte Eigenschaften auch bei propertyType „Custom“ (Tapir 1.7.2) und damit Paare über „Plattform-GUID“"
    - "archicad-lesen holt Elemente je Typ über API.GetElementsByType und zählt keine 2D-Elemente mehr"
    - "herunterladen funktioniert auch mit python.org-Python auf macOS (SSL-Fehler → curl-Fallback)"
    - "`bauen` baut Plattform-Bauteile als Morphs mit echter Geometrie, Ebene, Geschoss, Klasse, Umbau-Status, Stempel und Rücklese; ohne --ausfuehren nur Trockenlauf"
    - "`stempeln` reserviert nicht, schreibt die volle Wertform und meldet unklassifizierte Elemente als nicht möglich (6703)"
    - "Rezept und SKILL.md nennen den Live-Lauf BFW07 19.11.2026 und die Trigger-Sätze"
    - "`python3 -m pytest core/tests -q` ist grün"
  artifacts:
    - path: "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py"
      provides: "Befehle inkl. neuem `bauen`; Archicad.api; ifc_netze, zerlegen, morph_body, bauplan, bauen"
      contains: "def cmd_bauen"
    - path: "skills/archicad/scripts/plattform_abgleich/sab_zuordnung.json"
      provides: "IFC-Klasse → SAB-Klasse (Kopie der Vorlage)"
    - path: "core/tests/test_plattform_abgleich_skript.py"
      provides: "Tests ohne Archicad (Skript per importlib)"
    - path: "skills/archicad/recipes/plattform-abgleich.md"
      provides: "Abschnitt „Live-Lauf BFW07 19.11.2026“, Schritt 7 = `bauen`"
  key_links:
    - from: "cmd_bauen"
      to: "TapirCommand.CreateMorphs / ELM_SAB.SetLayerOfElements / API.SetClassificationsOfElements / API.SetPropertyValuesOfElements"
      via: "Archicad.befehl / Archicad.elm / Archicad.api in Paketen zu 25"
      pattern: "CreateMorphs"
    - from: "archicad_lesen"
      to: "API.GetElementsByType"
      via: "Archicad.api je Typ aus TYPEN"
      pattern: "GetElementsByType"
---

<objective>
Den Plattform-Abgleich nach dem ersten Live-Lauf (BFW07, 19.11.2026) live-tauglich machen („Projekt synchronisieren“):
vier Fehler im Skript beheben, die live bewährte Vorlage `morph_netz_bauen.py` als Befehl `bauen` in sauberen Funktionen
übernehmen, `stempeln` an die Live-Erkenntnisse anpassen, Tests ohne Archicad, Rezept + SKILL.md nachziehen.

Purpose: Der Ablauf soll beim nächsten Projekt ohne Ad-hoc-Skripte laufen.
Output: geändertes Skript, neue sab_zuordnung.json, neue Testdatei, aktualisiertes Rezept und SKILL.md — drei Commits.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@./CLAUDE.md
@skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
@.planning/quick/261119-tox-plattform-abgleich-live/morph_netz_bauen.py
@.planning/quick/261119-tox-plattform-abgleich-live/stempeln.py
@.planning/quick/261119-tox-plattform-abgleich-live/sab_zuordnung.json
@skills/archicad/recipes/plattform-abgleich.md

Arbeitsort: /Users/ap/.scratch/elmonkey/diff-plattform-wt, Zweig `elm-diff-plattform`. NUR die fünf Dateien aus
files_modified ändern. Nur Standardbibliothek. Deutsche Kommentare/Docstrings im Stil des bestehenden Codes.
Vor dem ersten Commit: `git config user.email mozzi666@gmail.com` (lokal, nicht --global). Nicht pushen.
Jede Commit-Nachricht endet mit der Zeile `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
Nur die eigenen Dateien stagen (`git add <pfad>`), nie `git add -A` — im Worktree liegen fremde ungetrackte uv.lock.

<interfaces>
Bestehend in plattform_abgleich.py (bleibt kompatibel):
- `class Fehler(RuntimeError)`
- `class Archicad: __init__(self, senden)`, `befehl(name, params=None, ns="TapirCommand") -> dict` (ExecuteAddOnCommand,
  gibt addOnCommandResponse zurück), `elm(name, params=None)` (ns ELM_SAB)
- `_http(port) -> senden(body: dict) -> dict`, `_pakete(liste, n=PAKET)`, `_lesen(pfad)`, `_schreiben(pfad, daten)`,
  `_nur_https(link)`, `eigenschaften_katalog(ac, alle=False) -> {„Gruppe.Name“: Definition}`
- `archicad_lesen(ac, typen=TYPEN, alle_eigenschaften=False) -> dict` mit Schlüssel `elemente` je
  {guid, ifcId, typ, name, geschoss, eigenschaften}
- `eigenschaften_schreiben(ac, eintraege, katalog, *, teamwork, ausfuehren) -> {ausgefuehrt, moeglich,
  nicht_moeglich, geschrieben, fehler}`
- `main(argv)` mit Hilfsfunktion `neu(name, fn, **extra)` für Unterbefehle
Aus elmonkey.abgleich (als `ab`): `ab.FORMAT`, `ab.PLATTFORM_GUID`, `ab.gleich(a, b)`, `ab.guid_norm(g)`.
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Live-Fehler im Skript beheben (Custom-Eigenschaften, Typen, SSL, stempeln) + Tests</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - eigenschaften_katalog übernimmt Einträge mit propertyType „Custom“ und „UserDefined“, nicht „BuiltIn“ (ohne alle=True)
    - archicad_lesen mit gefälschtem senden: ruft nie GetAllElements, ruft API.GetElementsByType genau einmal je Typ aus
      der übergebenen typen-Liste mit {"elementType": typ}; jedes gelieferte Element trägt `typ` = abgefragter Typ,
      auch wenn GetDetailsOfElements.type etwas anderes meldet
    - Archicad.api sendet {"command": name, "parameters": params} (nicht ExecuteAddOnCommand) und gibt result zurück
    - Fehler code 4001 ohne „no open project“: schlafen(15) und erneut, nach 60 Versuchen Fehler; 4001 mit „no open
      project“ sofort Fehler (schlafen injiziert, Test ohne echtes Warten)
    - herunterladen: urlopen wirft URLError mit CERTIFICATE_VERIFY_FAILED → subprocess.run(["curl","-sS","--fail",link])
      wird aufgerufen (monkeypatch), Ergebnis als JSON zurück; http:// wird weiterhin abgewiesen
    - stempeln (cmd_stempeln über gefälschtes Archicad): sendet kein ReserveElements, auch wenn isTeamwork true;
      SetPropertyValuesOfElements-Werte haben die Form {type:"string",status:"normal",value}; KI-Wert beginnt mit
      „KI generiert “ + heutiges Datum; ein Element ohne Klassifizierung landet in nicht_moeglich mit Grund
      „nicht klassifiziert (6703)“ und wird nicht geschrieben
  </behavior>
  <action>
Testdatei zuerst anlegen (RED), dann implementieren (GREEN). Die Testdatei lädt das Skript per importlib.util.spec_from_file_location
aus Path(__file__).resolve().parents[2] / "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py" (einmal als
Modul-Fixture/Modulvariable). Ein gemeinsamer Test-Fake `FakeSenden` (callable, protokolliert jeden Body in einer Liste,
beantwortet Befehle aus einem Dict {Befehlsname: Antwort oder Funktion(params)}; Befehlsname = commandName bei
ExecuteAddOnCommand, sonst command) wird von allen Tests (auch Task 2) genutzt.

Änderungen in plattform_abgleich.py:
(1) eigenschaften_katalog: Bedingung auf `p.get("propertyType") in ("UserDefined", "Custom")` — Kommentar: Tapir 1.7.2 meldet
benutzerdefinierte als „Custom“ (live BFW07 19.11.2026: 230 Custom, 0 UserDefined; vorher 0 Eigenschaften, keine Paare).
Gleiches gilt für alle Aufrufer (stempel_lesen, cmd_stempeln) automatisch.
(2) Archicad bekommt Konstruktor-Parameter `schlafen=time.sleep` und eine private Methode `_rufen(body)`, über die befehl und
die neue Methode `api(name, params=None) -> dict` senden: bei `succeeded` false und error.code == 4001 und „open project“
nicht im Fehlertext → schlafen(15), bis 60 Versuche (15 min), danach Fehler("… Archicad blieb 15 min gesperrt"). `api` sendet
{"command": name, "parameters": params or {}} und gibt `result` (oder {}) zurück; sonst Fehler mit Befehlsname und gekürztem
error. archicad_lesen: statt GetAllElements + Filter je Typ in typen `ac.api("API.GetElementsByType", {"elementType": t})`
→ `elements` (je {"elementId": {...}}); GetDetailsOfElements/GetIFCIdsOfElements/GetPropertyValuesOfElements weiter in Paketen,
aber ohne Typ-Filter, `typ` = t. Kommentar: GetDetailsOfElements.type entspricht nicht den TYPEN-Namen (live 89.823 Elemente
inkl. 2D gezählt).
(3) herunterladen: urlopen in try; bei urllib.error.URLError (oder ssl.SSLError), deren Text „CERTIFICATE_VERIFY_FAILED“
enthält, Fallback `subprocess.run(["curl", "-sS", "--fail", link], capture_output=True, timeout=900)` — Link vorher durch
_nur_https, nie shell=True; returncode ≠ 0 → Fehler mit stderr gekürzt; stdout als UTF-8-JSON zurück. Andere Fehler weiterreichen.
(5) eigenschaften_schreiben: neuer Keyword-Parameter `reservieren: bool = True`; ReserveElements nur bei teamwork and
reservieren. Wertform beim Schreiben {"type": "string", "status": "normal", "value": …} (wie Vorlage stempeln.py).
Neue Funktion `klassifiziert(ac, guids) -> set[str]`: Systeme über ac.api("API.GetAllClassificationSystems") →
classificationSystems[].classificationSystemId; dann paketweise ac.api("API.GetClassificationsOfElements",
{"elements": [{"elementId": {"guid": g}}], "classificationSystemIds": [{"classificationSystemId": s} …]}) →
elementClassifications[n].classificationIds[] — Element zählt als klassifiziert, wenn irgendein Eintrag
classificationId.classificationItemId.guid trägt. cmd_stempeln: vor eigenschaften_schreiben die nicht klassifizierten
GUIDs herausnehmen und als {"archicad", "plattformGuid", "grund": "nicht klassifiziert (6703)"} in nicht_moeglich der Ausgabe
melden; KI-Wert = "KI generiert " + datetime.date.today().isoformat() statt „ja“; Aufruf mit reservieren=False
(Kommentar: Reservieren löst in Teamwork beim Nutzer einen Sendeversuch aus, frisch gebaute gehören uns ohnehin).
archicad-schreiben bleibt bei reservieren=True. Docstring-Kopf des Moduls um die Zeile für `bauen` erst in Task 2 ergänzen.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && python3 -m pytest core/tests/test_plattform_abgleich_skript.py -q && python3 -m pytest core/tests -q</automated>
  </verify>
  <done>Alle neuen Tests aus behavior grün, übrige core-Tests grün; Commit „fix(plattform-abgleich): Custom-Eigenschaften, Elemente je Typ, SSL-Fallback, stempeln ohne Reservierung“ mit Co-Authored-By-Zeile.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Befehl `bauen` aus der Vorlage morph_netz_bauen.py (Funktionen + Trockenlauf-Test)</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, skills/archicad/scripts/plattform_abgleich/sab_zuordnung.json, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - zerlegen: zwei Würfel (je 12 Dreiecke), die sich genau eine Kante teilen (gemeinsame Punktindizes, Kante mit 4 Nachbarn) → 2 Körper mit je 12 Dreiecken und je 8 eigenen Punkten; entartete Dreiecke (zwei gleiche Punkte) fallen weg
    - morph_body: Würfel mit nach innen zeigenden Flächen → Ausgabe-Volumen (volumen über vertices/polygons) positiv; bodyType "Solid", vertices als {x,y,z}, polygons als {vertexIds:[…]}
    - zuordnung_laden: „IfcPlate?“ wird als „IfcPlate“ gefunden, „_hinweis“ ist kein Schlüssel; status_archicad: Abbruch→ToBeDemolished, Bestand→Existing, Neubau→New, sonst None
    - bauplan: Bauteil ohne Netz → ausgelassen „keine Geometrie im IFC“; IFC-Klasse ohne Zuordnung oder Klasse nicht im Archicad-System bzw. ohne Status → ausgelassen „Klasse/Status fehlt (…)“; --nur/--ohne filtern; Geschoss = höchstes Geschoss mit level ≤ Unterkante (sonst das unterste)
    - bauen-Trockenlauf mit Mini-IFC (zwei Bauteile mit je einem IFCTRIANGULATEDFACESET, eines mit Unterteil über IFCRELAGGREGATES): Rückgabe nennt zu_bauen 2, schreibt nur ausgelassen.json; das gefälschte senden hat KEIN CreateMorphs, SetLayerOfElements, SetClassificationsOfElements, SetPropertyValuesOfElements erhalten
  </behavior>
  <action>
sab_zuordnung.json = exakte Kopie von .planning/quick/261119-tox-plattform-abgleich-live/sab_zuordnung.json neben das Skript
(cp, Inhalt unverändert). Dann Tests (RED), dann Implementierung (GREEN) in plattform_abgleich.py als neuer Abschnitt
„IFC → Morph (Befehl bauen)“, Logik aus der Vorlage übernommen, aber lesbar zerlegt und ohne festen Pfad
(~/.scratch/…/sab_klassen.json und die feste System-GUID der Vorlage entfallen):
- Konstanten: `BAU_PAKET = 25`, `SAB_SYSTEM = "SAB_Klassifizierung_29"`, `STATUS = {"Abbruch": "ToBeDemolished", "Bestand":
  "Existing", "Neubau": "New"}`, `ZUORDNUNG = Path(__file__).with_name("sab_zuordnung.json")`, IFC-Base64-Alphabet aus der Vorlage.
- `guid_lang(g22) -> str` (Vorlage expand, Großbuchstaben-UUID). `ifc_netze(pfad) -> dict[str, list[tuple[list, list]]]`:
  STEP-Zeilen wie Vorlage (latin-1, Regex), GlobalId→GUID für Nicht-IFCREL/Pset/Quantity, IfcRelAggregates-Kinder rekursiv
  dem Elternteil zugeschlagen, Netze aus IFCPRODUCTDEFINITIONSHAPE → Repräsentationen → IFCTRIANGULATEDFACESET
  (Punkte aus der Punktliste, Flächen 1-basiert → 0-basiert); nur GUIDs mit mindestens einem Netz im Ergebnis.
- `zerlegen(P, F)` wie Vorlage (Union-Find über Kanten mit genau 2 Nachbarn, entartete weg, eigene Punktlisten je Körper).
- `volumen(P, F)` (Vorzeichen-Volumen) und `morph_body(netze) -> (body, box)`: für jedes Netz zerlegen, Körper mit negativem
  Volumen umdrehen, Punkte/Flächen anhängen; box = [xmin, ymin, zmin, xmax, ymax, zmax].
- `zuordnung_laden(pfad=ZUORDNUNG)` (Schlüssel ohne „?“, ohne „_“-Schlüssel), `status_archicad(text)`, `geschoss_index(stories, z)`.
- `bauplan(bauliste, netze, register, zuordnung, klassen, stories, nur=None, ohne=()) -> (plan, ausgelassen)` — rein, ohne
  Archicad: bauliste["bauteile"][] mit plattformGuid, name, ebene, ifc.klasse; register["neu"][] mit guid, status; klassen =
  {Klassen-id: item-GUID}. GUID-Vergleich über Großschreibung. Plan-Eintrag wie Vorlage: g, name, ebene, klasse, status, soll,
  morph {basePoint 0,0,0, body, floorIndex}.
- `sab_klassen(ac, name=SAB_SYSTEM) -> (system_id, {id: guid})`: System per Name aus API.GetAllClassificationSystems (fehlt →
  Fehler), Baum aus API.GetAllClassificationsInSystem rekursiv (classificationItem.classificationItemId.guid, .id, children);
  wo id fehlt, API.GetDetailsOfClassificationItems {classificationItemIds:[{classificationItemId:{guid}}]} nach id nachholen.
- `bauen(ac, ordner, ifc, bauliste, register, zuordnung, *, nur, ohne, vorhanden, ausfuehren, projekt_soll=None) -> dict`:
  GetProjectInfo/GetStories (Tapir), sab_klassen, bauplan, ausgelassen.json schreiben. Ohne ausfuehren: Rückgabe
  {trockenlauf: True, projekt, zu_bauen, ausgelassen, ebenen, klassen, status, dreiecke} — nichts weiter senden. Mit ausfuehren:
  wenn projekt_soll gesetzt und ≠ projectName → Fehler, nichts gebaut. Property-IDs über ac.api("API.GetPropertyIds") wie
  Vorlage (Plattform-GUID, KI generiert, BuiltIn Category_RenovationStatus; fehlende → Fehler). Je Paket BAU_PAKET: neue (nicht in
  vorhanden {plattformGuid: archicadGuid}) per befehl("CreateMorphs", {"morphsData": […]}), vorhandene nur fertigstellen;
  angelegt.json sofort schreiben; elm("SetLayerOfElements", {elements, layerName, "reserve": False}) je Ebene (Kommentar: sonst
  Sendeversuch beim Nutzer je Paket); api("API.SetClassificationsOfElements"); api("API.SetPropertyValuesOfElements") mit
  Stempel/KI-Text {type:"string",status:"normal",value} und Status {type:"singleEnum",status:"normal",value:{type:
  "nonLocalizedValue",nonLocalizedValue}}; Rücklese API.Get3DBoundingBoxes (≤ 0,002 m), API.GetPropertyValuesOfElements,
  API.GetClassificationsOfElements, Tapir GetDetailsOfElements (isClosed, floorIndex) wie Vorlage; nach jedem Paket gebaut.json
  ([{plattformGuid, archicadGuid}]) und ruecklese.json ({rueck, fehler}). Die 4001-Warteschleife kommt aus Archicad._rufen (Task 1).
  Rückgabe {gebaut, stimmt, fehler, geschlossen, naechster_schritt: Paare in ergebnis.json basis.paare ergänzen, hochladen,
  archicad_abgleich_melden}.
- `cmd_bauen(o)`: liest Dateien, --nur/--ohne als Komma-Listen, --vorhanden optional, projekt_soll aus ordner/archicad.json falls
  vorhanden; main: `neu("bauen", cmd_bauen, ifc={"required": True}, bauliste={"required": True}, register={"required": True},
  zuordnung={"default": ""}, nur={"default": ""}, ohne={"default": ""}, vorhanden={"default": ""}, ausfuehren={"action":
  "store_true"})`. Modul-Docstring um die Aufrufzeile für `bauen` und die Dateien angelegt.json/gebaut.json/ruecklese.json/
  ausgelassen.json ergänzen.
Mini-IFC im Test als Text in tmp_path schreiben (ISO-10303-21 DATA-Abschnitt mit IFCCARTESIANPOINTLIST3D,
IFCTRIANGULATEDFACESET, IFCSHAPEREPRESENTATION, IFCPRODUCTDEFINITIONSHAPE, zwei Produkten mit 22-stelligen GlobalIds aus dem
Alphabet, einem IFCRELAGGREGATES); der Fake antwortet auf GetProjectInfo, GetStories, API.GetAllClassificationSystems,
API.GetAllClassificationsInSystem.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && python3 -m pytest core/tests -q && python3 skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py bauen --help</automated>
  </verify>
  <done>Alle Tests grün, `bauen --help` zeigt die Optionen, sab_zuordnung.json identisch zur Vorlage (cmp); Commit „feat(plattform-abgleich): Befehl bauen (Morphs aus Plattform-IFC, Klasse, Status, Stempel, Rücklese)“ mit Co-Authored-By-Zeile.</done>
</task>

<task type="auto">
  <name>Task 3: Rezept und SKILL.md auf Live-Stand BFW07 19.11.2026</name>
  <files>skills/archicad/recipes/plattform-abgleich.md, skills/archicad/SKILL.md</files>
  <action>
Rezept: Kopfzeile-Kommentar „<!-- 2026-11-10, noch NICHT live gelaufen -->“ → „<!-- 2026-11-10, live BFW07 19.11.2026 -->“.
Direkt unter der Überschrift eine Zeile Trigger: „Auslöser: „Projekt synchronisieren“, „Plattform und Archicad abgleichen“.“
Schritt 7 umstellen: Konnektor `modell_exportieren {modellarten: ["plattform"]}` liefert das Plattform-IFC (Archicad-Koordinaten),
Bauliste (`archicad_abgleich_bauliste`, nur ~20 GUIDs je Aufruf) liefert Ebene/Name/IFC-Klasse, Register liefert den Status;
dann `plattform_abgleich.py bauen --ordner $O --ifc <plattform.ifc> --bauliste bauliste.json --register register.json
[--zuordnung …] [--nur …] [--ohne …] [--vorhanden vorhanden.json]` als Trockenlauf, nach dem Ja `--ausfuehren`; `bauen` stempelt
selbst, `stempeln` nur noch für von Hand gebaute; ausgelassen.json dem Zeichner nennen. Im Abschnitt „Bekannt / offen“ den
Punkt „Noch nicht gegen ein laufendes Archicad geprüft …“ durch Verweis auf den Live-Lauf ersetzen. Neuer Abschnitt
„## Live-Lauf BFW07 19.11.2026 <!-- 2026-11-19 -->“ mit Stichpunkten: Bauliste liefert nur konvexe Hülle (vereinfacht,
395/432 unbrauchbar) → echte Form über modell_exportieren + `bauen`; Bauliste nur ~20 GUIDs je Aufruf; Ebenen-Bauteile (nur in
Skizzen-Ebenen) und Vorschläge kennt der Abgleich nicht; Paare melden: ergebnis.json basis.paare um gebaut.json ergänzen,
hochladen, archicad_abgleich_melden (Vorschau, dann bestaetigt); Cmd+S/Senden in Teamwork scheitert an fremder Reservierung
der Ebenen-Einstellungen — neue Ebene erst senden, wenn freigegeben; Eigenschaften an unklassifizierten Elementen → 6703;
Tapir 1.7.2 meldet eigene Eigenschaften als „Custom“; GetDetailsOfElements.type ≠ Typnamen → Elemente je Typ; SSL-Fehler
python.org-Python → curl-Fallback; SetLayerOfElements mit reserve:false; Messwerte archicad-lesen 5–11 min bei 90k Elementen,
bauen 431 Morphs ≈ 25 min. Abschnitt Diff-Checker bleibt unverändert (der ist weiterhin nicht live gelaufen).
SKILL.md Zeile 134: „noch nicht live gelaufen“ → „live BFW07 19.11.2026“, Trigger-Sätze „Projekt synchronisieren“ /
„Plattform und Archicad abgleichen“ in dieselbe Zeile, Befehl `bauen` kurz nennen, Datums-Marker auf <!-- 2026-11-19 -->.
Keine projektbezogenen GUIDs/Ebenennamen ins Rezept (nur Zahlen des Laufs).
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && grep -c "live BFW07 19.11.2026" skills/archicad/recipes/plattform-abgleich.md skills/archicad/SKILL.md && grep -q "Projekt synchronisieren" skills/archicad/SKILL.md && grep -q "plattform_abgleich.py bauen" skills/archicad/recipes/plattform-abgleich.md && ! grep -q "noch NICHT live gelaufen -->" <(head -1 skills/archicad/recipes/plattform-abgleich.md) && python3 -m pytest core/tests -q</automated>
  </verify>
  <done>Rezept-Kopf, Trigger, Schritt 7, neuer Live-Abschnitt und SKILL.md-Zeile aktualisiert; Commit „docs(skill): Plattform-Abgleich live BFW07 19.11.2026 (bauen, Trigger)“ mit Co-Authored-By-Zeile; nichts gepusht.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Plattform-Links → Skript | Download-/Ablage-Links aus dem Konnektor (untrusted Strings) |
| Plattform-IFC → Skript | IFC-Datei wird geparst (untrusted Inhalt) |
| Skript → Archicad (Teamwork) | Schreibende Befehle im geteilten Projekt |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-tox-01 | Tampering/Elevation | herunterladen curl-Fallback | mitigate | nur https via _nur_https, Argumentliste ohne shell=True, Timeout |
| T-tox-02 | Tampering | bauen in falschem Projekt | mitigate | Projektname gegen archicad.json prüfen vor --ausfuehren; ohne --ausfuehren nur Trockenlauf |
| T-tox-03 | Denial of Service | ifc_netze Regex-Parser | accept | Datei stammt aus eigener Plattform, lokaler Lauf |
| T-tox-04 | Repudiation | gebaute Elemente | mitigate | Stempel Plattform-GUID + „KI generiert <Datum>“ + Rücklese; gebaut/ruecklese.json je Paket |
| T-tox-05 | Tampering | Teamwork-Reservierung | mitigate | reserve:false / reservieren=False nur für frisch gebaute; nie löschen |
</threat_model>

<verification>
`python3 -m pytest core/tests -q` grün; `git log --oneline -3` zeigt drei Commits mit Autor-Mail mozzi666@gmail.com;
`git status --short` zeigt außer den fremden uv.lock und dem Quick-Ordner nichts Ungestagtes aus files_modified.
</verification>

<success_criteria>
Alle sieben Änderungspunkte des Auftrags umgesetzt, Tests aus der Aufgabenbeschreibung vorhanden und grün, nur die fünf
erlaubten Dateien geändert, nicht gepusht.
</success_criteria>

<output>
Create `.planning/quick/261119-tox-plattform-abgleich-live/261119-tox-SUMMARY.md` when done
</output>
