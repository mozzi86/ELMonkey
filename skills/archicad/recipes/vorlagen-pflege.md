# Bürovorlage pflegen (.tpl)

Eine Bürovorlage ist der Ort, an dem sich Datenqualität am billigsten herstellen lässt und
am teuersten fehlt: jeder Fehler darin vererbt sich in jedes künftige Projekt. Dieses
Rezept beschreibt den Ablauf eines Vorlagen-Umbaus so, dass am Ende jede Änderung belegt
ist — und die ungesicherte Arbeit dazwischen nicht verloren geht.

Live durchlaufen 2026-07-30 an einer AC29-Bürovorlage (446 Favoriten, 87 Ebenen,
122 Baustoffe, 160 User-Properties).

## Inhaltsverzeichnis

1. [Reihenfolge](#reihenfolge)
2. [Schritt 0 — Sicherungsstrategie](#schritt-0--sicherungsstrategie)
3. [Schritt 1 — Ist-Stand live lesen](#schritt-1--ist-stand-live-lesen)
4. [Schritt 2 — Bibliotheken einbetten](#schritt-2--bibliotheken-einbetten)
5. [Schritt 3 — Attribute](#schritt-3--attribute)
6. [Schritt 4 — Favoriten und Defaults](#schritt-4--favoriten-und-defaults)
7. [Schritt 5 — Properties](#schritt-5--properties)
8. [Schritt 6 — IFC-Übersetzer](#schritt-6--ifc-übersetzer)
9. [Schritt 7 — Abschlussbeleg](#schritt-7--abschlussbeleg)
10. [Fallen](#fallen)

## Reihenfolge

Die Reihenfolge ist nicht beliebig — jeder Schritt setzt Ergebnisse des vorherigen voraus:

```
0  Sicherung + Arbeitskopie
1  Ist-Stand lesen           (API, read-only)
2  Bibliotheken einbetten    (API)      ← Favoriten brauchen ihre GDL-Teile
3  Attribute: Ebenen, Baustoffe, Kombis (API + UI)  ← Favoriten verweisen darauf
4  Favoriten patchen + Defaults setzen  (API)
5  Properties anlegen        (UI)
6  IFC-Übersetzer            (UI)
7  Abschlussbeleg + .tpl sichern
```

Wer 4 vor 3 macht, patcht Favoriten auf Ebenen, die noch umbenannt oder gelöscht werden.
Wer 2 nach 4 macht, importiert Favoriten, deren Bibliothekselement fehlt.

## Schritt 0 — Sicherungsstrategie

- Die **Original-`.tpl` bleibt unangetastet** — an ihr hängen laufende Projekte.
- Arbeiten geschieht in einer aus der Vorlage geöffneten Datei; Zwischenstände als
  `<Name> <YYYYMMDD>_<HHMM>_ENTWURF.tpl` sichern.
- Alte Zwischenstände nach `_alt/` bzw. `_Archiv/` räumen, nicht löschen.
- **API-Änderungen sind nicht auf der Platte.** Der Nutzer muss sichern; wir erinnern
  aktiv daran, bevor der nächste große Block beginnt — nicht erst am Ende.

## Schritt 1 — Ist-Stand live lesen

Nicht aus Dateien im Vorlagenordner ableiten, sondern **im laufenden Projekt messen**.
Die geöffnete Vorlage ist die Wahrheit; exportierte XML-Stände (Klassifikations-Exporte,
Property-Listen) sind oft älter und ein Reimport würde neuere Arbeit vernichten.

Was in einen brauchbaren Ist-Stand gehört:

| Feld | Aufruf |
|---|---|
| Klassifikationssysteme (wie viele, welche Edition) | `classifications_get_all_classification_systems` |
| User-Properties (Anzahl, Gruppen, Datentypen) | `API.GetAllPropertyIds` + `API.GetDetailsOfProperties` → Key `propertyDefinitions` |
| Ebenen + Kombinationen | 2-Schritt-Muster, s. [`attribute-und-ebenen.md`](../reference/attribute-und-ebenen.md) |
| Baustoffe / Verbünde / Profile | dito |
| Favoriten (Anzahl, Typen, Klassifizierungsstand) | `favorites_export_favorites` in einen Ordner, dann auswerten |
| Bibliotheken | UI-Bibliothekenmanager (API-Read defekt) |
| IFC-Typ-Stichprobe | `dev_get_ifc_type_of_elements` an je 2–3 Wänden/Türen/Decken |

Ergebnis als kurzes Befund-Dokument festhalten, mit Zahlen. Diese Zahlen sind später der
Vorher-Wert für die Erfolgskontrolle.

## Schritt 2 — Bibliotheken einbetten

`library_add_files_to_embedded_library` funktioniert und verträgt große Mengen — **in
Batches** (bewährt: 60 Dateien pro Call, 380 Dateien in 7 Batches, jeweils voller
`success`). Eine `.lib`-Bibliothek wird dabei als Dateibaum eingebettet: Objekt, Label,
Makros und Bitmaps einzeln.

Wichtig:

- **Version prüfen, nicht Namen.** Zwei Projekte können dasselbe Objekt in verschiedenen
  Ausbaustufen enthalten. Welche Variante die gewünschte Erweiterung hat, entscheidet ein
  Blick in die Parameterliste (etwa: ist der gesuchte Bandtyp als Klartext vorhanden?),
  nicht der Dateiname.
- **Die Rücklese geht nur im UI** — `library_get_available_library_parts` ist defekt.
  Leere Antwort ≠ nichts eingebettet.
- Der physische Ablageort der eingebetteten Bibliothek wechselt pro Instanz
  (`…/Graphisoft/AutoSave-AC-ARM-29-N/Emb_<ID>/content`) — nicht hart darauf verlassen.

## Schritt 3 — Attribute

Reihenfolge innerhalb des Schritts: **erst prüfen, dann umbenennen, zuletzt löschen.**

1. Dubletten ermitteln (normalisierter Namensvergleich + tatsächliche Verwendung).
2. Fehlende Attribute anlegen (`attributes_create_building_materials` für ganze Gruppen).
3. Renames nur, wenn keine AVA-/Auswertungsfilter mit *equals* darauf zeigen.
4. Löschen: leere Ebenen per API; Baustoffe/Verbünde/Profile im Attributmanager (UI), weil
   dort die Ersatzwahl gestellt wird.
5. Ebenenkombinationen gegenprüfen — inkl. `intersectionGroupNr` bei neuen Ebenen.

Details und Fallen: [`attribute-und-ebenen.md`](../reference/attribute-und-ebenen.md).

## Schritt 4 — Favoriten und Defaults

Der Round-Trip Export → textbasiert patchen → Import (mit `AddPars/`!) → Frisch-Export als
Beleg. Vollständig beschrieben in
[`favoriten-und-defaults.md`](../reference/favoriten-und-defaults.md) — inklusive der
AddPars-Falle, die einen ersten Importversuch garantiert scheitern lässt, und der
generischen Layer-Tag-Regel.

Danach die Werkzeug-Defaults aus je einem Favoriten setzen, damit auch ohne
Favoritenklick klassifiziert gezeichnet wird. Dach bleibt UI.

## Schritt 5 — Properties

Anlegen per UI (Eigenschaften-Manager), prüfen per API. Drei Muster, die wiederkehren:

- **Enum statt Freitext**, wenn eine Property in Überschreibungen oder Filtern verwendet
  wird. Eine `string`-Property greift in einer Grafik-Überschreibung nicht, die auf
  Enum-Werte (GUIDs) prüft — sie sieht aus wie gepflegt und wirkt nicht.
- **Neue Property statt Umdeutung** einer bestehenden. Wer eine gewachsene Property
  umbaut, bricht alles, was darauf zeigt; eine zweite, klar benannte Property ist
  billiger.
- **Enum schlank halten.** Kombinierte Werte („Fliesen R10") explodieren kombinatorisch —
  besser zwei Properties (Belag + Rutschhemmung), notfalls per Expression verknüpft
  ([`property-expression-linking.md`](../reference/property-expression-linking.md)).

Nicht vergessen: die **Verfügbarkeit** je Klassifizierung ankreuzen, sonst ist die
Property am Element `notAvailable`.

## Schritt 6 — IFC-Übersetzer

UI, und der wirksamste Einzelschritt gegen den Proxy-Bug. Vorher/Nachher mit
`dev_get_ifc_type_of_elements` an denselben Elementen belegen. Details:
[`ifc-mapping-und-diagnose.md`](../reference/ifc-mapping-und-diagnose.md).

## Schritt 7 — Abschlussbeleg

Ein Vorlagen-Umbau ist erst fertig, wenn die Zahlen aus Schritt 1 gegen den neuen Stand
gestellt sind. Beleg-Tabelle mit drei Spalten: *Was*, *vorher*, *nachher* — und für jede
Zeile die Messmethode. Positivprobe und Negativprobe (was **nicht** geändert wurde) gehören
beide dazu.

Zum Schluss: `.tpl` sichern, Dateinamen und Ablageort prüfen, alte Zwischenstände räumen.

## Fallen

- **`projectPath` lügt.** Eine aus einer `.tpl` geöffnete, unbenannte Datei meldet weiter
  den Vorlagenpfad. Zustand am Inhalt prüfen, nicht am Pfad.
- **Modale Dialoge blockieren alle API-Calls** (Code `4001`). UI-Schritte bündeln, nicht
  mit API-Arbeit verschachteln.
- **Port wechselt** bei jedem Neustart — nach jeder Nutzeraktion neu ermitteln.
- **Ordner-Verwechslung.** Wenn der Nutzer selbst exportiert oder importiert, unbedingt
  einen eindeutig benannten Ordner vorgeben (Desktop, sprechender Name mit Datum). Ein
  Nutzer-Export in unseren Arbeitsordner überschreibt die gepatchten Dateien — zweimal
  erlebt. Irreführend gewordene Arbeitsordner sofort umbenennen.
- **Ein `success: true` ist kein Beleg.** Import, Default-Apply und Embed melden Erfolg
  auch dann, wenn nichts oder das Falsche passiert ist. Es gilt nur, was ein frischer Read
  zeigt.
