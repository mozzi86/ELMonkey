# Abgleich Archicad ↔ Schwarz Plattform (bidirektional) <!-- 2026-11-10, live BFW07 19.11.2026 -->

Auslöser: „Projekt synchronisieren“, „Plattform und Archicad abgleichen“.

Gleicht das **offene Archicad-Projekt** (BIMcloud/Teamwork) mit dem **wirksamen Stand der Plattform** ab und führt
beide zusammen:

| Richtung | Was | Wie |
|---|---|---|
| Plattform → Archicad | Bauteile, die nur die Plattform kennt | nativ **neu bauen** (neue Archicad-GUID), Stempel „Plattform-GUID“ |
| Plattform → Archicad | geänderte Bauteilinformationen | Eigenschaften setzen, zurücklesen |
| Archicad → Plattform | neue und in Archicad geänderte Bauteile | Netze (`netze` → `archicad_netze.json.gz`) im Abgleich-Vorschlag, keine Gesamt-IFC <!-- 2026-11-20 --> |
| Archicad → Plattform | geänderte Bauteilinformationen | EIN Vorschlag am Modell (Freigabe in der App) |
| Archicad → Plattform | Oberflächen mit Texturbildern | `ExportSurfaces` → Texturbibliothek |
| beide | Unklares | **Frage an den Zeichner**, gebündelt — ohne Antwort wird nichts geschrieben |

Braucht: ELMonkey-Add-on (HTTP-Port, meist 19723), den Konnektor „Schwarz Platform“ und das Skript
[`scripts/plattform_abgleich/plattform_abgleich.py`](../scripts/plattform_abgleich/plattform_abgleich.py)
(nur Standardbibliothek). Der Rechenkern ist `elmonkey.abgleich` (`core/`), getestet ohne Archicad.

## Regeln

- **Projekt bestätigen lassen**, bevor irgendetwas gelesen oder geschrieben wird (Projektname aus `archicad-lesen`
  gegen die Projektnummer der Plattform). Die Plattform kennt den Teamwork-Namen nicht.
- **Nie raten.** Jede Frage aus `fragen.md` wird dem Zeichner gestellt. „Weiß ich nicht“ = Frage bleibt offen und
  kommt beim nächsten Abgleich wieder.
- **Nie löschen.** In Archicad löscht nur der Zeichner von Hand (Liste `loeschen_von_hand`); in der Plattform wird
  höchstens ausgeblendet, und das als Vorschlag.
- Alles, was Archicad ändert, erst als Trockenlauf zeigen, dann mit `--ausfuehren`. Teamwork: das Skript reserviert;
  danach **senden**.
- Die Links aus `archicad_abgleich_lesen` sind Schlüssel auf Projektdaten: nur ans Skript geben, nicht in Tickets,
  Mails oder andere Chats kopieren.

## Ablauf

Arbeitsordner je Lauf, z. B. `~/Abgleich/P001-2026-11-10` (unten `$O`).

1. **Archicad lesen** — `plattform_abgleich.py archicad-lesen --ordner $O`
   Zeigt Projektname, Teamwork, Anzahl Bauteile. → Zeichner bestätigt das Projekt.
2. **Plattform lesen** — Konnektor `archicad_abgleich_lesen {projekt}` → `lauf`, `stand_link`, `ergebnis_ablage`
   (und `netze_ablage`, sobald die Plattform sie liefert, siehe Schritt 8).
3. **Planen** — `plattform_abgleich.py planen --ordner $O --stand-link "<stand_link>"`
   Schreibt `plan.json` und `fragen.md`. Zahlen dem Zeichner zeigen. Auffällig viele „nur in Archicad“ oder „nur in
   der Plattform“ heißt meist: falsches Projekt, ausgeblendete Ebenen oder ein Teilmodell — erst klären.
4. **Fragen stellen** — jede Frage aus `fragen.md` (eine Frage gilt für alle genannten Bauteile; Ausnahmen je GUID
   möglich). Antworten nach `$O/antworten.json`:
   ```json
   {"F1": "archicad", "F2": {"wahl": "plattform", "ausnahmen": {"<guid>": "archicad"}}, "F3": "lassen"}
   ```
   Mögliche Antworten je Frageart stehen in der Frage (`eigenschaft`: archicad | plattform | ueberspringen ·
   `fehlt_in_archicad`: lassen | plattform_ausblenden | in_archicad_bauen · `fehlt_in_plattform`: lassen |
   in_plattform_importieren | in_archicad_loeschen · `in_plattform_ausgeblendet`: lassen | in_archicad_loeschen |
   plattform_einblenden).
5. **Entscheiden** — `plattform_abgleich.py entscheiden --ordner $O` → `ergebnis.json`.
6. **Archicad: Eigenschaften** — `plattform_abgleich.py archicad-schreiben --ordner $O` (Trockenlauf), nach dem Ja
   `--ausfuehren`. Das Skript liest zurück; „nicht möglich“ (Eigenschaft fehlt in Archicad oder ist ein Ausdruck)
   dem Zeichner nennen.
7. **Archicad: neue Bauteile bauen** <!-- 2026-11-20 --> — mit echter Geometrie aus der neuen Bauliste (Plattform
   PR 404, Staging seit 20.11.2026, noch nicht live gegen Archicad gelaufen): Konnektor
   `archicad_abgleich_bauliste {projekt, guids}` in Paketen zu höchstens 200 GUIDs (`netz=link` ist Vorgabe); die
   Antwort enthält nur Kennzahlen + `bauliste_link` (https, 2 h gültig). Alle Links innerhalb der 2 h übergeben:
   `plattform_abgleich.py bauen --ordner $O --bauliste <link1> <link2> … --register $O/register.json` (lokale
   Dateien gehen auch; das Register liefert den Umbau-Status). Dubletten werden per Plattform-GUID zusammengeführt
   (spätere Datei gewinnt). **Nicht bearbeitete GUIDs** (`nicht_bearbeitet`) → Abbruch vor jedem Archicad-Aufruf:
   Bauliste für diese GUIDs neu abrufen; `--trotzdem` nur nach Rücksprache. IFC (`--ifc`, Konnektor
   `modell_exportieren {modellarten: ["plattform"]}`) ist nur für Verbund-Unterteile mit eigener GUID nötig
   (Entscheidung Mudi 19.11.2026: Kopf + Unterteile); ohne IFC wird ein Verbund EIN Morph über alle Körper.
   Zuerst Trockenlauf, weitere Optionen `[--zuordnung …] [--nur GUID,…] [--ohne GUID,…] [--vorhanden v.json]
   [--verbund einzeln|zusammen]`. Zahlen (zu bauen, ausgelassen, `quelle`, Ebenen, Klassen) und `netz_pruefen`
   (Bauteile mit nicht mannigfaltigen oder offenen Kanten, Summen in `netz_kennzahlen`; auch `netz_pruefen.json`)
   dem Zeichner zeigen, nach dem Ja dasselbe mit `--ausfuehren`. Je Bauteil ein Morph, Ebene, Geschoss aus der
   Unterkante, Klasse (`sab_zuordnung.json`), Umbau-Status, Stempel „Plattform-GUID“ + „KI generiert <Datum>“ und
   Rücklese (`ruecklese.json`, Paare in `gebaut.json`). `bauen` stempelt selbst; `stempeln --gebaut …` nur noch für von
   Hand gebaute Bauteile (es reserviert nicht, unklassifizierte meldet es als „nicht klassifiziert (6703)“).
   `ausgelassen.json` dem Zeichner nennen: `status fehler` (nie gebaut), reine Hülle ohne IFC („nur Hülle“),
   Klasse/Status fehlt. Fehlt die Eigenschaft „Plattform-GUID“ im Projekt: mit dem Zeichner anlegen (Gruppe
   „Allgemeine Werte“, Text).
8. **Plattform: neue und geänderte Archicad-Bauteile als Netze** <!-- 2026-11-20 --> — **keine Gesamt-IFCs mehr
   (Weisung Mudi 20.11.2026)**; `plattform_abgleich.py ifc` + `modell_hochladen` nicht mehr für den Abgleich nehmen.
   `plattform_abgleich.py netze --ordner $O [--paket 50]` (bzw. MCP `elmonkey_diff_plattform {…, netze: true}`):
   nimmt nur `plattform.neu_importieren` (Art `neu`) und die Einträge aus `diff.geometrie` mit
   `richtung: archicad→plattform` (Art `geaendert`, Archicad-Box seit der Basis bewegt, Plattform nicht), liest deren
   Körper per `TapirCommand.Get3DGeometryOfElements` in Paketen, trianguliert und schreibt
   `$O/archicad_netze.json.gz` (Format `sab-archicad-netze/1`, Spezifikation im Kern `elmonkey.netze`). Ist in Archicad
   ein anderes Projekt offen als beim Lesen, liest es nichts. Fehlt die IFC-Klasse (archicad.json aus `archicad-lesen`
   hat keine), liest es sie für die gewählten GUIDs über `ELM_SAB.GetDiffDataELM {elements}` nach (Rückfall
   `GetIFCTypeOfElements`), sonst würde in der Plattform alles Proxy. <!-- 2026-11-20 --> Stützen/Träger: die Körper
   hängen an den Segmenten (`GetSubelementsOfHierarchicalElements`), `netze` holt sie dort. **Ausgeblendete Ebenen
   der gewählten Bauteile blendet `netze` kurz ein und danach genau diese wieder aus** (Entscheidung Mudi 20.11.2026,
   `ELM_SAB.SetLayerAttributes` mit Rücklese; Ergebnis `ebenen_eingeblendet`, `ebenen_zurueckgesetzt` ok|fehler —
   bei „fehler“ dem Zeichner die Ebenen sofort nennen, er blendet sie von Hand aus). Teamwork: nur, wenn niemand
   anderes die Ebenen-Einstellungen reserviert hat (`GetLockableObjectSetStatus LayerSettingsDialog`), sonst Hinweis.
   Alte Variante: `--ebenen-nicht-einblenden` (MCP: `netze_ebenen_einblenden: false`). `fehler`, `ohne_koerper`
   (je Typ) und `ohne_koerper_umbau` (je Umbau-Status) dem Zeichner nennen: Abbruch ohne Körper → Umbau-Filter auf
   „alles anzeigen“ (Dokument → Umbau → Umbau-Filter), dann `netze` erneut; übrige meist inaktive Design-Option
   (`designOption` im Fehler, wenn Archicad 29 sie nennt) — bewusst nicht übertragen. Ohne `GetDiffDataELM` (älteres Add-on, 4010)
   kommen Ebene aus `TapirCommand.GetDetailsOfElements.layerIndex` und Umbau-Status aus der eingebauten Eigenschaft
   `Category_RenovationStatus` (`API.GetPropertyIds` + `API.GetPropertyValuesOfElements`, `nonLocalizedValue`). Braucht ein `ergebnis.json` aus dem Diff-Checker (unten) — der Skript-Ablauf ohne Diff kennt
   keine Geometrie und liefert nur `neu`.
   **Braucht die Plattform-Seite:** `netze_ablage` aus `archicad_abgleich_lesen` und ein `archicad_abgleich_melden`,
   das die Datei liest — PR in SchwarzPlatform steht aus. Ohne `netze_ablage` die Datei liegen lassen und es dem
   Zeichner sagen; nicht auf IFC ausweichen.
9. **Plattform: Ergebnis melden** — `plattform_abgleich.py hochladen --ordner $O --ablage "<ergebnis_ablage>"
   [--netze-ablage "<netze_ablage>"]` (lädt `ergebnis.json` und, falls da, `archicad_netze.json.gz`), dann Konnektor
   `archicad_abgleich_melden {projekt, lauf, gebaut}` (Vorschau) → nach dem Ja `bestaetigt=true`. Es entsteht EIN
   Vorschlag (neue Bauteile + Netz-Ersatz für geänderte); freigegeben wird in der App.
10. **Oberflächen** — `plattform_abgleich.py oberflaechen --ordner $O` (Export + ZIP), dann Konnektor
    `archicad_oberflaechen_hochladen {projekt}` → ZIP mit dem genannten curl-Befehl hochladen; die Antwort ist der
    Prüfplan. Nach dem Ja `archicad_oberflaechen_einpflegen {ablage_auftrag, bestaetigt: true}` (Bibliothek +
    Belegung im Projekt).
11. **Teamwork senden**, dem Zeichner die Zusammenfassung zeigen: gebaut / geschrieben / offen / von Hand zu löschen.

## Wie gepaart und verglichen wird

- Paar = (1) Paar aus dem letzten Abgleich, (2) Stempel „Plattform-GUID“ am Archicad-Bauteil, (3) gleiche GUID bzw.
  IFC-GlobalId (Bauteil kam per IFC aus Archicad).
- Verglichen werden Eigenschaften, die **beide Seiten unter demselben Namen** kennen („Gruppe.Name“ in Archicad =
  „Pset.Name“ in der Plattform), dazu alles aus der Basis des letzten Abgleichs. Stempel und Technikfelder nie.
- Mit Basis: wer sich vom letzten gemeinsamen Wert entfernt hat, gewinnt; beide verschieden → Frage.
  Ohne Basis (erster Abgleich): ein Wert füllt die leere Seite; zwei verschiedene Werte → Frage.
- Geometrie: im Skript-Ablauf **nicht** verglichen. Der Diff-Checker unten vergleicht sie ohne IFC-Export und setzt je
  Abweichung `richtung` gegen die Basis; die in Archicad geänderten gehen als Netze mit (Schritt 8). <!-- 2026-11-20 -->

## Bekannt / offen

- Der neue Bau-Weg über Bauliste-Netze (Schritt 7) ist noch nicht live gegen Archicad gelaufen. <!-- 2026-11-20 -->
- Die Netz-Übergabe (Schritt 8) ist nur mit Attrappen getestet, nicht live; Plattform-Seite steht aus. <!-- 2026-11-20 -->
- Live gelaufen am 19.11.2026 (BFW07), Erkenntnisse im Abschnitt „Live-Lauf BFW07 19.11.2026“ unten.
- Eigenschaftsnamen beider Seiten passen nur, wenn der IFC-Übersetzer die Archicad-Gruppen als gleichnamige Psets
  ausgibt. Passt das nicht, findet der Vergleich nichts (keine falschen Änderungen, aber auch kein Abgleich).
- Werte kommen aus Archicad als Anzeigetext; „0,24“ und 0.24, „Ja“ und true gelten als gleich.
- Das Skript braucht den Kern `elmonkey`; Reihenfolge: installiertes elmonkey-core, dann `core/` im ELMonkey-Repo, dann im Spiegel mozzi86/-archicad unter `scripts/_kern/elmonkey`. `_kern` schreibt der Workflow skill-spiegeln bei jedem Spiegeln (Herkunft in `scripts/_kern/HERKUNFT.txt`), nicht von Hand ändern. Meldung „Modul 'elmonkey' nicht gefunden“ → elmonkey-core installieren oder Skill neu spiegeln/installieren. <!-- 2026-11-20 -->

## Live-Lauf BFW07 19.11.2026 <!-- 2026-11-19 -->

- ÜBERHOLT <!-- 2026-11-20 --> (neue Bauliste mit echten Netzen, PR 404, Staging seit 20.11.2026, bis 200 GUIDs je
  Aufruf, siehe Schritt 7): ~~Die Bauliste liefert nur die konvexe Hülle (395 von 432 unbrauchbar) → echte Form über
  `modell_exportieren` + `bauen`.~~ ~~Die Bauliste gibt nur ~20 GUIDs je Aufruf her.~~
- Ebenen-Bauteile (nur in Skizzen-Ebenen) und Vorschläge kennt der Abgleich nicht.
- Paare melden: in `ergebnis.json` `basis.paare` um `gebaut.json` ergänzen, hochladen, `archicad_abgleich_melden`
  (Vorschau, dann `bestaetigt`).
- Cmd+S/Senden in Teamwork scheitert an fremder Reservierung der Ebenen-Einstellungen — die neue Ebene erst senden,
  wenn sie freigegeben ist.
- Eigenschaften an unklassifizierten Elementen scheitern mit 6703.
- Tapir 1.7.2 meldet eigene Eigenschaften als „Custom“ (nicht „UserDefined“).
- `GetDetailsOfElements.type` entspricht nicht den Typnamen → Elemente je Typ über `API.GetElementsByType` holen.
- SSL-Fehler mit dem python.org-Python auf macOS → `herunterladen` fällt auf curl zurück.
- `SetLayerOfElements` mit `reserve: false`, sonst löst jedes Paket beim Nutzer einen Sendeversuch aus.
- Messwerte: `archicad-lesen` 5–11 min bei 90.000 Elementen, `bauen` 431 Morphs ≈ 25 min.

## Diff-Checker: nur die Unterschiede, ohne IFC-Export <!-- 2026-11-16, noch NICHT live gelaufen -->

Statt `archicad-lesen` + `planen` (nur Eigenschaften) und Komplett-IFC für die Geometrie: MCP-Werkzeug
`elmonkey_diff_plattform` (ab ELMonkey-Add-on 0.11.13, MCP 0.3.0). Es liest jedes Element über
`ELM_SAB.GetDiffDataELM` (ohne Umbau-Filter, ohne Ebenen-Filter, IFC-Klasse/Attribute/Eigenschaften über die IFC-API,
Box aus dem 3D-Modell nach Verschneidungen), vergleicht mit dem `stand_link` und schreibt `ergebnis.json` im selben
Format (`sab-archicad-abgleich/1`) plus `diff` (neu / weg / Eigenschaften / Geometrie je Seite mit Delta, Toleranz 3 mm).

1. Schritt 2 oben (`archicad_abgleich_lesen`), dann `elmonkey_diff_plattform {stand: "<stand_link>"}`.
2. Zahlen zeigen; `diff.geometrie_ungeprueft` und `diff.hinweise` immer mit nennen (z. B. Elemente auf ausgeblendeten
   Ebenen haben keine belastbare Box).
   Ohne MCP: `plattform_abgleich.py diff --ordner $O --stand-link "<stand_link>"` (dieselbe Logik).
3. Weiter mit Schritt 8 (`netze`) und 9 (`hochladen --netze-ablage`, `archicad_abgleich_melden` mit `gebaut` =
   `diff.gebaut_vorschlag`). Die Plattform
   macht aus `diff.geometrie` Geometrie-Vorschläge im selben Vorschlag; `geometrie.nicht_automatisch` und
   `plattform_geaendert` aus der Antwort dem Zeichner zeigen.

Details, Fallen und die ehrliche Einschätzung der Geometrie: [`reference/diff-plattform.md`](../reference/diff-plattform.md).

## Klassifizierungssystem und fehlende Eigenschaften <!-- 2026-11-20 -->

- `bauen` findet das System automatisch (`SAB_Klassifizierung_29` oder `Graphisoft Klassifizierung`); `--system NAME` erzwingt eines. Der gefundene Name steht im Ergebnis unter `klassifizierung`.
- Fehlende Eigenschaften lassen sich per `TapirCommand.CreatePropertyDefinitions` anlegen, nur mit `isEditable: true` und `defaultValue {basicDefaultValue {type: string, status: userUndefined}}` (sonst „failed to create the property“); Verfügbarkeit = alle Klassen-Items. Nur nach Ja des Zeichners.
