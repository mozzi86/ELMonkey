---
status: complete
completed: 2026-09-03
slug: element-preview-render
---

# Summary — Element-Vorschau rendern + Renderer-Hänger

30 Zeilen in 3 bestehenden Dateien, keine neuen Dateien.

| Datei | Was |
|---|---|
| `recipes/library-objects.md` | neuer Abschnitt „Vorschaubild eines Elements per API rendern" (Tapir `GetElementPreviewImage`, 3D/512 px, Base64-PNG; 2D unbrauchbar; Positivfall zuerst) |
| `reference/mcp-conventions.md` | Unterabschnitt „Renderer-Hänger" (Pflanze_Mittel_1800 ~7 min, Abbruch beim ersten Hänger, Wächter statt Retry, Skip-Liste); Absatz „Port → Prozess zuordnen" (`lsof`, Fehldiagnose THN statt Möbel, drei Instanzen) |
| `reference/schedule-pipeline.md` | Ausweg-Hinweis am Gotcha „Property leer, GDL-Parameter befüllt" |

## Belege
- 18 von 24 bildlosen Möbelpositionen live gerendert (Port 19724, AC29 Teamwork, ELM_SAB 0.9.14), 0,1–30 s je Objekt.
- Hänger: PID 14543 bei 95–120 % CPU, Zustand R, API taub; Rückkehr nach ~7 min per Wächter erfasst.
- Port→PID via `lsof`: 19723→9015, 19724→14543, 19725→14850.
