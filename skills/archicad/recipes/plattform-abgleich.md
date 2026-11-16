# Abgleich Archicad ↔ Schwarz Plattform (bidirektional) <!-- 2026-11-10, noch NICHT live gelaufen -->

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

Arbeitsordner je Lauf, z. B. `~/Abgleich/BFW07-2026-11-10` (unten `$O`).

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
7. **Archicad: neue Bauteile bauen** — GUIDs aus `ergebnis.json` → `archicad.neu_bauen`, in Paketen ≤ 200:
   Konnektor `archicad_abgleich_bauliste {projekt, guids}` → je Bauteil Werkzeug, Geschoss, Ebene, Parameter
   (Archicad-Meter). Bauen wie im Verfahren `sab-archicad-auftrag` (Stuetze → Stütze, Traeger → Träger, Decke,
   Objekt, Morph), zurücklesen. Bauteile mit `status: fehler` nicht raten. Gebaute Paare nach `$O/gebaut.json`
   `[{"plattformGuid": "…", "archicadGuid": "…"}]`, dann
   `plattform_abgleich.py stempeln --ordner $O --gebaut $O/gebaut.json --ausfuehren`.
   Fehlt die Eigenschaft „Plattform-GUID“ im Projekt: mit dem Zeichner anlegen (Gruppe „Allgemeine Werte“, Text).
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

## Diff-Checker: nur die Unterschiede, ohne IFC-Datei <!-- 2026-11-16, NICHT live gelaufen -->

Für den Fall „in Archicad wurde weitergezeichnet, was ist jetzt anders als in der Plattform?“. Ersetzt den Umweg
über ein Komplett-IFC (AFWC 16.11.: 27 MB, Export + Zerlegen + Schein-Unterschiede).

1. Konnektor `archicad_abgleich_lesen {projekt}` → `lauf`, `stand_link`, `ergebnis_ablage`.
2. `plattform_abgleich.py diff --ordner $O --stand-link "<stand_link>"` — liest Archicad direkt (alle Elemente,
   auch vom Umbau-Filter ausgeblendete), vergleicht, schreibt `diff.json` und `ergebnis.json`. Ändert nichts.
3. Projektname und Zahlen dem Zeichner zeigen: `neu`, `weg`, `eigenschaften_bauteile`, `geometrie_bauteile`,
   `geometrie_ungeprueft`. **`geometrie_ungeprueft` nie als „gleich“ ausgeben.**
4. `plattform_abgleich.py hochladen …`, dann `archicad_abgleich_melden` (Vorschau, nach dem Ja `bestaetigt=true`).

Was der Diff abfängt (Fallen aus dem Live-Lauf):

| Falle | Verhalten |
|---|---|
| Rohe Hüllbox statt geschnittener Geometrie | Verglichen wird nur die Körper-Box aus `ELM_SAB.GetSolidBounds` (ab 0.11.13). Ohne den Befehl oder ohne Körper: `geometrie_ungeprueft`, kein Vergleich. |
| Mehrschichtige Bauteile | Archicad hält alle Schichten im selben Element; die Körper werden zu einer Box vereint. |
| Umbau-Filter blendet Abbruch aus | `GetAllElements` liest alle Elemente. Ohne Körper im 3D landen sie in `geometrie_ungeprueft`, nicht in `weg`. |
| Öffnungs-GUIDs je Export neu | Öffnungen sind weder `neu` noch `weg`, nur gezählt (`oeffnungen`). |
| Räume | Getrennt unter `raeume`, nie `neu`. |
| Schreibweisen (`.T.`/„Ja“, `IFCLABEL('NEW')`/`['NEW']`, `\X\E4`) | Vor dem Vergleich normalisiert. |
| Mengen ohne Geometrieänderung | `Qto_*`, `BaseQuantities` und Mengen-Gruppen zählen nicht. |
| Plattform-eigene Bauteile ohne Partner | Unter `nur_plattform`, nicht `weg`; Paare über `gebaut` in `archicad_abgleich_melden`. |
| Feste Klassenliste | Es gibt keine: jedes Bauteil-Element zählt, der IFC-Typ kommt aus `GetIFCTypeOfElements`. |

Geometrie-Delta: je Seite `x-`/`x+`/`y-`/`y+`/`z-`/`z+` in Metern, Archicad minus Plattform, in Achsrichtung.
Toleranz 3 mm (`--toleranz`).

**Voraussetzungen, die noch fehlen:**

- Die Plattform muss je Bauteil eine `bbox` in Archicad-Metern in `stand.json` mitgeben. Solange sie fehlt, meldet
  der Diff für alle Paare „ohne Geometrievergleich“ und vergleicht nur Eigenschaften, neu und weg.
- `plattform.geometrie` in `ergebnis.json` versteht `archicad_abgleich_melden` noch nicht. Bis dahin die
  Geometrie-Unterschiede aus `diff.json` von Hand über `modell_bauteile_aendern` als Vorschlag schreiben
  (Grenzen: `laenge|dicke|hoehe` nur bei rechteckigem Grundriss, siehe `plattform-bauteile-uebernehmen.md`).

## Wie gepaart und verglichen wird

- Paar = (1) Paar aus dem letzten Abgleich, (2) Stempel „Plattform-GUID“ am Archicad-Bauteil, (3) gleiche GUID bzw.
  IFC-GlobalId (Bauteil kam per IFC aus Archicad).
- Verglichen werden Eigenschaften, die **beide Seiten unter demselben Namen** kennen („Gruppe.Name“ in Archicad =
  „Pset.Name“ in der Plattform), dazu alles aus der Basis des letzten Abgleichs. Stempel und Technikfelder nie.
- Mit Basis: wer sich vom letzten gemeinsamen Wert entfernt hat, gewinnt; beide verschieden → Frage.
  Ohne Basis (erster Abgleich): ein Wert füllt die leere Seite; zwei verschiedene Werte → Frage.
- Geometrie wird **nicht** verglichen. Geänderte Geometrie kommt aus Archicad über den IFC-Import (Schritt 9).

## Bekannt / offen

- Noch nicht gegen ein laufendes Archicad geprüft (Stand 10.11.2026) — erster Lauf an einer Projektkopie.
- Eigenschaftsnamen beider Seiten passen nur, wenn der IFC-Übersetzer die Archicad-Gruppen als gleichnamige Psets
  ausgibt. Passt das nicht, findet der Vergleich nichts (keine falschen Änderungen, aber auch kein Abgleich).
- Werte kommen aus Archicad als Anzeigetext; „0,24“ und 0.24, „Ja“ und true gelten als gleich.
