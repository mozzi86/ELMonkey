# Referenzmodell-Abgleich (zwei Modelle im selben Projekt)

*Live-verifiziert 2026-07-14, THN: 4.500 Wände klassifiziert + Properties,
153 Türnummern übernommen. Muster: verzerrtes generiertes Modell ↔ gepflegtes
Referenzmodell, ~350 m versetzt im selben Teamwork-Projekt.*

## Referenz-Quellen (drei Wege)

Das Referenzmodell muss NICHT in derselben Datei liegen:

| Quelle | Zugriff | Versatz | Parallel lesbar? |
|---|---|---|---|
| **Gleiches Projekt** (THN-Fall) | eine API, räumlicher Versatz | Passpunkt-Paar | nein — ein Main-Thread |
| **Zweite Archicad-Instanz** | eigener Port (Discovery listet alle; multiconn) | meist 0 — echte Koordinaten | **JA** — eigener Prozess/Main-Thread; Reader auf Port B parallel zum Writer auf Port A |
| **IFC-Datei** | offline via ifcopenshell, ganz ohne Archicad | 0 (Projektkoordinaten) | **JA** — beliebig parallel, keine API-Last |

Zweite Instanz ist der sauberste Weg: kein Versatz-Gerechne, keine
Verwechslungsgefahr beim Schreiben (Ziel-Port ≠ Quell-Port), und Lese-Sweeps
laufen parallel zur Schreibarbeit. IFC ist ideal für reine Daten-Extraktion
(Properties, Klassifizierung, Geometrie) und als Snapshot/Archiv des Standes.

## Parallelisierungs-Matrix (Ein-Schreiber-Regel beachten)

Gleichzeitig möglich:
- **1 Schreiber** auf der Ziel-Instanz (seriell, häppchenweise)
- **n Leser** auf ANDEREN Instanzen (2. Archicad, andere Ports)
- **n Offline-Worker**: IFC-Parsing (ifcopenshell), DWG-Parsing (ezdxf),
  shapely-Analysen, Matching-Rechnung, Doku/Git
- NICHT parallel: zweiter Reader auf DERSELBEN Instanz — Tapir-Befehle laufen
  ohnehin seriell über den Main-Thread (kein Speedup, nur Timeout-Risiko).

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
- **Öffnungen (Opening) sind API-blind** (AC29): `GetDetailsOfElements` →
  details = „Not yet supported element type", `API.Get3DBoundingBoxes` →
  Fehler 6000 für ALLE Openings. Nur die 2D-Box + top-level floorIndex/id
  sind lesbar — der **Wirt ist nicht ermittelbar**. Achtung: floorIndex ist
  das Erzeugungs-Geschoss, nicht das des Wirts.
- **Snapshot-Diff statt Wirt-Abfrage**: Muss ein Element gelöscht werden, das
  Öffnungen tragen KÖNNTE (Decken-Duplikat etc.): vorher alle Opening-GUIDs
  + 2D-Boxen + IDs snapshotten, löschen (Wirt reißt seine Öffnungen mit),
  danach diffen — die Verschwundenen waren seine und lassen sich aus dem
  Snapshot auf dem Ersatz-Element rekonstruieren. THN: Decken-Duplikat OG4
  gelöscht, Diff = 0 → Durchbrüche hingen nachweislich am verbleibenden Element.

## Arbeitsblätter sind eigene Datenbanken (live 2026-07-16)

`GetElementsByType`/alle Element-Reads liefern nur die Datenbank des **aktiven
Fensters**. Arbeitsblätter (z. B. Brandschutz-Pläne je Geschoss) sind eigene
2D-Datenbanken: Im Grundriss sind ihre Elemente unsichtbar (Symptom: Text auf
Schraffur sichtbar im Blatt, aber „kein Hatch gefunden") — der User muss das
jeweilige Blatt-Fenster aktivieren, dann dieselben Abfragen erneut fahren.
Elemente wirken dadurch auch scheinbar „doppelt", wenn Blätter Kopien enthalten.
Workflow je Blatt: öffnen lassen → Sweep → Selektion zeigen → konvertieren.
2D-Konvertierungs-Muster „Etikett-Schraffur → Text-Deckung" siehe
[fills-hatches.md](../recipes/fills-hatches.md).

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

## Zonen kopieren (live-verifiziert 2026-07-14, 632/632)

- `CreateZones` (Param `zonesData`): floorIndex, name, numberStr,
  categoryAttributeId{guid}, stampPosition, stampAngle, fixedStampAngle,
  geometry (polygonOutline/polygonArcs/holes). **Kein layerIndex-Feld** —
  Ebene danach per SetDetailsOfElements setzen (sonst aktive Ebene!).
- `GetDetailsOfElements` für Zonen liefert per DIREKTEM Tapir-HTTP alles
  (name, numberStr, Stempel, Polygon inkl. Bögen + Löcher) — der bekannte
  AC29-Schema-Bug betrifft nur den MCP-Wrapper.
- **name/numberStr sind Create-only** — kein Modify-Pfad; SetDetails kann bei
  Zonen nur floor/layer + Stempel (typeSpecificDetails).
- Falle: **Mikro-Duplikatpunkte** (µm-Abstand!) im Quellpolygon →
  `-2130313102 Failed to create new Zone`. Distanz-Dedup der Punkte hilft
  (exaktes Dedup reicht NICHT).

## Befund-Nachtrag OG2 (2026-07-14 abends)

Die These „OG2-Linienwerk liegt auf Fremdimport-Alt-Ebenen (2MWTRAG_0 …)"
war FALSCH — dort lagen 0–1 Elemente; alles ist längst auf SAB-Ebenen migriert.
Das echte OG2-Problem: Begrenzungslinien schließen schlecht (7.395 Regionen,
aber nur 8 Teile > 20 m²). Lehre: **Ebenen-Hypothesen erst per Histogramm
verifizieren** (Elementzahl je Ebene je Geschoss), dann Pipeline bauen.

## Cross-DB-Operationen (live 2026-07-16)

- **Lesen über alle Datenbanken ohne Fensterwechsel**: `GetElementsByType` (u. a.)
  akzeptiert `databases: [{databaseId: {guid}}]`. DB-GUIDs beschaffen:
  `API.GetNavigatorItemTree` (ProjectMap) → `GetDatabaseIdFromNavigatorItemId`.
- **`DeleteElements` funktioniert per GUID auch quer zur aktiven DB** (getestet:
  Grundriss-Elemente gelöscht, während ein Arbeitsblatt aktiv war). Erst an
  1 Element testen, dann Masse. **Create bleibt an die aktive DB gebunden.**
- Text-Dupletten-Muster (Bestand): gleicher Inhalt + gleiches Geschoss +
  Abstand < 10 cm → einer bleibt. **Seiten-Filter (y-Grenze) nicht vergessen** —
  Referenz-Seite nur nach expliziter User-Freigabe anfassen. THN-Endstand
  2026-07-16: 8.235 Dupletten gelöscht, beide Seiten 0 (Referenz nach Freigabe
  + Reservierung + Ebenen-Einblenden mitbereinigt).

## DeleteElements versagt STILL (live 2026-07-16)

`DeleteElements` antwortet `{"success": true}` und löscht trotzdem NICHTS, wenn
- das Element nicht **reserviert** ist (Teamwork), ODER
- seine Ebene **ausgeblendet** ist (isHidden — auch reserviert!).

Kein Fehlercode, keine executionResults — der einzige Beweis ist die
**Rücklese** (Element noch per GUID lesbar?). Deshalb bei jedem Massen-Delete:
1 Testelement löschen → Existenz prüfen → erst dann Masse → Stichprobe danach.
Ebenen-Status prüfen: `API.GetAttributesByType` ('Layer') →
`API.GetLayerAttributes` (isLocked/isHidden), Namen gegen layerIndex der
Elemente matchen. Abhilfe: User blendet alle Ebenen ein (API kann Attribute
nicht ändern), dann löschen, dann zurück. THN: 2.199 sichtbare gelöscht,
2.850 auf hidden Layern blieben unlöschbar bis zum Einblenden.

Tückischste Folge: Ein früherer Massenlauf kann als Erfolg gemeldet worden
sein, obwohl NICHTS passiert ist (THN: 2.004 „gelöschte" Dupletten waren
tags darauf alle noch da — identische Zahl im Re-Sweep war der Verräter).
Deshalb gehört zu jedem „erledigt" ein **frischer Komplett-Sweep**, nicht
nur die Erfolgsmeldung des Delete-Calls. Auch der User-Blick taugt nicht als
Verifikation, wenn die betroffenen Ebenen ausgeblendet sind — er sieht genau
das nicht, was übrig blieb.
