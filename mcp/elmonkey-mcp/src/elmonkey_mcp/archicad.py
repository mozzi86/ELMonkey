"""Kompatibilität: alter Modulpfad elmonkey_mcp.archicad → Add-on Archicad (gleiches Modulobjekt)."""
import sys

from .addons.archicad import api as _modul

sys.modules[__name__] = _modul
