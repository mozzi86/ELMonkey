"""Add-on Kern: die ELMonkey-Suche für Bauteillisten aus jeder Quelle (Archicad-Lesebefehle, IFC, Plattform).

Liest und schreibt nichts selbst — die KI holt die Bauteile, der Kern filtert sie mit derselben
Kriteriensprache wie überall (und/oder/nicht, typ, geschoss, name, status, eigenschaft/wert, guids).
"""
from __future__ import annotations

from typing import Any

import elmonkey as em

NAME = "kern"


def _regeln(a: dict) -> em.Regeln:
    r = em.Regeln()
    if a.get("feld_geschoss"):
        r.f_geschoss = str(a["feld_geschoss"])
    if a.get("felder_status"):
        f = a["felder_status"]
        r.status_felder = tuple([f] if isinstance(f, str) else f)
    return r


def register(mcp) -> None:
    @mcp.tool(name="elmonkey_finden", title="ELMonkey: Bauteile finden",
              description="Filtert eine Bauteilliste wie 'Suchen & aktivieren': kriterium mit und/oder/nicht beliebig tief, "
                          "typ, geschoss, name, beschreibung, status (Bestand|Neubau|Abbruch|Temporär), eigenschaft (+ wert: "
                          "Text oder {gleich|enthaelt|beginnt|regex|min|max}, Dezimalkomma geht; oder vorhanden), guids. "
                          "bauteile = [{guid, type, name, properties{Feld: Wert}}]. feld_geschoss / felder_status, wenn die "
                          "Quelle andere Feldnamen nutzt (Archicad z. B. 'Renovierungsstatus'). Liest und schreibt nichts.")
    def elmonkey_finden(bauteile: list[dict], kriterium: dict, felder: list[str] | None = None,
                        schluessel: bool = False, feld_geschoss: str | None = None,
                        felder_status: list[str] | None = None) -> dict[str, Any]:
        r = _regeln({"feld_geschoss": feld_geschoss, "felder_status": felder_status})
        treffer = em.finden(bauteile, kriterium, r)
        out: dict[str, Any] = {"anzahl": len(treffer), "guids": [e.get("guid") for e in treffer],
                               "bauteile": [em.kurz(e, felder or (), r) for e in treffer[:300]]}
        if schluessel:
            out["eigenschaften"] = em.schluessel(treffer)
        return out
