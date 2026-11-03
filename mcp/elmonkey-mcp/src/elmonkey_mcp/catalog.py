"""Kompatibilität: alter Modulpfad elmonkey_mcp.catalog → Add-on Archicad (gleiches Modulobjekt)."""
import sys

from .addons.archicad import catalog as _modul

sys.modules[__name__] = _modul
