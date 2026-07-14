# Türen aus Aufschlag-Bögen (Türwand-Pattern)

*Live-verifiziert 2026-07-14, THN: 307 Türen, davon 153 mit Referenz-Nummern.*

**Kernproblem (hart erarbeitet):** Türen in **Polygonwänden** sind per API nicht
steuerbar — `centerOffset` wird gespeichert, aber ignoriert; die Tür landet
immer am selben internen Punkt (≈ Kantenmitte), `MoveElements` verschiebt
unvorhersehbar entlang Bandkanten, die 2D-BoundingBox lügt (Türsymbol!).
Position nur über `API.Get3DBoundingBoxes` messen.

**Lösung (User-Idee, SAB-Konvention): das Türwand-Pattern.**
Pro Tür entsteht ein Trio:
1. **Türwand**: kurze GERADE Wand (Türbreite + 2×15 cm, Banddicke, Geschosshöhe)
   auf der Türen-Ebene (A_013_TUER) in der Flucht des Wandbands — gerade Wände
   respektieren `centerOffset` exakt.
2. **Tür** in der Türwand (`CreateDoors`, Favorit, `centerOffset = Länge/2`).
3. **Öffnung** (`CreateOpenings`) schneidet das Polyband im Türwand-Bereich
   frei (volle Höhe − 1 cm) — kein Doppel-Mauerwerk.

Begründung: Staatliches Bauamt braucht alle Türen auf einer Ebene auswertbar,
und BIM-Türen müssen in einer Wand sitzen. Mehrere Wände voreinander sind
dabei akzeptiert (SAB-Entscheid).

## Geometrie aus dem Aufschlag-Bogen

- Bogen-`origin` = **Anschlagpunkt**, `radius` = **Türblattbreite** (0,55–1,35 m
  = Normaltüren; Ausreißer = Tore/Schlitze → Skip-Liste).
- Türachse: die Bogen-Endpunkt-Richtung, die **parallel zur nächsten Bandkante**
  liegt (|cos| > 0,7, sonst „schräge Tür" → Skip); Tür-Zentrum = Anschlag + r/2·Achse.
- **Aufschlag-Flags**: `reflected` und `oSide` hängen an der Schwingseite
  S = sign(cross(Türachse, offene Bogenrichtung)). Kalibrierung an EINER vom
  User korrigierten Tür (THN: S=+1 → beide Flags true, S=−1 → beide false).
  Erst 1 Tür bauen, User korrigiert im Dialog, Flags per GetDetailsOfElements
  zurücklesen — dann skalieren.
- `sillHeight` explizit **0** setzen (Favoriten bringen eigene Brüstung mit,
  Bau_Tür z. B. 0,15!).

## Gotchas

- **CreateDoors braucht ein offenes GRUNDRISS-Fenster** — im 3D-Fenster kommt
  „Failed to create door" (Geschoss ist egal, nur der Fenstertyp zählt).
  Kostete uns eine Stunde Fehlersuche quer durch alle anderen Hypothesen.
- Teamwork: Tür = Wand-Änderung → Host muss **reserviert** sein. Senden gibt
  Reservierungen frei! Symptome sind generische Fehlercodes, nie „nicht reserviert".
- Manche Favoriten schlagen per API fehl (THN: alle „T …"-Türfavoriten →
  „Failed to create door"; `Bau_Tür` geht). Vorab je Favorit 1 Testtür.
- **Türnummern**: stecken oft in der Element-**ID** (nicht als Property!).
  Setzen über BuiltIn-Property `General_ElementID`
  (API.GetPropertyIds mit {'type':'BuiltIn','nonLocalizedName':'General_ElementID'}).
- Referenz-Übernahme: Positions-Matching eindeutig-greedy, Geschoss-streng,
  Radius ≤ 3 m (Knautschzonen!); Median-Distanz als Qualitätsmaß ausgeben.

## Worked Example (THN, 2026-07-14)

904 Bögen → 307 Türen (EG 108, OG1 87, OG3 52, UG-1 44, Rest klein).
361 übersprungen mit ehrlicher Grund-Liste: 60 % „kein Wandband am Anschlag"
(Wandlücken im generierten Modell), Rest Fragmente/Ausreißer/schräg.
153 Türnummern aus dem Referenzmodell übernommen (Median-Match 0,52 m).
Script: Session-Scratchpad `tueren_mass.py` (Muster für Skill-Script v2).
