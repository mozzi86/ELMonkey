# Decken + Deckendurchbrüche aus 2D-Konturen

*Live-verifiziert 2026-07-14, THN: 21 Decken mit ~300 Durchbruch-Löchern über
5 Geschosse — inkl. eines Archicad-ABSTURZES und seiner Lehren.*

Script: [`../scripts/decken_aus_konturen/`](../scripts/decken_aus_konturen/).
Folge-Workflow von [`konturen-zu-waende.md`](konturen-zu-waende.md).

## Pipeline

1. **Footprint pro Geschoss** = Union aller geschlossenen Regionen aus den
   Begrenzungs-Ebenen. Wand-Ebenen allein reichen NICHT — erst **Fassaden- und
   Tür-/Fensterlinien schließen die Lücken** an Öffnungen (live: OG-Footprint
   sprang von 325 auf 3.478 m², als Fassade+Öffnungen dazukamen).
2. **Durchbrüche**: `AUSSPAR_BODEN` des Geschosses + `AUSSPAR_DECKE` des
   Geschosses DARUNTER (SAB-Lesart: Decke über X). Durchbruch-**Kreuzsymbole**
   (Rechteck + Diagonalen) werden durch polygonize→union automatisch zum
   Rechteck bereinigt. Zuordnung: Loch-Zentrum muss im Deckenteil liegen.
3. **Erzeugen**: Tapir `CreateSlabs` — Löcher direkt als `holes` im selben
   Request (ein Arbeitsgang statt CreateOpenings hinterher). Danach
   `SetDetailsOfElements`: Ziel-Ebene (A_09_TRAGDECKE) + Geschoss.
   Referenzebene `Top`, Level = Geschoss-Level → Rohdecke hängt nach unten.

## ⚠️ Absturz-Lehren (2026-07-14, alle live erlebt)

- **Riesen-Requests crashen Archicad KOMPLETT** (Bug-Reporter, Prozess weg):
  21 Decken mit ungefilterten Union-Polygonen (zigtausend Punkte) in EINEM
  Request → Fatal Crash. Seitdem: **eine Decke pro Request**, Fortschritts-
  Datei nach jedem Erfolg, bei Verbindungsabbruch SOFORT stoppen und Archicad
  prüfen — nie blind neu senden.
- **Teamwork-Konsequenz eines Crashes**: „Reinigung der Lokalen Daten" beim
  Neustart verwirft UNGESENDETE Änderungen (kostete 101 von 2.723 Wänden).
  → Nach jedem größeren Batch **sofort senden lassen**.
- **`APIERR_IRREGULARPOLY`** (Code 114, als `-2130313102` im JSON; Low-Byte =
  DevKit-Offset): Archicad lehnt Polygone mit Nadel-Spikes/Selbstberührungen ab,
  die shapely für valid hält. Gegenmittel: **Spike-Buffer** `buffer(-2cm).buffer(+2cm)`
  (negativ ZUERST — frisst Spikes; kann Polygon in Teile zerlegen → alle Teile erzeugen).
- **mm-Rundung kann Polygone selbstschneidend machen**: `round(x, 3)` auf einen
  validen Ring → Ring-Selbstschnitt. Validierung/`buffer(0)`-Reparatur muss
  **NACH** der Rundung passieren, nicht davor (Funktion `safe_ring`).
- Fehlercode-Diagnose: Low-Byte des negativen Codes gegen
  `APIdefs_ErrorCodes.h` halten (APIErrorStart + n) — `114 = IRREGULARPOLY`,
  `105 = BADPOLY`.

## Regeln (SAB, User-definiert 2026-07-14)

- Eine Decke pro Geschoss-Footprint (nicht raumweise); Teile ≥ 20 m².
- Ziel-Ebene A_09_TRAGDECKE, Dicke pauschal (0,30 m), OK = Geschoss-Level.
- Kein Markierungsrahmen nötig: der Grundriss ist als DB **geschossübergreifend
  lesbar** — Geschoss-Zuordnung über `floorIndex` der gelesenen Konturen.

## Worked Example (THN, 2026-07-14)

| Geschoss | Decken | Löcher | Bemerkung |
|---|---|---|---|
| UG-1 | 4 | 3 | |
| EG | 3 | 108 | Hauptdecke 4.825 m² — brauchte Spike-Buffer |
| OG1 | 6 | 89 | |
| OG3 | 5 | 95 | |
| OG4 | 3 | 0 | |

OG2/UG-2 offen: Linienwerk liegt dort auf Alt-Ebenen (`2MWTRAG_0` u. ä. aus
Fremd-Import) — Begrenzungs-Set muss projektspezifisch erweitert werden.

## Ausblick (recherchiert, noch nicht gebaut)

Gleiche Quelldaten-Lage für: Wanddurchbrüche (`CreateOpenings`, A_21_AUSSPAR_WAND),
Türen (908 Aufschlag-Bögen auf A_013_TUER → `CreateDoors` mit Host-Wand — Tür in
Polywand funktioniert per API!), Stützen (`CreateColumns`), Unterzüge (`CreateBeams`,
Linienpaare), Dach (`CreateRoofs`), Bemaßung (`CreateAssociativeDimensions`,
`CreateWallThicknessDimensions`). Lücken in Tapir: Curtain Walls, Skylights,
Wandschlitze mit Tiefenbegrenzung, Höhenkoten → ELM_SAB-Kandidaten v0.5.
