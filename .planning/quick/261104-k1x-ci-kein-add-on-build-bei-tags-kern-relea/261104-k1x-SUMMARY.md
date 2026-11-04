---
status: complete
---
# 261104-k1x — Ergebnis
- `build-elm-sab-tapir.yml`: `branches: ['**']` → Tag `core-v0.2.1` löste keinen Add-On-Build aus (geprüft).
- `core-release.yml`: Wheel + sdist am Release, Notiz mit Pinzeile `#sha256=`.
- Kern 0.2.1, README-Installationszeile auf die Wheel-Datei.
- Geprüft: Release `core-v0.2.1` hat `elmonkey_core-0.2.1-py3-none-any.whl` + `.tar.gz`; pip mit richtiger Prüfsumme installiert 0.2.1, mit verfälschter bricht es ab (Hash-Fehler). Kern-Tests 19/19.
- Abweichung vom GSD-Ablauf: Plan und Umsetzung direkt durch den Orchestrator (drei kleine Änderungen), ohne Planer-/Ausführer-Agenten.
