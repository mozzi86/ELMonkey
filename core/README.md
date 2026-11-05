# elmonkey-core 0.3.0

Neutraler Kern des Python-Monkeys. Nur Standardbibliothek; IFC-Ausgabe über das Extra `[ifc]`.

## Module

- `elmonkey` – Kern: finden (und/oder/nicht), Vorprüfung mit Codes, Rücklese.
- `elmonkey.plattform` – Klebe-Logik für die Schwarz Plattform.
- `elmonkey.katalog` – Bauteilkatalog-Maschine: Typen aus Schichten, Bauplan mit Ständerwerk und Verstärkungen,
  Bearbeiten mit Versionen, feste Typ-GUIDs. `elmonkey.katalog.ifc` schreibt IFC4/IFC4X3-Bibliotheken
  (braucht ifcopenshell).

## Installation

    pip install elmonkey-core            # Kern + Katalog ohne IFC
    pip install "elmonkey-core[ifc]"     # zusätzlich elmonkey.katalog.ifc (ifcopenshell >= 0.8)

## Minimalbeispiel

```python
from elmonkey.katalog import Katalog, bauen
from elmonkey.katalog import ifc            # nur mit [ifc]

k = Katalog.laden("mein/katalog-ordner")     # baustoffe.json + typen/*.json
plan = bauen.wand(k, "mein-typ-schluessel", 5.0, 2.75)
datei = ifc.typbibliothek(k, "IFC4", pset_praefix="Pset_Meins")
```

Profile und Verstärkungen (herstellerneutral) liefert das Paket mit; eine `profile.json` bzw.
`verstaerkungen.json` im Katalog-Ordner oder die Parameter `profile=`/`verstaerkungen=` überschreiben sie.

## Datenformat

Der Aufrufer bringt die Daten mit: `baustoffe.json` (`{"baustoffe": [...]}`) und `typen/*.json`
(`{"typen": [...]}`). Ein kleines erfundenes Beispiel liegt in `core/tests/daten/katalog`.
Es gibt keine Bürodaten im Paket.

## Ausgabe anpassen

`ifc.Schreiber`, `materialbibliothek`, `typbibliothek` und `muster_projekt` nehmen `name=`, `pset_praefix=`
und `textur_url=` entgegen; `muster_projekt` zusätzlich `rollen=` (Rolle → Typschlüssel; Pflicht: `aussenwand`,
`staenderwand`, `rohdecke`). Die Plattform stellt darüber ihre SAB-Ausgabe her (z. B. `pset_praefix="Pset_SAB"`).

## Tests

    cd core && python -m pytest -q tests

Die IFC-Tests werden ohne ifcopenshell übersprungen.
