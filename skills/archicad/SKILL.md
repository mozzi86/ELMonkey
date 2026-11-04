---
name: archicad
description: |
  Use for any task in a running Archicad project via the MCP server (mcp__archicad__*).
  Trigger when the user says things like: "klassifiziere alle Wände / Fenster / Räume",
  "Bodenbelag synchronisieren", "IFC-Diagnose / Proxy-Bug", "Zonen-Property setzen",
  "DWG-Decken importieren", "Bulk-Klassifikation nach SAB", "wieviele Zonen / Räume /
  Elemente sind im Modell", "welche Instanz läuft gerade", "welches Projekt ist offen",
  "erstelle Wand / Fenster / Objekt", "verschiebe Elemente", "Property-Wert lesen /
  schreiben", "Klassifikations-System / KG-300 / Layer / Story / Element-Filter" —
  and any call to mcp__archicad__* tools.
---

# Archicad-Skill

Dieser Skill befähigt Claude, in einem laufenden Archicad-Projekt zu arbeiten — Wände zu erstellen, Bestand abzufragen, Materialien zuzuweisen, Bulk-Klassifizierungen durchzuführen — über den Archicad-MCP-Server. Er ist als Hub-and-Spoke organisiert: diese Datei ist immer geladen und dient als Einstieg; tiefere Details liegen in `reference/`-Dateien, die bei Bedarf nachgeladen werden; konkrete Element-Rezepte (Wand, Fenster, …) liegen in `recipes/`.

Drei Dinge bleiben durchgängig wichtig: wir holen vor jedem Auftrag Kontext (Warm-up), wir entdecken die nötigen MCP-Tools statt sie zu erraten (Discovery), und wir verändern Bestand nur mit ausdrücklicher Bestätigung (asymmetrische Sicherheit).

## Vor jedem Auftrag — Warm-up

Bevor wir mit einem Archicad-Auftrag loslegen, holen wir uns Kontext: welche Archicad-Instanz läuft, welches Projekt ist offen, welche Story ist aktiv, welche Einheit gilt, welche Layer sind sichtbar. Ohne diesen Kontext bauen wir Wände im falschen Geschoss oder mit falscher Einheit.

Wir ziehen diesen Kontext **pro Auftrag einmal** und cachen ihn dann. Bei einem neuen Auftrag starten wir frisch. Das Story-Feld behandeln wir als **volatil** — wenn der User in seiner Aufgabe einen anderen Floor erwähnt („im OG", „1. OG"), ziehen wir es neu, denn der User könnte währenddessen in Archicad das Geschoss gewechselt haben.

Welche genauen Felder wir holen und mit welchen MCP-Aufrufen — das steht im Detail in [`reference/workflow-context.md`](reference/workflow-context.md).

## Wie wir mit dem MCP-Server arbeiten — Discovery-Pattern

Der Archicad-MCP-Server ist Discovery-zentriert: er listet seine Tools nicht statisch auf, sondern erwartet, dass wir per Semantik-Suche das passende finden. Das ist gut so — Archicad-Versionen ändern sich, statische Tool-Namen würden veralten. Wir nutzen voll-qualifizierte Tool-Namen (`mcp__archicad__*`), damit klar ist, welcher MCP-Server gemeint ist, wenn mehrere im Projekt aktiv sind.

Der typische Ablauf für eine Operation:

1. **Aktion auf Englisch formulieren** — z. B. „create wall element on story" oder „delete elements by id". Der MCP-Index ist englisch.
2. **`mcp__archicad__archicad_discover_tools`** mit dieser Aktion als Query aufrufen. Wir bekommen eine Kandidaten-Liste zurück.
3. **Kandidaten lesen** — Name, Beschreibung, Parameter-Schema. Wir wählen den semantisch passendsten Treffer.
4. **`mcp__archicad__archicad_call_tool`** mit dem gewählten Tool-Namen und den Argumenten aufrufen. Der `port` aus dem Warm-up gehört immer dazu.
5. **Ergebnis prüfen** — Element-ID zurückbekommen, Erfolg verifizieren.

Wenn ein Discovery-Treffer nicht stimmt (Schemafehler, `tool not found`), versuchen wir die Discovery mit einer Synonym-Query („create wall" → „add wall" / „insert wall"). Nach zwei Fehlversuchen melden wir uns beim User und raten nicht weiter.

Mehr zu Discovery-Strategien, Fehlerklassen, Paginierung und Port-Handling siehe [`reference/mcp-conventions.md`](reference/mcp-conventions.md).

## Sicherheit — wir verändern dein Modell mit Bedacht

Das Sicherheitsmodell ist **asymmetrisch**: Operationen, die rückgängig zu machen sind, dürfen wir frei tun; Operationen, die Bestand verändern, brauchen Bestätigung. Das spiegelt einen natürlichen Architekten-Reflex — neu zeichnen ist schnell, Bestand anfassen mit Bedacht.

**SAFE-01 — Asymmetrisches Modell.** Create und Read sind frei. Wenn Claude eine Wand erstellt und sie sich als falsch herausstellt, ist Archicads Undo zur Stelle. Update und Delete dagegen erfordern eine Confirm-Schleife mit den betroffenen Element-IDs in einer Übersicht — weil ein falsches ID-Mapping echten Bestand zerstören kann, und Undo-Tiefe in komplexen Sessions endlich ist. Es gibt **keine Obergrenze** für die Batch-Größe einer Operation. Wenn 500 Wände dran sind, zeigen wir eine Summary mit Pro-Klasse-Counts und führen nach einem `ja` aus.

**SAFE-02 — Kein Update als Create+Delete.** Wir implementieren Updates nicht als Sequenz „lösche alt, erstelle neu". Wenn der MCP-Server keinen direkten Update-Endpoint für eine Operation anbietet, melden wir das dem User und lassen ihn entscheiden — wir umgehen die Confirm-Schleife nicht still durch einen harmlos-wirkenden Create gefolgt von einem Delete.

**SAFE-03 — Layer-not-visible-Ausnahme.** Create ist normalerweise frei, aber wenn der Ziel-Layer in Archicad nicht sichtbar ist, halten wir an und konfirmieren. Sonst landet das neue Element unsichtbar im Modell, der User merkt nichts, ein erneuter Versuch erzeugt Duplikate. Sichtbarkeit prüfen wir aus dem Warm-up (Feld „sichtbare Layer").

**SAFE-04 — Hosted-Element-Pre-Check bei Delete.** Bevor wir eine Wand löschen, fragen wir nach, welche Elemente auf ihr hosten — Fenster, Türen, Wandöffnungen — und zeigen sie im Confirm-Dialog. Sonst entstehen waisenförmige Öffnungen, die in IFC-Exporten stillschweigend Datenkorruption bedeuten.

**SAFE-05 — Element-ID-Threading.** Wenn wir aus einem Create eine Element-ID zurückbekommen (etwa eine Wand-ID), behalten wir sie im Arbeitsgedächtnis für die Dauer des Auftrags. Wenn der nächste Schritt ein Fenster in dieser Wand setzen will, verwenden wir die ID direkt als Host-Parameter — wir suchen sie nicht erneut. Ohne diese Regel brechen Multi-Element-Workflows (Wand → Fenster) stillschweigend ab, weil das Fenster keine Host-ID findet.

Das genaue Format des Confirm-Dialogs (Element-Auflistung 1–10 einzeln, > 10 als Summary mit `details`-Option, Antwortoptionen `ja` / `nein` / `details` / `abbrechen`) und das Verhalten bei „nein" oder Mid-Batch-Fehler siehe [`reference/mcp-conventions.md`](reference/mcp-conventions.md).

## Nach jedem Create — KI-Stempel <!-- 2026-07-17 -->

Standard (User-Vorgabe vom 2026-07-17): **Jedes Element, das Claude erstellt, bekommt unmittelbar nach dem Create die User-Property „Allgemeine Werte / KI generiert"** (Zeichenfolge) mit dem Wert `KI generiert <YYYY-MM-DD>` (Tagesdatum). Das gilt für alle Elementtypen und gehört zum Create-Workflow dazu — kein separates Confirm nötig, es ist Teil des Erstellens.

- Property-GUID **pro Projekt** frisch ermitteln (GUIDs sind projektspezifisch): `API.GetAllPropertyIds` (UserDefined) + `API.GetDetailsOfProperties`, Name `KI generiert`, Gruppe `Allgemeine Werte`.
- Setzen bevorzugt per HTTP-Bypass `API.SetPropertyValuesOfElements` (flaches Schema, `propertyValue {type:"string", status:"normal", value:"KI generiert <Datum>"}`), danach **Rücklese-Verifikation**.
- Fehlt die Property im Projekt: User kurz informieren und bitten, sie im Eigenschaften-Manager anzulegen (Zeichenfolge, Gruppe „Allgemeine Werte", verfügbar für alle Klassifizierungen). Nicht still weglassen, nicht selbst per API anlegen (Property-Create in AC29 fragil: defaultValue-Pflicht, eigene Gruppe nötig, Availability-Kopplung).

## API zuerst, UI nur wenn es sein muss <!-- 2026-07-30 -->

Der Nutzer will Arbeit im Hintergrund erledigt sehen, nicht die Maus übernehmen müssen.
Also: **bevor wir eine Aufgabe als „geht nur im UI" melden, fahren wir eine Discovery-Runde
mit zwei Synonym-Queries.** Der MCP-Umfang wächst; mehrere Dinge, die früher als UI-only
galten, sind längst per API machbar — Favoriten, Werkzeug-Defaults, Ebenen löschen,
Ebenenkombinationen, Baustoffe anlegen, IFC exportieren.

Bleibt ein echter UI-Schritt übrig, dann geben wir ihn als kurze Klickanleitung mit
Zeitangabe und Erfolgsbedingung — und prüfen anschließend **selbst per API**, statt den
Nutzer nachschauen zu lassen. UI-Schritte werden gebündelt, weil ein offener modaler Dialog
jeden API-Call blockiert (Code `4001`). Was UI-only ist, was es nur zu sein scheint, und die
Klickpfade: [`reference/ui-aufgaben.md`](reference/ui-aufgaben.md).

## Ein `success` ist kein Beleg <!-- 2026-07-30 -->

Mehrere Archicad-Endpoints melden Erfolg, ohne einen erbracht zu haben: Favoriten-Import
ignoriert Dateien still, `apply_favorites_to_element_defaults` quittiert unpassende
Favoriten, Library-Embed bestätigt Batches, deren Rücklese defekt ist. **Belegt ist eine
Änderung erst durch einen frischen Read** — bei Favoriten durch einen Frisch-Export, bei
Attributen durch erneutes Listen, bei Klassifizierung durch `dev_get_classifications_of_elements`.

Umgekehrt gilt dasselbe für Nullbefunde: **„0 Treffer" ist erst ein Befund, wenn die
Methode an einem bekannten Positivfall gegengeprüft wurde.** Ein Nullbefund ohne Gegenprobe
hat in dieser Arbeit schon zur falschen Empfehlung geführt, Attribute zu löschen, die in
Gebrauch waren — die Suche hatte den Attributtyp nicht unterschieden.

## Wo welches Wissen liegt

### Reference (Hintergrund + Konventionen)

- [`reference/mcp-conventions.md`](reference/mcp-conventions.md) — Discovery-Strategien im Detail, Fehlerklassen, Paginierung, Confirm-Format-Beispiele, Port-Handling.
- [`reference/workflow-context.md`](reference/workflow-context.md) — Die 7 Warm-up-Felder im Detail: Port, Projekt-Info, Längeneinheit, aktive Story, sichtbare Layer, Pen-Set, Klassifikations-System.
- [`reference/bulk-operations.md`](reference/bulk-operations.md) — Das universelle Read → Filter → Group → Confirm → Apply-Muster für Massen-Updates, inkl. Klassifizierungs-Spezifika, Property-Enum-Normalisierung und Identifier-Mapping.
- [`reference/schedule-pipeline.md`](reference/schedule-pipeline.md) — Export-Parse-Match-Update-Pipeline für Daten aus Archicad-Schedules (XLSX/CSV) bei Bulk-Updates, wenn MCP die Quell-Daten nicht direkt liefern kann.
- [`reference/property-expression-linking.md`](reference/property-expression-linking.md) — Native Archicad-Synchronisation zwischen GDL-Parameter und Property via Expression-Editor; oft eleganter als Bulk-Copy.
- [`reference/dwg-ifc-import.md`](reference/dwg-ifc-import.md) — Vorverarbeitungs-Pipeline für 2D-Lagepläne (DWG) zu lückenfreien IFC4-Decken-Modellen mit Z-Staffel, vor Archicad-Hotlink-Import. (Live-verifiziertes Lageplan-/KG-500-Spezialfall.)
- [`reference/dwg-ifc-kg300.md`](reference/dwg-ifc-kg300.md) — Verallgemeinerung der Pipeline auf KG 300 Baukonstruktionen (310 Baugrube bis 395 Sonstige): pro Cost-Group IFC-Entity + Polygonization-Strategie + DWG-Layer-Hints + Z-Schema, plus umgekehrte Richtung (Bulk-KG-Zuweisung auf bestehende Elemente) und Mapping zu SAB_Klassifizierung_29.
- [`reference/self-improvement.md`](reference/self-improvement.md) — Wie der Skill aus Sessions lernt: Reflection-Trigger am Auftrags-Ende, Datums-Marker für neue Einträge, Verification-Loop bei Wiederholfehlern.
- [`reference/mcp-extension.md`](reference/mcp-extension.md) — MCP-Stack-Architektur (tapir-archicad-mcp → Tapir-Add-On), Update-Prozedur, verifizierte Grenzen (kein Pen-Befehl, Layout-Buch-Lesen ja), Erweiterungs-Entscheidungsbaum und das hauseigene **ELM_SAB_Add-On** ([`ELM_SAB_Add-On/`](https://github.com/mozzi86/-archicad/tree/main/ELM_SAB_Add-On)) mit `SetPenOfElements`.
- [`reference/favoriten-und-defaults.md`](reference/favoriten-und-defaults.md) — Favoriten und Werkzeug-Standardeinstellungen per API umbauen: Export→Patch→Import-Round-Trip, die **AddPars-Falle**, textbasiertes XML-Patchen, Klassifizierungs-Block, Ebene an zwei Stellen, AttrTable-FourCC-Tabelle (live-verifiziert 2026-07-30 an 446 Favoriten).
- [`reference/attribute-und-ebenen.md`](reference/attribute-und-ebenen.md) — Ebenen, Ebenenkombinationen (inkl. `intersectionGroupNr`), Baustoffe/Verbünde/Profile: Zwei-Schritt-Lesemuster, was per API löschbar ist und was in den Attributmanager gehört, Dubletten-Rezept, warum Umbenennen riskanter ist als Löschen.
- [`reference/ifc-mapping-und-diagnose.md`](reference/ifc-mapping-und-diagnose.md) — Die **zwei getrennten IFC-Kanäle** (Typ-Zuordnungsbaum vs. Klassifizierungs-Checkbox), Übersetzer-Bäume, `dev_get_ifc_type_of_elements` als Proxy-Probe ohne Export, richtig zählen (RelatedObjects, nicht Relationen), Ziel-Mappings, Klassifikationsbäume per API.
- [`reference/ui-aufgaben.md`](reference/ui-aufgaben.md) — Was wirklich UI-only ist und was fälschlich dafür gehalten wird, modale Dialoge (`4001`), macOS-Automation (Screenshot ja, Klicks nein), das Anleitungsformat, Klickpfade, ungesicherte Arbeit.
- [`reference/referenzmodell-abgleich.md`](reference/referenzmodell-abgleich.md) — Zwei Modelle im selben Projekt abgleichen: Versatz per Passpunkt, Knautschzonen-Strategien (Hybrid statt Element-Matching), Property-/Klassifizierungs-Fallen (notAvailable, Enum-Formate), Teamwork-Diagnostik (live-verifiziert 2026-07-14).

### Recipes (pro Elementtyp)

Konkrete Rezepte werden in den Folgephasen mit live-verifizierten Inhalten gefüllt. In Phase 1 sind sie als geplante Speicherorte gelistet:

- [`recipes/initial-setup.md`](recipes/initial-setup.md) — **Einmalige Erstinstallation**: ELMonkey per Installer (Tapir eingebaut, KEIN separates Tapir) + MCP-Server `elmonkey-mcp` via uv + Claude-Registrierung + Smoke-Test. Hierhin, wenn `mcp__archicad__*`-Tools fehlen oder ein Kollege den Skill neu einrichtet. <!-- 2026-07-10 -->
- [`recipes/konturen-zu-waende.md`](recipes/konturen-zu-waende.md) — 2D-Konturen → Polygon-Wände: Pipeline, SAB-Regeln (Quell-Ebene=Ziel-Ebene), Gotchas, THN-Worked-Example (live-verifiziert 2026-07-13).
- [`recipes/decken-und-durchbrueche.md`](recipes/decken-und-durchbrueche.md) — Decken + Durchbruch-Löcher aus Konturen (Footprint-Union, CreateSlabs häppchenweise, Absturz-Lehren; live-verifiziert 2026-07-14).
- [`recipes/oeffnungen-aus-konturen.md`](recipes/oeffnungen-aus-konturen.md) — Durchbrüche (BD/FBA/WD) als Öffnungs-Elemente mit SAB-Favoriten, normalisiert Rechteck/Kernbohrung, Maße+Höhen aus DWG-Texten (GetTextsOfElements v0.5); inkl. ModifySlabs-Crash-Bug (live-verifiziert 2026-07-14).
- [`recipes/tueren-aus-boegen.md`](recipes/tueren-aus-boegen.md) — Türen aus DWG-Aufschlag-Bögen via Türwand-Pattern (Polywand-Türen sind per API unsteuerbar!); Flags aus Bogen-Geometrie, Nummern via Element-ID (live-verifiziert 2026-07-14).
- [`recipes/treppen-aus-stufenlinien.md`](recipes/treppen-aus-stufenlinien.md) — Treppenläufe aus Stufenlinien-Clustern (CreateStairs, realistische Steigung, Deckenausschnitt; live-verifiziert 2026-07-14).
- [`recipes/aussenwaende-aus-schraffur.md`](recipes/aussenwaende-aus-schraffur.md) — Schraffur-Trick: Außenwände aus Wand-Schraffur-Zellen, wenn Konturen nicht schließen (dünne Zellen vereinigen; live-verifiziert 2026-07-14).
- [`recipes/pfaehle-aus-kreisen.md`](recipes/pfaehle-aus-kreisen.md) — Bohrpfähle/Bohrpfahlwände aus 2D-Kreisen + Kreis-Polylinien (Kreis-Fit, Selektion-als-Muster, tiefe Stützen über mehrere Geschosse; live-verifiziert 2026-07-14).
- [`recipes/gelaende-aus-punktwolke.md`](recipes/gelaende-aus-punktwolke.md) — Gelände (Mesh) als Boden-„Abdruck" einer platzierten Punktwolke: E57 → 1-m-Bodenraster → Sublines-je-Zeile → Tapir CreateMeshes; Lage-Regel BBox-Min = Objektursprung, skirtLevel absolut (live-verifiziert 2026-09-07).
- [`recipes/vorlagen-pflege.md`](recipes/vorlagen-pflege.md) — Bürovorlage (.tpl) umbauen: die **Reihenfolge** (Bibliotheken → Attribute → Favoriten → Properties → IFC-Übersetzer), Sicherungsstrategie, Abschlussbeleg, Fallen (live durchlaufen 2026-07-30).
- [`recipes/innenansichten-bemassung.md`](recipes/innenansichten-bemassung.md) — Innenansichten/Fliesenspiegel bemaßen: Muster auslesen, Ketten ergänzen, Raster 0,25/0,15, Hilfslinien-Regel, OK FFB/UK Decke, Ansichtshöhe per API (ELMonkey ≥ 0.9.29; live Futurelab 2026-10-14). <!-- 2026-10-14 -->
- [`recipes/gebaeude-und-aussenanlage.md`](recipes/gebaeude-und-aussenanlage.md) — **Gebäude + Außenanlage komplett per API, fachlich und in 3D**: Höhenkette, Favoriten, Pfettendach (Pfetten/Sparren/Giebel-SEO), Straßenraum, Grundstück, „Tragende Funktion“, Fallen (Heimatgeschoss aus absoluter Höhe, CaptureView-Farben, Bibliotheksnamen). <!-- 2026-10-20 -->
- [`recipes/wall-operations.md`](recipes/wall-operations.md) — Wand-Operationen (Lesen, Erstellen, Modifizieren, Löschen, Klassifizieren — Create seit Tapir 1.5.3 via `elements_create_walls`).
- [`recipes/openings.md`](recipes/openings.md) — Fenster, Türen, Wandöffnungen.
- [`recipes/slabs-columns-beams.md`](recipes/slabs-columns-beams.md) — Decken, Stützen, Träger.
- [`recipes/zones.md`](recipes/zones.md) — Zonen / Räume.
- [`recipes/curtain-walls.md`](recipes/curtain-walls.md) — Fassaden, Pfosten-Riegel.
- [`recipes/library-objects.md`](recipes/library-objects.md) — Bibliothekselemente (Möbel, Sanitär, GDL allgemein).
- [`recipes/surfaces-materials.md`](recipes/surfaces-materials.md) — Surfaces, Building Materials, Composites.
- [`recipes/fills-hatches.md`](recipes/fills-hatches.md) — 2D-Schraffuren.
- [`recipes/lines-polylines.md`](recipes/lines-polylines.md) — 2D-Linien, Polylinien, Bögen, Splines.

Diese Dateien existieren als Skelette (Phase 1) und werden mit live-verifizierten Inhalten in den Folgephasen gefüllt.

## Am Ende jedes Auftrags — Lern-Check

Wenn ein Archicad-Auftrag abgeschlossen ist, stellen wir dem User eine einzige Frage:

> Lern-Check: War da was Neues — ein Tool-Name, eine Geometrie-Stolperfalle, ein Klassifikations-Trick — das in den Skill sollte?

Wenn ja, tragen wir es in die passende Datei ein, mit Datums-Marker. Mehr dazu in [`reference/self-improvement.md`](reference/self-improvement.md).

Es ist völlig in Ordnung, wenn die Antwort meistens „nein" ist. Wir wollen keine Skill-Bloat, sondern echte Lern-Gewinne festhalten.
