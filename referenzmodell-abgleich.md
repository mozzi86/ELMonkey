# Referenzmodell-Abgleich (zwei Modelle im selben Projekt)

*Live-verifiziert 2026-07-14, THN: 4.500 Wände klassifiziert + Properties,
153 Türnummern übernommen. Muster: verzerrtes generiertes Modell ↔ gepflegtes
Referenzmodell, ~350 m versetzt im selben Teamwork-Projekt.*

## Grundregeln

- **Referenzmodell ist READ-ONLY** — Kopierrichtung immer Referenz → Ziel.
  Vor jedem Set*/Modify*/Delete prüfen, auf welcher Seite die GUIDs liegen
  (räumlicher Cluster-Test über Zentren ist robust).
- **Versatz per Passpunkt-Paar**: User selektiert dasselbe Element in beiden
  Modellen nacheinander, wir lesen beide Positionen (GetSelectedElements +
  Get2DBoundingBoxes). Ein Paar reicht für Translation (THN: dx=0.001,
  dy=349.820 — millimetergenau trotz „Knautschzonen"-Sorge).

## Knautschzonen (verzerrte Alt-DWGs) — was funktioniert und was nicht

Abstands-Histogramm zuerst! (Ref-Zentren → nächstes Ziel-Element): THN zeigte
kontinuierliche Verschmierung 0–4 m → lokale Verzerrung, kein globaler Fehler.

- ❌ **Element-zu-Element-Matching** (auch mit Zellen-Offsets oder Anker-IDW-
  Gummituch): bei Polywand-Bändern vs. Einzelwänden nur ~25 % Trefferquote —
  Bänder fassen mehrere Ref-Wände, Fragmente konkurrieren, Eindeutigkeits-
  Margen fressen mehr als Interpolation gewinnt.
- ✅ **Hybrid nach Datenart** (der Durchbruch):
  - *Kategorien mit Ebenen-Logik* (Klasse): *Kreuztabelle* Ref-Ebene × Klasse
    aufstellen → oft deterministisch (LEICHTWAND→Wand, FASSADE→Fassade) →
    regelbasiert zuweisen, ganz ohne Geometrie.
  - *Räumliche Kategorien* (Gebäudeflügel/Bauteilname WB/WE/WD/WG): erst
    Verteilung plotten (BBox je Wert) — sind es disjunkte Regionen, reicht
    **kNN-Mehrheit** (k=7) über die Ref-Punkte. Immun gegen ±4 m Verzerrung.
  - *Elementweise Werte* (Brandschutz): nächste Ref-Wand ≤ 2,5 m, gleiche
    Etage, OHNE Eindeutigkeitszwang; ohne Treffer ehrlich leer lassen.
  - *Punkt-Elemente* (Türen für Nummern): eindeutig-greedy nach Distanz ≤ 3 m,
    Median-Distanz als Qualitätsmaß (THN: 0,52 m = sauber).

## Property-/Klassifizierungs-API (Fallen!)

- **notAvailable-Falle**: Custom-Properties sind an Klassifikations-Items
  gebunden. Unklassifizierte Elemente melden `status: notAvailable` und JEDES
  Schreiben scheitert. **Reihenfolge: erst SetClassificationsOfElements, dann
  Properties.**
- Schreibformate (API.SetPropertyValuesOfElements, `status: 'normal'` PFLICHT
  — sonst oneOf-Schemafehler):
  - singleEnum: `{'type':'singleEnum','status':'normal','value':{'type':'displayValue','displayValue':'WB'}}`
  - multiEnum: `{'type':'multiEnum','status':'normal','value':[{'enumValueId':{'type':'displayValue','displayValue':'…'}}]}`
  - Typ eines Properties VOR dem Schreiben per Rücklese ermitteln (Bauteilname
    war singleEnum, nicht string!).
- Property-Katalog: API.GetAllPropertyNames (liefert nur Namen) →
  API.GetPropertyIds (Namen→GUIDs). UserDefined haben `localizedName: [Gruppe, Name]`.
- **Element-ID** (z. B. Türnummer „WG.02.001 T30-2") ist KEIN Property-Feld in
  SetDetails — Schreiben über BuiltIn-Property `General_ElementID`.
- Klassifizierung leer = Item-GUID `00000000-…` — herausfiltern.
- GetClassificationsOfElements braucht die System-ID explizit
  (leere `classificationSystemIds`-Liste ⇒ leeres Ergebnis).

## Nützliche Lese-Muster

- `GetDetailsOfElements` liefert **floorIndex + layerIndex TOP-LEVEL** (nicht
  in details!) und für Polywände sogar `polygonOutline` → präzises
  Host-/Seiten-Matching. Schema-Bug-Elemente per **Bisektion** überspringen
  (siehe mcp-extension.md).
- `Get2DBoundingBoxes` existiert nur als offizielles `API.Get2DBoundingBoxes`
  (nicht in Tapir). Für Türen/Objekte mit 2D-Symbol lügt die 2D-Box —
  `API.Get3DBoundingBoxes` nehmen.

## Teamwork-Diagnostik

- **Senden gibt Reservierungen frei** → danach scheitern ALLE Änderungen mit
  generischen Fehlern („Failed to set classification item", „Failed to create
  door") — nie mit „nicht reserviert". Erste Frage bei plötzlichen
  Massen-Fehlschlägen: „Wann wurde zuletzt gesendet/reserviert?"
- **Reserve-all blockt den UI-Hauptthread** minutenlang (Kondition-Wait):
  GetProductInfo antwortet trotzdem (kein Main-Thread nötig), Tapir-Befehle
  nicht → so unterscheidet man „hängt" von „arbeitet". `sample <pid>` zeigt
  den Wait; BIMcloud-Erreichbarkeit separat prüfen (ping/curl).
- Nur **EIN API-Schreiber gleichzeitig** (auch bei Agenten-Delegation!) —
  parallele Schreiber produzieren „Invalid program status"-Kaskaden.
