---
slug: 261109-ii2
status: complete
---
# ELM_SAB.ExportSurfaces — Oberflächen + Texturbilder exportieren (0.11.12)

Auftrag Bernd (Teams 09.11.2026): „bitte in deinen Elmonkey einbauen, damit man die Texturen eines Projekts aus Archicad exportieren kann“. Vorlage: Bernds Zip `ELM_SAB_Oberflaechen-Export_2026-11-09.zip` (ExportSurfacesCommand.*, Format `sab-oberflaechen/1`, pruefen.py), abgeleitet aus dem Plattform-Add-on (SchwarzOberflaechen.cpp).

1. `Sources/ExportSurfacesCommand.hpp/.cpp` aus Bernds Vorlage, angepasst: ELMCommandBase, Fehler über `CreateErrorResponseELM` + `ELMResponseSchemaWithError` (Bernds `CreateFailedExecutionResult` hätte `success:false` neben `error` → 4009), `MigrationHelper.hpp` für AC27/28, `archicad_version` aus `ServerMainVers_*` statt fest 29, `fileLoc` nach dem Get über den Attribut-Zeiger freigeben, Windows-Benutzerordner per `_wdupenv_s`, Menüfunktion weggelassen.
2. Registrierung in `AddOnMain.cpp` (`elmSabCommands`, "0.11.12"), `ELM_SAB_VERSION` 0.11.12.
3. Format-Doku + Prüfskript nach `archicad-addon/ELM_SAB_Tapir/Tools/oberflaechen/`, Skill-Referenz + README.
4. Zweig pushen, CI AC27–30 × Mac/Win. Kein Merge ohne User; Live-Lauf in Archicad steht aus.
