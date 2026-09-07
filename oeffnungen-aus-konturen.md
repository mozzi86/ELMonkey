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
   `floorPlanPolygons`** auch für Polywände — das Feld heißt NICHT `polygonOutline`,
   Korrektur 2026-08-31) → Punkt-im-Polygon, gleiche Etage; Pass 2 mit Abstand
   ≤ 0,5 m (Durchbruch-Symbole liegen oft in Wandband-LÜCKEN, weil die
   Bestandslinien dort unterbrochen sind). **Besser als Punkt-im-Polygon:
   Achse + Dicke** — die Polygone sind an bestehenden Öffnungen aufgesplittet,
   siehe „Host-Deckung messen" unten. Decken analog über die Footprint-Polygone.
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

## ⚠️ Anti-Pattern: Wanddurchbruch als freistehendes Deckendurchbruch-Objekt <!-- 2026-08-31 -->

*Gemessen am THN 2026-08-31 an 996 WD/WS-Objekten aus dem Juli-Massenlauf.*

Wenn die Projektbibliothek kein Wand-Objekt hat (THN: nur „Bodendurchbruch Symbol",
„Deckendurchbruch Symbol", „Bodenschlitz" — Wanddurchbruch/Wandschlitz existieren nur
als **Fenster**-LibParts), ist die Versuchung groß, WD auf das Deckendurchbruch-Symbol
zu mappen und die Wahrheit in die Element-ID zu schreiben. Das skaliert nicht. Was
dabei live herauskam:

| Symptom | Messung |
|---|---|
| Platzierungswinkel | `angle = 0` bei **996 von 996** — jedes Objekt achsparallel, egal wie die Wand läuft |
| Objekttiefe `B` ≠ echte Wanddicke | **505 von 578** mit Wirt, Median 10 cm daneben, max **1,34 m** |
| davon auf dem Default `B = 0,25 m` | 171 |
| Wirtsbindung | keine — kein 3D-Schnitt, wandert bei Wandbewegung nicht mit, kennt die Wanddicke nicht |

`B` wird als Wandtiefe missbraucht und `ZZYZX` als Lochhöhe; sickert die Lochhöhe in
`B` durch, liegen 3-m-Rechtecke quer über dem Flur (User sieht es sofort). Eine
**Öffnung im Wirt hat all das strukturell nicht** — sie *ist* die Wanddicke und erbt
die Wandrichtung. Symbol-Objekte sind nur dort vertretbar, wo es gar keine Wirtswand
gibt (siehe nächster Abschnitt).

## Host-Deckung messen, bevor man umbaut <!-- 2026-08-31 -->

Vor der Entscheidung „Öffnung oder Symbol-Objekt" erst read-only zählen, wie viele
Kandidaten überhaupt einen Wirt finden würden. Rezept (THN: 13.155 Wände in 13 s,
996 Objekte gematcht):

1. `TapirCommand.GetElementsByType {elementType:"Wall"}` → alle GUIDs;
   `GetDetailsOfElements` in 250er-Batches mit Bisektion (Schema-Bug 4009).
2. Wandband bilden: `Straight`/`Trapezoid` → Achse `beg→end`, Dicke
   `(begThickness+endThickness)/2`; `Polygonal` → `floorPlanPolygons` unionieren.
   **Nicht** Punkt-in-Polygon als Primärtest (an Öffnungen aufgesplittet, siehe
   `reference/mcp-extension.md`).
3. Objektmitte = `origin + (A/2, B/2)` aus `GetDetailsOfElements` — nicht aus dem
   eigenen Register! Das Register kann veraltet sein (THN: Median 1,07 m Abweichung
   zwischen Juli-Register und heutiger Modellposition). **Das Modell ist die Wahrheit.**
   <!-- 2026-09-06 --> Vergleichsgröße ist immer die geometrische Mitte, nie `origin` —
   ein Lauf mit `origin` statt Mitte lieferte scheinbar schwankende Trefferzahlen
   (287 → 173 → 88); die Messung war falsch, nicht das Modell.
4. Grid-Index je `floorIndex` (Zelle 4 m), dann `clearance = dist(Mitte, Achse) − t/2`.
   Pass 1: `clearance ≤ 0`. Pass 2: `≤ 0,5 m` (Symbole liegen in Wandlücken).
5. Ergebnis als CSV mit `befund`, `wall_id`, `wall_th`, `tiefe_falsch_cm` ausgeben —
   das ist zugleich die Arbeitsliste.

THN-Ergebnis: **58 %** mit Wirt (27 % Pass 1, 31 % Pass 2), 69,5 % bei 1-m-Toleranz.
Von den wirtlosen lagen 114 zwischen 0,5 und 1,0 m — die Toleranz entscheidet also
spürbar. Vorsicht: Pass-2-Treffer können die FALSCHE Wand erwischen (Ecken, Schächte);
für die Messung tragbar, für den echten Umbau pro Objekt gegen Gewerke-Layer und
Textrichtung plausibilisieren.

**Wirtlos heißt nicht wertlos.** Am THN lagen 82 WD + 33 BD in einem Bereich, in dem
es 85 Stützen und 229 Schraffuren (Flucht-/Brandschutzsymbole, Deckenaussparungen),
aber keine einzige Wand und keine Decke gibt — ein Planbereich, der nie ins 3D gebaut
wurde. Die Objekte sitzen dort korrekt auf ihren gezeichneten Symbolen. Vor dem
Löschen von „wirtlosen" Durchbrüchen also IMMER prüfen, ob 2D-Inhalt darunter liegt
(`ELM_SAB.Get2DGeometryOfElements` über die Schraffuren, nach Cluster auszählen) —
und das Ergebnis **visuell rendern**, nicht nur zählen.

## Mittig in der Wand: Kriterium, Reihenfolge, Grenzen <!-- 2026-09-06 -->

- Kriterium „mittig in der Wand": `|clearance + Dicke/2| ≤ 2 cm` (NICHT
  `|clearance|` — ein Objekt exakt auf der Achse hat `clearance = −Dicke/2`).
- Reihenfolge Drehen → Tiefe → Zentrieren: MoveElements senkrecht zur
  Wandachse (längs bleibt Planposition); die kleinere Tiefe verschiebt die
  Mitte um (B_alt − B_neu)/2 — nach jedem Schritt neu messen und iterieren
  bis nichts mehr offen ist (THN Runde 2: 0/0/0).
- Tiefe = Wanddicke gilt nur für Durchbrüche (WD/MD/HD/BSK); Schlitze
  (WS/SWS) gehen planmäßig nicht durch die Wand und behalten ihre Tiefe.
- An Wandkreuzungen wird die Zuordnung nach dem Zentrieren mehrdeutig —
  bewusst belassen.

## ⚠️ Parser-Falle: Bauteil-Kürzel kollidiert mit der Durchbruch-Grammatik <!-- 2026-08-31 -->

Die Grammatik `[Gewerk] WD|BD|WS|DD B/H` trifft auch Raumnummern, wenn ein **Bauteil**
so heißt wie ein Durchbruchstyp. Am THN heißen die Häuser WA/WB/WD/WE/WG — Texte wie
`WD.01.022`, `WD.02.018 T90-1 RS` oder `WD.101 Kühlschrank für Lebensmittel` wurden
als Wanddurchbrüche gelesen, und die **Raumnummer landete als Breite in Metern**:
`WD.01.022` → A = 1,02 m, `WD.04.001` → A = 4,00 m, `WD.101` → A = 1,01 m. 20 solcher
Objekte standen zwei Monate klassifiziert und KI-gestempelt im Modell, darunter drei
Brandschutztüren und fünf Laborgeräte.

Gegenmittel:
- Grammatik verankern: nach `WD` muss ein **Trenner + Maß** kommen, ein `.` gefolgt
  von Ziffern (`WD\.\s*\d`) disqualifiziert den Treffer.
- Die Bauteil-Kürzel des Projekts aus der Property `Allgemeine Werte / Bauteilname`
  ziehen (`API.GetAllPropertyIds` → `GetDetailsOfProperties` → `possibleEnumValues`)
  und als Blacklist gegen die Typ-Kürzel prüfen. Am THN steht „WD" dort wörtlich drin.
- Plausibilitätsgrenze: eine Durchbruchsbreite von 4,00 m ist ein Alarm, kein Maß.

## Geschosslogik + Beschriftung der Durchbruch-Symbole <!-- 2026-09-06 -->

Die Objekte „Bodendurchbruch Symbol", „Deckendurchbruch Symbol" und „Bodenschlitz"
zeichnen Geschossdarstellung und Typtext SELBST: `lineTypeFloor` gilt im eigenen
Geschoss, `lineTypeCeiling` im Nachbargeschoss, und der Typtext wechselt
spiegelbildlich (Bodendurchbruch auf Geschoss n zeigt „BD", auf n−1 „DD";
Deckendurchbruch umgekehrt nach oben). Ob im Nachbargeschoss überhaupt gezeichnet
wird, hängt allein an der Elementeinstellung „Auf Geschossen zeigen" — nicht am
Skript. Per Tapir nicht setzbar; seit ELM_SAB 0.9.15 dafür
`ELM_SAB.SetStoryVisibilityOfElements` / `GetStoryVisibilityOfElements` (Befehl
+ Parameter vollständig dokumentiert in
[`../reference/mcp-extension.md`](../reference/mcp-extension.md#elm_sab-0915--auf-geschossen-zeigen),
hier nur der Fachkontext). Live verifiziert THN 2026-09-06:
`visibility: "HomeAndOneDown"` → Rücklese `showRelBelow: 1`, Sichtbeleg im
Grundriss (eigenes Geschoss durchgezogen, Geschoss darunter gestrichelt, Text
BD→DD). Die DevKit-Warnung, das Relativ-Geschoss-Feature sei für
`API_ObjectType` „not extended", trifft in AC29 damit NICHT zu.

**Beschriftung derselben Objekte:** Text = `symb_use_short` (nur wenn
`bShowPrefix`) + Typtext + `A` + „ /" + `B`. `iSymbUse`: 1 Elektro · 2 Gas ·
3 Heizung · 4 Lüftung · 5 Sanitär · 0 „Eigene" — es gibt KEIN Kälte und KEINE
Kombi-Gewerke (HS, HLS, SH, ESH …); die laufen über `iSymbUse=0` +
`bShowCustomText=true` + `symb_cust_text`. Bei String/Integer-Paaren
(`symb_use`/`iSymbUse`) führt der Integer, der String wird bei Regeneration
daraus abgeleitet. Ohne `bShowPrefix=1` erscheint das Gewerkskürzel NIE — am
THN stand es projektweit auf 0, deshalb las der Plan „BD 1,60 /0,25" ohne
Gewerk, während 992 Objekte zusätzlich pauschal auf „Lüftung" (Werksdefault)
standen.

## Nachlese in HL-/Trassenplänen: was ein Fund ist <!-- 2026-09-07 -->

- Fund-Definition: jede Wandkreuzung einer Trasse, die ein Öffnungssymbol trägt —
  gekreuzter Kasten, WD-Marker auch ohne Maßangabe, Klappe, Bowtie.
- ⚠️ Kernfalle: Maßangaben mit „/" sind KEIN Ausschlusskriterium; der WG-Pilotlauf
  filterte sie heraus und verlor dadurch rund 40 % der Funde.
- Maß-Kaskade: Maß aus dem Label; sonst Kanalquerschnitt + 10 cm, dann als unsicher
  markieren; sonst `MASS?`-Platzhalter — nie stillschweigend raten oder verwerfen.
- Einheit mm/cm pro Blatt am Kanalmaß verifizieren (Blätter mischen die Einheiten).
- Steigschächte = Rechteck mit Diagonalkreuz (BD/DD). KEIN Fund: Treppen-Bruchlinien,
  Rundstützen (Kreis mit Kreuz), kreuzschraffierte 60/60-Stützen.
- Entdopplung: Symbol und Fahne liegen bis 2 m auseinander → über Label-Signatur bzw.
  Klappen-ID zusammenführen, nicht über Abstand allein.
- Gewerk normalisieren: Kombi-Angabe „H/L/S" → „HLS" (Anschluss an die bestehende
  Kombi-Gewerk-Regel `iSymbUse=0` + `symb_cust_text` oben, per Querverweis).
- Klappen: FSK/SK/ESK werden als BSK geführt, Maße in mm.
- Rohrhülsen `ROHRH D=…` in Wänden werden runde Wanddurchführung HD mit DN — nicht
  als 20/20-WD abbilden.
- Nebenzeichnungen mit eigenem Lineal per Skalierungsfaktor ins Hauptlineal überführen.
- Blätter ohne Achsraster (1:20-Schachtpläne) über Wandflächen gegen die
  3D-BoundingBoxen der Modellwände registrieren (`API.Get3DBoundingBoxes`);
  am THN Residuen ≤ 8 mm erreicht.
