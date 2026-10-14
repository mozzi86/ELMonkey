---
phase: quick-261014-sp9
plan: 01
status: complete
---
# 261014-sp9 — ELM_SAB 0.9.29: Bemaßung in Ansichten (Beitrag S.)

- Quelle: Git-Bundle von S. (ZZ_Austausch/ELM, 14.10.), Branch elm-0.9.12-ansichtsbemassung, Basis 8ab23f7 (4.8., 73 Commits hinter main). Gesichert als lokaler Branch sp/elm-0.9.12-ansichtsbemassung-orig.
- Code a246649+d51c14d+aeffc79 als EIN Commit bde0f5a (Autorin S.) auf main 0.9.28 gesetzt, Version 0.9.29 (0.9.12–0.9.14 auf main anders belegt). SectionDimensionCommandsELM.{hpp,cpp} unverändert; Registrierung der 6 Befehle in AddOnMain.cpp neu eingefügt (Konflikt rein additiv).
- Doku b7d66ec als 020d3ab (Autorin S.), Konflikte additiv aufgelöst, Versionsangaben auf 0.9.29.
- a0605b4: -2130312912 = APIERR_HIDDENLAY (DevKit), Korrektur an zwei Einträgen.
- Review: Syntax DevKit 29 OK, kein C++20, keine Text-Memos, Helfer in anonymen Namensräumen, Befehle ohne Antwortschema (kein 4009), contentUStr wie DevKit-Beispiel.
- Nicht übernommen: lokaler arm64-Build (war nicht in den Commits).
- Offen: CI, Merge nach main, Einbau, Lese-Test (GetInteriorElevationsELM, GetSectionElementsELM).
