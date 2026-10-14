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
