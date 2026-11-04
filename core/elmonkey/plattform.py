"""Plattform-Klebe: der ELMonkey-Kern mit den Regeln der Schwarz Plattform.

Herkunft: PR #95 der Schwarz Plattform (backend/app/core/elmonkey.py). Dieses Modul enthält keinen
Plattform-Code, nur Feldnamen und Klebe-Logik. Das Plattform-Modul (`modell_stand`) wird als Objekt
`ms` hineingegeben (Duck-Typing): F_BESCHREIBUNG, STATUS, status_von, beruehrt, eigenschaften_patch,
falten, anwenden; optional F_GESCHOSS.

Einbau in der Plattform (~10 Zeilen):

    ms = modell_stand            # das Plattform-Modul core/modell_stand.py, als Objekt
    from elmonkey.plattform import Plattform

    _p = Plattform(ms)
    finden, kurz, passt = _p.finden, _p.kurz, _p.passt
    ops_ausrollen, vorpruefen, stand_nach = _p.ops_ausrollen, _p.vorpruefen, _p.stand_nach
    ruecklesen, blockiert, schluessel = _p.ruecklesen, _p.blockiert, _p.schluessel
    BLOCKIEREND, MIT_FINDEN, HOECHSTENS, ElmFehler = _p.BLOCKIEREND, _p.MIT_FINDEN, _p.HOECHSTENS, _p.ElmFehler
"""
from __future__ import annotations

from typing import Any, Optional

from . import kern
from .regeln import F_GESCHOSS, ElmFehler, Regeln


def regeln_aus(ms: Any) -> Regeln:
    """Regeln der Plattform aus ihrem Modul `ms`."""
    return Regeln(f_geschoss=getattr(ms, "F_GESCHOSS", F_GESCHOSS), f_beschreibung=ms.F_BESCHREIBUNG,
                  status_werte=dict(ms.STATUS), status_von=ms.status_von, beruehrt=ms.beruehrt,
                  eigenschaften_patch=ms.eigenschaften_patch)


def stand_nach(ms: Any, basis: list[dict], saetze: list[dict], basis_version: Any, *,
               als_aktiv: Optional[str] = None) -> dict[str, dict]:
    """Wirksamer Stand aus den gespeicherten Sätzen — `als_aktiv`: ein Vorschlag (id), so gefaltet,
    als wäre er freigegeben. So prüft die Rücklese auch Vorschläge, die erst in der App wirken."""
    if als_aktiv:
        saetze = [{**s, "status": "aktiv"} if s.get("id") == als_aktiv else s for s in saetze]
    ov = ms.falten(saetze, basis_version)
    return {e.get("guid"): e for e in ms.anwenden(basis, ov)}


class Plattform:
    BLOCKIEREND = kern.BLOCKIEREND
    MIT_FINDEN = kern.MIT_FINDEN
    HOECHSTENS = kern.HOECHSTENS
    ElmFehler = ElmFehler
    ruecklesen = staticmethod(kern.ruecklesen)
    blockiert = staticmethod(kern.blockiert)
    schluessel = staticmethod(kern.schluessel)

    def __init__(self, ms: Any):
        self.ms = ms
        self.regeln = regeln_aus(ms)

    def passt(self, e: dict, k: Any) -> bool:
        return kern.passt(e, k, self.regeln)

    def finden(self, elemente, kriterium) -> list[dict]:
        return kern.finden(elemente, kriterium, self.regeln)

    def kurz(self, e: dict, felder=()) -> dict:
        return kern.kurz(e, felder, self.regeln)

    def ops_ausrollen(self, ops, wirksam, alle, hoechstens: int = kern.HOECHSTENS):
        return kern.ops_ausrollen(ops, wirksam, alle, hoechstens, self.regeln)

    def vorpruefen(self, ops, wirksam, alle, *, skizzen=(), offene_vorschlaege=()):
        return kern.vorpruefen(ops, wirksam, alle, skizzen=skizzen, offene_vorschlaege=offene_vorschlaege,
                               regeln=self.regeln)

    def stand_nach(self, basis, saetze, basis_version, *, als_aktiv=None):
        return stand_nach(self.ms, basis, saetze, basis_version, als_aktiv=als_aktiv)
