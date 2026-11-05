"""Bauteilkatalog-Maschine: Typen aus Schichten, Bauplan/Ständerwerk, Bearbeiten mit Versionen.

Nur Standardbibliothek. `elmonkey.katalog.ifc` (IFC-Bibliotheken) braucht das Extra `[ifc]` (ifcopenshell)
und wird hier bewusst NICHT importiert.
"""
from . import bauen, bearbeiten, modell, werkstoff
from .modell import Katalog, KatalogFehler, benennen, doppelt, fingerabdruck, pruefen, typ_guid

__all__ = ["Katalog", "KatalogFehler", "benennen", "doppelt", "fingerabdruck", "pruefen", "typ_guid",
           "bauen", "bearbeiten", "modell", "werkstoff"]
