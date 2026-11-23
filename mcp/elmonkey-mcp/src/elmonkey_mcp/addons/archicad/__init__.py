"""Add-on Archicad: drei Werkzeuge, namensgleich mit tapir-archicad-mcp (der Archicad-Skill bleibt kompatibel)."""
from __future__ import annotations

from typing import Any

from . import catalog
from .api import call, list_instances

NAME = "archicad"


def _default_port() -> int:
    inst = [i for i in list_instances() if i.get("status") == "ok"]
    if len(inst) == 1:
        return inst[0]["port"]
    if not inst:
        raise RuntimeError("Kein Archicad mit offenem Projekt gefunden (Ports 19723-19744).")
    raise RuntimeError("Mehrere Archicad-Instanzen offen - bitte 'port' angeben: "
                       + ", ".join(f"{i['port']} ({i.get('projectName')})" for i in inst))


def register(mcp) -> None:
    @mcp.tool(name="discovery_list_active_archicads", title="List Active Archicad Instances",
              description="Listet laufende Archicad-Instanzen (Port, Projekt, Teamwork, Archicad-Version, ELMonkey-Version). "
                          "Der 'port' adressiert alle weiteren Befehle.")
    def discovery_list_active_archicads() -> list[dict]:
        return list_instances()

    @mcp.tool(name="archicad_discover_tools", title="Discover Archicad API Tools",
              description="Sucht passende Archicad-Befehle (ELMonkey: TapirCommand + ELM_SAB, dazu wichtige Graphisoft-API-Befehle) "
                          "per Stichwort, auch deutsch (z. B. 'Schraffur anlegen', 'Ebene von Elementen setzen'). Liefert Name "
                          "('Namensraum.Befehl'), Beschreibung und Eingabeschema fuer archicad_call_tool.")
    def archicad_discover_tools(query: str, port: int | None = None, max_results: int = 8) -> dict:
        p = port or _default_port()
        entries = catalog.load(p)
        hits = catalog.search(entries, query, max_results)
        out: dict[str, Any] = {"port": p, "results": [
            {"name": e["name"], "description": e["description"], "version": e["version"], "inputSchema": e["inputSchema"]}
            for e in hits]}
        n = catalog.note_for(p)
        if n:
            out["note"] = n
        return out

    @mcp.tool(name="archicad_call_tool", title="Execute Archicad API Command",
              description="Fuehrt einen Archicad-Befehl aus. 'name' = 'Namensraum.Befehl' aus archicad_discover_tools "
                          "(z. B. 'ELM_SAB.Create2DElements', 'TapirCommand.GetStories', 'API.GetElementsByType'); alte "
                          "tapir-archicad-mcp-Namen werden aufgeloest. 'arguments' = {'port': <Port>, 'params': {...}} - "
                          "Parameter duerfen auch direkt neben 'port' stehen. Fehler kommen als Text mit Archicad-Code zurueck.")
    def archicad_call_tool(name: str, arguments: dict | None = None) -> Any:
        args = dict(arguments or {})
        port = args.pop("port", None) or _default_port()
        args.pop("page_token", None)
        timeout = float(args.pop("timeout", 600))
        params = args.pop("params", None)
        if params is None:
            params = args
        ns, cmd = catalog.resolve(catalog.load(port), name)
        return call(int(port), ns, cmd, params, timeout=timeout)

    @mcp.tool(name="elmonkey_diff_plattform", title="ELMonkey: Diff-Checker Archicad ↔ Plattform",
              description="Vergleicht das offene Archicad-Modell mit dem Stand der Plattform, ohne IFC-Export. stand = "
                          "stand_link (https) aus archicad_abgleich_lesen oder Pfad zu stand.json; basis optional (sonst "
                          "die im Stand); plattform_geometrie optional (JSON guid→Box oder IFC der Plattform), wenn der "
                          "Stand keine Boxen traegt; gebaut = [{plattformGuid, archicadGuid}]; versatz [dx,dy,dz] m. "
                          "Liest Archicad nur (ELM_SAB.GetDiffDataELM, ab ELMonkey 0.11.13) und schreibt ergebnis.json "
                          "(Format sab-archicad-abgleich/1 + diff: neu/weg/Eigenschaften/Geometrie je Seite, Toleranz "
                          "3 mm) in den Arbeitsordner. netze=true: zusaetzlich die Koerper der neuen und in Archicad "
                          "geaenderten Bauteile (TapirCommand.Get3DGeometryOfElements, trianguliert) als "
                          "archicad_netze.json.gz (sab-archicad-netze/1) — ersetzt den Gesamt-IFC-Export. Nur mit "
                          "netze=true und netze_ebenen_einblenden (Vorgabe true) blendet es die ausgeblendeten Ebenen "
                          "der gewaehlten Bauteile kurz ein und danach genau diese wieder aus (Ruecklese, "
                          "Teamwork: nur ohne fremde Reservierung der Ebenen-Einstellungen); sonst nur lesend. "
                          "Antwort: Pfad, Zahlen, Beispiele, Hinweise.")
    def elmonkey_diff_plattform(stand: str, basis: str | None = None, plattform_geometrie: str | None = None,
                                gebaut: list[dict] | None = None, ordner: str | None = None,
                                versatz: list[float] | None = None, toleranz: float = 0.003,
                                netze: bool = False, netze_ebenen_einblenden: bool = True,
                                port: int | None = None) -> dict:
        from .diff import diff_lauf
        return diff_lauf(int(port or _default_port()), stand, basis=basis, plattform_geometrie=plattform_geometrie,
                         gebaut=gebaut, ordner=ordner, versatz=versatz, toleranz=toleranz, netze=netze,
                         netze_ebenen_einblenden=netze_ebenen_einblenden)
