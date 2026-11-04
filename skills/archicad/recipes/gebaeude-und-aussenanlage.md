# Gebäude + Außenanlage komplett per API (ELMonkey) <!-- 2026-10-20, Nachtrag Dachtragwerk 2026-10-20 abends -->

Live erarbeitet in der ELMonkey-Demonstration am 20.10.2026 (leeres Projekt, Bürovorlage, ELMonkey 0.9.32/0.9.33).
Skripte: `~/.scratch/elmonkey/demo/` (`elmonkey_demo.py`, `demo_teil2.py`, `demo_gelaende.py`, `demo_sparren.py`).

**Grundsatz (Weisung Mudi):** Wie ein Architekt planen, nicht wie ein „Möchtegern-Häuslebauer“. Das Maß der Dinge ist **3D/BIM** — 2D nur ergänzend. Vor dem ersten Aufruf die Konstruktion klären: Höhenkette, Auflager, Lastabtragung, Anschlüsse, Straßenraum, Grundstück.

## 1. Höhenkette zuerst festlegen

| Bezug | Beispiel Demo |
|---|---|
| OK FFB EG | ±0,00 |
| Bodenplatte | −0,30 … 0,00 (Favorit „Bodenplatte WU 30 cm“, Bezug Oberkante) |
| Gelände Grundstück | −0,15 (Haus 15 cm über Gelände) |
| Gehweg | −0,17 |
| Grünstreifen | −0,20 |
| Straße | −0,32 (15 cm Bordsteinabsatz) |
| Wandkrone / OK Rohdecke | 3,00 |

## 2. Reihenfolge

1. **Ebenen** anlegen (`TapirCommand.CreateLayers`) und **per Name** verwenden — alle Erstellbefehle nehmen ab 0.9.32 `layerName`. `ELM_SAB.GetLayers` liefert den echten Index.
2. **Werkzeug-Standard aus Favorit** (`ApplyFavoritesToElementDefaults`), dann erstellen. **Falle (bis 0.9.35 offen):** Dach- und Schalen-Favoriten der Bürovorlage scheitern mit `APIERR_BADINDEX` (Decken-/Wand-Favoriten gehen) → Dach-Aufbau direkt per `CreateRoofs compositeName`, oder einmal ein Dach anlegen und daraus `CreateFavoritesFromElements` (eigener Favorit funktioniert). Decken: `CreateSlabs` mit `level` = Oberkante (bei Bezug „Top“). Kontrolle: `ELM_SAB.GetSlabLevels` (OK/UK absolut).
3. **Wände** (`CreateWalls`, Achse mittig), **Türen/Fenster** mit `favoriteName` (`CreateDoors`/`CreateWindows`, `centerOffset` ab Wandanfang).
4. **Räume** automatisch: `CreateZones` mit `geometry.referencePosition` (wie Zauberstab, braucht geschlossene Wände).
5. **Dach als Konstruktion**, nicht als Hülle — siehe Abschnitt 3.
6. **Außenanlage in 3D**: Gelände als Netz (`CreateMeshes`, 3D-Punkte), Wege/Straße/Terrasse als Platten, Bordsteine als Unterzüge 15/30, Bäume als Bibliotheksobjekte, Baustoffe per `ModifySlabs buildingMaterialId` (Format: `attributeIds[i]["attributeId"]`!).
7. **Grundstück**: Zone „Grundstück Fl.-Nr. …“ (Fläche, BIM), Grenzsteine (Stützen 15/15), Grenze als geschlossene Polylinie mit Linientyp „Strichzweipunktlinie“ (`ELM_SAB.Create2DElements`).
8. **Statik-Vorbereitung**: eingebaute Eigenschaft **„Tragende Funktion“** bewusst setzen (Abschnitt 4).
9. **Kontrolle**: `ELM_SAB.FindElements` je Typ/Ebene/Eigenschaft, 3D-Bild per `ChangeWindow 3DModel` + `FitInWindow {}` + `CaptureView` (Fenstertyp vorher mit `GetCurrentWindowType` prüfen). Nahaufnahme: Außenanlagen-Ebenen kurz per `SetLayerAttributes` ausblenden, `FitInWindow {}`, Bild, wieder einblenden; Tragwerk zeigen = Dachhaut-Ebene ausblenden. **Nicht** `FitInWindow` mit Elementliste im 3D — setzt die Kamera ins Gelände.
10. **Sichern**: vor Umbauten Kopie der .pln (`ELM_SAB.SaveProjectCopy` ab 0.9.34; bis 0.9.33 scheiterte das Sichern mit `APIERR_READONLY` → Datei per `cp` kopieren, solange keine ungesicherten Änderungen offen sind).

## 3. Satteldach als Pfettendach

- Querschnitte: Fußpfetten 14/14 auf den Traufwänden (OK 3,14), Firstpfette 16/24 auf den Giebelwänden (OK = Sparren-UK am First), Sparren 8/20 e ≤ 0,85 m inkl. Ortgangsparren im Überstand, Dachhaut 10 cm auf den Sparren.
- Pfetten: `CreateBeams` (waagerecht; `zCoordinate` = **Oberkante**), Ebene danach per `ELM_SAB.SetLayerOfElements` (CreateBeams hat kein `layerName` im Schema).
- Sparren: `CreateBeams` mit `slantAngle` — **erst ab ELMonkey 0.9.33** (vorher ignorierte Archicad den Winkel, weil `isSlanted` fehlte; Create und Modify meldeten trotzdem Erfolg; live bestätigt 20.10.: Hub 3,315 m bei 4,5 m / 35°). Immer zuerst EINEN Testsparren abseits anlegen und per 3D-Box messen.
- **Endschnitt ist rechtwinklig zur Achse, nicht lotrecht** (Archicad-Standard): Für `zCoordinate` = Oberkante am Anfang gilt zMin = UK_Traufe + h/cos α − h·cos α (bei 8/20 und 35° liegen die Sparrenenden 8 cm höher als bei einem Lotschnitt). Die Lage stimmt, nur der Prüfwert muss den Schnitt kennen. Lotrechter Firstschnitt ist per API noch nicht setzbar (Backlog: `APIMemoMask_AssemblySegmentCut`).
- Dachfläche: `ELM_SAB.CreateRoofs` mit `pivotLine {p1,p2}` (Traufkante), `posSign: true` für beide Flächen (Pivot jeweils so orientiert, dass die Fläche zur linken Seite steigt), `zCoordinate` = **Unterseite an der Traufkante**. Auflager rechnen: z0 = Auflagerhöhe − Überstand·tan(Neigung).
- **Dachhaut auf Sparren = Mehrschicht „Oberdach ohne Sparren 8,5 cm“** (Lattung/Konterlattung/Deckung; die Dämmung liegt zwischen den Sparren, nicht in der Dachhaut). Ab 0.9.34 direkt `CreateRoofs … "compositeName": "Oberdach ohne Sparren 8,5 cm"`; vorher nachträglich per `SetStructureOfElements`. **Falle bis 0.9.33:** steht das Dach-Werkzeug auf Mehrschicht, ignoriert Archicad `thickness` still (Demo: 34,5 statt 10 cm). Ab 0.9.34 stellt `thickness` auf Basis-Aufbau um und meldet `structureNote`; die Antwort enthält `structureType` + `thickness`. Kontrolle immer über `thickness` der Rücklese bzw. Eigenschaft `General_Thickness`.
- Giebel: Giebelwände über den First erhöhen (`ModifyWalls height`) und mit beiden Dachflächen per `TapirCommand.CreateSolidElementLinks` `"SubtractionUpwards"` beschneiden. Wird eine Dachfläche gelöscht, verschwinden die Verknüpfungen → neu anlegen.
- Holz-Baustoffe: `ELM_SAB.SetStructureOfElements` (`basic`, `buildingMaterialName`) — Pfetten Brettschichtholz, Sparren Vollholz.

## 4. „Tragende Funktion“

- Eingebaute Eigenschaft (Gruppe Kategorie), Werte NUR per `nonLocalizedValue`: `LoadBearingElement` / `NonLoadBearingElement` / `Undefined` (per Anzeigename → 4007).
- Archicad vergibt beim Anlegen Standardwerte, die fachlich oft falsch sind (Straße, Gehwege, Bordsteine, Grenzsteine standen auf „tragend“) → immer explizit setzen und per `FindElements {"property":"Tragende Funktion","op":"=","value":"Tragende Elemente"}` kontrollieren.

## 5. Fallen (live erlebt)

| Falle | Abhilfe |
|---|---|
| Tapir wählt das **Heimatgeschoss aus der absoluten Höhe**: alles knapp unter ±0,00 landet im UG → im EG-Grundriss unsichtbar | Objekte/Lampen: `zReference:"story"` + `floorIndex`. Andere: `SetDetailsOfElements floorIndex` setzt den Abstand zum Geschoss beibehaltend (Sprung um die Geschosshöhe!) → danach `MoveElements` um −Geschosshöhe, Höhen per 3D-Box gegenprüfen |
| `Get3DBoundingBoxes` zeigt den **unbeschnittenen** Körper (SEO wirkt nicht) | SEO-Ergebnis im 3D-Bild prüfen |
| `CaptureView` gibt **RGB-Schraffuren falsch** (schwarz) wieder | Farben per Rücklese prüfen, nie wegen des PNGs Elemente ersetzen |
| Bibliotheksnamen täuschen: „Tisch“/„Tischgruppe“ nicht platzierbar, „Laubbäume“ weiß-transparent, „Baum detailliert“ wie Pappel | Testplatzierung abseits + Sichtprüfung; gut: „Esstisch 01“, „Konzeptioneller Baum“ |
| Werkzeug-Standardebene ausgeblendet (Objekt → „00 2D - Konstruktion“) | ab 0.9.32 meldet die Vorprüfung `[LAYER_HIDDEN]`; immer `layerName` mitgeben |
| Polygon-Löcher, die sich berühren → APIERR_IRREGULARPOLY | Abstand ≥ 1 cm zwischen Teilpolygonen |
| Versetzte Wegkontur überschneidet sich im Bogen | Offset mit Gehrung je Knick rechnen, Bogen als gerade Knicke |
| Kreise zählen als `Arc` | bei Zählungen/Filtern berücksichtigen |
| `4001 ongoing user input` / `ChangeWindow` wirkt nicht, während der Nutzer klickt | mit Wartezeit wiederholen, Fenstertyp zurücklesen |
| unbenanntes Projekt vor Bundle-Tausch | Nutzer **vorher sichern lassen** |
| `CreateRoofs thickness` wirkungslos bei Mehrschicht-Werkzeugstandard (bis 0.9.33) | `compositeName` bzw. ab 0.9.34 Umstellung mit `structureNote`; Dicke zurücklesen |
| Sparren-Prüfwert 8 cm daneben | Endschnitt rechtwinklig zur Achse einrechnen (Abschnitt 3) |
| `FitInWindow` mit Elementen im 3D → Kamera im Gelände | Ebenen ausblenden + `FitInWindow {}` |
| `SaveProject`/`SaveProjectCopy` → `APIERR_READONLY` (bis 0.9.33) | ab 0.9.34 behoben; vorher Kopie per Dateisystem |
| JSON-API-Port | `lsof -nP -a -p <PID> -iTCP -sTCP:LISTEN` — der Archicad-Port ist meist 19723; andere offene Ports (z. B. 65050) antworten nicht auf die JSON-API |
