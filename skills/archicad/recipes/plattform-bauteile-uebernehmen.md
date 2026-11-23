# Rezept: Bauteile aus der Schwarz Platform nach Archicad übernehmen <!-- 2026-11-16 -->

Live durchlaufen an Projekt-H (AC29 Teamwork, ELMonkey mit Tapir 1.7.2):
20 Tragtraversen (Ebene „Traversen Sanitär (LV 002)“, 2. OG + DG), die Claude in der Plattform
gezeichnet hatte, als Träger ins Archicad-Modell übernommen. Das Muster gilt für alles, was in der
Plattform mit `modell_bauen` entsteht (Duofix-Elemente, Kernbohrungen, Stahlteile …).

## Ablauf

1. **Projekt und Instanz:** `projekte_suchen` (Projektnummer, hier „Projekt-H“) und
   `discovery_list_active_archicads` (Projektname → Port; Ports wechseln bei jedem Neustart).
2. **Bauteile finden:** `modell_lesen` (Ebenen mit Zählung) → `modell_register_lesen` mit
   `modellart:"plattform"`. Die Ebene selbst (`modell_ebenen`) zeigt bei gebauten Bauteilen **keine
   Objekte** (`objekte: []`). Die Bauteile stehen nur im Register (`herkunft.ebene`). Große
   Antworten landen in einer Datei; dort mit Python filtern.
3. **Exakte Geometrie in Archicad-Koordinaten:** `modell_exportieren` mit
   `modellarten:["plattform"]`, `mit_ebenen:["<Ebene>"]`, `mit_bezuegen:false`. Das IFC liegt
   **in Archicad-Koordinaten** (Meter, z = absolute Archicad-Höhe), GUIDs wie in der Plattform.
   Download per `curl` in einen eigenen Ordner. `ifcopenshell` ist nicht installiert; das STEP
   reicht mit Regex: `IFCMEMBER` → Shape → `IFCTRIANGULATEDFACESET` → `IFCCARTESIANPOINTLIST3D`.
   Die Pset `SAB_Plattform` trägt `Skizzen-ID`, `Profil`, `Werkstoff`; `Qto_SAB_Plattform` die Länge.
   Achse und Dicke je Bauteil per Hauptachse (PCA) der xy-Punkte.
   Achtung: Das Export enthält **alle** Plattform-Bauteile (auch Traversen *innerhalb* der
   Duofix-Elemente); nach `Skizzen-ID`/Herkunft filtern.
4. **Gegenprobe:** Grundmodell-GUIDs der Plattform sind die Archicad-GUIDs. Eine Wand per
   `GetDetailsOfElements` lesen und mit `bauteil_geometrie` vergleichen.
5. **Anlegen** (hier `CreateBeams`, Breite = Blechdicke + Schenkel, Höhe 0,302, `anchorPoint
   BottomCenter`, `zCoordinate` = Unterkante, `floorIndex` explizit, Baustoff „Stahl verzinkt_760“).
   Bei Tapir 1.7.2: ohne `id`/`layerIndex` anlegen, danach Ebene und Element-ID setzen (siehe
   [`../reference/mcp-conventions.md`](../reference/mcp-conventions.md), „MCP-Schema ≠ installiertes Add-on“).
6. **Rücklesen:** `GetDetailsOfElements` + `Get3DBoundingBoxes` gegen die IFC-Box (hier mm-genau).
7. **KI-Stempel** setzen; fehlt die Property im Projekt (Projekt-H am 16.11.), der Nutzerin melden.
8. **Zeigen:** Grundriss per `ChangeWindow` öffnen, Bild speichern, Bauteile darüberzeichnen.

## Koordinaten, falls kein Export möglich ist

- `bauteil_geometrie` liefert **Projekt-Meter** (x Ost, y Nord ab Modelladresse) und Höhen **über
  Nullniveau**. `modell_register_lesen` liefert Unter-/Oberkante dagegen in **Archicad-Höhe**.
  Unterschied = `georeferenz_archicad.hoeheNN − nullniveauNN` (Projekt-H 12,52 m).
- Lage: Archicad hat oft keine Nordrichtung gesetzt (`project_get_geo_location` → north π/2,
  Survey Point 0), die Plattform aber eine eingepasste (`nordrichtungGrad`, Projekt-H 308,26°).
  Archicad → Projekt-Meter: `P = O + x·(cos θ, sin θ) + y·(−sin θ, cos θ)`, θ = nordrichtungGrad,
  O = Archicad-Ursprung (lat/lon aus `georeferenz_archicad` gegen `ursprung_wgs84`, m/° nach
  Breite). Damit lagen die Traversen auf ~1 cm. Ein Ausgleich über Wand-Hüllen der Plattform war
  ungenauer (±10 cm), weil die Hüllen Anschlusspunkte mitnehmen.

## Fallen

1. **Fast-achsparallel:** In der Plattform im gedrehten System gezeichnete Bauteile kommen um
   0,1–0,8° schief an (Rundung der Hüllen). Bei < 1,5° auf die Archicad-Achse einrasten (Mittel der
   Querkoordinate). Echte Schrägen stehen lassen und melden (Projekt-H: TRAV-OG2-04, 14 cm, 15°).
2. **Bauteile in Vorwänden** sind im gespeicherten Grundriss kaum zu sehen (Schnitthöhe, 22 mm).
   Das ist kein Fehler; zum Zeigen überzeichnen.
3. **Ebene:** Der Werkzeug-Standard kann eine fremde Ebene sein (Projekt-H Träger → „A_80_MASKENBLOCK“).
   Immer bewusst setzen und vorher prüfen, dass die Zielebene sichtbar und nicht gesperrt ist.

## Was der Nutzer entscheidet

- Werkzeug (Träger, Objekt, Morph), Ebene und Baustoff, wenn es keinen passenden Favoriten gibt.
  Projekt-H: Träger auf „A_28_EINBAU_SANIT“, Element-ID = Skizzen-ID in Großbuchstaben.
- Ob eingebettete Teile (Traversen in Duofix-Elementen) mitkommen.
- Senden in Teamwork.

## Rückweg: Archicad-Änderungen in die Plattform (ohne Komplett-IFC) <!-- 2026-11-16, live Projekt-H -->

Die Nutzerin will **kein ganzes IFC** übertragen. Live am 16.11.: Nur die Unterschiede wurden
einzeln als Vorschläge geschrieben (Projekt-H Vorschläge 19, 21–26).

1. `archicad_abgleich_lesen` → `stand.json` (alle Plattform-Bauteile mit IFC-Eigenschaften).
2. Vergleich je GlobalId, Toleranz 3 mm. Geometrie **wie im IFC** (getrimmt, Teile von Mehrschicht-
   Bauteilen zusammengefasst), nicht die rohe Archicad-3D-Box. Alle `IfcElement`-Klassen nehmen
   (sonst fehlt z. B. `IfcFurniture`), Werte normalisieren (`.T.`/Ja, `IFCLABEL('X')`, `\X\E4`),
   reine Mengenabweichungen nicht werten, Abbruch nicht durch den Umbau-Filter verlieren,
   `IfcOpeningElement`-GUIDs ignorieren (pro Export neu).
3. Schreiben mit `modell_bauteile_aendern` (Standard: Vorschlag): `eigenschaften_setzen`
   (status + psets) · `geometrie laenge {ende:[O,N], um}` entlang der Längsachse · `dicke {um, fest:
   Gegenseite}` quer · `hoehe {kante, bis: z + (hoeheNN − nullniveauNN)}` · `hinzufuegen` (platte/
   verbund, `koordinaten:"archicad"`, `geschoss` als GUID — zwei Geschosse heißen „Obergeschoss“) ·
   bei nicht rechteckigem Grundriss `ersetzen` mit `guid_behalten` (Schichtaufbau geht verloren).
4. Große Antworten (> 40 Ops) landen in einer Datei; nur `geschrieben`, `nr`, Fehler auswerten.
   Ein Fehler in einem Op verwirft den ganzen Satz — nicht rechteckige Bauteile vorher aussortieren.
5. Vorschläge sind in `modell_zeigen` und `modell_ansicht` nicht sichtbar (nur blau in Web/iPad).

**Geplant:** ELMonkey-Befehl `DiffPlattformELM`, der direkt im Archicad gegen `stand.json`
vergleicht, ohne Export. Anforderung in
`ZZ_Austausch/ELMonkey Diff-Checker Plattform-Abgleich 2026-11-16/ANFORDERUNG_Diff-Checker.md`.
Bis dahin: Export nur in den Scratchpad, danach löschen.
