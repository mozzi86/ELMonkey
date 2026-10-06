---
phase: quick-261006-io0
plan: 01
status: complete
commit: e1e886b
completed: 2026-10-06
---

# Quick 261006-io0: ELM_SAB 0.9.19 Summary

CreateLabels (floorInd aus der Eingabe, optional Vorlage-Etikett, Rücklese mit floorIndRespected/coordinatesMatch), GetLabelsOfElements (ein GetElemList-Durchlauf), SetLayerOfElements (Undo-Scope, Teamwork-Reservierung, Rücklese) und ShapePrims-Fallback in Get2DGeometryOfElements.

## Commits

- e1e886b feat(ELM_SAB): CreateLabels, GetLabelsOfElements, SetLayerOfElements, Get2DGeometry-ShapePrims 0.9.19

## CI

status.md nennt `e1e886b`, alle sechs Kombinationen gebaut (AC27/28/29 x Mac/Win), erster Versuch, keine Fix-Runden. Die status.md-Tabelle führt alle sechs Artefakte ohne Fehlschlag-Vermerk. NICHT installiert, NICHT live getestet; Bundle-Tausch erst nach Ende der Produktivsitzung (eine Instanz, Rückbau vor Diagnose).

## Deviations

- Ohne Vorlage wird das Memo aus ACAPI_Element_GetDefaults mitgegeben (wie Tapirs CreateElementsCommandBase), statt eines leeren Memos; sonst fehlen dem Default-Symbol-Etikett die AddPars.
- GetDefaults wird mit gesetztem label.parentType = Typ des Parents aufgerufen (Header-Hinweis), mit Rückfall ohne.
- `gh` war nicht angemeldet; CI-Status ausschließlich über den Branch ci-status geprüft (Logs nicht nötig, da grün).
- String-Literale in Fehlermeldungen ASCII (ae/ue), wie im Bestand.

## Offen / am laufenden Archicad zu verifizieren

- Testplan in reference/mcp-extension.md (Abschnitt 0.9.19), 6 Schritte.
- ShapePrims: Text-par2 als direkter UniCode-Zeiger (Header-Callback-Doku, Terminator-Prüfung gegen Blockgröße); poly-Bogenarray 0-basiert angenommen; Indizes der Koordinaten 1-basiert. Bei Abweichung am echten Türsymbol prüfen.
- Schwere Legacy-Meshes können bei ShapePrims hängen (einzeln abfragen).
- Ob GetDefaults mit parentType für den Parent-Typ ein Symbol-Etikett liefert; sonst templateLabelId verwenden.
- API_Element-Kopie der Vorlage: Spiegelungsfelder nicht angefasst; Rücklese (coordinatesMatch) deckt Abweichungen auf.

## Self-Check: PASSED
