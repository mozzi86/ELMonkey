# ELM_SAB_Tapir — Kombi-Add-On für Archicad

Das hauseigene Archicad-Add-On von Schwarz Architekturbüro Nürnberg. Es enthält
**Tapir 1.5.4 vollständig** plus die SAB-eigenen Befehle in einem einzigen Bundle —
beide Befehls-Namensräume, `TapirCommand` und `ELM_SAB`, sind gleichzeitig verfügbar.

Basis ist [Tapir](https://github.com/ENZYME-APD/tapir-archicad-automation) von
ENZYME APD, das seinerseits auf Tibor Lorantfys
[archicad-additional-json-commands](https://github.com/tlorantfy/archicad-additional-json-commands)
aufbaut.

## Download

Gebaut wird per GitHub Actions; die Artefakte liegen in der rollenden Vorabversion
[`elm-sab-tapir-latest`](../../releases/tag/elm-sab-tapir-latest).

| Archicad | macOS | Windows |
|---|---|---|
| **29** | [ELM_SAB_Tapir_AC29_Mac.zip](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC29_Mac.zip) | [ELM_SAB_Tapir_AC29_Win.apx](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC29_Win.apx) |
| **28** | [ELM_SAB_Tapir_AC28_Mac.zip](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC28_Mac.zip) | [ELM_SAB_Tapir_AC28_Win.apx](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC28_Win.apx) |
| **27** | [ELM_SAB_Tapir_AC27_Mac.zip](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC27_Mac.zip) | [ELM_SAB_Tapir_AC27_Win.apx](../../releases/download/elm-sab-tapir-latest/ELM_SAB_Tapir_AC27_Win.apx) |

Die Datei muss zur Archicad-Hauptversion passen. Ein AC29-Bundle lädt nicht in AC28 —
es erscheint dann gar nicht im Add-On-Manager, ohne Fehlermeldung. Die eigene Version
steht unter *Archicad → Über Archicad*.

## Installieren

**macOS**

1. Archicad mit `⌘Q` **komplett** beenden. Eine im Hintergrund weiterlaufende Instanz
   lädt sonst weiter das alte Add-On — daran ist schon mehr als ein Bundle-Tausch
   scheinbar wirkungslos geblieben.
2. ZIP entpacken, das `.bundle` nach `/Applications/Graphisoft/Archicad 29/Add-Ons/`
   legen (Pfad je Version anpassen).
3. Keine Sicherungskopien im `Add-Ons`-Ordner liegen lassen — Archicad versucht sie zu
   laden und meldet „Einige Add-Ons konnten nicht geladen werden".
4. Quarantäne-Flag entfernen, sonst blockiert macOS das Bundle stillschweigend:

   ```bash
   xattr -dr com.apple.quarantine "/Applications/Graphisoft/Archicad 29/Add-Ons/ELM_SAB_AC29_Mac.bundle"
   ```

**Windows**

1. Archicad beenden und im Task-Manager prüfen, dass kein `Archicad.exe` mehr läuft.
2. Die `.apx` nach `C:\Program Files\Graphisoft\Archicad 29\Add-Ons\` kopieren
   (Administratorrechte nötig).
3. Falls Windows die Datei als „aus dem Internet" markiert: Rechtsklick →
   *Eigenschaften* → *Zulassen* → *Übernehmen*.

Danach in Archicad unter **Optionen → Add-On-Manager** prüfen, dass das Add-On geladen ist.

## Funktioniert es?

Archicad mit einem beliebigen Projekt öffnen, dann:

```bash
curl -s localhost:19723 -H 'Content-Type: application/json' \
  -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"TapirCommand","commandName":"GetAddOnVersion"},"addOnCommandParameters":{}}}'
```

Erwartet wird `{"succeeded":true,"result":{"addOnCommandResponse":{"version":"1.5.4"}}}`.
Kommt keine Antwort, ist das Add-On nicht geladen. Läuft mehr als eine Archicad-Instanz,
hört die zweite auf Port `19724`.

Die SAB-eigenen Befehle prüft man über ihren eigenen Namensraum:

```bash
curl -s localhost:19723 -H 'Content-Type: application/json' \
  -d '{"command":"API.ExecuteAddOnCommand","parameters":{"addOnCommandId":{"commandNamespace":"ELM_SAB","commandName":"Get2DGeometryOfElements"},"addOnCommandParameters":{"elements":[]}}}'
```

## Die SAB-eigenen Befehle

Alle im Namensraum `ELM_SAB`, alle mit eingebauter Rücklese-Verifikation — ein
`NoError` der Archicad-API beweist bei Schreiboperationen nichts.

| Befehl | Wofür |
|---|---|
| `GetPenOfElements` / `SetPenOfElements` | Stifte lesen und setzen, inklusive der RGB-Overrides, die DWG-Importe direkt ins Element brennen. Weder Tapir noch die offizielle API können das. |
| `Get2DGeometryOfElements` | Linien, Bögen, Kreise, Polylinien und Schraffur-Polygone auslesen; funktioniert über Datenbankgrenzen hinweg. Antwortfeld heißt `geometryOfElements`. |
| `GetTextsOfElements` / `SetTextsOfElements` | Textinhalte von Texten und Etiketten lesen und schreiben. Zeilentrenner ist `\r`, nicht `\n`. |
| `SetTextSizeOfElements` | Schriftgröße von Text und Etikett, in mm oder als Faktor. |
| `SetAddParsOfElements` | GDL-Parameter über das AddPars-Memo — für Objekte, Lampen, Etiketten und Zonen. Bei Etiketten zwingend statt Tapirs `SetGDLParametersOfElements`, das dort abstürzt. |
| `CreatePolygonWalls` | Polygonwände. Tapir kann nur gerade Wände. |
| `CreateCurtainWallFromAxes` | Vorhangwände aus Pfostenachsen, auch nichtrechteckig (Giebel, Trapez, freies Polygon). |
| `GetColumnDetails` / `SetColumnDetails` | Kernmaße von Stützen. |
| `SetColumnRotation` | Absoluter Drehwinkel von Stützen. Tapirs `RotateElements` meldet hier Erfolg, ändert aber nichts. |

## Selbst bauen

Der Build läuft normalerweise in der CI
([`.github/workflows/build-elm-sab-tapir.yml`](../.github/workflows/build-elm-sab-tapir.yml)):
sie baut alle sechs Kombinationen aus AC27/28/29 und Mac/Windows und lädt sie in die
rollende Vorabversion. Ein Push auf `ELM_SAB_Tapir/**` löst sie aus, per
`workflow_dispatch` geht es auch von Hand.

Lokal gebraucht: CMake ≥ 3.17, Python, sowie vollständiges Xcode (macOS) beziehungsweise
Visual Studio (Windows). Das DevKit lädt der Build selbst:

```bash
python3 Tools/download_and_unzip.py \
  https://github.com/GRAPHISOFT/archicad-api-devkit/releases/download/29.3000/API.Development.Kit.MAC.29.3000.zip \
  Build/DevKits/AC29
cmake -B Build -G "Xcode" -DAC_VERSION=29 -DAC_API_DEVKIT_DIR="Build/DevKits/AC29/Support" .
cmake --build Build --config RelWithDebInfo
```

Zwei Dinge, die dabei gern übersehen werden:

- **Windows braucht für AC27 und AC28 das Toolset v142** (`-T v142`), erst ab AC29 gilt
  `v143`. Deshalb ist der CI-Runner auf `windows-2022` gepinnt — neuere Images liefern
  v142 nicht mehr mit.
- **Der C++-Standard wechselt mit der Version:** AC27/28 werden als C++17 kompiliert,
  AC29 als C++20. `CMakeCommon.cmake` erledigt das selbst; wer neue Befehle schreibt,
  darf darin aber keine C++20-Sprachfeatures verwenden, sonst bricht der AC27/28-Build.

Die MDID (Developer-ID `944131939`, Local-ID `1033975726`, registriert am 2026-07-13)
liegt in [`Sources/RFIX/AddOnFix.grc`](Sources/RFIX/AddOnFix.grc) und gilt
versionsunabhängig für alle Builds. Ohne offiziell registrierte MDID lädt Archicad ein
Add-On nicht, sondern meldet einen Distributor-Fehler oder schweigt ganz.

## Entwickler-Dokumentation

[Tapir-Wiki: Archicad Add-On Development](https://github.com/ENZYME-APD/tapir-archicad-automation/wiki/Archicad-Add-On-Development)
