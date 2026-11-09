---
slug: 261109-ii2
status: complete
---
# ELM_SAB.ExportSurfaces (0.11.12) — Zusammenfassung

- `Sources/ExportSurfacesCommand.hpp/.cpp` (Vorlage Bernds Zip 09.11., abgeleitet aus dem Plattform-Add-on), registriert in `AddOnMain.cpp` (`elmSabCommands`, "0.11.12"), `ELM_SAB_VERSION` 0.11.12.
- Anpassungen: Fehlerform ELM (`CreateErrorResponseELM` + `ELMResponseSchemaWithError`), MigrationHelper (AC27/28), `archicad_version` aus `ServerMainVers_*`, fileLoc-Freigabe robust, `_wdupenv_s` (Windows), UTF-8-sicheres Kürzen, kein Menüpunkt. Projektkürzel/Namen aus Kommentaren entfernt (öffentliches Repo).
- `Tools/oberflaechen/FORMAT_sab-oberflaechen-1.md` + `pruefen.py` (Selbsttest mit synthetischem Ordner: in Ordnung; Plattform-Leser `paket_lesen` liest ihn ohne Warnung).
- MCP 0.2.2: Synonyme Textur/Oberfläche/Material/exportieren, Test 8/8.
- Lokal: clang `-fsyntax-only -Wall -Wextra -Wshadow -Werror` gegen DevKit 28 (C++17), 29, 30 (C++20) ohne Befund.
- Offen: CI AC27–30 × Mac/Win (Lauf 42616767531), Live-Lauf in Archicad, Merge nach main + Release nur nach User-Ja.
