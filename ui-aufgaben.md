# UI-Aufgaben — was keine API hat, und wie wir damit umgehen

Manches in Archicad ist per API nicht erreichbar. Das ist kein Grund, den Nutzer die Maus
führen zu lassen, solange es einen API-Weg gibt — und kein Grund, ein UI-Erfordernis zu
verschweigen, wenn es keinen gibt.

**Grundhaltung:** So viel wie möglich im Hintergrund per API; für den echten Rest eine
präzise Klickanleitung, und danach — wenn möglich — eine **API-Rücklese als Beleg**. Nie
den Nutzer bitten, „mal zu schauen, ob's geklappt hat", wenn wir es selbst messen können.

Stand 2026-07-30, AC29 / Tapir 1.5.3.

## Inhaltsverzeichnis

1. [Was wirklich UI-only ist](#was-wirklich-ui-only-ist)
2. [Was fälschlich für UI-only gehalten wurde](#was-fälschlich-für-ui-only-gehalten-wurde)
3. [Modale Dialoge blockieren die API](#modale-dialoge-blockieren-die-api)
4. [macOS-Automation — was geht und was nicht](#macos-automation--was-geht-und-was-nicht)
5. [Das Anleitungsformat](#das-anleitungsformat)
6. [Klickpfade für die häufigen Fälle](#klickpfade-für-die-häufigen-fälle)
7. [Ungesicherte Arbeit](#ungesicherte-arbeit)

## Was wirklich UI-only ist

| Aufgabe | Warum | Belegt |
|---|---|---|
| **IFC-Übersetzer** — Auswahl, Typ-Zuordnungsbaum, Datenkonvertierungs-Checkboxen | kein API-Endpoint | 2026-07-30 |
| **Property-Definitionen ÄNDERN** (Verfügbarkeit, Enum-Werte, Typ einer bestehenden Property) | es gibt nur `create` und `delete`, **kein** `ModifyPropertyDefinitions`. Delete+Create ist SAFE-02-pflichtig — GUID-Referenzen (z. B. GDL `Property_Value_Of_Parent`) brechen dabei | 2026-08-04 |
| **Attributmanager**-Löschungen mit Ersatzwahl (Baustoffe, Verbünde, Profile) | API kennt die Profil-Verwendung nicht, würde den Ersatz still wählen | 2026-07-30 |
| **Dach-Werkzeug-Default** setzen | `apply_favorites_to_element_defaults` scheitert bei Roof (`-2130313114`) | 2026-07-30 |
| **Bibliothekenmanager** — fehlende Elemente auflösen, Zuordnung prüfen | kein API-Endpoint für Migrationsrückstände | 2026-07-30 |
| **Sichern unter / .tpl schreiben** | kein API-Endpoint | 2026-07-30 |
| **Teamwork Freigeben / Senden & Empfangen** | kein API-Endpoint | 2026-07-30 |
| Arbeitsumgebung, Tastaturkürzel, Palettenlayout | kein API-Endpoint | — |

## Was fälschlich für UI-only gehalten wurde

Diese Liste ist genauso wichtig — sie verhindert, dass wir Arbeit unnötig an den Nutzer
zurückgeben:

| Vermeintlich UI | Tatsächlich API |
|---|---|
| Favoriten ändern | `favorites_export_favorites` / `favorites_import_favorites` |
| Werkzeug-Standardeinstellungen | `favorites_apply_favorites_to_element_defaults` (außer Dach) |
| Leere Ebenen löschen | `attributes_delete_attributes` |
| Ebenenkombinationen anlegen/ändern | `attributes_create_layer_combinations` |
| Baustoffe anlegen (inkl. Priorität, Schraffur, Wärmedaten) | `attributes_create_building_materials` |
| Bibliotheksdateien einbetten | `library_add_files_to_embedded_library` |
| IFC exportieren | `dev_ifc_file_operation` (`method: "save"`) |
| „Welchen IFC-Typ bekäme diese Wand?" | `dev_get_ifc_type_of_elements` |
| **Property-Definitionen ANLEGEN** — inkl. Enum-Werten und Availability je Klassifikations-Item | `properties_create_property_definitions` (+ `properties_create_property_groups`). **Live verifiziert 2026-08-04**: 17 Brandschutz-Properties in einem Aufruf, Availability bauteilgerecht, Rücklese 177/177. Der alte Eintrag „UI-only, fragil" war ab Tapir 1.5.4 falsch |
| Klassifikations-**Items** in ein bestehendes System hängen | `dev_create_classification_items` mit `parentClassificationItemId` (Antwort `executionResults: []` ist ein Fehlalarm — Rücklese zählt) |
| Klassifikationssystem samt Baum anlegen | `dev_create_classification_systems` (schwerer Eingriff, s. IFC-Datei) |

**Regel:** Bevor eine Aufgabe als „geht nur im UI" gemeldet wird, eine Discovery-Runde mit
zwei Synonym-Queries fahren. Der MCP-Umfang wächst; alte Annahmen veralten.

## Modale Dialoge blockieren die API

Solange in Archicad ein **modaler Dialog** offen ist, schlägt **jeder** API-Call fehl —
Code `4001`, mit dem Dialognamen im Fehlertext (live erlebt mit Bibliothekenmanager und
Arbeitsumgebung).

Folgen für die Zusammenarbeit:

- Wer den Nutzer um einen UI-Schritt bittet, kann in dieser Zeit **nichts** parallel
  messen. Also erst alle API-Arbeit erledigen, **dann** die UI-Schritte bündeln.
- Ein plötzliches `4001` heißt nicht „Verbindung weg", sondern „ein Dialog ist offen".
  Fehlertext lesen, Dialognamen nennen, kurz um Schließen bitten — nicht in eine
  Retry-Schleife laufen.

Auch relevant: **der Port wechselt** mit jeder Instanz und jedem Neustart (19723/24/25 …).
Nach jeder Archicad-Aktion des Nutzers, die einen Neustart bedeuten könnte, per
`discovery_list_active_archicads` neu holen.

Und: `projectPath` kann **irreführend** sein. Eine aus einer `.tpl` geöffnete, noch nicht
gesicherte Datei meldet weiter den Pfad der Vorlage. Ob wir im richtigen Zustand arbeiten,
entscheidet der **Inhalt** (Ebenenzahl, vorhandene Löschungen), nicht der Pfad.

## macOS-Automation — was geht und was nicht

<!-- 2026-07-30, macOS 15/Darwin 25, AC29 -->

| Weg | Status |
|---|---|
| `screencapture` (Bildschirmaufnahme-Freigabe) | ✅ funktioniert — Screenshots zur Diagnose sind jederzeit möglich |
| System Events: Fenster/Menüs **auflisten** | ✅ funktioniert |
| System Events: **klicken** (`click at`, `click menu item`) | ❌ scheitert reproduzierbar mit `-25211`, trotz erteilter Bedienungshilfen-Rechte |

Praktische Konsequenz: **Sehen ja, Steuern nein.** Wir können einen Dialog selbst
anschauen und die Anleitung präzise auf das machen, was wirklich auf dem Schirm ist —
klicken muss der Nutzer. Ein Screenshot zur Absicherung ist billig und ersetzt jede
Rückfrage „welcher Knopf ist gemeint?".

Wenn der Nutzer UI-Zugang anbietet: den Screenshot-Teil dankend nehmen, die Klick-Automation
nicht versprechen.

## Das Anleitungsformat

Was sich bewährt hat, wenn ein UI-Schritt unvermeidlich ist:

1. **Ein Schritt pro Absatz**, in Klickreihenfolge, mit den echten Menütexten in der
   Sprache der Installation.
2. **Zeitangabe** dazu („10 Sekunden", „2 Minuten") — der Nutzer entscheidet dann selbst,
   ob er es jetzt macht.
3. **Was danach anders sein muss**, in einem Satz. Das ist die Erfolgsbedingung.
4. **Wie wir es prüfen**: welcher API-Read den Erfolg belegt. Wo keiner existiert (IFC-
   Übersetzer), stattdessen den Messweg nennen (Vorher/Nachher-Probe oder Test-Export).
5. Mehrere Schritte als **numerierte Liste bündeln**, damit der Nutzer sie in einem
   Durchgang abarbeitet — jeder Wechsel zwischen UI und API kostet ihn Kontext.

Nicht tun: den Nutzer um eine Bestätigung bitten, die wir selbst messen können. Nicht
tun: mehrere Punkte mit Rückfragen dazwischen verschachteln.

## Klickpfade für die häufigen Fälle

**IFC-Übersetzer auf Element-Typ** (behebt den Proxy-Bug):
`Datei → Interoperabilität → IFC → IFC-Übersetzer einstellen…` → Export-Übersetzer wählen
→ `IFC-Typen zuordnen zum Export` → oben **„Elemente zuweisen: Element-Typ"** → `OK`.
Prüfung: `dev_get_ifc_type_of_elements` an denselben Wänden wie vorher.

**Werkzeug-Default aus einem Favoriten** (Dach & Notfälle): Werkzeug in der Toolbox
aktivieren → in der Favoriten-Palette auf den Favoriten **doppelklicken**.
Prüfung: ein Testelement zeichnen und `dev_get_classifications_of_elements` lesen.

**Property anlegen:** `Optionen → Eigenschaften-Manager` → `Neue Eigenschaft` → Gruppe
wählen/anlegen, Datentyp setzen, unten **Verfügbarkeit** für die Klassifizierungen ankreuzen.
Prüfung: `API.GetAllPropertyIds` (UserDefined) + `API.GetDetailsOfProperties` (Antwort
liegt unter `propertyDefinitions`), plus `properties_get_property_definition_availability`.

**Attributmanager:** `Optionen → Elementattribute → Attributmanager` → Typ links wählen →
Eintrag markieren → `Löschen` → im Ersatz-Dialog die zu behaltende Variante wählen.
Prüfung: Attribut-Liste per API neu lesen und Namen gegenrechnen.

**Bibliothekenmanager:** `Datei → Bibliotheken und Objekte → Bibliothekenmanager`.
Achtung: modal ⇒ währenddessen keine API-Calls. Die Rücklese eingebetteter Teile geht
derzeit **nur** hier, weil `library_get_available_library_parts` defekt ist (liefert immer
leere Listen mit hohem `skippedCount`, Codes `-2130313112` / `-2130312214`) — ein leeres
Ergebnis dieses Tools ist **kein** Beleg dafür, dass nichts eingebettet wurde.

## Ungesicherte Arbeit

API-Änderungen leben im geöffneten Projekt, nicht auf der Platte. Wer eine Stunde lang
Favoriten, Attribute und Bibliotheken per API umbaut, hat eine Stunde ungesicherter Arbeit
im RAM — und `Sichern` gibt es nicht per API.

Deshalb:

- Bei längeren Umbauten **von selbst** auf den Sicherungsstand hinweisen, und zwar
  **vor** dem nächsten großen Schritt, nicht am Ende.
- Bei Vorlagen: **niemals die Original-`.tpl` überschreiben** — daran hängen laufende
  Projekte. Zwischenstände mit Datum und Uhrzeit im Namen.
- Nach dem Sichern kurz gegenprüfen, dass Name und Ort stimmen (Archicads Dialog kürzt
  lange Namen und schlägt gern den letzten Ordner vor, nicht den gewünschten).
