"""Regeln: was ein Kern über das jeweilige Modell wissen muss, und nur das.

Der Kern (finden, vorpruefen, ruecklesen) arbeitet auf neutralen Bauteilen:

    {"guid": "…", "type": "IfcWall", "name": "…", "properties": {"Pset.Feld": "Wert", …},
     "ausgeblendet": bool?, "geaendert": bool?, "plattform": bool?, "vertices": […]?}

Was sich je Quelle unterscheidet — wo der Umbau-Status steht, wie ein offener Vorschlag seine
Bauteile nennt, wie `eigenschaften_setzen` in einen Patch übersetzt wird — steckt in `Regeln`.
`STANDARD` ist neutral; Archicad- und Plattform-Add-on geben ihre eigenen Regeln hinein.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Iterable, Optional


class ElmFehler(ValueError):
    pass


STATUS_WERTE = {"bestand": "Bestand", "neubau": "Neubau", "neu": "Neubau", "abbruch": "Abbruch",
                "rückbau": "Abbruch", "temporär": "Temporär (Baustelle)", "temporaer": "Temporär (Baustelle)",
                "temporär (baustelle)": "Temporär (Baustelle)"}

F_GESCHOSS = "Verortung.Geschoss"
F_BESCHREIBUNG = "IFC.Beschreibung"
F_NAME = "IFC.Name"
F_STATUS = "Status"
STATUS_FELDER = (F_STATUS, "Renovierungsstatus", "SAB_Planung.Status")


def _status_aus(props: dict, felder: Iterable[str]) -> Optional[str]:
    for f in felder:
        w = props.get(f)
        if w not in (None, ""):
            return STATUS_WERTE.get(str(w).strip().lower(), str(w))
    return None


def _beruehrt_neutral(satz: dict) -> set[str]:
    """Ein offener Vorschlag nennt seine Bauteile neutral als `guids`."""
    roh = satz.get("guids") or []
    return {str(g) for g in ([roh] if isinstance(roh, str) else roh)}


def _patch_neutral(regeln: "Regeln", op: dict, e: dict) -> tuple[dict, list[dict]]:
    """eigenschaften_setzen → (Patch, Vorher/Nachher-Zeilen). Felder: name, beschreibung, status,
    eigenschaften {Feld: Wert}, psets {Pset: {Feld: Wert}}. Leerer Wert = Eigenschaft entfernen."""
    props = e.get("properties") or {}
    patch: dict[str, Any] = {"properties": {}}
    diff: list[dict] = []

    def prop(schluessel: str, wert: Any) -> None:
        wert = None if wert in (None, "") else str(wert)
        if props.get(schluessel) != wert:
            patch["properties"][schluessel] = wert
            diff.append({"feld": schluessel, "vorher": props.get(schluessel), "nachher": wert})

    if "name" in op:
        name = str(op["name"] or "").strip()
        if not name:
            raise ElmFehler("name darf nicht leer sein")
        if name != e.get("name"):
            patch["name"] = name
            diff.append({"feld": "name", "vorher": e.get("name"), "nachher": name})
        prop(regeln.f_name, name)
    if "beschreibung" in op:
        prop(regeln.f_beschreibung, op["beschreibung"])
    if "status" in op:
        s = regeln.status_werte.get(str(op["status"] or "").strip().lower())
        if not s:
            raise ElmFehler("status: Bestand | Neubau | Abbruch | Temporär")
        prop(regeln.status_felder[0], s)
    for f, w in (op.get("eigenschaften") or {}).items():
        prop(str(f), w)
    for pset, felder in (op.get("psets") or {}).items():
        if not isinstance(felder, dict):
            raise ElmFehler(f"psets.{pset}: Objekt {{Feld: Wert}}")
        for f, w in felder.items():
            prop(f"{pset}.{f}", w)
    if not patch["properties"]:
        del patch["properties"]
    return patch, diff


@dataclass
class Regeln:
    f_geschoss: str = F_GESCHOSS
    f_beschreibung: str = F_BESCHREIBUNG
    f_name: str = F_NAME
    status_felder: tuple = STATUS_FELDER
    status_werte: dict = field(default_factory=lambda: dict(STATUS_WERTE))
    # Austauschbar je Quelle; None = neutrale Fassung oben
    status_von: Optional[Callable[[dict], Optional[str]]] = None
    beruehrt: Optional[Callable[[dict], set]] = None
    eigenschaften_patch: Optional[Callable[[dict, dict], tuple]] = None

    def status(self, e: dict) -> Optional[str]:
        if self.status_von:
            return self.status_von(e)
        return _status_aus(e.get("properties") or {}, self.status_felder) or e.get("status")

    def guids_von(self, satz: dict) -> set:
        return (self.beruehrt or _beruehrt_neutral)(satz)

    def patch(self, op: dict, e: dict) -> tuple[dict, list[dict]]:
        if self.eigenschaften_patch:
            return self.eigenschaften_patch(op, e)
        return _patch_neutral(self, op, e)


STANDARD = Regeln()
