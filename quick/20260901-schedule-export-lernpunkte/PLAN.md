---
gsd_plan_version: 1.0
type: quick
slug: schedule-export-lernpunkte
created: 2026-09-01
---

# Quick Task — Zwei Schedule-Export-Lernpunkte aufnehmen

Zwei live verifizierte Befunde aus der ARZ-Möbel-Session (2026-09-01, Projekt
`Möbel_LOY_datei_New_work_Futurelab_V29_1652`, Port 19724, AC29 Teamwork) in den
Skill aufnehmen. Beide mit Datums-Marker `<!-- 2026-09-01 -->`.

## Befund 1 — Preise liegen im GDL-Parameter, die Property ist leer

Die Custom-Property „MÖBEL Klassifizierung / Preis Netto"
(`A6EA4B12-064D-0949-8D3C-B2EF45FBD919`) steht bei **allen 1.322 Möbeln auf
0,000**. Die echten Einkaufspreise stecken im GDL-Parameter „Einkauf netto" und
erscheinen nur im Schedule-Export, nicht über `GetPropertyValuesOfElements`.
Der Call meldet Erfolg — man bekommt 0 € und merkt den Fehler nicht.

Ist die Fortschreibung des bestehenden Abschnitts „GDL-Parameter vs.
expression-verlinkte Property als Quelle" (2026-06-11), wo derselbe Effekt für
„Preisklasse" dokumentiert ist. **Anhängen statt duplizieren.**

## Befund 2 — Schedule-Exporte enthalten eingebettete Summenzeilen

Ein gruppierter Schedule-Export schreibt Zwischensummen als normale
Tabellenzeilen: Gruppierungsspalte leer, GUID-Spalte leer, Stückzahl in einer
Textspalte, Zwischensumme in der Wertspalte. Im Beispiel-Export 7 solche Zeilen
bei 1.334 Datenzeilen. Ungefiltert werden sie als echte Elemente mitgezählt.

Filter: nur Zeilen **mit gefüllter GUID-Spalte** sind echte Elemente.
Gegenprobe: eigene Gruppensummen müssen die eingebetteten Zwischensummen exakt
treffen (614.163,52 / 102.818,00 / 148.553,01 / 30.535,00 / 930,00;
Gesamt 896.999,53 €).

## Tasks

1. `reference/schedule-pipeline.md` — Befund 2 als Unterabschnitt in „Schritt 2 —
   Parse"; Befund 1 als Quellen-Warnung bei „Wann diese Pipeline verwenden";
   je ein Gotcha-Bullet.
2. `reference/bulk-operations.md` — Befund 1 als Absatz an den bestehenden
   GDL-vs-Property-Abschnitt anhängen.
3. `recipes/library-objects.md` — Querverweis in „Gotchas" auf beide Befunde.
4. Commit (atomic).

## Nicht-Ziele

- Keine neuen Dateien, keine Struktur-Änderung.
- Keine projektspezifischen Daten (Layer, Zonen) in den Skill — nur das
  übertragbare Muster; GUIDs nur als Beleg-Beispiel.
