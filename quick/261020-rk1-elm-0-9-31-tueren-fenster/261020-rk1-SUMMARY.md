---
phase: quick-261020-rk1
plan: 01
status: complete
---
# 261020-rk1 — ELM_SAB 0.9.31: GDL-Parameter für Türen/Fenster (Nachbau Beitrag Roman)

Quelle: Bundle ZZ_Austausch/RK/ELM_SAB_AC29_Mac.bundle (Roman, lokal gebaut 19.10. 10:32, universal, Basis 0.9.30, selbst als 0.9.31 nummeriert). Quellcode nicht verfügbar (lag in temporärem Claude-Scratchpad). Auftrag Mudi 20.10.: selbst nachbauen, Bundle zerlegen.

## Analyse (Binärvergleich gegen installiertes 0.9.30, arm64)
- Lesbare Unterschiede nur: SetObjectParametersForce-Beschreibung/Schema + Meldung „Nur Object, Lamp, Label, Zone, Door oder Window“; Tapir SetGDLParametersOfElements-Meldung „supports Object/Lamp/Door/Window only“.
- Disassembly ApplyToOne: Typprüfung = Bitmaske 0x300C0 (Object/Lamp/Label/Zone) ODER (typeID & ~1) == 4 (Window/Door); Skylight nicht.
- A/B-Spiegelung: Objektzweig unverändert; neuer Zweig für Window/Door schreibt A/B an Element-Offset 176/184 = window/door.openingBase.width/height (Offsets per DevKit-Probe bestätigt, identisch mit object.xRatio/yRatio).

## Nachbau
- SetObjectParametersForceCommand.cpp: Typprüfung + Meldung, Window/Door-Zweig openingBase.width/height mit Maske, Schema-Beschreibung wortgleich.
- ElementGDLParameterCommands.cpp: Sperre lässt Door/Window durch (Schreibpfad hatte openingBase schon), Meldung wortgleich.
- AddOnMain.cpp: Registrierungstext wortgleich.
- Check: Syntax mit Vorab-Headern OK (3 Dateien).
- Offen: CI, Live-Test an Tür/Fenster (A/B setzen, Öffnungsmaß zurücklesen), Gegenprobe gegen Romans Bundle-Strings nach CI-Build.
