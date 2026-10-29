# Plan – ELMonkey 0.10.1: Stifte von Bauteilen lesen und setzen

Stand 29.10.2026 · Auslöser: THN-Ausführungspläne, rote Wandkonturen aus der Zeit, als die Brandschutzeigenschaften in der Wand gepflegt wurden. Brandschutz läuft jetzt über die grafische Überschreibung.
Ziel im Modell: Bauteile schwarz-weiß; die Schraffur kommt aus dem Baustoff oder dem Verbund, also keine Überschreibung der Schnittflächenstifte am Element.

## Warum ein neuer Befehl
- Für die Stifte von Bauteilen gibt es weder in Tapir 1.7.2 noch in der offiziellen JSON-API einen Befehl. Das wurde am 29.10. mit drei Discovery-Abfragen geprüft.
- `ELM_SAB.GetPenOfElements` und `ELM_SAB.SetPenOfElements` können nur 2D-Elemente.
- Über Eigenschaften sind die Stifte auch nicht lesbar: Archicad hat keine eingebaute Eigenschaft dafür.

## Zwei neue Befehle (Namespace ELM_SAB)

### `GetConstructionPensOfElements` (nur lesen)
Eingabe: `elements[]`, optional `includeComposites: true`.
Ausgabe je Element: Typ, Ebene, Geschoss, Aufbau (Basis / Mehrschicht / Profil + Index) und die Stifte:

| Typ | Felder (DevKit 29.3100, `APIdefs_Elements.h`) |
|---|---|
| Wand | `contPen`, `contPen3D`, `contLtype`, `aboveViewLinePen`, `cutFillPen`*, `cutFillBackgroundPen`* |
| Stütze | `corePen`, `venLinePen`, `coreSymbolPen`, `hiddenLinePen`, `belowViewLinePen`, `aboveViewLinePen`, `coverFill*Pen`, `cutFill*`* |
| Unterzug | `cutContourLinePen`, `refPen`, `hiddenLinePen`, `belowViewLinePen`, `aboveViewLinePen`, `coverFill*Pen`, `cutFill*`* |
| Decke | `pen`, `sectContPen`, `hiddenContourLinePen`, `floorFillPen`, `floorFillBGPen`, `cutFill*`* |
| Dach / Schale | `shellBase`: `pen`, `sectContPen`, `aboveViewLinePen`, `floorFill*Pen`, `cutFill*`* |
| Fassade | `pen`, `frameCutLinePen`, `panelCutLinePen`, `uncutLinePen`, `overheadLinePen`, `cutFill*`* |
| Morph | `cutLinePen`, `uncutLinePen`, `overheadLinePen`, `coverFillPen`, `cutFill*`* |

\* `API_OverriddenPen` = `APIOptional<API_PenIndex>`. Ausgabe: `{overridden: bool, pen}`.

Mit `includeComposites` kommen zusätzlich die Konturstifte der Verbundschichten (`framePen`, `linePen`) jedes verwendeten Verbunds dazu. So sehen wir, ob das Rot **im Element** steckt oder **im Verbund-Attribut**. Steckt es im Verbund, ist das kein Element-Lauf mehr, sondern eine eigene Entscheidung.

### `SetConstructionPensOfElements` (schreibt)
Eingabe je Element (alle Felder optional):
- `cutLinePen`: der Schnittlinien- bzw. Konturstift. Je Typ wird er auf das passende Feld abgebildet (Wand `contPen`, Decke `sectContPen`, Unterzug `cutContourLinePen` usw.).
- `uncutLinePen` und `overheadLinePen`.
- `clearCutFillOverride: true`: setzt `cutFillPen` und `cutFillBackgroundPen` auf „nicht überschrieben“. Damit gilt die Schraffur des Baustoffs.
- Für Sonderfälle zusätzlich ein Rohfeld-Durchgriff `fields: {<Feldname>: pen}`.

Ablauf:
1. Vorprüfung `[CODE]` wie bei den übrigen 0.10-Befehlen.
2. Teamwork-Reservierung wie bei Delete.
3. Schreiben mit `ACAPI_Element_Change` und Feldmaske in einem einzigen Undo-Schritt.
4. Sofort zurücklesen; die Antwort meldet je Element `ok` oder einen Fehler.
5. Gesetzte Reservierungen wieder freigeben.

Zusätzlich gibt es `dryRun: true`: Der Befehl meldet nur, was sich ändern würde.

Bekannte Fallen, im Befehl abgefangen und je Element gemeldet:
- Ausgeblendete Ebene: Archicad meldet „permission denied“ (`6001`).
- Hotlink-Elemente.
- Elemente, die ein anderer Nutzer reserviert hat.

## Umsetzung
1. Über `/gsd-quick` im Repo `mozzi86/-archicad`, Zweig `elm-0.10.1-bauteilstifte` von `main`. `ELM_SAB_VERSION` wird 0.10.1.
2. Lokal mit `syn1.sh 27|28|29` (mit `STRICT=1`) und dem DevKit-30-Header prüfen, danach Build per CI.
3. **Live-Test nur im leeren Testprojekt, nie in THN:**
   - Wand, Stütze, Unterzug, Decke, Dach, Fassade und Morph je einmal mit rotem Stift und überschriebener Schraffur anlegen.
   - Lesen, setzen, zurücklesen, Undo prüfen.
   - Zusätzlich Ebene ausgeblendet und Verbundwand.
   - Neue Fälle kommen als Abschnitt in `gegenprobe_0100.py`.
4. Bundle tauschen nur bei geschlossenem Archicad. Heute sind 8 Instanzen offen, darunter THN-Teamwork. Rückbau-Kopie von 0.10.0 vorher sichern.
5. Doku in `README.md` und `reference/mcp-extension.md`, außerdem der passende Werkzeugeintrag in elmonkey-mcp.

## Danach in THN (eigener Schritt mit Freigabe)
1. Inventur aller Bauteile: 5.658 Wände, 539 Stützen, 196 Unterzüge, 638 Decken, 103 Dächer, 252 Fassaden und 28 Morphs. Mit `includeComposites` und der RGB-Auflösung über die Stifttabelle der Ausführungsplan-Ansicht.
2. Übersicht: welche Stifte farbig sind, wie viele Elemente es je Typ und Geschoss betrifft, und ob ein Verbund beteiligt ist.
3. **Zielstift festlegen.** Vorschlag: der Stift, den die schwarzen Wände in THN schon tragen (Mehrheit) bzw. der Wandfavorit der Bürovorlage. Stift 1 ist zwar in allen SAB-Tabellen schwarz, hat aber die falsche Strichstärke.
4. Freigabe, dann `dryRun`, dann der echte Lauf je Typ. Vorher Teamwork „Senden“ abstimmen und ausgeblendete Ebenen kurz einblenden.
5. Nachher-Inventur als Beleg: 0 farbige Stifte, 0 überschriebene Schnittflächen. Danach „Zeichnungen aktualisieren“ und den Ausführungsplan zur Sichtprüfung zeigen.
