# Außenwände aus Schraffur (wenn Konturen nicht schließen)

*Live-verifiziert 2026-07-14, THN UG-1: 11 Außenwand-Bänder.*

**Problem:** Die Kontur-Pipeline ([konturen-zu-waende.md](konturen-zu-waende.md))
findet nur GESCHLOSSENE Regionen zwischen Linien. Außenwände am Gebäuderand
schließen oft nicht (Bestandsplan endet dort, Böschungs-/Geländelinien fehlen)
→ ganze Außenwandzüge fehlen im Modell, obwohl sie im Plan klar sichtbar sind.

**Trick:** Die Wand-SCHRAFFUR liefert die fehlenden Kanten. Bestands-DWGs haben
oft eine eigene Schraffur-Ebene je Wandtyp (THN: `A_01_TRAGWAND_SCHR` neben
`A_01_TRAGWAND`). Die Schraffurlinien zerhacken das Wandinnere in viele schmale
Zellen — und die sind geschlossen, auch wenn die Wandkontur selbst offen ist.

## Verfahren

1. Segmente BEIDER Ebenen sammeln (Kontur + `_SCHR`) und gemeinsam
   `shapely.polygonize` → viele kleine Zellen.
2. **Dünn-Filter**: Zellen mit `area/convex_hull.area`-Verhältnis egal, aber
   `2*area/perimeter` (mittlere Dicke) < ~0,6 m behalten — das sind die
   Schraffur-Schnipsel IM Wandquerschnitt. Große Raumzellen fallen raus.
3. `unary_union` der dünnen Zellen → zusammenhängende Wand-Bänder;
   `buffer(0)` + spike_clean wie üblich.
4. **Dedup gegen Bestand**: vorhandene Wand-Polygone cachen (wall_geo_cache)
   und Bänder verwerfen, die schon >50 % überdeckt sind — sonst doppelt der
   Trick alle Innenwände, die die normale Pipeline schon erzeugt hat.
5. Rest via ELM_SAB `CreatePolygonWalls` (Geschosshöhe, Quell-Layer), danach
   klassifizieren (Wand) + räumliche Properties per kNN wie im
   [Referenzmodell-Abgleich](../reference/referenzmodell-abgleich.md).

## Grenzen

- Funktioniert nur, wo Schraffur existiert. THN UG-2 hatte KEINE
  Schraffur-Ebene → 0 Treffer, Außenwände dort nur manuell/andere Quelle.
  Vorab prüfen: Element-Histogramm je Ebene je Geschoss (`*_SCHR`-Ebenen zählen).
- Schraffur-Muster mit weitem Linienabstand (> Wanddicke) erzeugt Zellen, die
  der Dünn-Filter verwirft → Lücken im Band. Sichtprüfung durch den User bleibt
  Pflicht.

## Worked Example (THN UG-1, 2026-07-14)

`A_01_TRAGWAND` + `A_01_TRAGWAND_SCHR` polygonisiert → dünne Zellen vereinigt
→ 11 neue Außenwand-Bänder (h=3,50, Geschoss −1), die die Kontur-Pipeline
komplett übersehen hatte. Dedup verhinderte ~200 Duplikate.
