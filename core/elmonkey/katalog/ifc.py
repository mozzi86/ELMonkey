"""Bauteilkatalog → IFC4 / IFC4X3 (ifcopenshell). Spec §6.

Drei Erzeugnisse:
- `materialbibliothek()` — IfcProject + IfcProjectLibrary, je Baustoff ein IfcMaterial mit Kategorie, Psets
  (Pset_MaterialCommon, Pset_MaterialThermal, <Präfix>_Material) und Darstellung (IfcSurfaceStyle mit Farbe
  und Textur-Verweis (`textur_url`), IfcFillAreaStyle mit ≤ 2 Schraffurscharen nach DIN).
  Hinweis: IfcMaterial ist keine IfcDefinitionSelect — Materialien lassen sich der Bibliothek nicht per
  IfcRelDeclares zuordnen; sie stehen in der Datei und werden von den Typen benutzt.
- `typbibliothek()` — dieselben Materialien + alle Katalogtypen, per IfcRelDeclares in der IfcProjectLibrary.
- `muster_projekt()` — Site/Building/Storey mit gebauten Bauteilen (Schichtwand als Extrusion mit
  IfcMaterialLayerSetUsage, Ständerwand zerlegt, Decke + Fußbodenaufbau, Tür und Fenster in Öffnungen).
"""
from __future__ import annotations

import math
import uuid
from typing import Any, Optional

import ifcopenshell
import ifcopenshell.api
import ifcopenshell.guid

from . import werkstoff as ws
from .bauen import Bauplan, Koerper
from .modell import Katalog, KatalogFehler, SCHICHT_KLASSEN, benennen, typ_guid

SCHEMATA = {"IFC4": "IFC4", "IFC4X3": "IFC4X3_ADD2"}
_NS = uuid.UUID("6f1c2a52-6d2b-4f0e-9a3e-5ab000000002")
COMMON = {"IfcWall": "Pset_WallCommon", "IfcSlab": "Pset_SlabCommon", "IfcCovering": "Pset_CoveringCommon",
          "IfcDoor": "Pset_DoorCommon", "IfcWindow": "Pset_WindowCommon", "IfcMember": "Pset_MemberCommon",
          "IfcPlate": "Pset_PlateCommon", "IfcBuildingElementPart": None}


def gid(*teile: str) -> str:
    return ifcopenshell.guid.compress(uuid.uuid5(_NS, "|".join(teile)).hex)


def _rgb(h: str) -> tuple[float, float, float]:
    h = (h or "#bfbfbf").lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))


class Schreiber:
    def __init__(self, k: Katalog, schema: str = "IFC4", name: str = "Bauteilkatalog", datei=None,
                 modell_kontext=None, body=None, pset_praefix: str = "Pset_Katalog",
                 textur_url: str = "urn:elmonkey:textur/"):
        """datei/modell_kontext/body: in eine bestehende Datei schreiben (Plattform-Export, core/modell_export)."""
        if schema.upper() not in SCHEMATA:
            raise ValueError("schema IFC4 oder IFC4X3")
        self.pset_praefix = pset_praefix
        self.textur_url = textur_url
        self.k = k
        self.schema = SCHEMATA[schema.upper()]
        r = ifcopenshell.api.run
        self.r = r
        self.materialien: dict[str, Any] = {}
        self.typen: dict[str, Any] = {}
        self.layersets: dict[str, Any] = {}
        self.klassifikationen: dict[str, Any] = {}
        self.bibliothek = None
        self.zaehler: dict[str, int] = {}
        if datei is not None:
            self.f, self.modell, self.body = datei, modell_kontext, body
            self.projekt = datei.by_type("IfcProject")[0]
            self.achse_ctx = r("context.add_context", self.f, context_type="Model", context_identifier="Axis",
                               target_view="GRAPH_VIEW", parent=self.modell)
            return
        self.f = ifcopenshell.file(schema=self.schema)
        self.projekt = r("root.create_entity", self.f, ifc_class="IfcProject", name=name)
        self.projekt.GlobalId = gid(name, "projekt")
        einheiten = [r("unit.add_si_unit", self.f, unit_type=t) for t in ("LENGTHUNIT", "AREAUNIT", "VOLUMEUNIT", "PLANEANGLEUNIT")]
        einheiten.append(r("unit.add_si_unit", self.f, unit_type="MASSUNIT", prefix="KILO"))
        r("unit.assign_unit", self.f, units=einheiten)
        self.modell = r("context.add_context", self.f, context_type="Model")
        self.body = r("context.add_context", self.f, context_type="Model", context_identifier="Body",
                      target_view="MODEL_VIEW", parent=self.modell)
        self.achse_ctx = r("context.add_context", self.f, context_type="Model", context_identifier="Axis",
                           target_view="GRAPH_VIEW", parent=self.modell)

    # ── Hilfen

    def _zahl(self, art: str, w):
        return self.f.create_entity(art, w)

    def _pset(self, ziel, name: str, werte: dict) -> None:
        werte = {k: v for k, v in werte.items() if v is not None and v != ""}
        if not werte:
            return
        p = self.r("pset.add_pset", self.f, product=ziel, name=name)
        self.r("pset.edit_pset", self.f, pset=p, properties=werte)

    def _zaehle(self, k: str) -> None:
        self.zaehler[k] = self.zaehler.get(k, 0) + 1

    # ── Klassifizierung (DIN 276, Graphisoft-Übergang, bSDD)

    def _system(self, schluessel: str):
        if schluessel not in self.klassifikationen:
            f = self.f
            if schluessel == "din276":
                c = f.createIfcClassification("DIN", "2018-12", None, "DIN 276 Kosten im Bauwesen", None, None, None)
            elif schluessel == "graphisoft":
                c = f.createIfcClassification("Graphisoft", "1.0", None, "Graphisoft Klassifizierung", "Übergang bis Archicad-Ausstieg 2026", None, None)
            else:
                c = f.createIfcClassification("buildingSMART", "4.3", None, "bSDD IFC 4.3",
                                              None, "https://identifier.buildingsmart.org/uri/buildingsmart/ifc/4.3", None)
            self.klassifikationen[schluessel] = c
        return self.klassifikationen[schluessel]

    def _klassifizieren(self, ziel, kl: dict) -> None:
        f = self.f
        refs = []
        if (d := kl.get("din276")) and d.get("kg"):
            refs.append(f.createIfcClassificationReference(None, d["kg"], f"KG {d['kg']}", self._system("din276"), None, None))
        if kl.get("graphisoft"):
            refs.append(f.createIfcClassificationReference(None, kl["graphisoft"], kl["graphisoft"], self._system("graphisoft"), None, None))
        if kl.get("bsdd_klasse"):
            refs.append(f.createIfcClassificationReference(kl["bsdd_klasse"], kl["bsdd_klasse"].rsplit("/", 1)[-1],
                                                           None, self._system("bsdd"), None, None))
        for ref in refs:
            f.createIfcRelAssociatesClassification(gid(ziel.GlobalId, ref.Identification or ""), None, None, None, [ziel], ref)

    # ── Material

    def _stile(self, b: dict):
        f = self.f
        r, g, bl = _rgb(b.get("farbe"))
        farbe = f.createIfcColourRgb(None, r, g, bl)
        styles = [f.createIfcSurfaceStyleShading(farbe, 0.0)]
        if b.get("textur"):
            quelle = b["textur"]
            url = (f"https://ambientcg.com/view?id={quelle.split(':', 1)[1]}" if quelle.startswith("ambientcg:")
                   else f"{self.textur_url}{quelle.replace(':', '/')}")
            tex = f.createIfcImageTexture(True, True, "MODULATE", None, None, url)
            styles.append(f.createIfcSurfaceStyleWithTextures([tex]))
        flaeche = f.createIfcSurfaceStyle(b["name"], "BOTH", styles)
        stile = [flaeche]
        sch = b.get("schraffur") or ws.schraffur(b["werkstoffklasse"])
        if sch["scharen"]:
            linien = []
            for s in sch["scharen"][:2]:
                font = f.createIfcDraughtingPreDefinedCurveFont("dashed") if s["gestrichelt"] else \
                    f.createIfcDraughtingPreDefinedCurveFont("continuous")
                cs = f.createIfcCurveStyle(None, font, f.createIfcPositiveLengthMeasure(0.00018), f.createIfcColourRgb(None, 0, 0, 0), False)
                linien.append(f.createIfcFillAreaStyleHatching(cs, f.createIfcPositiveLengthMeasure(s["abstand_m"]), None, None,
                                                               math.radians(s["winkel"])))
            stile.append(f.createIfcFillAreaStyle(f"DIN {ws.KLASSEN[sch['klasse']][0]}", linien, False))
        item = f.createIfcStyledItem(None, stile, None)
        rep = f.createIfcStyledRepresentation(self.modell, "Style", "Material", [item])
        return rep

    def material(self, schluessel: str):
        if schluessel in self.materialien:
            return self.materialien[schluessel]
        b = self.k.baustoff(schluessel)
        f = self.f
        m = f.createIfcMaterial(b["name"], ws.KLASSEN[b["werkstoffklasse"]][0], b["kategorie_ifc"])
        kw = b.get("kennwerte") or {}
        self._pset(m, "Pset_MaterialCommon", {"MassDensity": kw.get("dichte_kg_m3") or None})
        self._pset(m, "Pset_MaterialThermal", {"ThermalConductivity": kw.get("lambda_W_mK") or None,
                                               "SpecificHeatCapacity": kw.get("waermekapazitaet_J_kgK") or None})
        h = b.get("herkunft") or {}
        self._pset(m, f"{self.pset_praefix}_Material", {"Schluessel": b["schluessel"], "Werkstoffklasse": b["werkstoffklasse"],
                                            "Verschneidungsprioritaet": b.get("verschneidungsprioritaet"),
                                            "Textur": b.get("textur"), "Schraffur_DIN": ws.KLASSEN[b["werkstoffklasse"]][3],
                                            "Graue_Energie_MJ_kg": kw.get("graue_energie_MJ_kg"), "CO2_kg_kg": kw.get("co2_kg_kg"),
                                            "Archicad_Name": b["name"], "Archicad_Kategorie": h.get("archicad_kategorie"),
                                            "Archicad_Schraffur": h.get("archicad_schraffur"), "Quelle": h.get("vorlage")})
        f.createIfcMaterialDefinitionRepresentation(None, None, [self._stile(b)], m)
        self.materialien[schluessel] = m
        self._zaehle("IfcMaterial")
        return m

    def alle_materialien(self) -> None:
        for s in self.k.baustoffe:
            self.material(s)

    # ── Bibliothek

    def _bibliothek(self, name: str):
        if self.bibliothek is None:
            self.bibliothek = self.r("root.create_entity", self.f, ifc_class="IfcProjectLibrary", name=name)
            self.bibliothek.GlobalId = gid(name, "bibliothek")
            self.f.createIfcRelDeclares(gid(name, "declares-projekt"), None, None, None, self.projekt, [self.bibliothek])
        return self.bibliothek

    # ── Geometrie

    def _platz(self, xyz=(0.0, 0.0, 0.0), winkel: float = 0.0, z_ab: bool = False, rel=None):
        f = self.f
        ax = f.createIfcAxis2Placement3D(f.createIfcCartesianPoint(tuple(map(float, xyz))),
                                         f.createIfcDirection((0.0, 0.0, 1.0)),
                                         f.createIfcDirection((math.cos(winkel), math.sin(winkel), 0.0)))
        return f.createIfcLocalPlacement(rel, ax)

    def _quader(self, x, y, z, richtung=(0.0, 0.0, 1.0)):
        """Extrusion eines Rechtecks [x0,x1]×[y0,y1] von z0 um (z1-z0) — in lokalen Koordinaten."""
        f = self.f
        dx, dy, dz = x[1] - x[0], y[1] - y[0], z[1] - z[0]
        prof = f.createIfcRectangleProfileDef("AREA", None,
                                              f.createIfcAxis2Placement2D(f.createIfcCartesianPoint((x[0] + dx / 2, y[0] + dy / 2)), None),
                                              dx, dy)
        pos = f.createIfcAxis2Placement3D(f.createIfcCartesianPoint((0.0, 0.0, float(z[0]))), None, None)
        return f.createIfcExtrudedAreaSolid(prof, pos, f.createIfcDirection(richtung), dz)

    def _form(self, items, kontext=None, art="SweptSolid"):
        return self.f.createIfcShapeRepresentation(kontext or self.body, "Body", art, items)

    # ── Typen

    def _layerset(self, t: dict):
        s = f"{t['schluessel']}@{t.get('version', 1)}"
        if s in self.layersets:
            return self.layersets[s]
        f = self.f
        lagen = []
        for sch in t["schichten"]:
            b = self.k.baustoff(sch["baustoff"])
            m = None if b["werkstoffklasse"] == "luft" else self.material(sch["baustoff"])
            vent = sch.get("belueftet")
            lage = f.createIfcMaterialLayer(m, float(sch["dicke_m"]),
                                            True if vent else (None if b["werkstoffklasse"] != "luft" else "UNKNOWN"),
                                            b["name"] if b["werkstoffklasse"] != "luft" else "Luftschicht",
                                            None, sch.get("rolle"), None)
            if sch.get("staenderwerk"):
                sw = sch["staenderwerk"]
                lage.Description = f"Ständerwerk {sw['profil']} e={sw.get('achsabstand_m', 0.625)} m, Füllung {b['name']}"
            lagen.append(lage)
        ls = f.createIfcMaterialLayerSet(lagen, t["kurzname"][:255], t["langname"][:255])
        self.layersets[s] = ls
        return ls

    def typ(self, t_roh: dict):
        t = benennen(t_roh, self.k)
        s = f"{t['schluessel']}@{t.get('version', 1)}"
        if s in self.typen:
            return self.typen[s]
        f = self.f
        kl = t["ifc"]["klasse"]
        tt = f.create_entity(kl + "Type", GlobalId=typ_guid(t), Name=t["kurzname"][:255], Description=t["langname"][:255],
                             PredefinedType=t["ifc"]["predefined_type"])
        if t["ifc"]["predefined_type"] == "USERDEFINED":
            tt.ElementType = t["ifc"]["object_type"]
        if kl in SCHICHT_KLASSEN:
            ls = self._layerset(t)
            f.createIfcRelAssociatesMaterial(gid(s, "mat"), None, None, None, [tt], ls)
            sw = next((x["staenderwerk"] for x in t["schichten"] if x.get("staenderwerk")), None)
            if sw:
                self._pset(tt, f"{self.pset_praefix}_Staenderwerk", {"Profil": sw["profil"], "Achsabstand": float(sw.get("achsabstand_m", 0.625)),
                                                         "UW_Anschluss": bool(sw.get("uw_anschluss", True)),
                                                         "Querriegel_Hoehen": ", ".join(str(q["hoehe_m"]) for q in sw.get("querriegel") or []) or None,
                                                         "Norm": sw.get("norm", "DIN 18183-1"),
                                                         "Verstaerkungsarten": ", ".join(self.k.verstaerkungen)})
        else:
            self._oeffnungstyp(tt, t)
        pc = {k: v for k, v in (t.get("pset_common") or {}).items() if v is not None}
        pc["Reference"] = t["kurzname"][:255]
        if COMMON.get(kl):
            self._pset(tt, COMMON[kl], pc)
        self._pset(tt, f"{self.pset_praefix}_Katalog", {"Schluessel": t["schluessel"], "Version": int(t.get("version", 1)),
                                            "Kurzname": t["kurzname"], "Langname": t["langname"], "Fingerabdruck": t["fingerabdruck"],
                                            "Rohling": bool(t.get("rohling")), "Ebene": t.get("ebene"), "Basiert_auf": t.get("basiert_auf"),
                                            "GDL_Referenz": (t.get("gdl_referenz") or {}).get("objekt"),
                                            "Quelle": (t.get("herkunft") or {}).get("vorlage")})
        self._klassifizieren(tt, t.get("klassifizierung") or {})
        self.typen[s] = tt
        self._zaehle(kl + "Type")
        return tt

    def _oeffnungstyp(self, tt, t: dict) -> None:
        f = self.f
        z = t.get("zarge") or {}
        if t["ifc"]["klasse"] == "IfcDoor":
            tt.OperationType = t.get("oeffnungsart", "SINGLE_SWING_LEFT")
            bl = t.get("blatt") or {}
            lining = f.createIfcDoorLiningProperties(gid(t["schluessel"], "lining"), None, "Zarge", z.get("art"),
                                                     z.get("tiefe_m"), z.get("spiegel_m"), None, None, None, None, None,
                                                     None, None, None, None, None, None)
            panel = f.createIfcDoorPanelProperties(gid(t["schluessel"], "panel"), None, "Türblatt", None,
                                                   bl.get("dicke_m"), "SWINGING", 1.0, "LEFT", None)
            tt.HasPropertySets = [lining, panel]
            mats = [m for m in (z.get("baustoff"), bl.get("baustoff")) if m]
        else:
            tt.PartitioningType = t.get("oeffnungsart", "SINGLE_PANEL")
            lining = f.createIfcWindowLiningProperties(gid(t["schluessel"], "lining"), None, "Blendrahmen", z.get("art"),
                                                       z.get("tiefe_m"), z.get("spiegel_m"), None, None, None, None, None,
                                                       None, None, None, None, None)
            fl = t.get("fluegel") or {}
            panel = f.createIfcWindowPanelProperties(gid(t["schluessel"], "panel"), None, "Flügel", None,
                                                     "SIDEHUNGLEFTHAND", "MIDDLE", fl.get("tiefe_m"), fl.get("breite_rahmen_m"), None)
            tt.HasPropertySets = [lining, panel]
            mats = [m for m in (z.get("baustoff"), (t.get("verglasung") or {}).get("baustoff")) if m]
        if mats:
            cs = f.createIfcMaterialConstituentSet(t.get("kurzname_fest") or t["schluessel"], None,
                                                   [f.createIfcMaterialConstituent(n, None, self.material(m), None, None)
                                                    for n, m in zip(("Rahmen", "Blatt/Verglasung"), mats)])
            f.createIfcRelAssociatesMaterial(gid(t["schluessel"], "mat"), None, None, None, [tt], cs)
        # Typgeometrie: Zarge + Blatt/Glas (erzeugt, Spec §2 Nr. 8)
        items = [self._quader(*q) for q in oeffnung_quader(t)]
        rmap = f.createIfcRepresentationMap(f.createIfcAxis2Placement3D(f.createIfcCartesianPoint((0.0, 0.0, 0.0)), None, None),
                                            self._form(items))
        tt.RepresentationMaps = [rmap]

    def alle_typen(self, bibliothek: str = "Bauteiltypen") -> None:
        lib = self._bibliothek(bibliothek)
        tts = [self.typ(t) for t in self.k.typen.values()]
        self.f.createIfcRelDeclares(gid(bibliothek, "declares-typen"), None, None, None, lib, tts)

    # ── Bauteile (Muster-Projekt)

    def struktur(self, geschoss: str = "EG"):
        r = self.r
        site = r("root.create_entity", self.f, ifc_class="IfcSite", name="Grundstück")
        geb = r("root.create_entity", self.f, ifc_class="IfcBuilding", name="Muster")
        gs = r("root.create_entity", self.f, ifc_class="IfcBuildingStorey", name=geschoss)
        for o, n in ((site, "site"), (geb, "geb"), (gs, "gs")):
            o.GlobalId = gid("muster", n)
        r("aggregate.assign_object", self.f, relating_object=self.projekt, products=[site])
        r("aggregate.assign_object", self.f, relating_object=site, products=[geb])
        r("aggregate.assign_object", self.f, relating_object=geb, products=[gs])
        pl = self._platz()
        site.ObjectPlacement = pl
        geb.ObjectPlacement = self._platz(rel=pl)
        gs.ObjectPlacement = self._platz(rel=geb.ObjectPlacement)
        gs.Elevation = 0.0
        self.geschoss = gs
        self._inhalt: list = []
        return gs

    def _typ_zuweisen(self, e, guid: str, typ) -> None:
        """Ein Typ hat höchstens EINE IfcRelDefinesByType (SET [0:1]) — weitere Bauteile hängen sich dort an."""
        rel = next(iter(typ.Types or ()), None)
        if rel is None:
            self.f.createIfcRelDefinesByType(gid(guid, "typ"), None, None, None, [e], typ)
        else:
            rel.RelatedObjects = list(rel.RelatedObjects) + [e]

    def _bauteil(self, klasse: str, name: str, guid: str, platz, typ=None, pt: Optional[str] = None, ot: Optional[str] = None):
        e = self.f.create_entity(klasse, GlobalId=guid, Name=name[:255], ObjectPlacement=platz)
        if typ is not None:
            self._typ_zuweisen(e, guid, typ)
        elif pt and hasattr(e, "PredefinedType"):
            e.PredefinedType = pt
            if pt == "USERDEFINED" and ot:
                e.ObjectType = ot
        self._zaehle(klasse)
        return e

    def wand(self, plan: Bauplan, bauteil_id: str, von, winkel: float, z0: float = 0.0, status: str = "NEW"):
        t = benennen(plan.typ, self.k)
        guid = gid("bauteil", bauteil_id)
        platz = self._platz((von[0], von[1], z0), winkel, rel=self.geschoss.ObjectPlacement)
        w = self._bauteil("IfcWall", t["kurzname"], guid, platz, self.typ(plan.typ))
        self._status(w, "Pset_WallCommon", status)
        self.wand_darstellen(w, plan, platz, typ_zuweisen=False)
        self._inhalt.append(w)
        return w

    def wand_darstellen(self, w, plan: Bauplan, platz, typ_zuweisen: bool = True) -> None:
        """Geometrie, Material und Typ einer (schon angelegten) Wand aus dem Bauplan — auch für den
        Plattform-Export (core/modell_export → core/bauteilkatalog/export_anbindung.py)."""
        t = benennen(plan.typ, self.k)
        f = self.f
        w.ObjectPlacement = platz
        if typ_zuweisen:
            self._typ_zuweisen(w, w.GlobalId, self.typ(plan.typ))
        achse = f.createIfcShapeRepresentation(self.achse_ctx, "Axis", "Curve2D",
                                               [f.createIfcPolyline([f.createIfcCartesianPoint((0.0, 0.0)),
                                                                     f.createIfcCartesianPoint((float(plan.laenge), 0.0))])])
        self._pset(w, f"{self.pset_praefix}_Katalog", {"Schluessel": t["schluessel"], "Version": int(t.get("version", 1))})
        if plan.art == "schichten":
            w.Representation = f.createIfcProductDefinitionShape(None, None, [achse, self._form([self._quader(*_k(plan.koerper[0]))])])
            usage = f.createIfcMaterialLayerSetUsage(self._layerset(t), "AXIS2", "POSITIVE", 0.0, None)
            f.createIfcRelAssociatesMaterial(gid(w.GlobalId, "usage"), None, None, None, [w], usage)
            if plan.staenderwerk:
                self._pset(w, f"{self.pset_praefix}_Staenderwerk_Bauteil", {"Ausgabe": "Schichtwand (Option für Archicad-Partner)",
                                                                **plan.staenderwerk})
            return
        # Element-Decomposition: Wand ohne eigenen Körper, Teile tragen Geometrie und Material
        w.Representation = f.createIfcProductDefinitionShape(None, None, [achse])
        kinder = []
        for i, k in enumerate(plan.koerper):
            kg = gid(w.GlobalId, "teil", str(i), k.name)
            e = self._bauteil(k.klasse, k.name, kg, self._platz(rel=platz), None, k.predefined_type, k.object_type)
            e.Representation = f.createIfcProductDefinitionShape(None, None, [self._form([self._quader(*_k(k))])])
            if k.baustoff:
                f.createIfcRelAssociatesMaterial(gid(kg, "mat"), None, None, None, [e], self.material(k.baustoff))
            if k.eigenschaften:
                nm = f"{self.pset_praefix}_Wandverstaerkung" if k.object_type == "Wandverstärkung" else f"{self.pset_praefix}_Teil"
                self._pset(e, nm, {a: (str(v) if not isinstance(v, (int, float, bool)) else v)
                                   for a, v in k.eigenschaften.items()})
            kinder.append(e)
        f.createIfcRelAggregates(gid(w.GlobalId, "agg"), None, None, None, w, kinder)

    def _status(self, e, pset: str, status: str) -> None:
        f = self.f
        enum = f.createIfcPropertyEnumeration("PEnum_ElementStatus", [f.createIfcLabel(x) for x in
                                              ("NEW", "EXISTING", "DEMOLISH", "TEMPORARY", "OTHER", "NOTKNOWN", "UNSET")], None)
        prop = f.createIfcPropertyEnumeratedValue("Status", None, [f.createIfcLabel(status)], enum)
        vorhanden = [r.RelatingPropertyDefinition for r in getattr(e, "IsDefinedBy", []) or []
                     if r.is_a("IfcRelDefinesByProperties") and r.RelatingPropertyDefinition.Name == pset]
        if vorhanden:
            vorhanden[0].HasProperties = list(vorhanden[0].HasProperties) + [prop]
        else:
            ps = f.createIfcPropertySet(gid(e.GlobalId, pset), None, pset, None, [prop])
            f.createIfcRelDefinesByProperties(gid(e.GlobalId, pset, "rel"), None, None, None, [e], ps)

    def flaeche(self, typ_ref: str, bauteil_id: str, x, y, oberkante: float, status: str = "NEW"):
        """Decke/Belag als Rechteck, Schichten von OK nach unten (AXIS3, NEGATIVE)."""
        t = benennen(self.k.typ(typ_ref), self.k)
        typ = self.typ(self.k.typ(typ_ref))
        D = sum(s["dicke_m"] for s in t["schichten"])
        guid = gid("bauteil", bauteil_id)
        kl = t["ifc"]["klasse"]
        platz = self._platz((0.0, 0.0, oberkante), rel=self.geschoss.ObjectPlacement)
        e = self._bauteil(kl, t["kurzname"], guid, platz, typ)
        e.Representation = self.f.createIfcProductDefinitionShape(None, None, [self._form([self._quader(x, y, (0.0, D), (0.0, 0.0, -1.0))])])
        usage = self.f.createIfcMaterialLayerSetUsage(self._layerset(t), "AXIS3", "NEGATIVE", 0.0, None)
        self.f.createIfcRelAssociatesMaterial(gid(guid, "usage"), None, None, None, [e], usage)
        self._status(e, COMMON[kl], status)
        self._pset(e, f"{self.pset_praefix}_Katalog", {"Schluessel": t["schluessel"], "Version": int(t.get("version", 1))})
        self._inhalt.append(e)
        return e

    def oeffnung(self, wand, plan: Bauplan, typ_ref: str, bauteil_id: str, abstand: float, bruestung: float = 0.0,
                 status: str = "NEW"):
        """Tür/Fenster in eine Wand: IfcOpeningElement (voids) + Füllung (fills), Geometrie vom Typ (MappedItem)."""
        f = self.f
        t = benennen(self.k.typ(typ_ref), self.k)
        typ = self.typ(self.k.typ(typ_ref))
        B, H, D = t["breite_m"], t["hoehe_m"], plan.dicke
        if abstand < 0 or abstand + B > plan.laenge:
            raise ValueError(f"{bauteil_id}: Öffnung passt nicht in die Wand")
        guid = gid("bauteil", bauteil_id)
        op = self._bauteil("IfcOpeningElement", f"Öffnung {t['kurzname']}", gid(guid, "op"),
                           self._platz((abstand, 0.0, bruestung), rel=wand.ObjectPlacement), None, "OPENING")
        op.Representation = f.createIfcProductDefinitionShape(None, None, [self._form([self._quader((0.0, B), (-0.01, D + 0.01), (0.0, H))])])
        f.createIfcRelVoidsElement(gid(guid, "voids"), None, None, None, wand, op)
        kl = t["ifc"]["klasse"]
        e = self._bauteil(kl, t["kurzname"], guid, self._platz(rel=op.ObjectPlacement), typ)
        e.OverallWidth, e.OverallHeight = float(B), float(H)
        tiefe = (t.get("zarge") or {}).get("tiefe_m", D)
        ziel = f.createIfcCartesianTransformationOperator3D(None, None, f.createIfcCartesianPoint((0.0, (D - tiefe) / 2, 0.0)), None, None)
        e.Representation = f.createIfcProductDefinitionShape(None, None, [
            f.createIfcShapeRepresentation(self.body, "Body", "MappedRepresentation", [f.createIfcMappedItem(typ.RepresentationMaps[0], ziel)])])
        f.createIfcRelFillsElement(gid(guid, "fills"), None, None, None, op, e)
        self._status(e, COMMON[kl], status)
        self._pset(e, f"{self.pset_praefix}_Katalog", {"Schluessel": t["schluessel"], "Version": int(t.get("version", 1))})
        self._inhalt.append(e)
        return e

    def fertig(self) -> ifcopenshell.file:
        if getattr(self, "_inhalt", None):
            self.f.createIfcRelContainedInSpatialStructure(gid("muster", "enthalten"), None, None, None, self._inhalt, self.geschoss)
        return self.f


def _k(k: Koerper):
    return (k.x, k.y, k.z)


def oeffnung_quader(t: dict) -> list[tuple]:
    """Zarge (links, rechts, oben[, unten]) + Blatt bzw. Glas in Öffnungskoordinaten (x Breite, y Tiefe, z Höhe)."""
    B, H = t["breite_m"], t["hoehe_m"]
    z = t.get("zarge") or {}
    s = z.get("spiegel_m", 0.03)
    T = z.get("tiefe_m", 0.1)
    q = [((0.0, s), (0.0, T), (0.0, H)), ((B - s, B), (0.0, T), (0.0, H)), ((s, B - s), (0.0, T), (H - s, H))]
    if t["ifc"]["klasse"] == "IfcDoor":
        d = (t.get("blatt") or {}).get("dicke_m", 0.04)
        q.append(((s, B - s), (T - d, T), ((t.get("blatt") or {}).get("spalt_unten_m", 0.005), H - s)))
    else:
        q.append(((s, B - s), (0.0, T), (0.0, s)))
        g = (t.get("verglasung") or {}).get("dicke_m", 0.024)
        q.append(((s, B - s), (T / 2 - g / 2, T / 2 + g / 2), (s, H - s)))
    return q


# ── Erzeugnisse

def materialbibliothek(k: Katalog, schema: str = "IFC4", name: str = "Materialbibliothek", **schreiber_kw) -> ifcopenshell.file:
    s = Schreiber(k, schema, name, **schreiber_kw)
    s._bibliothek(name)
    s.alle_materialien()
    return s.fertig()


def typbibliothek(k: Katalog, schema: str = "IFC4", name: str = "Bauteiltypen", **schreiber_kw) -> ifcopenshell.file:
    s = Schreiber(k, schema, name, **schreiber_kw)
    s.alle_materialien()
    s.alle_typen()
    return s.fertig()


ROLLEN_PFLICHT = ("aussenwand", "staenderwand", "rohdecke")
ROLLEN_OPTIONAL = ("innenwand", "fenster", "tuer", "fussboden")


def muster_projekt(k: Katalog, rollen: dict[str, str], schema: str = "IFC4", staenderwand_als: str = "teile",
                   name: str = "Bauteilkatalog Muster", **schreiber_kw) -> tuple[ifcopenshell.file, dict]:
    """Muster (Spec §6): Schichtwand mit Fenster, Innenwand mit Tür, Ständerwand mit Stützklappgriff,
    Rohdecke + Fußbodenaufbau. Raum 5,00 × 3,20 m, Höhe 2,75 m.

    `rollen` ordnet Rollen Typschlüsseln zu. Pflicht: aussenwand, staenderwand, rohdecke. Optional: innenwand,
    fenster (in der Außenwand), tuer (in der Innenwand), fussboden — fehlt eine, entfällt das Element."""
    from . import bauen
    fehlt = [r for r in ROLLEN_PFLICHT if not rollen.get(r)]
    if fehlt:
        raise KatalogFehler(f"muster_projekt: Pflichtrolle fehlt: {', '.join(fehlt)}")
    s = Schreiber(k, schema, name, **schreiber_kw)
    s.struktur("EG")
    H = 2.75
    plaene = []
    aussen = bauen.wand(k, rollen["aussenwand"], 5.0, H)
    plaene.append(aussen)
    w1 = s.wand(aussen, "muster-aussenwand", (0.0, 0.0), 0.0)
    if rollen.get("fenster"):
        s.oeffnung(w1, aussen, rollen["fenster"], "muster-fenster", 1.87, 0.9)
    if rollen.get("innenwand"):
        innen = bauen.wand(k, rollen["innenwand"], 5.0, H)
        plaene.append(innen)
        w2 = s.wand(innen, "muster-innenwand", (5.0, 3.62), math.pi)
        if rollen.get("tuer"):
            s.oeffnung(w2, innen, rollen["tuer"], "muster-tuer", 0.5, 0.0)
    gk = bauen.wand(k, rollen["staenderwand"], 2.94, H, [{"art": "stuetzklappgriff-din18040", "abstand_m": 0.95, "hoehe_m": 0.85}],
                    staenderwand_als)
    plaene.append(gk)
    s.wand(gk, "muster-staenderwand", (0.0, 3.36), -math.pi / 2)
    s.flaeche(rollen["rohdecke"], "muster-rohdecke", (0.0, 5.0), (0.0, 3.62), -0.15)
    if rollen.get("fussboden"):
        s.flaeche(rollen["fussboden"], "muster-fussboden", (0.0, 5.0), (0.0, 3.62), 0.0)
    f = s.fertig()
    return f, {"bauplaene": [p.zusammenfassung() for p in plaene], "zaehler": s.zaehler}


def pruefen(f: ifcopenshell.file, regeln: bool = True) -> dict:
    """Schema- und WHERE-Regel-Prüfung + Konformität (Proxys, Bauteile ohne Typ)."""
    import ifcopenshell.validate
    logger = ifcopenshell.validate.json_logger()
    ifcopenshell.validate.validate(f, logger, express_rules=regeln)
    fehler = [" ".join(str(x) for x in (s.get("message"), s.get("instance"), s.get("attribute")) if x)[:300]
              for s in logger.statements]
    bauteile = [e for e in f.by_type("IfcElement") if not e.is_a("IfcFeatureElement")]
    teile = {r.RelatedObjects[i].id() for r in f.by_type("IfcRelAggregates") if r.RelatingObject.is_a("IfcElement")
             for i in range(len(r.RelatedObjects))}
    ohne_typ = [e.Name for e in bauteile if e.id() not in teile and not getattr(e, "IsTypedBy", None)]
    return {"gueltig": not fehler, "anzahl_fehler": len(fehler), "fehler": fehler[:30], "mit_regeln": regeln,
            "proxys": len(f.by_type("IfcBuildingElementProxy")), "ohne_typ": ohne_typ,
            "materialien": len(f.by_type("IfcMaterial")), "typen": len(f.by_type("IfcTypeObject"))}
