---
phase: quick-261006-lod
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp
  - ELM_SAB_Tapir/Sources/AddOnVersion.hpp
  - ELM_SAB_Tapir/Sources/AddOnMain.cpp
  - reference/mcp-extension.md
autonomous: true
requirements: [QUICK-261006-lod]

must_haves:
  truths:
    - "CreateWallOpenings meldet sourceDeleted:true nur, wenn das Quellsymbol nach ACAPI_Element_Delete per Header-Lesen wirklich weg ist"
    - "Überlebt das Quellsymbol trotz NoError, steht sourceDeleted:false und sourceKeptReason nennt 'ACAPI_Element_Delete meldete Erfolg, Element existiert weiter (Reservierung?)' samt Code"
    - "Ein Quellsymbol, das im Teamwork nicht reservierbar war (conflicts), wird gar nicht erst zum Löschen geschickt und bekommt einen eigenen sourceKeptReason"
    - "Freigabe nach dem Lauf schickt nur noch existierende Elemente an ACAPI_Teamwork_ReleaseElements (gelöschte Quellen nicht)"
    - "GetAddOnVersion liefert 0.9.21; CreateWallOpenings ist mit Version 0.9.21 registriert"
    - "CI auf ci-status zeigt für den neuen SHA grüne Builds (AC27/28/29 × Mac/Win)"
  artifacts:
    - path: "ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp"
      provides: "Schritt 10 mit Existenzprüfung, sourceConflict-Behandlung, gefilterte Freigabe"
      contains: "LoadElementHeaderByGuid"
    - path: "ELM_SAB_Tapir/Sources/AddOnVersion.hpp"
      contains: "#define ELM_SAB_VERSION \"0.9.21\""
    - path: "reference/mcp-extension.md"
      contains: "## ELM_SAB 0.9.21"
  key_links:
    - from: "CreateOne() Schritt 10"
      to: "LoadElementHeaderByGuid (CommandBase.hpp:169)"
      via: "Header-Lesen nach ACAPI_Element_Delete"
      pattern: "LoadElementHeaderByGuid \\(req\\.sourceGuid"
    - from: "Execute() Reservierungsblock"
      to: "OpeningRequest.sourceConflict"
      via: "conflicts.ContainsKey(sourceGuid)"
      pattern: "sourceConflict"
---

<objective>
ELM_SAB 0.9.21: Das Quellsymbol-Löschen in `ELM_SAB.CreateWallOpenings` genauso absichern wie 0.9.20 `DeleteElements` (Commit 80a43b4). Nach `ACAPI_Element_Delete` per Header prüfen, ob das Quellsymbol wirklich weg ist; sonst `sourceDeleted=false` mit klarem `sourceKeptReason`. Reservierungslücken (Quelle in Konflikt; reserve:false) konsistent behandeln.

Purpose: Live-Befund 2026-10-06 (THN) — `ACAPI_Element_Delete` meldet NoError, löscht unreservierte Teamwork-Elemente aber still nicht. CreateWallOpenings hätte dann `sourceDeleted:true` gemeldet, obwohl das KI-Symbol noch neben der neuen Öffnung steht (Doppelung im Modell).
Output: Ein Fix-Commit (Code + Version), ein Docs-Commit, push origin main, CI grün. KEIN Bundle installieren.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp
@reference/mcp-extension.md

Muster: `git show 80a43b4 -- ELM_SAB_Tapir/Sources/ElementCommands.cpp` (DeleteElementsCommand 0.9.20). CI-Prüfung wie in .planning/quick/261006-j8h-elm-sab-0-9-20-deleteelements-reserve-ve/261006-j8h-PLAN.md Task 2.

<interfaces>
Aus ELM_SAB_Tapir/Sources/CommandBase.hpp:169:
  bool LoadElementHeaderByGuid (const API_Guid& elementGuid, API_Elem_Head& elementHeader);   // true = Element existiert

CreateWallOpeningsCommand.cpp (Stand 0.9.18/0.9.20):
- struct OpeningRequest (Z. 122–136): sourceGuid, wallGuid, cx, cy, width, height, hasBottom, bottomElevation, hasTop, topElevation, round, hasElementId, elementId, propertyValues
- struct OpeningResult (Z. 138–154): ... bool sourceDeleted; GS::UniString sourceKeptReason; ...
- static void CreateOne (const OpeningRequest& req, bool classify, API_Guid classificationItemGuid, API_Guid kiStampPropertyGuid, API_Guid elementIdPropertyGuid, const GS::UniString& kiStampOverride, bool deleteSource, OpeningResult& res)  — Schritt 10 Z. 532–558, endet mit:
    toDelete.Push (req.sourceGuid);
    if (ACAPI_Element_Delete (toDelete) == NoError) res.sourceDeleted = true;
    else res.sourceKeptReason = "ACAPI_Element_Delete abgelehnt";
- Execute: reserve (Std. true), teamwork = ACAPI_Teamwork_HasConnection (); Reservierungsblock Z. 645–664 sammelt Wirtswände + (bei deleteSource) Quellen in `reserved`, Konflikte in `GS::HashTable<API_Guid, short> conflicts`; Wand-Konflikt-Schleife Z. 666–673; Freigabe Z. 704–705: `if (reserve && teamwork && !reserved.IsEmpty ()) ACAPI_Teamwork_ReleaseElements (reserved, false);`
- Response-Schema (Z. 96–108) ist locker (results.items = beliebiges Objekt) — keine Schemaänderung nötig.

AddOnMain.cpp Z. 1052–1055:
  err |= RegisterCommand<CreateWallOpeningsCommand> (elmSabCommands, "0.9.18", "ELM_SAB: Ersetzt KI-Durchbruch-Symbole ... Braucht Archicad 29.");
AddOnVersion.hpp Z. 11: #define ELM_SAB_VERSION "0.9.20"   (ADDON_VERSION "1.5.4" NICHT anfassen)

reference/mcp-extension.md: 0.9.18-Abschnitt ab Z. 1157; 0.9.20-Abschnitt Z. 1342–1372 (Dateiende), darin Bullet "**Bekannte Verwandte:** Das Quellsymbol-Löschen in `ELM_SAB.CreateWallOpenings` prüft nur den Rückgabewert und ist unverändert. Dort weiter per Rücklese prüfen."
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Schritt 10 absichern, Quell-Konflikte + Freigabe konsistent, Version 0.9.21</name>
  <files>ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp, ELM_SAB_Tapir/Sources/AddOnVersion.hpp, ELM_SAB_Tapir/Sources/AddOnMain.cpp</files>
  <action>
Befund der Reservierungsprüfung (Planungsanalyse, vom Executor kurz gegenzulesen): (a) Bei reserve:true + Teamwork wird die Quelle zwar mitreserviert, aber ein Konflikt auf der QUELLE wird nirgends ausgewertet — CreateOne schickt sie trotzdem an ACAPI_Element_Delete und meldet NoError als Erfolg. (b) Bei reserve:false wird die Quelle nie reserviert → genau der stille Nicht-Lösch-Fall. (c) Die Freigabe übergibt auch bereits gelöschte Quell-GUIDs. Fix in CreateWallOpeningsCommand.cpp, kein C++20 (keine designated initializers, kein std::format, kein contains()):

1. OpeningRequest um `bool sourceConflict = false;` erweitern (Kommentar: Quelle war im Teamwork nicht reservierbar).
2. In Execute direkt nach der bestehenden Wand-Konflikt-Schleife (Z. 666–673) eine Schleife ergänzen: wenn `deleteSource` und `requests[i].sourceGuid != APINULLGuid` und `conflicts.ContainsKey (requests[i].sourceGuid)` → `requests[i].sourceConflict = true`. requests wird vor runAll befüllt, ist also nicht const — passt.
3. Schritt 10 in CreateOne: nach der KI-Stempel-Prüfung (Z. 548–552) und VOR dem Delete: wenn `req.sourceConflict` → `res.sourceKeptReason = "Quellobjekt nicht reservierbar (anderer Nutzer, ausgeblendete Ebene oder Hotlink) - nicht geloescht"; return;`. Danach Delete-Ergebnis in `GSErrCode delErr` halten, dann `API_Elem_Head head = {}; const bool stillThere = LoadElementHeaderByGuid (req.sourceGuid, head);` und `res.sourceDeleted = !stillThere;`. Wenn stillThere: bei `delErr == NoError` → `sourceKeptReason = GS::UniString::SPrintf ("ACAPI_Element_Delete meldete Erfolg, Element existiert weiter (Reservierung?) - Code %d", (Int32) delErr)`; bei delErr != NoError → `GS::UniString::SPrintf ("ACAPI_Element_Delete abgelehnt - Code %d", (Int32) delErr)`. Ist das Element weg, aber delErr != NoError, trotzdem sourceDeleted=true (Wirklichkeit zählt, wie in 0.9.20). Hinweis im Reason bei reserve:false ist nicht nötig, „(Reservierung?)“ deckt es ab. Achtung: GS::UniString::SPrintf verwenden, nicht Printf (0.9.20 hat Printf überall auf SPrintf umgestellt). LoadElementHeaderByGuid stammt aus CommandBase.hpp — prüfen, ob es über CreateWallOpeningsCommand.hpp bereits eingebunden ist (grep `#include` im .hpp); falls nicht, `#include "CommandBase.hpp"` ergänzen. Die Hilfe muss innerhalb des `#ifdef ServerMainVers_2900`-Zweigs benutzt werden (dort liegt CreateOne bereits) — keine neuen ungenutzten Helfer außerhalb, Build läuft mit -Werror / /WX.
4. Kommentar über Schritt 10 ergänzen: „ELM_SAB 0.9.21 (2026-10-06, THN): ACAPI_Element_Delete meldet NoError, loescht unreservierte Teamwork-Elemente aber still nicht (siehe DeleteElements 0.9.20). Daher Existenz per Header pruefen.“ (ASCII-Umlaute wie im Rest der Datei).
5. Freigabe (Z. 704–705) wie in 0.9.20 nur für noch existierende Elemente: vor dem Release `reserved` auf GUIDs filtern, für die LoadElementHeaderByGuid true liefert und die nicht in `conflicts` stehen (fremd reservierte nie freigeben). Nur wenn die gefilterte Liste nicht leer ist, ACAPI_Teamwork_ReleaseElements aufrufen. Hinweis: Dieser Code liegt in Execute innerhalb von `#else` (AC29-Zweig) — passt.
6. AddOnVersion.hpp: ELM_SAB_VERSION "0.9.20" → "0.9.21". ADDON_VERSION unverändert lassen.
7. AddOnMain.cpp: RegisterCommand<CreateWallOpeningsCommand> Version "0.9.18" → "0.9.21"; Beschreibung am Ende ergänzen um „ ELM_SAB 0.9.21: sourceDeleted nur, wenn das Quellsymbol danach wirklich fehlt (Header-Pruefung); nicht reservierbare Quellen werden nicht geloescht.“ (keine Umlaute, wie die übrigen Beschreibungen).

Commit A (nur diese drei Dateien): `fix(ELM_SAB): CreateWallOpenings prüft Löschung des Quellsymbols 0.9.21` mit kurzem Body (Anlass THN 2026-10-06, Lücke analog DeleteElements 0.9.20, Quell-Konflikt + Freigabe bereinigt), letzte Zeile `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Commit-Autor lokal wie bisher (git config im Repo nicht ändern, falls schon gesetzt).
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && grep -c 'ELM_SAB_VERSION "0.9.21"' ELM_SAB_Tapir/Sources/AddOnVersion.hpp && grep -A1 'RegisterCommand<CreateWallOpeningsCommand>' ELM_SAB_Tapir/Sources/AddOnMain.cpp | grep -c '"0.9.21"' && grep -v '^\s*//' ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp | grep -c 'LoadElementHeaderByGuid' && grep -c 'sourceConflict' ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp && grep -c 'existiert weiter (Reservierung?)' ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp && ! grep -n 'UniString::Printf' ELM_SAB_Tapir/Sources/CreateWallOpeningsCommand.cpp && git log -1 --format=%s | grep -c '0.9.21'</automated>
  </verify>
  <done>Schritt 10 prüft per Header; sourceConflict verhindert Löschversuch an fremd reservierten Quellen; Freigabe nur für existierende, eigene Reservierungen; Versionen 0.9.21; Commit A lokal vorhanden.</done>
</task>

<task type="auto">
  <name>Task 2: Doku 0.9.20/0.9.21, Commit, Push, CI bis grün (max 4 Fix-Runden)</name>
  <files>reference/mcp-extension.md</files>
  <action>
1. In reference/mcp-extension.md im 0.9.20-Abschnitt das Bullet „**Bekannte Verwandte:** …“ ersetzen durch: „**Bekannte Verwandte:** Das Quellsymbol-Löschen in `ELM_SAB.CreateWallOpenings` hatte dieselbe Lücke — in 0.9.21 behoben (siehe unten).“
2. Am Dateiende neuen kurzen Abschnitt `## ELM_SAB 0.9.21 — CreateWallOpenings prüft Löschung des Quellsymbols <!-- 2026-10-06 -->` anfügen, Stil wie 0.9.20 (Bullets Anlass / Änderung / Folge für Aufrufer / Stand), Inhalt: Anlass = gleiche THN-Falle; Änderung = nach `ACAPI_Element_Delete` Existenzprüfung per Header, `sourceDeleted:true` nur wenn wirklich weg, sonst `sourceKeptReason` „ACAPI_Element_Delete meldete Erfolg, Element existiert weiter (Reservierung?) - Code N“; fremd reservierte Quelle (Konflikt bei `reserve:true`) wird nicht gelöscht, eigener Grund „Quellobjekt nicht reservierbar …“; Freigabe nur für noch existierende eigene Reservierungen; Befehlsversion 0.9.21. Folge: `reserve:false` mit `deleteSource:true` lässt im Teamwork das Quellsymbol typischerweise stehen — jetzt sichtbar statt still. Stand: gebaut (CI), noch NICHT live verifiziert. Dazu `### Testplan` mit: (1) GetAddOnVersion → 0.9.21; (2) THN-Testwand mit KI-Symbol, `deleteSource:true`, Standard `reserve` → Öffnung angelegt, `sourceDeleted:true`, GetElementEditState des Symbols exists:false; (3) dasselbe mit `reserve:false` → `sourceDeleted:false`, `sourceKeptReason` enthält „existiert weiter (Reservierung?)“, Symbol existiert; (4) Symbol von anderem Nutzer reserviert → Öffnung entsteht, `sourceKeptReason` „nicht reservierbar“, Symbol unverändert; (5) nach dem Lauf ist nichts mehr von uns reserviert; Cmd+Z nimmt Öffnung + Löschung in einem Schritt zurück. Hinweis aus Memory: Danis handgezeichnete Durchbrüche nie als Testobjekt — nur KI-gestempelte Testsymbole.
3. Commit B: `docs(ELM_SAB): 0.9.21 CreateWallOpenings Quell-Löschprüfung` mit letzter Zeile `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
4. `git push origin main`. Danach `git fetch origin ci-status && git show FETCH_HEAD:status.md` wiederholen, bis der SHA von HEAD (oder neuer) erscheint — warten per Monitor bzw. Hintergrundbefehl mit until-Schleife, kein Vordergrund-sleep. Erwartet: alle Builds (AC27/28/29 × Mac/Win) grün. Bei rotem Build: Log über `gh run view --log-failed` lesen, gezielt fixen (typisch: -Werror unused variable im AC27/28-Zweig, size_t→UIndex-Verengung unter MSVC /WX, fehlendes Include), als `fix(ELM_SAB): …` committen mit Co-Authored-By-Zeile, pushen, erneut prüfen. Maximal 4 Fix-Runden; danach stoppen und Ursache dokumentieren. KEIN Bundle installieren, Archicad nicht anfassen.
  </action>
  <verify>
    <automated>cd /Users/ap/.claude/repos/archicad-skill && grep -c '## ELM_SAB 0.9.21' reference/mcp-extension.md && grep -c 'in 0.9.21 behoben' reference/mcp-extension.md && git fetch -q origin main ci-status && test "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" && git log -2 --format=%B | grep -c 'Co-Authored-By: Claude Opus 5.5' && git show FETCH_HEAD:status.md | head -30</automated>
  </verify>
  <done>Doku zeigt 0.9.20-Verwandte als behoben und einen 0.9.21-Abschnitt mit Testplan; beide Commits auf origin/main mit Co-Authored-By-Zeile; ci-status/status.md nennt den neuen SHA mit grünen Builds für alle Ziele, oder nach 4 Runden ist sauber dokumentiert, woran es scheitert. Kein Bundle installiert.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| MCP-Aufrufer → ELM_SAB-Befehl | JSON-Parameter (GUIDs, deleteSource, reserve) steuern Löschung im Teamwork-Modell |
| Add-On → Teamwork-Server | Reservierung/Freigabe wirkt auf geteiltes Projekt |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-lod-01 | Repudiation | CreateOne Schritt 10 | mitigate | sourceDeleted nur nach Header-Existenzprüfung; Fehlercode im sourceKeptReason |
| T-lod-02 | Tampering | Quellsymbol fremd reserviert | mitigate | sourceConflict → kein Löschversuch an Elementen anderer Nutzer |
| T-lod-03 | Tampering | ACAPI_Teamwork_ReleaseElements | mitigate | nur eigene, existierende Reservierungen freigeben (conflicts ausgeschlossen) |
| T-lod-04 | Tampering | handgezeichnete Durchbrüche | accept | bestehende KI-Stempel-Pflicht vor Löschung bleibt unverändert |
</threat_model>

<verification>
- Alle grep-Gates aus Task 1 + 2 grün.
- CI ci-status: neuer SHA, alle Ziele grün.
- Kein Bundle installiert, keine Archicad-Interaktion.
</verification>

<success_criteria>
ELM_SAB 0.9.21 auf origin/main, CI grün; CreateWallOpenings meldet ein überlebendes Quellsymbol ehrlich als sourceDeleted:false mit Grund; Doku aktualisiert mit Testplan für die Live-Verifikation.
</success_criteria>

<output>
Create `.planning/quick/261006-lod-elm-sab-0-9-21-createwallopenings-quells/261006-lod-SUMMARY.md` when done
</output>
