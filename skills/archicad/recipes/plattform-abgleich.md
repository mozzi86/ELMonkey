# Abgleich Archicad ↔ Schwarz Plattform (bidirektional) <!-- 2026-11-10, live BFW07 19.11.2026 -->

Auslöser: „Projekt synchronisieren“, „Plattform und Archicad abgleichen“.

Gleicht das **offene Archicad-Projekt** (BIMcloud/Teamwork) mit dem **wirksamen Stand der Plattform** ab und führt
beide zusammen:

| Richtung | Was | Wie |
|---|---|---|
| Plattform → Archicad | Bauteile, die nur die Plattform kennt | nativ **neu bauen** (neue Archicad-GUID), Stempel „Plattform-GUID“ |
| Plattform → Archicad | geänderte Bauteilinformationen | Eigenschaften setzen, zurücklesen |
| Archicad → Plattform | Bauteile, die nur Archicad kennt | IFC → `modell_hochladen` → Import-Vorschlag |
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
2. **Plattform lesen** — Konnektor `archicad_abgleich_lesen {projekt}` → `lauf`, `stand_link`, `ergebnis_ablage`.
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
7. **Archicad: neue Bauteile bauen** — mit echter Geometrie, nicht als Hülle: Konnektor `modell_exportieren
   {modellarten: ["plattform"]}` liefert das Plattform-IFC (Archicad-Koordinaten); Konnektor
   `archicad_abgleich_bauliste {projekt, guids}` (nur ~20 GUIDs je Aufruf) liefert Ebene, Name, IFC-Klasse nach
   `$O/bauliste.json`; das Register liefert den Umbau-Status (`$O/register.json`). Dann als Trockenlauf
   `plattform_abgleich.py bauen --ordner $O --ifc <plattform.ifc> --bauliste $O/bauliste.json --register $O/register.json
   [--zuordnung …] [--nur GUID,…] [--ohne GUID,…] [--vorhanden v.json]`, Zahlen (zu bauen, ausgelassen, Ebenen,
   Klassen) dem Zeichner zeigen, nach dem Ja dasselbe mit `--ausfuehren`. Je Bauteil ein Morph, Ebene, Geschoss aus der
   Unterkante, Klasse (`sab_zuordnung.json`), Umbau-Status, Stempel „Plattform-GUID“ + „KI generiert <Datum>“ und
   Rücklese (`ruecklese.json`, Paare in `gebaut.json`). `bauen` stempelt selbst; `stempeln --gebaut …` nur noch für von
   Hand gebaute Bauteile (es reserviert nicht, unklassifizierte meldet es als „nicht klassifiziert (6703)“).
   `ausgelassen.json` (keine Geometrie, Klasse/Status fehlt) dem Zeichner nennen. Fehlt die Eigenschaft
   „Plattform-GUID“ im Projekt: mit dem Zeichner anlegen (Gruppe „Allgemeine Werte“, Text).
8. **Plattform: Ergebnis melden** — `plattform_abgleich.py hochladen --ordner $O --ablage "<ergebnis_ablage>"`,
   dann Konnektor `archicad_abgleich_melden {projekt, lauf, gebaut}` (Vorschau) → nach dem Ja `bestaetigt=true`.
   Es entsteht EIN Vorschlag; freigegeben wird in der App.
9. **Plattform: neue Archicad-Bauteile** (wenn `plattform.neu_importieren` nicht leer) —
   `plattform_abgleich.py ifc --ordner $O [--translator "<Name>"]`, dann `modell_hochladen` (Ablage-Link, IFC
   hochladen). Der Import-Vorschlag zeigt die neuen Bauteile; Konflikte entscheidet der Freigebende.
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
- Geometrie: im Skript-Ablauf **nicht** verglichen (geänderte Geometrie kommt über den IFC-Import, Schritt 9). Der
  Diff-Checker unten vergleicht sie ohne IFC-Export.

## Bekannt / offen

- Live gelaufen am 19.11.2026 (BFW07), Erkenntnisse im Abschnitt „Live-Lauf BFW07 19.11.2026“ unten.
- Eigenschaftsnamen beider Seiten passen nur, wenn der IFC-Übersetzer die Archicad-Gruppen als gleichnamige Psets
  ausgibt. Passt das nicht, findet der Vergleich nichts (keine falschen Änderungen, aber auch kein Abgleich).
- Werte kommen aus Archicad als Anzeigetext; „0,24“ und 0.24, „Ja“ und true gelten als gleich.
- Das Skript braucht den Kern `elmonkey`; Reihenfolge: installiertes elmonkey-core, dann `core/` im ELMonkey-Repo, dann im Spiegel mozzi86/-archicad unter `scripts/_kern/elmonkey`. `_kern` schreibt der Workflow skill-spiegeln bei jedem Spiegeln (Herkunft in `scripts/_kern/HERKUNFT.txt`), nicht von Hand ändern. Meldung „Modul 'elmonkey' nicht gefunden“ → elmonkey-core installieren oder Skill neu spiegeln/installieren. <!-- 2026-11-20 -->

## Live-Lauf BFW07 19.11.2026 <!-- 2026-11-19 -->

- Die Bauliste liefert nur die konvexe Hülle (vereinfacht, 395 von 432 unbrauchbar) → die echte Form kommt über
  `modell_exportieren` + `bauen`.
- Die Bauliste gibt nur ~20 GUIDs je Aufruf her.
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
3. Weiter mit Schritt 8 (`hochladen`, `archicad_abgleich_melden` mit `gebaut` = `diff.gebaut_vorschlag`). Die Plattform
   macht aus `diff.geometrie` Geometrie-Vorschläge im selben Vorschlag; `geometrie.nicht_automatisch` und
   `plattform_geaendert` aus der Antwort dem Zeichner zeigen.

Details, Fallen und die ehrliche Einschätzung der Geometrie: [`reference/diff-plattform.md`](../reference/diff-plattform.md).
