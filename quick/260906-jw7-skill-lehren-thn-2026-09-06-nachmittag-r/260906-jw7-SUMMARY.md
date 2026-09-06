---
plan: 260906-jw7
type: execute
status: complete
---

# Quick Task 260906-jw7: THN-Nachmittag-Erkenntnisse eingetragen

Vier live verifizierte Erkenntnisse der THN-Sitzung 2026-09-06 (Nachmittag) in
den Archicad-Skill eingetragen, jeweils mit `<!-- 2026-09-06 -->`-Marker.

## Änderungen

1. **`recipes/library-objects.md`** (1 Zeile ersetzt): Falsche Aussage
   „`angle` ist ein normaler GDL-Parameter → `SetGDLParametersOfElements`"
   durch das verifizierte RotateElements-Rezept ersetzt (Objektmitte-Berechnung,
   `SetDetailsOfElements` kennt kein `angle`, Tiefe `B` bleibt über
   `SetGDLParametersOfElements` setzbar).
2. **`recipes/oeffnungen-aus-konturen.md`** (+16 Zeilen): Punkt 3 im Abschnitt
   „Host-Deckung messen" um den Vergleichsgröße-Zusatz (Mitte statt origin)
   ergänzt; neuer Abschnitt „Mittig in der Wand: Kriterium, Reihenfolge, Grenzen"
   mit den vier verifizierten Punkten (Mittig-Kriterium, Reihenfolge
   Drehen→Tiefe→Zentrieren, Schlitze behalten Tiefe, Wandkreuzungen
   mehrdeutig).
3. **`recipes/zones.md`** (+11 Zeilen, neuer Gotcha #11): Bauteilname-Fallback
   ohne Zonen (Wand-Property „Bauteilname" statt Ordner-/Datei-/Stempelname).
4. **`reference/mcp-conventions.md`** (+9 Zeilen): Drei Bulletpunkte an den
   bestehenden Abschnitt „Betriebs-Fallen (THN-Live-Sitzung)" angehängt
   (print-Pufferung, Modell-Cache statt Pro-Geschoss-Wiederholung, keine
   parallelen Lesezugriffe während Schreiblauf) — kein neuer Abschnitt.

## Deviations from Plan

None — plan executed exactly as written.

## Verifikation

- `grep -c '2026-09-06'`: library-objects.md=1, oeffnungen-aus-konturen.md=4,
  zones.md=1, mcp-conventions.md=3
- `grep -n 'normaler GDL-Parameter' recipes/library-objects.md`: leer (alte
  Aussage entfernt)
- `git diff --stat`: 4 Dateien, 37 Insertions, 1 Deletion

## Self-Check: PASSED

Alle vier Zieldateien enthalten die neuen Marker; alte falsche Rotation-Zeile
entfernt; keine Doppelung zu Commit c6100f7 (Vormittags-Einträge).
