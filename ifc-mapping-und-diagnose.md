# IFC — Übersetzer, Zuordnungsbäume, Diagnose

Der häufigste IFC-Fehlbefund in Archicad lautet „alles wird `IfcBuildingElementProxy`" —
und der zweithäufigste „die Klassifizierung fehlt im IFC". Das sieht nach einem Problem
aus. Es sind **zwei unabhängige Fehler mit zwei getrennten Fixes**, und wer nur einen
behebt, sieht das Symptom weiter und hält den Fix für wirkungslos.

Stand 2026-07-30, live an AC29 verifiziert.

## Inhaltsverzeichnis

1. [Die zwei Kanäle](#die-zwei-kanäle)
2. [Der Übersetzer und seine Bäume](#der-übersetzer-und-seine-bäume)
3. [Diagnose ohne Export — der IFC-Typ-Probe](#diagnose-ohne-export--der-ifc-typ-probe)
4. [Diagnose mit Export](#diagnose-mit-export)
5. [Richtig zählen: Elemente, nicht Relationen](#richtig-zählen-elemente-nicht-relationen)
6. [Typische Mapping-Fehler im Bestand](#typische-mapping-fehler-im-bestand)
7. [Ziel-Mappings](#ziel-mappings)
8. [Klassifikationsbäume per API](#klassifikationsbäume-per-api)

## Die zwei Kanäle

| | Symptom | Ursache | Fix |
|---|---|---|---|
| **A** | alles `IfcBuildingElementProxy` | Übersetzer-Einstellung „Elemente zuweisen: **Klassifizierung**", während das Quellsystem nicht (mehr) passt ⇒ alles fällt auf den Default-Knoten | Übersetzer auf **Element-Typ** umstellen — **UI** |
| **B** | SAB-/Bürodklassifizierung fehlt im IFC | die Elemente sind gar nicht klassifiziert (typisch: Favoriten und Werkzeug-Defaults tragen keine Klasse) | Favoriten + Defaults klassifizieren — **API**, siehe [`favoriten-und-defaults.md`](favoriten-und-defaults.md) |

Dokumentarisch sind es zwei getrennte Übersetzer-Kanäle (Graphisoft-Hilfe AC28/29):
„IFC-Typen zuordnen" schreibt **Entity + PredefinedType**, die Checkbox
„Klassifizierungen" unter *Datenkonvertierung* schreibt **`IfcClassificationReference`**.

**Der Beleg, dass A und B unabhängig sind:** ein Projekt mit 98,4 % klassifizierten
Elementen exportierte trotzdem 100 % Proxy. Die vermutete Verkettung
„unklassifiziert → Proxy" existiert dort nicht.

## Der Übersetzer und seine Bäume

Die IFC-Übersetzer sind **nicht per API editierbar** (AC29 / Tapir 1.5.3). Weder Auswahl,
noch Typ-Zuordnung, noch die Datenkonvertierungs-Checkboxen. Alles UI:

```
Datei → Interoperabilität → IFC → IFC-Übersetzer einstellen…
  ├─ Export-Übersetzer wählen (z. B. „Allgemeiner Übersetzer IFC4")
  ├─ „IFC-Typen zuordnen zum Export"  →  der Zuordnungsbaum
  │     oben: „Elemente zuweisen:  ( ) Klassifizierung   (•) Element-Typ"
  │     darunter: Baum mit einer Zeile je Quell-Kategorie
  │       └─ je Zeile: IFC-Entity (IFC2x3 + IFC4) + PredefinedType
  └─ „Datenkonvertierung" → Checkbox „Klassifizierungen" (Kanal B im Export)
```

Zwei Eigenschaften des Baums, die beim Pflegen wichtig sind:

- **Der Baum ist übersetzer-gebunden, nicht projekt-global.** Wer zwei Export-Übersetzer
  hat (IFC2x3 Coordination und IFC4 Reference), pflegt zwei Bäume. Ein „Proxy-Bug" kann
  in einem Übersetzer sitzen und im anderen nicht — real erlebt: IFC4 ReferenceView
  lieferte alles als Proxy, IFC2x3 CoordinationView desselben Projekts echte Klassen.
  **Bei jedem Proxy-Befund zuerst fragen: mit welchem Übersetzer wurde exportiert?**
- **Im Modus „Klassifizierung"** hängen die Baumzeilen an Klassifikations-Items des
  gewählten Quellsystems. Wechselt das System (Neuimport, neue Edition, anderer Name),
  finden die Zeilen ihr Item nicht mehr und fallen auf den Default — das ist Fehler A in
  Reinform. Im Modus **„Element-Typ"** hängen die Zeilen an Archicads Werkzeugtypen und
  sind damit immun gegen Klassifikations-Umbauten. **Darum ist Element-Typ die robustere
  Grundeinstellung für eine Bürovorlage.**

Manche Büros notieren das Soll-Mapping als Text in der **Beschreibung** des
Klassifikations-Items (z. B. „Zuordnung: IfcDoor / DOOR"). Das ist eine Dokumentations-
Konvention, keine Automatik — Archicad liest daraus nichts. Nützlich als Prüfliste beim
Pflegen des Baums, und per API lesbar über
`classifications_get_details_of_classification_items`.

## Diagnose ohne Export — der IFC-Typ-Probe

<!-- 2026-07-30 live verifiziert, AC29 -->

**`dev_get_ifc_type_of_elements`** liefert pro Element den IFC-Typ, den es beim Export
bekäme — ohne zu exportieren:

```json
{"port": <port>, "params": {"elements": [{"elementId": {"guid": "…"}}]}}
→ {"elementIFCTypes": [{"elementId": {...},
                        "ifcType": "IfcBuildingElementProxy",
                        "typeObjectIFCType": "IfcBuildingElementProxyType"}]}
```

Damit ist der Proxy-Bug in **zwei Calls** nachweisbar: `elements_get_elements_by_type`
mit `Wall`, dann den IFC-Typ der ersten paar Wände lesen. Kommt für eine Wand
`IfcBuildingElementProxy` zurück, ist Fehler A aktiv — kein Export, kein Grep, keine
Interpretation.

> Live-Beispiel: In der Büro-Vorlage („Ohne Titel", Port 19725) liefern die ersten drei
> von 19 Wänden alle `IfcBuildingElementProxy`. Genau der Befund, den der Übersetzer-Fix
> beheben muss.

Verwandte Reads: `dev_get_ifc_properties_of_elements` (Psets je Element),
`dev_get_ifc_ids_of_elements` und `dev_get_elements_by_ifc_ids` (Rückweg von einer IFC-GUID
zum Archicad-Element — praktisch beim Abarbeiten von Prüfberichten aus Solibri/BIMcollab).

**Nach einer Übersetzer-Änderung dieselbe Probe erneut fahren.** Sie ist die schnellste
Erfolgskontrolle; ob sie den *gerade aktiven* oder den zuletzt benutzten Übersetzer
spiegelt, ist nicht dokumentiert — daher immer den Vorher-Wert derselben Elemente
vergleichen, nicht absolut interpretieren.

## Diagnose mit Export

Der Export selbst geht per API: **`dev_ifc_file_operation`** mit
`method: "save"` und `ifcFilePath`. Auswählbar ist der Übersetzer dabei **nicht** — es
gilt die Einstellung aus dem UI. Für eine Vorher/Nachher-Diagnose also: exportieren,
Übersetzer im UI umstellen, erneut exportieren, beide Dateien vergleichen.

Grober Erstblick in eine IFC ohne Werkzeuge:

```bash
grep -c 'IFCWALLSTANDARDCASE' modell.ifc
grep -c 'IFCBUILDINGELEMENTPROXY' modell.ifc
```

## Richtig zählen: Elemente, nicht Relationen

Häufigste Fehlmetrik: „nur 3 Klassifikations-Einträge bei 544 Bauteilen — der Export ist
kaputt". Falsch. **Archicad schreibt eine `IfcRelAssociatesClassification` pro
Klassifikations-Item**, und alle Elemente dieser Klasse hängen als `RelatedObjects` an
dieser einen Relation. Drei Relationen können 500 Elemente abdecken.

Zählen muss man also die **RelatedObjects** über alle Relationen, dedupliziert. Ein
kleines Python-Skript, das das tut, hat sich als Standardwerkzeug bewährt (Regex auf
`IFCRELASSOCIATESCLASSIFICATION`, Klammerinhalt der RelatedObjects splitten, in ein Set).

Kennzahl fürs Ergebnis: **klassifizierte Elemente / exportierte Bauteile**. In einem
kranken Projekt waren das 2 von 544.

## Typische Mapping-Fehler im Bestand

Beim Übertragen eines gewachsenen Zuordnungsbaums in eine neue Vorlage die Zeilen **nicht
blind mitkopieren**. Real gefundene Tippfehler, die stillschweigend zu falschen oder
leeren Entities führen:

| Ist | Soll |
|---|---|
| `BORREL_ROOF` | `BARREL_ROOF` |
| `IfcCloumn` | `IfcColumn` |
| `IfcOpening` | `IfcOpeningElement` |
| `NOTEFINED` | `NOTDEFINED` |

Und: ein `PredefinedType` gilt nur, wenn er im **offiziellen Enum** der jeweiligen
IFC-Version steht. Alles andere muss über `USERDEFINED` + `ObjectType` transportiert
werden — sonst verwirft der Importer den Wert. Beim Aufbauen einer Mapping-Tabelle also
je Zeile mitführen, ob der Wert echter Enumwert ist.

## Ziel-Mappings

| Archicad-Typ | IFC-Entity | PredefinedType |
|---|---|---|
| Wand | `IfcWallStandardCase` | STANDARD |
| Tür | `IfcDoor` | — |
| Fenster | `IfcWindow` | — |
| Decke (Rohbau) | `IfcSlab` | FLOOR |
| Decke (Bodenplatte) | `IfcSlab` | BASESLAB |
| Decke (abgehängt) | `IfcCovering` | CEILING |
| Dach | `IfcRoof` | — |
| Stütze | `IfcColumn` | — |
| Träger / Unterzug | `IfcBeam` | BEAM |
| Treppe | `IfcStair` | — |
| Geländer | `IfcRailing` | — |
| Vorhangfassade | `IfcCurtainWall` | — |
| Zone | `IfcSpace` | — |
| Möbel | `IfcFurnishingElement` | — |
| Sanitäreinrichtung | `IfcSanitaryTerminal` | — |
| Leuchte | `IfcLightFixture` | — |
| Öffnung / Durchbruch | `IfcOpeningElement` | — |

## Klassifikationsbäume per API

Lesen:

- `classifications_get_all_classification_systems` — welche Systeme gibt es
- `classifications_get_all_classifications_in_system` — der **ganze Baum** (kann groß
  werden, ~145 KB; besser per HTTP-Bypass holen)
- `classifications_get_details_of_classification_items` — Name, ID, Beschreibung je Item
- `dev_get_classifications_of_elements` — Klasse je Element, funktioniert auch für
  Sub-Elemente hierarchischer Elemente

Schreiben:

- `dev_create_classification_systems` — legt ein **System samt verschachtelter Items** an
  (`id`, `name`, `description`, `children`). Das ist ein schwerer Eingriff: Properties
  sind über ihre *Availability* an Klassifikations-Items gebunden, und Item-GUIDs sind
  importspezifisch. Ein neues System nebenher anzulegen kostet die Property-Verfügbarkeit
  aller bestehenden Properties. **Nur mit ausdrücklicher Bestätigung und nur, wenn ein
  Merge im UI nicht der bessere Weg ist.**

**Item-GUIDs sind nie projektübergreifend gültig.** Immer im Zielprojekt frisch ermitteln.
In Favoriten-XMLs und in Übersetzer-Bäumen wird ohnehin über **Klartextnamen** verknüpft —
das ist der portable Weg.

**Vor dem Umbau eines gewachsenen Systems:** die Property-Landschaft zählen. Hängen viele
User-Properties per Availability am alten System (real: 160 Properties in 11 Gruppen), ist
„einfach auf ein Standardsystem umstellen" ein Tagesprojekt mit Datenverlustrisiko — und
meist ist das gewachsene System bereits die aktuelle Graphisoft-Basis plus Büro-Erweiterungen.
