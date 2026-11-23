# Rezept: Modell aus Archicad in die Schwarz Platform holen <!-- 2026-11-16 -->

Live durchlaufen an Projekt-I (AC29, Gesamtmodell inkl. TGA-Hotlink) am 16.11.2026.
Auftrag „hol es aus Archicad“ heißt **volles Paket**: richtige Objekte und Geometrien aus dem richtigen
IFC, richtige Baustoffe, richtige Oberflächen. Grafische Überschreibungen dürfen nie als Material im
Modell landen. Die Umbau-Farben (Neubau rot, Abbruch gelb) sind in der Plattform ein Anzeige-Schalter
(PR SchwarzArchitekturbuero/SchwarzPlatform#345), kein Material.

## Ablauf

1. **Aktives Fenster prüfen, nicht wechseln.** `TapirCommand.GetCurrentWindowType` lesen; es muss
   `FloorPlan` (oder 3D) melden. Der Nutzer holt den Grundriss selbst nach vorn. Die Ansicht darf keine
   Umbau- oder Grafik-Überschreibung tragen (Arbeitsfilter). **Nie per `ChangeWindow` wechseln**, siehe Fallen.
2. **Export:** `TapirCommand.IFCFileOperation` mit `method: "save"`, `translatorName`,
   `elementsToExport: "EntireProject"` (ab ELMonkey 0.11.12). Am Projekt-I rund 7 min, 490 MB, 18.233 Objekte
   inklusive TGA-Hotlink.
3. **Prüfen vor dem Hochladen:**
   - Soll-Ist per GlobalId gegen die Archicad-Elemente (`GetIFCIdsOfElements`). Leere Tür-/Fensteröffnungen
     kommen als `IfcOpeningElement` mit eigener Kennung, das Gelände steckt im `IfcSite`.
   - Surface-Styles auszählen (`IFCSTYLEDITEM` → `IFCSURFACESTYLE`). Steht ein einzelner Stil über 50 %
     (z. B. „Farbe rot“), ist eine Überschreibung eingebacken: **nicht hochladen**. Projekt-I: schlechter Export
     61 Stile, davon 92,5 % „Farbe rot“; guter Export 117 Stile mit echten Materialien.
4. **In die Plattform:** Der Konnektor nimmt nur **400 MB** (curl-Link und `sharepoint`-Pfad). Die Datei
   in die SharePoint-Projektablage legen (Zeichnungen/Aktuell/IFC (3D-Modell)/JJJJ-MM-TT-IFC-Modell/),
   ca. 15–20 min warten, bis `dokumente(suche=…)` sie als Dokument zeigt, dann `modell_hochladen` mit
   `document_id`. Wandlung am Projekt-I ca. 9 min, danach `modell_import_freigeben`.
5. **Alte Stände:** mit `modell_version_entfernen` weich entfernen (wiederherstellbar).
6. **Oberflächen:** `ELM_SAB.ExportSurfaces` (`targetFolder`) → Ordner als ZIP packen, **mit NFC-Dateinamen**
   (Python `zipfile` + `unicodedata.normalize("NFC", …)`; das macOS-`zip` liefert NFD, dann meldet der
   Prüfplan Bilder mit Umlauten als „Bild fehlt“) → `archicad_oberflaechen_hochladen` → Prüfplan lesen →
   `archicad_oberflaechen_einpflegen`. Vorher sichert die Plattform die alte Belegung.
7. **Rücklesen:** `modell_lesen` (Bauteilzahl, Status je Bauteil) und Oberflächen-Zuordnungen am Projekt
   gegen den Prüfplan; Sichtprüfung durch den Nutzer in der Plattform.

## Fallen

1. **Absturz bei `ChangeWindow`:** Auf bestimmte Ausschnitte/Geschosse hat es Archicad am Projekt-I zweimal
   abstürzen lassen. In solchen Dateien keine Fenster per API wechseln.
2. **Eingebackene Überschreibung:** Ein IFC, das mit roter Überschreibung exportiert wurde, nicht
   verwenden, auch wenn Geometrie und Mengen stimmen. Die Datei eindeutig benennen und den gültigen Stand
   daneben kennzeichnen, damit keiner die falsche hochlädt. Der Wandler der Plattform erkennt solche
   Farben bisher nicht und warnt nicht.
3. **Fehlende Elemente:** Elemente ohne gültigen 3D-Körper fehlen im Export. Projekt-I: 18 Elemente
   (Fenster, Türen, dünne Wand-Reststücke, Unterzüge, Rinnen mit Höhe 0) plus Punktwolken und Gelände.
   Bei den Türen war die 3D-Ausdehnung entartet; **vermutlich kein gültiger 3D-Körper**. Unabhängig vom
   Umbaufilter (zwei Exporte, identische GlobalId-Menge). Die Elemente in Archicad bereinigen oder
   bewusst so lassen und dem Nutzer melden.
4. **Umbaufilter:** Der aktive Umbaufilter ist per API nicht lesbar; geprüft werden kann nur seine Wirkung
   im Export (Schritt 3).
5. **Pläne:** PDFs aus der Projektablage liegen schon als Dokumente in der Plattform (SharePoint-Spiegel);
   nicht nachladen, erst `dokumente` fragen.
6. **Beobachten:** `modell_lesen` meldete direkt nach der Freigabe einmal „Modelldatei nicht lesbar:
   OrderedDict mutated during iteration“. Bei Wiederholung erneut lesen und melden.

## Was der Nutzer entscheidet

- Welcher Übersetzer und welcher Ausschnitt exportiert wird, und dass das Fenster passt (Grundriss vorn).
- Ob fehlende Elemente in Archicad bereinigt werden.
- Die Sichtprüfung der Oberflächen und Texturen in der Plattform.
