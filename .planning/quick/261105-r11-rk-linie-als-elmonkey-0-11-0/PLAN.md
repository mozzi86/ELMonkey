---
slug: 261105-r11
status: planned
---
# RKs Linie elm-0.10.8-bogenwand als ELMonkey 0.11.0

1. 11 RK-Commits (39abbc6..db90a56, Bundle ZZ_Austausch 04.11.) per `git am --3way --directory=archicad-addon` auf Zweig `elm-0.11.0-rk-integration`.
2. Konflikt nur `AddOnVersion.hpp` → `ELM_SAB_VERSION "0.11.0"`.
3. Öffentlich-Regel (User 05.11.): Autor RK → `RK <rk@users.noreply.github.com>`, Projektkürzel BFW aus Kommentaren entfernt.
4. Fallen aus RKs Handoff → `skills/archicad/reference/mcp-extension.md`.
5. Push Zweig (nicht main), CI AC27–30 × Mac/Win 8/8. Kein Einbau ohne User.
