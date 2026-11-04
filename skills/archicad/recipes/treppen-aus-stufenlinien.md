# Treppen aus 2D-Stufenlinien

*Live-verifiziert 2026-07-14, THN: 20 Treppenläufe aus A_06_TREPPE-Linienwerk.*

Tapir `CreateStairs` braucht nur: `baseLinePoints` (2 Punkte = gerader Lauf),
`zCoordinate`, `totalHeight`, `flightWidth`, `stepNum`, `riserHeight`.

## Erkennung (Cluster-Verfahren)

1. **Stufenlinien-Kandidaten**: Linien 0,8–3,0 m auf den Treppen-Ebenen
   (A_06_TREPPE, F_05_TREPPE; NICHT A_061_GELAENDER — das sind Handläufe!).
2. **Cluster**: gleiche Richtung (±5°), Mittelpunkte entlang der Senkrechten
   aufgereiht mit Abstand 0,2–0,45 m (= Auftritt) und Querversatz < 1 m.
   Greedy: besten Cluster nehmen, Mitglieder entfernen, wiederholen bis < 4 Stufen.
3. **Ableitung**: Baseline = Mittelpunkt-Kette ± halber Auftritt verlängert;
   `flightWidth` = Median der Stufenlinien-Längen (clamp 0,8–2,5);
   `stepNum` = Linienzahl + 1.

## SAB-Regeln (User-definiert 2026-07-14)

- **Steigung realistisch, an die Zeichnung gebunden**: `riserHeight` fix
  ~0,175 m, `totalHeight = stepNum × 0,175` — der Lauf endet auf seiner echten
  Teilhöhe. NICHT die Geschosshöhe auf einen Teillauf verteilen (38-cm-Stufen!).
  Mehrläufige Podest-Treppen bleiben getrennte Läufe (bewusst „grob, aber ehrlich").
- **Decke im Treppenraum ausschneiden**: je Lauf ein `CreateOpenings` in der
  Decke des Geschosses darüber (Punkt-in-Polygon-Host), basePoint = Laufmitte,
  width = Lauflänge + 0,4, height = Laufbreite + 0,4, z = Level darüber.

## Gotchas

- Viele Elemente auf Treppen-Ebenen sind KEINE Stufen (Umrisse, Pfeile,
  Handläufe) — der 0,2–0,45-m-Abstands-Filter ist das wirksame Sieb.
- Wendel-/gebogene Treppen erzeugt das Verfahren nicht (Bogen-Stufen fallen
  durch die Parallelitäts-Prüfung) — Restliste ausgeben.
