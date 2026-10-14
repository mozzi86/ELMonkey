"""ELMonkey-MCP: drei Werkzeuge, namensgleich mit tapir-archicad-mcp (Skill bleibt kompatibel)."""
from __future__ import annotations

from typing import Any

from mcp.server.fastmcp import FastMCP

from . import catalog
from .archicad import call, list_instances

mcp = FastMCP("archicad")


def _default_port() -> int:
    inst = [i for i in list_instances() if i.get("status") == "ok"]
    if len(inst) == 1:
        return inst[0]["port"]
    if not inst:
        raise RuntimeError("Kein Archicad mit offenem Projekt gefunden (Ports 19723-19744).")
    raise RuntimeError("Mehrere Archicad-Instanzen offen - bitte 'port' angeben: "
                       + ", ".join(f"{i['port']} ({i.get('projectName')})" for i in inst))


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


def main() -> None:
    mcp.run(transport="stdio")


if __name__ == "__main__":
    main()
