# Rezept: Zeichnungen auf Layouts platzieren und zuschneiden <!-- 2026-11-11 -->

Live durchlaufen am Futurelab-Fliesenspiegel (12 Layouts, AC29 Teamwork, Tapir 1.7.2 +
ELMonkey): Je Layout ein Ausschnitt „EG/1.OG RAUMSTEMPEL“ (Grundriss-Ausschnitt, auf dem nur die
Zonenstempel sichtbar sind), zugeschnitten auf den Stempel des jeweiligen Raums. Die alten
explodierten Stempel (Linien, Texte, Schraffur) wurden ersetzt. Das Muster gilt für jede
Grundriss-Zeichnung, die auf einen Modellpunkt zugeschnitten werden soll.

## Ablauf

1. **Layouts und Zeichnungen lesen:** `GetNavigatorItemTree {navigatorMapId:"LayoutBook"}` (Antwort
   ist verschachtelt: `children[].navigatorItem`), Layout-Datenbank über
   `GetDatabaseIdFromNavigatorItemId`. Danach `ChangeWindow {windowType:"Layout", databaseId}`.
   **`windowType` ist in Tapir 1.7.2 Pflicht.** Dann `GetElementsByType {elementType:"Drawing"}`
   und `GetDetailsOfElements`. Das liefert `pos`, `angle`, `drawingScale`, `clipPolygon`,
   `bounds`, `titleLibraryPartIndex` (0 = ohne Titel), `nameType` und `navigatorItemId`.
   Achtung: Diese `navigatorItemId` ist eine interne Verknüpfungs-ID und nicht die ID in der
   Ausschnittmappe. Die Quelle für `CreateDrawings` holst du aus dem `PublicViewMap`-Baum.
2. **Ansichtsdrehung lesen:** `GetViewSettings` → `rotation` (Bogenmaß) und `zoom` (in
   gedrehten Ansichtskoordinaten).
3. **Abbildung Modell → Layout** (Zeichnung mit `angle` 0, `modelOffset` 0):
   `L = pos + drawingScale · (R(rotation) · M + t)`. Dabei ist `t` eine **feste Verschiebung je
   Ausschnitt**. Futurelab: EG (84,52 / −23,97), 1.OG (0 / 0).
   `t` bekommst du aus einer schon platzierten Zeichnung desselben Ausschnitts (Beispiel des
   Nutzers) oder per Ausgleich über mehrere Zuschnitte gegen Zonenschwerpunkte. Danach
   **messen**: Erst die Messung am Bild hat für das 1.OG gezeigt, dass `t` dort 0 ist; die
   Schätzung aus den Zuschnitten lag um 6 mm daneben.
4. **Zonenstempel:** `FindElements` (ELM_SAB) mit `type:"Zone"` + `story`, danach
   `GetDetailsOfElements` → `stampPosition` = Mitte des Stempels. Die Stempelgröße auf Papier
   liefert `Get2DGeometryOfElements` (ELM_SAB) auf die Zone (hier 36,3 × 39,3 mm, unabhängig vom
   Maßstab).
5. **Freie Stelle suchen:** Zuschnitt = Stempel + Rand. Hindernisse: `clipPolygon` der anderen
   Zeichnungen (**nicht `bounds`**, denn bei gedrehten Zeichnungen ist die achsparallele Box viel
   größer als der Inhalt), Texte, Zeichnungstitel, das Blatt (`GetLayoutSettings` →
   `horizontalSize`/`verticalSize` in mm, Plankopf rechts ca. 185 mm). Am besten in der Nähe der
   alten Stelle suchen.
6. **Anlegen:** `CreateDrawings` mit `navigatorItemId` (Ausschnittmappe), `layoutDatabaseId`,
   `nameType:"ViewOrSourceFileName"`, `drawingScale`, `angle 0`, `clipPolygon` (in
   Layout-Metern). **Gleich danach `SetDetailsOfElements {typeSpecificDetails:{pos}}` mit der
   berechneten Lage setzen.** Siehe Falle 1.
7. **Prüfen:** Bild vorher und nachher mit `SaveCurrentWindowAsImage {entireDrawing:true}`.
   Die Differenz der dunklen Pixel ergibt die Box des alten Stempels (verschwunden) und des
   neuen (dazugekommen). Die alte Box mit bekannten Layout-Koordinaten eicht den Bildmaßstab,
   daraus folgt der Versatz des neuen Stempels in mm. Bei Abweichung `pos` nachsetzen; der
   Zuschnitt bleibt dabei liegen.
8. **Alte Elemente löschen** (nach Freigabe) und mit frischem Lesen je Layout zählen.

## Fallen

1. **`CreateDrawings` rechnet `pos` beim Anlegen um.** Das Add-on legt mit Ankerpunkt MM an, und
   Archicad speichert eine andere Lage. Gemessen: gespeichert = übergeben − drawingScale ·
   (zoom.xMin, zoom.yMin) des Ausschnitts. Der Inhalt lag 44 m (Modell) daneben, der Zuschnitt
   war leer. Den `clipPolygon` speichert Archicad dagegen wie übergeben. Darum immer nachträglich
   `pos` per `SetDetailsOfElements` setzen. Das ändert den Zuschnitt nicht (live geprüft).
2. **Ein voll wirkendes Testbild beweist nichts:** Solange der alte Stempel noch daliegt, sieht
   ein leerer neuer Zuschnitt richtig aus. Erst den alten löschen oder den neuen versetzt setzen,
   dann ansehen.
3. **Zeichnungstitel kommt aus dem Werkzeug-Standard** und ist per API nicht abschaltbar:
   `DeleteElements` auf `titleElementId` schlägt fehl, `SetDetailsOfElements` kennt keinen
   Titel, und `ApplyFavoritesToElements`/`ApplyFavoritesToElementDefaults` mit einem
   Zeichnungs-Favoriten meldet „Failed to get favorite“ (Tapir 1.7.2). Lösung: Der Nutzer stellt
   einmal das Zeichnungswerkzeug per Favorit auf „ohne Titel“ (Werkzeug Zeichnung anklicken,
   nichts ausgewählt, Favorit doppelklicken). Danach legt `CreateDrawings` ohne Titel an
   (`titleLibraryPartIndex` 0). Vorher eine Testzeichnung anlegen und den Titel prüfen.
4. **Zeichnungen tragen keine Eigenschaften:** `SetPropertyValuesOfElements` → 7203 „Element not
   supported“. Der KI-Stempel ist an Zeichnungen nicht möglich; dem Nutzer melden.
5. **`GetElementsByType Drawing` funktioniert bei aktivem Layout** (Tapir 1.7.2, live
   2026-11-11). Voraussetzung: vorher `ChangeWindow` mit `windowType:"Layout"`. Die ältere
   THN-Notiz („0 Treffer“) in `reference/mcp-extension.md` gilt so nicht mehr allgemein.
6. **`SaveCurrentWindowAsImage`:** Das zurückgegebene `zoom` ist nicht der Bildausschnitt (bei
   mehreren Layouts gleich, egal welches Blatt). Liegt ein Element weit außerhalb des Blatts
   (zum Beispiel ein verrutschter Zeichnungstitel), wird das Blatt im Bild winzig, und die
   Größe kann zwischen vorher und nachher wechseln. Dann ist der Bildvergleich nicht
   auswertbar, und man prüft per Augenschein.
7. **Nachbarstempel:** Im Ausschnitt mit Maßstab 1:20 sind die Stempel im Modell etwa
   0,7 × 0,8 m groß. Rechnerisch überlappende Nachbarzonen waren im Bild trotzdem nicht zu sehen
   (Ebene oder Ausschnitt). Den Nachbartest nur als Warnung nehmen und am Bild entscheiden.
8. **Teamwork:** `CreateDrawings`, `SetDetailsOfElements` und `DeleteElements` auf Layouts gingen
   ohne eigene Reservierung, weil der Nutzer die Layouts reserviert hatte.

## Was der Nutzer entscheidet

- Rand um den Zuschnitt: **großzügig** (15 mm), weil Stempel mit der Zeit mehr Zeilen bekommen.
  5 mm waren dem Nutzer zu knapp.
- Lage: auf freier Fläche, möglichst einheitlich; Ausnahmen sind in Ordnung, wenn der Platz
  nicht reicht.
- Alte Stempel löschen: vorher fragen.

## Layouts mit Grundriss und Wandansichten anordnen <!-- 2026-11-12 -->

Live am Futurelab (12 Fliesenspiegel-Layouts). Regeln der Nutzerin: Der Grundriss liegt in der
Mitte der freien Fläche neben dem Plankopf. Jede Wandansicht fluchtet mit ihrer Wand. Die Unterkante
der Ansicht liegt 10 mm vor der Grundriss-Zuschnittkante. Schräge Ansichten werden so weit nach außen
gezogen, bis nichts mehr überlappt. Texte „Ansicht X (1:20)“ stehen in der Lücke, im Uhrzeigersinn,
beginnend bei der Ansicht, die am ehesten nach oben zeigt. Ansicht 1 liegt oben waagrecht.

- **Abbildung Innenansicht → Layout:** `L = pos + drawingScale · R(angle) · P`, P = Ansichtskoordinate
  (x entlang der Wand, y absolute Höhe). Unterkante = vertikale Untergrenze der Innenansicht.
- **Flucht ohne Wandsuche:** Bei allen Ansichten außer einer galt `pos·w = (posGrundriss + s·R(aG)·t)·w`
  (w = R(angle)·(1,0)). Das Ansichts-x ist also die Projektion der Modellkoordinate. Ausnahme: eine
  Ansicht mit eigenem Koordinatenursprung. Dort an der gegenüberliegenden Ansicht ausrichten (Rechteckraum).
- **Zeichnung ↔ Ansichtsdatenbank:** Die `navigatorItemId` einer platzierten Zeichnung ist eine interne
  ID ohne Datenbank. Zuordnung über den Inhalt: x-Bereich der Bodenplatte der Ansichtsdatenbank gegen
  das P-Fenster des Zuschnitts, beschränkt auf den Ansichtsordner des Raums (benachbarte Räume teilen Koordinaten).
- **Freie Fläche:** A1 0,02–0,656 × 0,006–0,588, A0 0,02–1,004 × 0,006–0,835 (Plankopf 0,185 breit).
- **Überlappung:** Zuschnitt-Polygone prüfen (nicht `bounds`). Je Paar die kürzere Verschiebung entlang
  der eigenen Normalen wählen, schräg gegen gerade nur die schräge. Leere Zuschnittränder vorher kürzen.
- **Grundriss-Titel entfernen:** `ChangeDrawingLink` auf dieselbe Quelle legt die Zeichnung mit dem
  aktuellen Werkzeug-Standard (ohne Titel) neu an. Danach `pos` zurücksetzen und per Bildvergleich
  belegen, dass nur der Titel fehlt. **In Teamwork vorher reservieren:** Sonst entsteht die neue
  Zeichnung, aber die alte bleibt stehen (Duplikat).
- **Drehen einer Zeichnung:** `SetDetailsOfElements` mit `angle` und dem um `pos` mitgedrehten `clipPolygon`.
- **Textanker:** Ein Text mit Anker „RightTop“ lässt sich per API nicht auf „LeftTop“ umstellen
  (stiller No-Op). Die Position um `widthMm` aus `ELM_SAB.GetTextsOfElements` versetzen.
