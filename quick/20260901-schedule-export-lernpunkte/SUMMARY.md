---
status: complete
completed: 2026-09-01
slug: schedule-export-lernpunkte
---

# Summary — Schedule-Export-Lernpunkte

Zwei live verifizierte Befunde aus der ARZ-Möbel-Session aufgenommen.
32 Zeilen in 3 bestehenden Dateien, keine neuen Dateien, keine Struktur-Änderung.

| Datei | Was |
|---|---|
| `reference/schedule-pipeline.md` | Quellen-Warnung bei „Wann diese Pipeline verwenden"; neuer Unterabschnitt „Eingebettete Summenzeilen filtern" in Schritt 2; 2 Gotcha-Bullets |
| `reference/bulk-operations.md` | Absatz „Fortschreibung 2026-09-01" am bestehenden GDL-vs-Property-Abschnitt |
| `recipes/library-objects.md` | 2 Querverweis-Bullets in „Gotchas" |

## Entscheidung beim Einbau

Befund 1 wurde **angehängt statt neu angelegt**: `bulk-operations.md` dokumentiert
seit 2026-06-11 denselben Effekt für „Preisklasse" (uniform 1.664× Default).
Der neue Fall ist die Fortschreibung — die Property ist zwei Quartale später
immer noch Default (479 Architekturmöbel auf „RV Standard"), und der Preis-Fall
kommt dazu. Ein eigener Abschnitt hätte den Skill aufgebläht und die
Verbindung verschleiert.

## Belege

- Property „Preis Netto" (`A6EA4B12-…`) = 0,000 bei allen 1.322 Möbeln,
  Port 19724, AC29 Teamwork.
- Export mit 1.334 Datenzeilen, davon 7 eingebettete Summenzeilen;
  eigene Gruppensummen treffen die Zwischensummen auf den Cent
  (Gesamt 896.999,53 €).
