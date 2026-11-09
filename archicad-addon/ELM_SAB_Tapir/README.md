# ELM_SAB_Tapir — Kombi-Add-On für Archicad

> **Drei Monkeys, funktionsgleich:** Archicad-Monkey (Add-On), Python-Monkey (Kern + MCP) und Plattform-Monkey (Schwarz Plattform) arbeiten gleich und werden immer angeglichen — eine Änderung an einem kommt im selben Zug in die beiden anderen. Siehe [README im Wurzelordner](../../README.md#drei-monkeys--funktionsgleich-immer-angeglichen).

Das hauseigene Archicad-Add-On von Schwarz Architekturbüro Nürnberg. Es enthält
**Tapir 1.7.2 vollständig** (seit ELMonkey 0.10.0; vorher 1.5.4) plus die SAB-eigenen Befehle in einem einzigen Bundle —
beide Befehls-Namensräume, `TapirCommand` und `ELM_SAB`, sind gleichzeitig verfügbar.

Basis ist [Tapir](https://github.com/ENZYME-APD/tapir-archicad-automation) von
ENZYME APD, das seinerseits auf Tibor Lorantfys
[archicad-additional-json-commands](https://github.com/tlorantfy/archicad-additional-json-commands)
aufbaut.

## Lizenzen / Licenses

ELMonkey enthält Code aus folgenden MIT-lizenzierten Projekten; die Lizenztexte liegen in diesem Ordner und gehören zu jeder Weitergabe (Quelltext und Bundles):

| Projekt | Lizenz | Datei |
|---|---|---|
| [Tapir](https://github.com/ENZYME-APD/tapir-archicad-automation) — Copyright (c) 2024 Enzyme APD | MIT | [`LICENSE-TAPIR`](LICENSE-TAPIR) |
| [archicad-additional-json-commands](https://github.com/tlorantfy/archicad-additional-json-commands) — Copyright (c) 2023 Tibor Lorántfy | MIT | [`LICENSE-ARCHICAD-ADDITIONAL-JSON-COMMANDS`](LICENSE-ARCHICAD-ADDITIONAL-JSON-COMMANDS) |
| Graphisoft Add-On-Vorlage (Basis des alten `ELM_SAB_Add-On`) — Copyright (c) 2020 GRAPHISOFT | MIT | [`../ELM_SAB_Add-On/LICENSE.md`](https://github.com/mozzi86/-archicad/tree/main/ELM_SAB_Add-On/LICENSE.md) |

## Download

Gebaut wird per GitHub Actions; die Artefakte liegen in der rollenden Vorabversion
[`elm-sab-tapir-latest`](https://github.com/mozzi86/ELMonkey/releases/tag/elm-sab-tapir-latest).

| Archicad | macOS | Windows |
|---|---|---|
| **29** | [ELM_SAB_Tapir_AC29_Mac.zip](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC29_Mac.zip) | [ELM_SAB_Tapir_AC29_Win.apx](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC29_Win.apx) |
| **28** | [ELM_SAB_Tapir_AC28_Mac.zip](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC28_Mac.zip) | [ELM_SAB_Tapir_AC28_Win.apx](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC28_Win.apx) |
| **27** | [ELM_SAB_Tapir_AC27_Mac.zip](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC27_Mac.zip) | [ELM_SAB_Tapir_AC27_Win.apx](https://github.com/mozzi86/ELMonkey/releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC27_Win.apx) |

Die Datei muss zur Archicad-Hauptversion passen. Ein AC29-Bundle lädt nicht in AC28 —
es erscheint dann gar nicht im Add-On-Manager, ohne Fehlermeldung. Die eigene Version
steht unter *Archicad → Über Archicad*.

## Installieren

**macOS**

1. Archicad mit `⌘Q` **komplett** beenden. Eine im Hintergrund weiterlaufende Instanz
   lädt sonst weiter das alte Add-On — daran ist schon mehr als ein Bundle-Tausch
   scheinbar wirkungslos geblieben.
2. ZIP entpacken, das `.bundle` nach `/Applications/Graphisoft/Archicad 29/Add-Ons/`
   legen (Pfad je Version anpassen).
3. Keine Sicherungskopien im `Add-Ons`-Ordner liegen lassen — Archicad versucht sie zu
   laden und meldet „Einige Add-Ons konnten nicht geladen werden".
4. Quarantäne-Flag entfernen, sonst blockiert macOS das Bundle stillschweigend:

   ```bash
   xattr -dr com.apple.quarantine "/Applications/Graphisoft/Archicad 29/Add-Ons/ELM_SAB_AC29_Mac.bundle"
   ```

**Windows**

1. Archicad beenden und im Task-Manager prüfen, dass kein `Archicad.exe` mehr läuft.
2. Die `.apx` nach `C:\Program Files\Graphisoft\Archicad 29\Add-Ons\` kopieren
   (Administratorrechte nötig).
3. Falls Windows die Datei als „aus dem Internet" markiert: Rechtsklick →
   *Eigenschaften* → *Zulassen* → *Übernehmen*.

Danach in Archicad unter **Optionen → Add-On-Manager** prüfen, dass das Add-On geladen ist.

## Funktioniert es?

Archicad mit einem beliebigen Projekt öffnen, dann:

```bash
curl -s localhost:19723 -H 'Content-Type: application/json' \
  -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"TapirCommand","commandName":"GetAddOnVersion"},"addOnCommandParameters":{}}}'
```

Erwartet wird `{"succeeded":true,"result":{"addOnCommandResponse":{"version":"1.7.2"}}}`.
Kommt keine Antwort, ist das Add-On nicht geladen. Läuft mehr als eine Archicad-Instanz,
hört die zweite auf Port `19724`.

Die SAB-eigenen Befehle prüft man über ihren eigenen Namensraum:

```bash
curl -s localhost:19723 -H 'Content-Type: application/json' \
  -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"ELM_SAB","commandName":"Get2DGeometryOfElements"},"addOnCommandParameters":{"elements":[]}}}'
```

## Die SAB-eigenen Befehle

Alle im Namensraum `ELM_SAB`, alle mit eingebauter Rücklese-Verifikation — ein
`NoError` der Archicad-API beweist bei Schreiboperationen nichts.

| Befehl | Wofür |
|---|---|
| `GetPenOfElements` / `SetPenOfElements` | Stifte lesen und setzen, inklusive der RGB-Overrides, die DWG-Importe direkt ins Element brennen. Weder Tapir noch die offizielle API können das. |
| `GetConstructionPensOfElements` / `SetConstructionPensOfElements` | Ab 0.10.1: Stifte von Bauteilen (Wand, Stütze, Unterzug, Decke, Dach, Schale, Fassade, Morph) lesen und setzen — Schnitt-/Kontur-, unverdeckte und Überkopf-Linien je Typ, Schnittflächen-Überschreibung löschen (`clearCutFillOverride`, Schraffur wieder aus Baustoff/Verbund), Rohfelder über `fields`, Verbundschicht-Stifte mit `includeComposites`; `dryRun`, Vorprüfung `[CODE]`, ein Undo-Schritt, Rücklese. `GetPenOfElements`/`SetPenOfElements` bleiben für 2D-Elemente. |
| `Get2DGeometryOfElements` | Linien, Bögen, Kreise, Polylinien und Schraffur-Polygone auslesen; funktioniert über Datenbankgrenzen hinweg. Antwortfeld heißt `geometryOfElements`. Ab 0.9.19 liefern alle übrigen Typen (Tür, Fenster, Objekt, Lampe, Etikett …) per ShapePrims-Fallback `primitives[]` (Zeichenprimitive wie im Grundriss, `source: "shapePrims"`), z. B. den Türaufschlag als `arc`. Ab 0.9.26 auch Splines (`coordinates` 0-basiert, `bezierDirs`, `closed`, `autoSmooth`). |
| `Create2DElements` | Legt Linie, Bogen, Kreis, Polylinie, Spline und Schraffur in der aktiven Grundriss-Datenbank an. Die Ausgabe von `Get2DGeometryOfElements` kann 1:1 zurückgegeben werden. Fehler je Element (gültige Elemente im selben Aufruf werden trotzdem angelegt), ein Undo-Schritt, Rücklese je Element (Geschoss, Ebene, Koordinaten, Länge bzw. Fläche/Umfang). Schraffur-RGB-Flags werden ohne Angabe gelöscht, damit kein Werkzeug-Override durchrutscht. Ab 0.9.26, noch nicht live verifiziert. |
| `GetTextsOfElements` / `SetTextsOfElements` | Textinhalte von Texten und Etiketten lesen und schreiben. Zeilentrenner ist `\r`, nicht `\n`. |
| `SetTextSizeOfElements` | Schriftgröße von Text und Etikett, in mm oder als Faktor. |
| `SetAddParsOfElements` | GDL-Parameter über das AddPars-Memo — für Objekte, Lampen, Etiketten und Zonen. Bei Etiketten zwingend statt Tapirs `SetGDLParametersOfElements`, das dort abstürzt. |
| `CreatePolygonWalls` | Polygonwände aus geschlossenen Konturen, ab 0.10.8 mit Bogensegmenten (`polygonArcs`) und Umbaustatus. Tapir kann nur gerade Wände. Ab 0.11.10 stellt der Befehl die Wand-Werkzeug-Voreinstellung vor dem Erzeugen auf Basis-Struktur (und danach zurück), weil Archicad `APIWtyp_Poly` sonst verwirft — siehe Absatz 0.11 unten. `referenceLineEdge` wählt die Kante der Referenzlinie, `debug: true` liefert Voreinstellung und Rücklese. `bottomOffset` erbt ohne Angabe die Werkzeug-Voreinstellung (Bürovorlage: −0,15 m) — für Geschossniveau `bottomOffset: 0` mitgeben. |
| `GetPolygonWallInfo` / `ClonePolygonWall` | Diagnose (ab 0.11.5/0.11.7): Rohfelder und Polygon-Memo einer Wand lesen; eine Wand 1:1 samt Memo versetzt neu anlegen. |
| `CreateCurtainWallFromAxes` | Vorhangwände aus Pfostenachsen, auch nichtrechteckig (Giebel, Trapez, freies Polygon). |
| `GetColumnDetails` / `SetColumnDetails` | Kernmaße von Stützen. |
| `SetColumnRotation` | Absoluter Drehwinkel von Stützen. Tapirs `RotateElements` meldet hier Erfolg, ändert aber nichts. |
| `GetStoryVisibilityOfElements` / `SetStoryVisibilityOfElements` | „Auf Geschossen zeigen" von Objekten und Lampen lesen und setzen. Presets (HomeOnly … AllRelevant) oder Rohfelder. Weder Tapir noch die offizielle JSON-API fassen diese Felder an. |
| `CreateLabels` | Legt assoziative Symbol-Etiketten an. Anders als Tapirs `CreateLabels` bleibt `floorInd` aus der Eingabe erhalten (auch abweichend vom Parent-Geschoss), die Zeigerlinie wird nicht abgeleitet oder gespiegelt, optional `templateLabelId` (libPart, Ebene, AddPars, Zeigerlinie). Rücklese mit `floorIndRespected` und `coordinatesMatch`. Ab 0.9.19. |
| `GetLabelsOfElements` | Etiketten je Owner-Element mit einem einzigen Durchlauf über alle Etiketten statt einer Abfrage je Element. Nur aktuelle Datenbank (Grundriss). Ab 0.9.19. |
| `SetLayerOfElements` | Ebene per `layerId` oder `layerName` setzen, im Undo-Scope, mit Teamwork-Reservierung (Standard) und Rücklese je Element. Ab 0.9.19. |
| `FindElements` | Suche wie „Suchen & aktivieren" mit `and`/`or`/`not` über Typ, Geschoss, Ebene, Eigenschaft, Klassifizierung, Bibliotheksteil, GDL, IFC, Flags, Teamwork, Wirt/Öffnung, Zone, Bereich, Auswahl, Design-Option; liest immer die Grundriss-Datenbank (alle Geschosse), nie das aktive Fenster. `scope`, `return`-Eigenschaften, `criteriaStats` je Kriterium, optional `select` mit Rücklese. Nur lesen. Ab 0.9.23. |
| `ExportSurfaces` | Exportiert alle Oberflächen des Projekts samt Texturbildern als Ordner (`oberflaechen.json` + `texturen/` + `LIESMICH.md`, Format `sab-oberflaechen/1`): Farbe, Glanz, Transparenz, Kachelmaß im Modell (`breite_m`/`hoehe_m`), Drehung, Flags. Bilder aus der eingebetteten Bibliothek und aus `.lcf`-Containern kommen über Archicads Dateisystem mit; was nicht lesbar ist, steht mit `bild_fehlt` drin. `targetFolder` optional (Standard: Downloads). Nur lesen. Prüfen ohne Archicad: [`Tools/oberflaechen/pruefen.py`](Tools/oberflaechen/pruefen.py), Format: [`Tools/oberflaechen/FORMAT_sab-oberflaechen-1.md`](Tools/oberflaechen/FORMAT_sab-oberflaechen-1.md). Ab 0.11.12. |

Ab 0.9.24 kommen Aufruffehler (fehlender oder ungültiger Parameter) bei `SetPenOfElements`, `SetTextSizeOfElements`, `SetStoryVisibilityOfElements`, `SetLayerOfElements`, `CreateWallOpenings` und `SetObjectParametersForce` als lesbare Fehlermeldung beim Client an statt als `4009 Invalid command response` (Details: [reference/mcp-extension.md](../../skills/archicad/reference/mcp-extension.md), Abschnitt 0.9.24).

Ab 0.9.25 speichert `CaptureView` das PNG mit Echtfarbtiefe (macOS `MiC`, Windows `TC24`) statt `FromSourceImage`, der wahrscheinlichen Ursache des APIERR_BADID. Liefert Archicad trotzdem APIERR_BADID, folgt ein Rückfall auf `256C`. Die Antwort meldet die benutzte Tiefe in `colorDepth`. Noch nicht live verifiziert (Details: [reference/mcp-extension.md](../../skills/archicad/reference/mcp-extension.md), Abschnitt 0.9.25).

Ab 0.9.26 legt `Create2DElements` 2D-Elemente an (Line, Arc, Circle, PolyLine, Spline, Hatch), als Schreib-Gegenstück zu `Get2DGeometryOfElements`, der dafür einen eigenen Spline-Zweig bekommt. Anlass sind die farbigen Füllflächen im Flucht- und Rettungswegeplan WiSchu, denn Tapir kann nur Polylinien anlegen. Live verifiziert am 13.10. (WiSchu): alle sechs Typen, Fläche/Umfang exakt, Fehler je Element lesbar. Ab 0.9.27 (live verifiziert) meldet die Rücklese Kreise korrekt (Archicad speichert sie als Bogen mit `whole`), `Get2DGeometryOfElements` liefert `subPolyEnds`; ab 0.9.28 (live verifiziert) legt `Create2DElements` damit auch Schraffuren mit Löchern an (Details: [reference/mcp-extension.md](../../skills/archicad/reference/mcp-extension.md), Abschnitte 0.9.26/0.9.27).

Ab 0.9.31 stürzt `CreateSlabs` nach Favoriten-Standards nicht mehr ab, und GDL-Parameter lassen sich auch bei Türen und Fenstern setzen (Öffnungsmaße springen mit). Ab 0.9.32 prüfen schreibende Befehle vorab und melden Gründe als `[CODE]` (ausgeblendete/gesperrte Ebene, gesperrt, Hotlink, fremd reserviert, falsches Fenster, Undo-Klammer, keine Wirkung); alle Tapir-Erstellbefehle nehmen `layerName`; neu sind `GetLayers`, `SetLayerAttributes`, `GetSlabLevels`; Objekte/Lampen kennen `zReference` absolut oder geschossbezogen. Live verifiziert (Details: [reference/mcp-extension.md](../../skills/archicad/reference/mcp-extension.md), Abschnitte 0.9.31/0.9.32).

Ab 0.10.8 (zusammengeführt in 0.11.0): `CreateWalls`/`ModifyWalls` mit `flipped` und negativem `arcAngle` (Bogen nach links, Außenseite bleibt), `renovationStatus` beim Erzeugen von Wänden, Decken, Dächern und Polygonwänden, Oberflächen je Seite (`referenceMaterial`/`oppositeMaterial`/`sideMaterial`, `topMaterial`/`bottomMaterial`), `GetDrawingLinks`, `GetHotlinkInstances` (Namensraum `TapirCommand`), `ReplaceDrawingSource`, Ausschnitte/Zeichnungen für Designoptionen, Bibliotheksteil-Skripte lesen/schreiben. Live verifiziert 05.11. (Schreibtest 14/14 in leerer INT-Vorlage).

Ab 0.11.10 funktionieren Polygonwände unabhängig von der Vorlage. Hintergrund: Archicad 28 und 29 entscheiden beim `ACAPI_Element_Create` anhand der **Wand-Werkzeug-Voreinstellung der Datei**, ob `APIWtyp_Poly` angenommen wird — ist sie mehrschichtig (INT- und Bürovorlage), entsteht eine gerade Normalwand aus `begC/endC`, mit `begC == endC` sogar ein Absturz (SIGABRT im Anschluss-Update). `CreatePolygonWalls` stellt die Voreinstellung deshalb per `ACAPI_Element_ChangeDefaults` auf Basis, erzeugt die Wand und stellt sie zurück (0.11.11). Die Ursache wurde mit `GetPolygonWallInfo`/`ClonePolygonWall` eingegrenzt und ist weitergegeben: [Tapir-Issue #1914](https://github.com/ENZYME-APD/tapir-archicad-automation/issues/1914), [Graphisoft-Community-Thread](https://community.graphisoft.com/t5/Archicad-C-API/I-would-like-to-know-how-to-create-a-wall-using-Wall-Geometry/m-p/719915). Details: [reference/mcp-extension.md](../../skills/archicad/reference/mcp-extension.md), Abschnitt „0.11.1–0.11.11 Polygonwände".

## Selbst bauen

Der Build läuft normalerweise in der CI
([`.github/workflows/build-elm-sab-tapir.yml`](../../.github/workflows/build-elm-sab-tapir.yml)):
sie baut alle acht Kombinationen aus AC27/28/29/30 und Mac/Windows und lädt sie in die
rollende Vorabversion. Ein Push auf `ELM_SAB_Tapir/**` löst sie aus, per
`workflow_dispatch` geht es auch von Hand.

Lokal gebraucht: CMake ≥ 3.17, Python, sowie vollständiges Xcode (macOS) beziehungsweise
Visual Studio (Windows). Das DevKit lädt der Build selbst:

```bash
python3 Tools/download_and_unzip.py \
  https://github.com/GRAPHISOFT/archicad-api-devkit/releases/download/29.3000/API.Development.Kit.MAC.29.3000.zip \
  Build/DevKits/AC29
cmake -B Build -G "Xcode" -DAC_VERSION=29 -DAC_API_DEVKIT_DIR="Build/DevKits/AC29/Support" .
cmake --build Build --config RelWithDebInfo
```

Zwei Dinge, die dabei gern übersehen werden:

- **Windows braucht für AC27 und AC28 das Toolset v142** (`-T v142`), erst ab AC29 gilt
  `v143`. Deshalb ist der CI-Runner auf `windows-2022` gepinnt — neuere Images liefern
  v142 nicht mehr mit.
- **Der C++-Standard wechselt mit der Version:** AC27/28 werden als C++17 kompiliert,
  AC29 als C++20. `CMakeCommon.cmake` erledigt das selbst; wer neue Befehle schreibt,
  darf darin aber keine C++20-Sprachfeatures verwenden, sonst bricht der AC27/28-Build.

Die MDID (Developer-ID `944131939`, Local-ID `1033975726`, registriert am 2026-07-13)
liegt in [`Sources/RFIX/AddOnFix.grc`](Sources/RFIX/AddOnFix.grc) und gilt
versionsunabhängig für alle Builds. Ohne offiziell registrierte MDID lädt Archicad ein
Add-On nicht, sondern meldet einen Distributor-Fehler oder schweigt ganz.

## Entwickler-Dokumentation

[Tapir-Wiki: Archicad Add-On Development](https://github.com/ENZYME-APD/tapir-archicad-automation/wiki/Archicad-Add-On-Development)
