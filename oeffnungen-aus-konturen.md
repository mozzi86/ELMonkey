# Öffnungen (Durchbrüche) aus 2D-Konturen + DWG-Beschriftungen

*Live-verifiziert 2026-07-14, THN: 620 Öffnungen (BD/FBA/WD) mit SAB-Favoriten,
davon 301 Wanddurchbrüche mit Maßen/Höhen aus den DWG-Texten.*

**SAB-Grundregel (User-definiert):** Durchbrüche werden IMMER mit dem
**Öffnungs-Tool** gesetzt (Favoriten-Ordner „Schlitze+Durchbrüche"), NIE als
Polygon-Löcher in der Decke — nur Öffnungs-Elemente sind etikettierbar.
Jede Öffnung wird **normalisiert**: Rechteck oder runde Kernbohrung, niemals
freie Polygonform.

## Quell-Ebenen und SAB-Lesart

| Ebene | Bedeutung | Favorit | Host |
|---|---|---|---|
| `A_22_AUSSPAR_DECKE` auf X | Loch in Decke ÜBER X | `BD Rechteck, 50/50` / `BD rund, 50` | Decke Geschoss X+1 |
| `A_23_AUSSPAR_BODEN` auf X | Aussparung im Boden von X | `FBA Rechteck, 50/50` / `FBA rund, 50` | Decke Geschoss X |
| `A_21_AUSSPAR_WAND` auf X | Wanddurchbruch | `WD Rechteck, 50/50, UK 2,15` / `WD rund, 50, Achse 2,40` | Wand Geschoss X |
| `A_21_AUSSPAR_WAND_BS` | **Beschriftungs-TEXTE** (Maße + Höhen!) | — | Datenquelle |
| WS/BS/HS-Texte | Wandschlitze | `WS senkrecht`/`WW waagerecht` | ⚠️ per API nicht tiefenbegrenzt → Warteliste |

## Pipeline

1. **Konturen** lesen (`ELM_SAB.Get2DGeometryOfElements`), pro Geschoss polygonisieren.
2. **Normalisieren**: Konturen, die sich berühren (< 2 cm), clustern (Bowtie-Paare
   aus Kreuz-Symbolik = EIN Durchbruch!) → **minimales umschließendes Rechteck**
   (shapely `minimum_rotated_rectangle`) oder Kreis (Flächen-/Seitenverhältnis-Test).
3. **Beschriftungen** lesen (`ELM_SAB.GetTextsOfElements`, ≥ v0.5) und parsen:
   - Maß: `[Gewerk] WD|BD B/H` — z. B. `H WD 15/20` (Heizung, 15×20 cm).
     Einheit: Wert ≥ 10 oder ohne Dezimalpunkt = cm, sonst m (`2.00/1.00` = Meter!).
   - Höhe: `OK=UKD` (bündig unter Rohdecke), `UK=35 U.UKD` (35 cm unter UK Decke),
     `OK=300.44` / `UK=…` (absolute NN-Kote — **NN-Offset des Projekts erfragen!**
     THN: ±0,00 = 298,00 üNN).
   - Zuordnung: Radius 4 m, gleiches Geschoss; Maßtexte eindeutig-greedy
     (nächstes Paar zuerst), Höhentexte teilbar.
4. **Host finden**: Wände via `GetDetailsOfElements` (liefert **floorIndex +
   polygonOutline** auch für Polywände!) → Punkt-im-Polygon, gleiche Etage;
   Pass 2 mit Abstand ≤ 0,5 m (Durchbruch-Symbole liegen oft in Wandband-LÜCKEN,
   weil die Bestandslinien dort unterbrochen sind). Decken analog über die
   Footprint-Polygone.
5. **Erzeugen**: `ApplyFavoritesToElementDefaults([favorit])` einmal pro Gruppe,
   dann `CreateOpenings` in 25er-Batches mit Fortschritts-Datei.

## CreateOpenings — live erarbeitete Semantik

- **`ownerElementId: {guid}` ist PFLICHT** (Wirt-Wand/-Decke) — fehlt es, kommt
  nur ein nichtssagender Schema-Fehler („required … #/openingsData/0").
  Exaktes Schema je Item: `{ownerElementId, basePoint (3D), width?, height?}`,
  `additionalProperties: false`. Bei Schema-Rätseln: Tapir-Quelle lesen
  (`ExtendedElementCommands.cpp`, GetInputParametersSchema — raw.githubusercontent
  funktioniert mit curl auch ohne Token). <!-- 2026-07-15 -->
- **Wand-Öffnungen brauchen `width` UND `height`** — ohne height:
  „Can't use empty polygon!". Decken-Öffnungen: width+height = Grundriss-Maße.
- **`basePoint.z` = absolute UK der Öffnung** (projektnull-bezogen) und
  ÜBERSCHREIBT die Höhenverankerung des Favorits (UK 2,15 etc. gilt nur im
  UI-Handbetrieb). z immer selbst rechnen: `z = UK`; bei `OK=UKD`:
  `z = Level(Geschoss+1) − Deckendicke − H`.
- Funktioniert in **Polywänden** (getestet) und geraden Wänden gleichermaßen.
- Favorit liefert Form/Attribute — Maße und Z-Lage kommen aus den Parametern.
- Rotation ist im Schema NICHT vorhanden → gedrehte Decken-Durchbrüche werden
  achsparallel gesetzt + Nacharbeitsliste (oder ELM_SAB-Erweiterung).

## ⚠️ Tapir-Bugs / No-Gos (live erlebt, AC29 + Tapir 1.5.3)

- **`ModifySlabs` mit `polygonOutline` → FATALER Archicad-CRASH** (auch bei
  Mini-Payload). `holes: []` allein → „No slab fields to modify" (leere Liste
  zählt nicht als Feld). Löcher entfernen daher: **Decke löschen + lochfrei neu
  erzeugen** (Create ist stabil). → Bug an Tapir-Maintainer melden.
- Tapir kann keine Texte lesen („Not yet supported") → `ELM_SAB.GetTextsOfElements`.

## Worked Example (THN, 2026-07-14)

620 Öffnungen: 159 BD + 133 FBA (in 21 Decken) + 301 WD (davon 148 mit Maß+Höhe
aus Text, 80 Fallback OK=UKD). Offen: 383 ohne Host-Wand (Wandlücken /
verlorene Wände), 72 Warteliste OG2-Decke, 51 gedrehte achsparallel gesetzt,
48 Schlitz-Texte (WS/BS) für v0.5+.
