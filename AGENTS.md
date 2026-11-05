# ELMonkey – Einstieg für KIs (Claude, ChatGPT, Cursor, Codex, …)

Du arbeitest mit Bauwerksmodellen eines Architekturbüros: in **Archicad** (über das ELMonkey-Add-On
und seine JSON-API) oder in der **Schwarz Plattform** (Grundmodell + Änderungsschicht).
Diese Regeln gelten überall gleich.

## Regeln

1. **Erst finden, dann handeln.** Ziele über Kriterien bestimmen (`typ`, `geschoss`, `name`, `status`,
   `eigenschaft`/`wert`, `und`/`oder`/`nicht`), nicht über geratene GUIDs.
2. **Vorprüfung ernst nehmen.** Meldet sie `NOT_FOUND`, `GELOESCHT`, `UNGUELTIG` oder `ZU_VIELE`, wird
   nichts geschrieben — der Fehler geht an den Menschen, nicht in einen Umweg.
3. **Ein `success` ist kein Beleg.** Nach jedem Schreiben frisch lesen und je Bauteil vergleichen.
   Erst `verifiziert: true` heißt erledigt.
4. **Bestand nur mit Zustimmung ändern.** Anlegen und Lesen sind frei; Ändern und Löschen brauchen eine
   Übersicht der betroffenen Bauteile und ein ausdrückliches Ja. Kein Ändern als Löschen + Neuanlegen.
5. **„0 Treffer“ ist erst ein Befund, wenn die Suche an einem bekannten Treffer gegengeprüft wurde.**
6. **Was du erzeugst, kennzeichnest du** (Archicad: Eigenschaft „KI generiert <Datum>“ + GUID-Register).

## Wo was liegt

- `core/` – Kern `elmonkey` (Python, nur Standardbibliothek). Neutrales Bauteil:
  `{guid, type, name, properties{…}, ausgeblendet?, geaendert?, plattform?, vertices?}`.
  `elmonkey.plattform` ist die Klebe-Logik für die Schwarz Plattform.
  `elmonkey.katalog` = Bauteilkatalog-Maschine (Typen aus Schichten, Bauplan/Ständerwerk, Bearbeiten mit Versionen,
  IFC-Bibliotheken über `elmonkey.katalog.ifc`, braucht das Extra `[ifc]` = ifcopenshell). Baustoffe/Typen bringt der
  Aufrufer als Ordner mit — die SAB-Bürodaten liegen in der Plattform, nie in diesem öffentlichen Repo;
  Profile/Verstärkungen (herstellerneutral) liegen als Paketdaten bei.
- `mcp/elmonkey-mcp` – MCP-Server (Python-Monkey).
- `archicad-addon/ELM_SAB_Tapir` + `installer` – C++-Add-On (Archicad-Monkey), gebaut von der CI.
- `skills/archicad` – Archicad-Skill, wird nach `mozzi86/-archicad` gespiegelt.
- `clients/` – MCP-Konfigurationen je Client. `plugin/` – Claude-Plugin (⏳).
- Schwarz Plattform: bindet den Kern als festes Release ein und gibt ihr Modul an `elmonkey.plattform`.
