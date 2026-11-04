# Pfähle aus 2D-Kreisen (Bohrpfähle, Bohrpfahlwände)

*Live-verifiziert 2026-07-14, THN: 49 Einzelpfähle + 82 Bohrpfahlwand-Pfähle.*

Gründungspfähle stehen in Bestandsplänen als Kreise — aber in ZWEI Gestalten:

| Quelle | Ebene (THN) | Geometrie | Erkennung |
|---|---|---|---|
| Einzelpfähle | `A_04_BESCHRIFT` (!) | echte DXF-Kreise (Arc 360°) | Radius-Fenster Ø 0,50–0,80 |
| Bohrpfahlwand | `A_999_HILFSLINIE` | **geschlossene PolyLines**, die Kreise nur annähern | Kreis-Fit |

Nicht auf „sinnvolle" Ebenen verlassen — Pfähle lagen auf der Beschriftungs-
bzw. Hilfslinien-Ebene. Histogramm je Ebene zuerst.

## Kreis-Fit für Polylinien

```python
cx, cy = mean(xs), mean(ys)
dists  = [hypot(x-cx, y-cy) for x, y in pts]
r      = mean(dists)
ist_kreis = (max(dists) - min(dists)) < 0.3 * r   # Spread-Qualität
```

## Selektion-als-Muster (der eigentliche Trick)

Der User selektiert ein paar Beispiel-Elemente („das sind die 2d kreise die
pfähle werden") → Signatur extrahieren (Elementtyp, Ebene, Geschoss,
Kreis-Fit-Radius) → GANZES Projekt nach derselben Signatur sweepen
(Radius-Toleranz 0,7–1,3×), Zentren-Dedup. THN: 38 selektierte → 82 gefundene
Pfahl-Kreise, exakt die ganze Bohrpfahlwand-Kette. Erspart jede Ebenen-/
Filter-Diskussion: die Selektion IST die Spezifikation.

## Erzeugen

- `CreateColumns` mit rundem Favoriten (THN: „Stütze rund 30"), dann
  Durchmesser per Detail überschreiben (Fit-Radius × 2, THN: 0,748).
- **Tiefe Pfähle über mehrere Geschosse**: `coordinates.z` = absolute UK
  (THN: −6,315), `height` = Gesamtlänge (6,315 → OK = ±0,00). Ein Element,
  kein Geschoss-Gestückel.
- Danach `SetDetailsOfElements`: floorIndex (Heimatgeschoss, z. B. −2) +
  layerIndex (A_014_STUETZE) — Create legt sonst aufs aktive Geschoss/Ebene.

## Gotchas

- Auf den Kreis-Ebenen liegen auch Nicht-Pfähle: THN hatte 5 Riesen-Ringe
  Ø 1,2–2,8 auf A_999 → außerhalb des Radius-Fensters lassen und dem User als
  Restliste melden, nicht raten.
- Identische Kreise mehrfach im DWG (Block-Reste): Zentren-Dedup (< 5 cm)
  vor dem Erzeugen.
