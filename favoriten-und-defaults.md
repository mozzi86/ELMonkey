# Favoriten und Werkzeug-Standardeinstellungen

Favoriten sind der wirksamste Hebel für Datenqualität in einem Büro — und der am
häufigsten übersehene. Ein Favorit trägt nicht nur Geometrie und Aussehen, sondern auch
**Klassifizierung, Ebene, Baustoff, Profil, Property-Werte**. Wer über Favoriten zeichnet
— also praktisch jeder — erbt genau das, was im Favoriten steht. Steht dort nichts, hat
jedes neu gezeichnete Bauteil ab dem ersten Klick eine Datenlücke, die später niemand
mehr systematisch findet.

Die Werkzeug-Standardeinstellungen („Defaults") sind die zweite Ebene desselben Hebels:
was gilt, wenn der Nutzer *keinen* Favoriten anklickt.

Alles hier ist am 2026-07-30 live an einer Büro-Vorlage mit 446 Favoriten verifiziert
(Archicad 29, Tapir-MCP). Verifikationsstand: 314 Favoriten klassifiziert, 132 bewusst
leer, 50 Ebenen-Fehlverlinkungen korrigiert, per Frisch-Export gegengeprüft.

## Inhaltsverzeichnis

1. [Werkzeuge — Tool-Inventar](#werkzeuge--tool-inventar)
2. [Der Round-Trip: Export → Patch → Import](#der-round-trip-export--patch--import)
3. [Die AddPars-Falle](#die-addpars-falle)
4. [XML-Regeln: nie ElementTree](#xml-regeln-nie-elementtree)
5. [Klassifizierung im Favoriten](#klassifizierung-im-favoriten)
6. [Ebene im Favoriten — zwei Stellen](#ebene-im-favoriten--zwei-stellen)
7. [AttrTable: RefId als FourCC](#attrtable-refid-als-fourcc)
8. [Werkzeug-Standardeinstellungen setzen](#werkzeug-standardeinstellungen-setzen)
9. [Verifikation — nur per Frisch-Export](#verifikation--nur-per-frisch-export)
10. [Worked Example: 446 Favoriten](#worked-example-446-favoriten)

## Werkzeuge — Tool-Inventar

<!-- alle 2026-07-30 live verifiziert, AC29 -->

| Tool | Was es tut | Wichtige Parameter |
|---|---|---|
| `favorites_export_favorites` | schreibt Favoriten als Ordner mit einer XML je Favorit — oder als eine `.prefs`-Binärdatei | `path` (Endung `.prefs` ⇒ Einzeldatei, sonst Ordner), `names` = Teilmenge |
| `favorites_import_favorites` | liest Ordner oder `.prefs` zurück ins Projekt | `path`, `conflictPolicy` (`Error`/`Skip`/`Overwrite`/`Append`, Default `Overwrite`), `importFolders`, `targetFolder` |
| `favorites_apply_favorites_to_element_defaults` | setzt die Werkzeug-Defaults aus benannten Favoriten | `favorites` = Liste von **Favoritennamen** (exakt, inkl. Umlaute) |
| `favorites_create_favorites_from_elements` | macht aus vorhandenen Elementen Favoriten | `elementId` + `favorite` (Name) je Eintrag |

**Ordner-Export ist der Arbeitsmodus.** Nur er gibt pro Favorit eine lesbare, patchbare
XML. Die `.prefs`-Variante ist ein Blob und taugt nur als Ganzes-Backup.

**Vor jeder Patch-Aktion:** den unveränderten Export wegsichern
(`tar czf favs_original.tar.gz favs_export/`). Er ist die einzige Rückfallebene, wenn ein
Patch-Lauf etwas zerlegt — Undo greift bei Favoriten-Import nicht zuverlässig.

## Der Round-Trip: Export → Patch → Import

```
1. favorites_export_favorites   → Ordner mit N XMLs + AddPars/
2. tar czf backup.tar.gz <Ordner>
3. Skript patcht die XMLs TEXTBASIERT (nicht ElementTree!)
4. AddPars/ mit in den Import-Ordner kopieren
5. favorites_import_favorites   conflictPolicy=Overwrite, importFolders=true
6. frisch exportieren und gegen die Soll-Werte prüfen
```

Schritt 6 ist nicht optional. Der Import meldet Erfolg auch dann, wenn er Dateien
stillschweigend ignoriert hat.

## Die AddPars-Falle

**Das ist die wichtigste Einzelinformation dieser Datei.**

GDL-basierte Favoriten — Objekt, Tür, Fenster, Treppe, Vorhangfassade, MEP — speichern
ihre GDL-Parameter **nicht in der XML**, sondern in Sidecar-Dateien in einem
Unterordner `AddPars/` neben den XMLs. Die XML verweist über eine innere OdbObj-GUID
darauf.

Fehlt `AddPars/` im Import-Ordner, dann werden diese Favoriten **abgelehnt** — mit einer
Meldung, die in eine völlig falsche Richtung zeigt:

- UI: „333 XML-Dateien werden ignoriert, keine gültige Favoriten XML"
- API: Tapir-Fehler `4009`

Beide Meldungen legen einen XML-Formatfehler nahe. Es ist keiner. Es fehlt der Ordner.

**Regel:** Der Import-Ordner muss immer alles enthalten, was der Export erzeugt hat —
XMLs *und* `AddPars/`. Wer nur die geänderten XMLs in einen neuen Ordner kopiert,
verliert genau die 2/3 der Favoriten, die GDL benutzen. Am einfachsten: Export-Ordner
kopieren, im Kopie-Ordner patchen, den Kopie-Ordner importieren.

Aufgedeckt wurde das erst durch einen **Referenz-Export des Nutzers aus der UI** und
einen Byte-Diff gegen den eigenen Ordner. Merksatz für ähnliche Sackgassen: *einen
funktionierenden Referenzfall erzeugen lassen und binär vergleichen*, statt die
Fehlermeldung weiter zu interpretieren.

## XML-Regeln: nie ElementTree

Archicads Favoriten-Parser ist byte-empfindlich. Ein Rewrite mit Pythons
`xml.etree.ElementTree` erzeugt eine syntaktisch gültige, für Archicad **ungültige**
Datei. Drei Unterschiede reichen:

| Archicad schreibt | ElementTree schreibt |
|---|---|
| `<?xml version="1.0" encoding="UTF-8" standalone="no"?>` | ohne `standalone` |
| `<Tag />` (Leerzeichen vor Slash) | `<Tag/>` |
| Schlusszeile / Whitespace-Layout | normalisiert |

**Also: textbasiert patchen.** Original einlesen, per Regex genau die Zielstelle
ersetzen, zurückschreiben. Alles andere Byte für Byte unangetastet lassen.

Zweite Falle auf macOS: **Unicode-Normalisierung**. Dateinamen mit Umlauten kommen als
NFD vom Dateisystem, Namen aus JSON/Archicad als NFC. Ein Mapping `Dateiname → Sollwert`
verfehlt sonst still jeden Favoriten mit Umlaut (im Ernstfall 77 von 446). Beide Seiten
mit `unicodedata.normalize('NFC', s)` vergleichen.

## Klassifizierung im Favoriten

Der Block sitzt im Typ-Container (`WallDefault`, `ObjectDefault`, `DimDefault`, …) und
sieht im Original leer so aus: `<ClassificationItemSet/>`. Gefüllt gehört er in genau
diesem Format (Tabs wie Archicad, Klartextnamen — **keine GUIDs**):

```xml
<ClassificationItemSet>
	<ClassificationSystemUserID>
		<Name>SAB_Klassifizierung_29</Name>
		<EditionVersion>02</EditionVersion>
	</ClassificationSystemUserID>
	<ClassificationItemUserID>Trockenbauwand</ClassificationItemUserID>
</ClassificationItemSet>
```

Dass hier **Namen statt GUIDs** stehen, ist ein Vorteil: der Patch ist projektübergreifend
portabel. GUIDs von Klassifikations-Items sind importspezifisch und dürfen nie zwischen
Projekten kopiert werden.

**Nur das ERSTE `ClassificationItemSet` je Datei füllen.** Eine Favoriten-XML kann bis
zu ~65 weitere enthalten — die gehören zu verschachtelten Sub-Defaults (Fenster im
Wandfavoriten, Sub-Elemente einer Fassade). Wer alle füllt, klassifiziert Sub-Elemente
falsch mit.

**Was bewusst leer bleibt:** 2D und Dokumentation — Bemaßung, Text, Etikett, Schnitt,
Detail, Zeichnung, Marker. Dort ist „keine Klassifizierung" die fachlich richtige
Antwort, nicht eine Lücke. In der Praxis waren das 132 von 446.

**Klassifiziert wird nach Funktion, nicht nach Werkzeug.** Ein „Abgehängte Decke
0,6 × 0,6" im `CurtainWallDefault` ist eine Decke; „Beton, Sockel" im `StairDefault` ist
eine Treppe, keine Wand. Wer nach Container-Typ mappt, produziert genau hier Fehler.

## Ebene im Favoriten — zwei Stellen

Damit ein Favorit auf der richtigen Ebene landet, müssen **zwei** Stellen zusammenpassen:

1. **Das Layer-Tag im Typ-Container.** Sein Name variiert je Elementtyp:
   `Layer` (DimDefault; bei LineDefault verschachtelt in `LineBaseDefault`),
   `AngDimLayer` (AngDimDefault), `HatchLayer` (HatchDefault) …
   ⇒ **generisch über `tag.endswith('Layer')` gehen.** Wer hart auf `Layer` prüft,
   verändert stillschweigend nur einen Teil (real erlebt: 35 von 50).
2. **Der `LAY2`-Eintrag in der `AttrTable`** — dort sind **`Name` und `Index`** beide
   auf die Ziel-Ebene zu setzen.

## AttrTable: RefId als FourCC

Die `AttrTable` eines Favoriten listet Attribute **aller Typen durcheinander**. Der Typ
steckt in `RefId` als FourCC-Zahl:

| RefId | FourCC | Bedeutung |
|---|---|---|
| 1279351090 | `LAY2` | **Ebene** |
| 1279872562 | `LIN2` | Linientyp |
| 1179208755 | `FIL3` | Schraffur |
| 1296127026 | `MAT2` | Oberfläche |
| 1112359252 | `BMAT` | Baustoff |
| 1347571526 | `PROF` | Profil |
| 1296388179 | `MEPS` | MEP-System |

**Wer nach Namen sucht, ohne `RefId` zu prüfen, bekommt Unsinn.** Genau das führte in der
Vorlagen-Session zu einem Fehlbefund („diese 8 Ebenen sind verwaist") — die Ebenen waren
in Gebrauch, nur zeigte der Namensvergleich auf Schraffuren und Oberflächen gleichen
Namens. Der Nutzer entdeckte es an einem Symptom, das die Auswertung nie zeigte: das
Bemaßungswerkzeug stand auf der Ebene „10 Dämmung".

## Werkzeug-Standardeinstellungen setzen

`favorites_apply_favorites_to_element_defaults` nimmt **Favoritennamen** (nicht GUIDs)
und setzt damit das Default des zugehörigen Werkzeugs. Ein Aufruf pro Werkzeug ist der
saubere Weg — bei einer Liste ist nicht kontrollierbar, welcher Favorit welches Werkzeug
gewinnt.

Abzudecken sind: Wand, Vorhangfassade, Stütze, Träger, Decke, Dach, Schale, Morph, Netz,
Treppe, Geländer, Objekt, Lampe, Fenster, Tür, Dachfenster, Öffnung, Zone.

**Bekannte Grenzen** (2026-07-30, AC29):

- **Dach schlägt fehl** — alle Roof-Favoriten liefern `-2130313114`. Nur per UI setzbar:
  Dach-Werkzeug aktivieren, in der Favoriten-Palette **doppelklicken**.
- **Verbund-Favoriten sind keine Element-Favoriten.** „KS 17,5 cm beidseitig Gipsputz"
  ist ein Verbund, kein Wandfavorit — als Wand-Default scheitert er. Stattdessen einen
  echten Wandfavoriten wählen.
- Bei Fehlschlag prüfen, ob der Name exakt stimmt (Umlaute, Komma, doppelte Leerzeichen).

Nach dem Setzen **rücklesen**: einen Testfall zeichnen oder einen Frisch-Export der
Defaults prüfen. Ein `success: true` ist hier kein Beleg.

## Verifikation — nur per Frisch-Export

Die einzige belastbare Prüfung ist: **neu exportieren und die Soll-Tabelle gegenrechnen.**
Nicht die Import-Antwort, nicht ein Stichproben-Screenshot.

Gute Kennzahlen für den Abschlussbericht:

- „N von N Favoriten mit Soll-Klasse haben sie" (positiv)
- „M von M bewusst leeren sind leer" (negativ-Gegenprobe — fängt Über-Patchen)
- „0 mit falscher Klasse" (Kreuzprobe gegen die Mapping-Tabelle)

## Worked Example: 446 Favoriten

Vorlage mit 446 Favoriten, alle mit leerem `<ClassificationItemSet/>`.

1. **Export** in einen Arbeitsordner → 446 XMLs + `AddPars/`, sofort als `.tar.gz` gesichert.
2. **Mapping** `Datei (NFC) → SAB-Item` als JSON gebaut: 314 mit Klasse, 132 bewusst leer,
   95 als `unsicher` markiert und vom Nutzer durchgesehen, bevor gepatcht wurde.
3. **Patch 1 — Klassifizierung:** textbasiert, erstes `ClassificationItemSet` je Datei.
4. **Patch 2 — Ebenen:** 50 fehlverlinkte Favoriten (Bemaßung auf „10 Dämmung",
   Etiketten auf „10 Wand innen tragend", Barrierefrei-Schraffuren auf „99 Import
   Fachplaner"). Layer-Tag generisch + `LAY2`-Eintrag in `AttrTable`.
5. **Import** mit `conflictPolicy: Overwrite`, `importFolders: true` — beim ersten
   Versuch nur 113 von 446, weil `AddPars/` fehlte; mit Ordner **446/446**.
6. **Frisch-Export als Gegenprobe:** 314/314 Klassen gesetzt, 132/132 korrekt leer,
   50/50 Ebenen korrekt, 0 falsch.

Zuordnungslogik für die Ebenen war dabei **nicht der Maßstab, sondern Rohbau/Ausbau** —
die Favoritennamen tragen „roh"/„fertig" schon in sich, und ein maßstabsbenannter Layer
(„Bemaßung 1:50") trägt bei 1:20 oder 1:200 ohnehin nicht mehr.
