# Quick 261006-lod: ELM_SAB 0.9.21 CreateWallOpenings Quell-Löschprüfung

**Ergebnis:** CreateWallOpenings prüft nach `ACAPI_Element_Delete` per Header, ob das Quellsymbol wirklich weg ist. `sourceDeleted` ist nur dann true, wenn es fehlt.

## Commits
- 082db11 fix(ELM_SAB): CreateWallOpenings prüft Löschung des Quellsymbols 0.9.21
- 2ef26c2 docs(ELM_SAB): 0.9.21 CreateWallOpenings Quell-Löschprüfung

Beide auf origin/main gepusht.

## Änderungen
- `OpeningRequest.sourceConflict`, gesetzt in Execute, wenn die Quelle in `conflicts` steht (nur bei deleteSource).
- Schritt 10: Konflikt führt zu eigenem sourceKeptReason und es gibt keinen Löschversuch. Nach dem Delete wird per `LoadElementHeaderByGuid` geprüft. Überlebt das Symbol, steht dort "existiert weiter (Reservierung?) - Code N" bzw. "abgelehnt - Code N". Ist es weg, zählt das als gelöscht, auch bei delErr != NoError.
- Freigabe: nur eigene (nicht in conflicts), noch existierende GUIDs.
- Version 0.9.21 in AddOnVersion.hpp und bei RegisterCommand. ADDON_VERSION unverändert.
- Doku: reference/mcp-extension.md mit 0.9.20-Verweis und neuem 0.9.21-Abschnitt samt Testplan.

## CI (ci-status, SHA 2ef26c2, Lauf 37472513401)
Alle sechs Bundles gebaut: AC27/AC28/AC29 je Mac (.zip) und Win (.apx). Keine Fehlschläge, 0 Fix-Runden.

## Abweichungen
Keine. `reserve:false` behält seine Bedeutung. Dann bleibt die Quelle im Teamwork meist stehen, das wird jetzt sichtbar gemeldet. Nicht live verifiziert, kein Bundle installiert, Archicad nicht angesprochen.
