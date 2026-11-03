"""ELMonkey-MCP: ein Server, Add-ons je Quelle. Standard: Archicad + Kern.

ELMONKEY_ADDONS=archicad,kern wählt die Add-ons (Komma-Liste). Der Servername bleibt „archicad“, damit
bestehende Einrichtungen (mcp__archicad__*) und der Archicad-Skill unverändert weiterlaufen.
"""
from __future__ import annotations

import os

from mcp.server.fastmcp import FastMCP

from .addons import archicad as _archicad
from .addons import kern as _kern

ADDONS = {m.NAME: m for m in (_archicad, _kern)}

mcp = FastMCP("archicad")


def aktive_addons() -> list[str]:
    roh = os.environ.get("ELMONKEY_ADDONS", "archicad,kern")
    namen = [n.strip() for n in roh.split(",") if n.strip()]
    falsch = [n for n in namen if n not in ADDONS]
    if falsch:
        raise RuntimeError(f"ELMONKEY_ADDONS: unbekannt {falsch} — verfügbar: {', '.join(ADDONS)}")
    return namen


for _n in aktive_addons():
    ADDONS[_n].register(mcp)


def main() -> None:
    mcp.run(transport="stdio")


if __name__ == "__main__":
    main()
