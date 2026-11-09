# Format `sab-oberflaechen/1`

Ein Ordner je Export:

```
<Projekt>_oberflaechen_<JJJJMMTT-hhmmss>/
  oberflaechen.json
  texturen/        die Bilder, die eine Oberfläche nutzt (jedes nur einmal)
  LIESMICH.md
```

## oberflaechen.json

```json
{
  "format": "sab-oberflaechen/1",
  "projekt": "Testprojekt",
  "datei": "/Pfad/zur/Datei.pln",
  "plattform_projekt": null,
  "archicad_version": 29,
  "quelle": "ELM_SAB_Add-On ExportSurfaces 0.11.12",
  "exportiert_am": "2026-01-01T12:00:00",
  "zaehler": { "oberflaechen": 455, "mit_bild": 246, "bild_fehlt": 0, "ohne_textur": 209, "bilder": 222 },
  "oberflaechen": [
    {
      "name": "Ziegel Klinker rot",
      "index": 57,
      "guid": "…",
      "art": "matt",
      "farbe": "#9a4a33",
      "spiegelfarbe": "#ffffff",
      "emission": "#000000",
      "umgebung_pc": 70, "diffus_pc": 80, "spiegelung_pc": 10, "transparenz_pc": 0,
      "glanz": 0.15,
      "textur": {
        "name": "Klinker 01",
        "datei": "texturen/Klinker 01.jpg",
        "breite_m": 1.2, "hoehe_m": 0.6,
        "drehung_grad": 0,
        "alpha": false, "bump": false,
        "spiegeln_x": false, "spiegeln_y": false,
        "zufall_versatz": false, "transparenz_aus_alpha": false
      }
    }
  ]
}
```

| Feld | Bedeutung |
|---|---|
| `name` | Name der Archicad-Oberfläche. Der IFC-Stilname im Modell ist genau dieser Name, darüber ordnet die Plattform zu. |
| `index`, `guid` | Attribut-Index und -GUID in dieser Datei |
| `art` | allgemein · einfach · matt · metall · kunststoff · glas · leuchtend · konstant · unbekannt |
| `farbe` | Oberflächenfarbe (surfaceRGB) als `#rrggbb` |
| `*_pc` | Prozentwerte aus Archicad (0–100) |
| `glanz` | shine / 100 |
| `textur` | `null`, wenn die Oberfläche keine Textur hat |
| `textur.datei` | relativer Pfad im Ordner, oder `null` mit `bild_fehlt` (Grund) |
| `breite_m`, `hoehe_m` | Größe EINER Bildkachel im Modell in Metern (xSize/ySize) |
| `drehung_grad` | rotAng in Grad |
| Flags | aus `texture.status`: UseAlpha, BumpPattern, MirrorX/Y, RandomShift, TransPattern |

Zahlen stehen immer mit Punkt. Text ist UTF-8, Umlaute bleiben erhalten. Gleich benannte Bilder
aus verschiedenen Bibliotheken bekommen `_2`, `_3` … angehängt.
