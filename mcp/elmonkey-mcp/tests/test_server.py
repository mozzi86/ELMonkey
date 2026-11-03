"""Server mit Add-ons, ohne Archicad: Werkzeugliste und die Kern-Suche."""
import asyncio
import json

from elmonkey_mcp import server


def _werkzeuge():
    return {t.name for t in asyncio.run(server.mcp.list_tools())}


def test_standard_addons_registriert():
    assert {"discovery_list_active_archicads", "archicad_discover_tools", "archicad_call_tool",
            "elmonkey_finden"} <= _werkzeuge()


def test_alte_modulpfade_bleiben():
    from elmonkey_mcp import archicad, catalog
    from elmonkey_mcp.addons.archicad import api
    from elmonkey_mcp.addons.archicad import catalog as neu
    assert catalog is neu and archicad is api


def test_elmonkey_finden_mit_archicad_feldnamen():
    bauteile = [{"guid": "A", "type": "Wall", "name": "AW", "properties": {"Geschoss": "1. OG", "Renovierungsstatus": "Abbruch"}},
                {"guid": "B", "type": "Wall", "name": "IW", "properties": {"Geschoss": "EG", "Renovierungsstatus": "Bestand"}}]
    roh = asyncio.run(server.mcp.call_tool("elmonkey_finden", {
        "bauteile": bauteile, "kriterium": {"geschoss": "OG", "status": "Abbruch"},
        "feld_geschoss": "Geschoss", "felder_status": ["Renovierungsstatus"]}))
    inhalt = roh[0] if isinstance(roh, tuple) else roh
    erg = json.loads(inhalt[0].text)
    assert erg["anzahl"] == 1 and erg["guids"] == ["A"] and erg["bauteile"][0]["status"] == "Abbruch"
