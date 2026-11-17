# Rezept: Innenansichten / Fliesenspiegel bemaßen <!-- 2026-10-14 -->

Live durchlaufen am Fliesenspiegel Futurelab EG (8 Innenansichten, 34 Einzelansichten,
~300 Maßketten, Teamwork, AC29). Voraussetzung: **ELMonkey ≥ 0.9.29** (Befehle siehe
`reference/mcp-extension.md` → „Bemaßung in Ansichten"), Fallen siehe
`reference/mcp-conventions.md` → „Fallen beim Ändern von Maßketten".

## Ablauf

1. **Ansichten finden:** `API.GetNavigatorItemTree {navigatorTreeId:{type:"ViewMap"}}` per HTTP, Ordner filtern, je `InteriorElevationItem` → `GetDatabaseIdFromNavigatorItemId`.
2. **Jede Ansicht einmal öffnen** (`ChangeWindow`), sonst ist ihre Datenbank leer und Maße zeigen 0. Der Nutzer sieht dabei mit.
3. **Muster auslesen:** `GetDimensionDetailsELM` auf die fertige Muster-Ansicht. Daraus Reihenfolge, Abstände, Bezugsarten, Custom-Texte (OK FFB, UK Decke, „X") und Vorlage-Ketten (je eine vertikale, horizontale, Einpunkt-Kette) für `CreateDimensionsLikeTemplateELM`.
4. **Inventur je Ansicht:** `GetSectionElementsELM` (nur Elemente mit gültigen `bounds`) + Bibliotheksnamen der Eltern über `GetDetailsOfElements`.
   - Raumrahmen x: Bodenfliesen-Decke (Ebene Fliesen, y≈OK RFB) auf Wandflächen schnappen; vorhandene Gesamtlänge hat Vorrang.
   - Sanitär „wichtig": WC, Urinal, Waschbecken, Waschtischplatte — Zubehör (Spender, Bürsten) nicht.
   - Öffnungen: Türen auf Wand-Ebene mit Breite > 0,3 m; Kabinentüren nicht.
5. **Bestand klassifizieren** (je Kette, im Ansichtskontext): Fliesen-Detail, Fliesenhöhe, Sanitär, Öffnungen, Raumhöhe; oben Fliesen, Sanitär, Öffnungen, Gesamt. Ketten mit Fugenbezug und kurzer Reichweite = Fliesen-Detail.
6. **Fehlendes ergänzen** mit Vorlage-Einstellungen; Tür/Bodenfliese assoziativ (Knoten wie im Muster), sonst statisch; Rücklese der Punktlage.
7. **Vollständig machen:** jede Kette von Rand zu Rand (außer Fliesen-Detail). Bestehende Ketten können keine Punkte dazubekommen → ersetzen (anlegen, prüfen, alt löschen im Aufräumdurchgang). Lücken ≤ 1,5 cm: Endpunkt auf Rand setzen statt Mini-Segment.
8. **Lage:** erste Kette 0,25 von der Ansicht, dann je 0,15 — rechts und oben, in Musterreihenfolge. Verschieben mit `MoveElements` (bewegt nur die Maßlinie, Punkte bleiben) — vorher reservieren.
9. **Schrift & Hilfslinien** mit `SetDimensionAppearanceELM`: Text 2,5 mm. Hilfslinie 0,15, wenn sie die nächstinnere Maßlinie kreuzt (deckungsgleich auf einem inneren Messpunkt zählt nicht), 0,08, wenn sie in eine Maßzahl ragt, sonst 0,20. Maßzahlen liegen oben **über** der Linie, rechts **links** der Linie (zur Ansicht). Größe bei 1:20: Höhe ≈ 0,05 m.
10. **Höhenkoten:** OK FFB und UK Decke als Einpunkt-Ketten mit Custom-Text, beide in einer Flucht (gleiches x).
11. **Ansichtshöhe = UK Decke:** `SetInteriorElevationVerticalRangeELM` (nur `vertMax`), danach obere Endpunkte statisch verschieben (`witnessCoordinates`), Raumhöhe von OK FFB bis UK Decke, obere Ketten neu ins Raster.
12. **Abschlussprüfung** mit frischem Read: Anzahl je Ansicht vorher/nachher, keine Doppel (gleiche Querlage), Reihenfolge, Raster, Vollständigkeit, Schrift, Hilfslinien-Regel, Koten-Flucht.

## Was der Nutzer selbst entscheiden muss

- **UK Decke:** Modell und Planstand können auseinanderliegen (hier Planstand 2,70, Modell-Abhangdecke 3,05/3,03). Nicht raten — nachfragen oder messen lassen.
- Bestand löschen oder ersetzen: vorher fragen (SAFE-02). Freigegeben war hier „ersetzen".
- Teamwork **Senden/Freigeben** macht der Nutzer.

## Neubemaßung aus der Geometrie (zweiter Durchgang) <!-- 2026-11-12 -->

Live am Futurelab, 52 Wandansichten EG + 1.OG: Nach Umplanung passten 253 von 1.185 Maßpunkten
nicht mehr. Statt Ketten einzeln zu flicken, wurden alle Ketten **aus der aktuellen Geometrie neu
erzeugt** (450 neu, 422 alt gelöscht). Das ist schneller und einheitlicher, sobald mehr als ein paar
Punkte abweichen.

**Geometrie je Ansicht** (alles aus `GetSectionElementsELM` mit `withBounds`, ohne Fenster öffnen):
- Raumbreite x0/x1 = Bodenbelag-Objekt (Ebene „20 Bodenbelag“), sonst Decke „20 Fußbodenaufbau“.
- Unterkante der Ansicht = kleinstes `yMin` (EG 0,045), UK Decke = Abhangdecke, OK FFB = Oberkante Bodenbelag.
- Fliesen: Rahmen auf der Fliesenebene ≤ 6 mm dick = Fugen (senkrecht bzw. waagrecht); Fliesenhöhe = Oberkante der Paneele.
- Ansichtskoordinaten: x = Modellkoordinate entlang der Wand (Projektion), y = absolute Höhe.
- Sanitär „wichtig“ über den Bibliotheksnamen (WC, Urinal, Waschbecken, Ausguss) plus **Waschtischplatte = Decke auf der Sanitär-Ebene**. „Schüssel“ ist ein Abfallbehälter, also Zubehör.
- Türen: Ebene „Wand innen“ und breiter als 0,3 m; Kabinentüren liegen auf einer eigenen Ebene.

**Ketten** (rechts 0,25 + k·0,15 neben x1, oben 0,25 + k·0,15 über UK Decke, von innen nach außen):
rechts Fliesen-Detail (erste zwei reguläre Fugen) · Fliesenhöhe · Sanitär · Öffnungen · Raumhöhe (OK FFB–UK);
oben Fliesen · Sanitär · Öffnungen · Gesamt. Rand zu Rand = Unterkante der Ansicht bis UK Decke bzw. x0–x1.

**Sondertexte erhalten:** Die Nutzerin markiert Restmaße mit „X“ (am ersten oder letzten Abschnitt)
und knapp abweichende Fliesen mit dem Nennmaß („60“, „40“, „0^5“). Der Text eines Messpunkts gilt für
den Abschnitt **vor** dem Punkt. Übernahme: „X“ am Kettenanfang/-ende bleibt dort, Nennmaße auf den
neuen Abschnitt gleicher Länge (±1,2 cm) und gleicher Lage (±6 cm).

**Fallen:**
1. **Mosaik:** Senkrechte Fugen im Abstand < 10 cm bilden ein Mosaikfeld. Dort nur die Außenkanten
   bemaßen, sonst liegen Dutzende Maßzahlen übereinander. Waagrechte Mosaikfugen nicht ins Fliesen-Detail nehmen.
2. **Doppelte Fugen:** Leicht versetzte, überlappende Fugenrahmen (zwei Fassaden übereinander) zu einer zusammenfassen.
3. **Vorlage muss existieren:** `CreateDimensionsLikeTemplateELM` braucht eine noch vorhandene
   Vorlage-Kette. Nach dem Löschen der Altketten eine der neuen Ketten als Vorlage nehmen. Sonst
   fehlt `elements` in der Antwort, und die Ansicht steht leer da.
4. **Vorlage mit gemessenem ersten Punkt:** Der Notiztyp des ersten Vorlagepunkts gilt für alle Punkte.
   Eine Vorlage mit Sondertext am ersten Punkt macht alle Punkte zu leeren Sondertexten. Für die Koten je
   eine eigene Vorlage nehmen (OK FFB und UK Decke haben gespiegelte Textlage).
5. **Teamwork beim Massenlöschen:** Nach rund 140 schnell gelöschten Ketten riss die Teamwork-Verbindung
   ab, und Delete meldete nur noch „0 gelöscht“. Danach in Paketen von 6–7 Ansichten mit Pausen
   weitergemacht: Fenster wechseln, 1–1,5 s warten, reservieren, löschen, frisch zurücklesen. Lief fehlerfrei.
6. **Zuschnitte danach:** Neue Ketten ragen über alte Zuschnitte. Den Zuschnitt aus dem tatsächlichen
   Inhalt bilden (`Get2DGeometryOfElements` der 2D-Elemente + Schnittelemente) + 3 mm, oben + 4 mm für
   die Maßzahl; die Unterkante bleibt. Danach die Layouts neu anordnen (`recipes/zeichnungen-auf-layouts.md`).
7. **OK FFB = Oberkante des Bodenfliesen-Objekts** (Regel der Nutzerin, 12.11.): Oberkante
   „SAB Bodenfliese liegend“ auf Ebene „20 Bodenbelag“, nicht die Rohdecke und nicht die
   Fliesen-Decke. Futurelab: EG 0,055 (lichte Höhe 2,975, Unterkante der Ansicht 0,045),
   1.OG 5,10 (lichte Höhe 2,60). Die 3D-Box dieses GDL-Objekts ist unzuverlässig, die Höhe aus
   dem Ansichtsschnitt (`GetSectionElementsELM withBounds`) nehmen. <!-- 2026-11-16 -->
8. **Vor der Bemaßung die Höhen bestätigen lassen.** Die Nutzerin gibt OK FFB und UK Decke vor;
   ohne diese Angabe nicht anfangen. <!-- 2026-11-16 -->
