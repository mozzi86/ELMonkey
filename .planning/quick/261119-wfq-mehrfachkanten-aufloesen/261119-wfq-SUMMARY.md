# Quick 261119-wfq: Mehrfachkanten per Fächer-Trennung auflösen

**One-liner:** `mannigfaltig()` trennt den Dreiecksfächer je Eckpunkt bei Kanten mit mehr als 2 Dreiecken. `zerlegen()` wendet es je Komponente an und zerlegt danach neu.

## Commit
- b47c1ae `fix(plattform_abgleich): Mehrfachkanten per Fächer-Trennung auflösen` (lokal, kein Push, Branch fix-aggregation-eltern)
- Dateien: `skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py`, `core/tests/test_plattform_abgleich_skript.py`

## Änderungen
- Der alte `zerlegen`-Körper heißt jetzt `_komponenten`.
- `mannigfaltig(P, F)` ist neu. Es arbeitet auf Kopien und fragt Nachbarschaften nur auf den Originalflächen ab. Das ist die Verbesserung gegenüber dem Prototyp. Winkelpaarung (`paar_von`, `atan2`) ist nicht übernommen.
- `zerlegen` teilt nur dann neu auf, wenn `mannigfaltig` Punkte hinzugefügt hat. Saubere Netze bleiben unverändert.
- Zwei neue Tests: Voxel-Netz mit Kante aus 4 Dreiecken (1 Körper, Volumen 6, Punkte +2) und Würfel unverändert.
- Gesamtsuite: 172 passed, 2 skipped.

## Offline-Check gegen echte Daten (plattform_v43_r48.ifc, nur lesend)

| GUID | Körper nach zerlegen | Kanten mit >2 Dreiecken | offene Kanten |
|---|---|---|---|
| FB6C1C1C-7DD4-474B-8CEC-5347AE25D3CF | 1 | 0 | 0 |
| 6E3202C0-D914-43A5-837F-76A27CAB6D7D | 1 | 0 | 0 |
| 222A877D-C433-4C04-92C3-A380496A50FD | 1 | 0 | 0 |
| 201B6073-E1DF-4EF0-95ED-B40649F8476D | 1 | 0 | 0 |
| 703B9DB5-450C-4B56-A94C-0C3F86B92D7F | 1 | 0 | 0 |
| 4DF331A5-76C6-4F26-8FD6-5523BEC69584 | 56 | 0 | 224 |

Bei 4DF331A5 sind die 224 offenen Kanten ein offenes Flächennetz und kein Fehler der Zerlegung. Das passt zu den live gebauten "5 closed solids" von 6.
Der Check lief mit einem Skript im Scratchpad (`chk.py`), das Teile über `ifc_teile` und `_zusammen` lädt.

## Deviations
None. Der Plan wurde wie beschrieben ausgeführt.

## Self-Check: PASSED
