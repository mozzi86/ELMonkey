#!/usr/bin/env python3
"""
Build IFC for a single class.
Usage: step_build_single.py <pkl> <class_name> <ifc_out>
"""
import sys, pickle, uuid, time
import ifcopenshell

CLASS_SPECS = {
    "Strasse":   {"top_z": -0.15, "thickness": 0.15, "color": (0.25, 0.25, 0.25)},
    "Bordstein": {"top_z":  0.00, "thickness": 0.15, "color": (0.75, 0.75, 0.75)},
    "Gebaeude":  {"top_z":  6.00, "thickness": 6.00, "color": (0.85, 0.65, 0.45)},
}

# WICHTIG: gemeinsamer globaler Origin damit alle Klassen-IFCs zueinander passen
def global_offset():
    bx, by = float("inf"), float("inf")
    for pn in ["strasse.pkl", "bordstein.pkl", "gebaeude.pkl"]:
        with open(pn, "rb") as fp:
            d = pickle.load(fp)
        for lab, outer, holes in d["polys"]:
            for x,y in outer:
                if x < bx: bx = x
                if y < by: by = y
    return bx, by

def cguid(): return ifcopenshell.guid.compress(uuid.uuid4().hex)


def main():
    pkl = sys.argv[1]
    cls_name = sys.argv[2]
    out = sys.argv[3]
    spec = CLASS_SPECS[cls_name]

    t0 = time.time()
    with open(pkl,"rb") as fp:
        polys = pickle.load(fp)["polys"]
    print(f"[{time.time()-t0:.1f}s] {cls_name}: {len(polys)} Polygone", flush=True)

    bx, by = global_offset()
    print(f"[{time.time()-t0:.1f}s] Globaler Offset: ({bx:.1f}, {by:.1f})", flush=True)

    f = ifcopenshell.file(schema="IFC4")
    person = f.create_entity("IfcPerson", FamilyName="Elmokadem", GivenName="Mohamed")
    org    = f.create_entity("IfcOrganization", Name="schwarz architekturbuero")
    pao    = f.create_entity("IfcPersonAndOrganization", ThePerson=person, TheOrganization=org)
    app    = f.create_entity("IfcApplication", ApplicationDeveloper=org, Version="3.1",
                              ApplicationFullName="DWG2IFC Slab Builder v3",
                              ApplicationIdentifier="dwg2ifc-v3")
    owner  = f.create_entity("IfcOwnerHistory", OwningUser=pao, OwningApplication=app,
                              ChangeAction="ADDED", CreationDate=0)
    units = f.create_entity("IfcUnitAssignment", Units=[
        f.create_entity("IfcSIUnit", UnitType="LENGTHUNIT", Name="METRE"),
        f.create_entity("IfcSIUnit", UnitType="AREAUNIT",   Name="SQUARE_METRE"),
        f.create_entity("IfcSIUnit", UnitType="VOLUMEUNIT", Name="CUBIC_METRE"),
        f.create_entity("IfcSIUnit", UnitType="PLANEANGLEUNIT", Name="RADIAN"),
    ])
    origin3d = f.create_entity("IfcCartesianPoint", Coordinates=[0.0, 0.0, 0.0])
    zdir = f.create_entity("IfcDirection", DirectionRatios=[0.0, 0.0, 1.0])
    xdir = f.create_entity("IfcDirection", DirectionRatios=[1.0, 0.0, 0.0])
    ydir = f.create_entity("IfcDirection", DirectionRatios=[0.0, 1.0, 0.0])
    world_ax = f.create_entity("IfcAxis2Placement3D", Location=origin3d, Axis=zdir, RefDirection=xdir)
    model_ctx = f.create_entity("IfcGeometricRepresentationContext", ContextType="Model",
                                 CoordinateSpaceDimension=3, Precision=1e-5,
                                 WorldCoordinateSystem=world_ax, TrueNorth=ydir)
    body_ctx = f.create_entity("IfcGeometricRepresentationSubContext",
                                ContextIdentifier="Body", ContextType="Model",
                                ParentContext=model_ctx, TargetView="MODEL_VIEW")
    project = f.create_entity("IfcProject", GlobalId=cguid(), OwnerHistory=owner,
                               Name=f"BuPol Umgebung ({cls_name})",
                               RepresentationContexts=[model_ctx], UnitsInContext=units)
    def lp(rel, z=0.0):
        p = f.create_entity("IfcCartesianPoint", Coordinates=[0.0, 0.0, float(z)])
        a = f.create_entity("IfcAxis2Placement3D", Location=p, Axis=zdir, RefDirection=xdir)
        return f.create_entity("IfcLocalPlacement", PlacementRelTo=rel, RelativePlacement=a)

    site_pl  = lp(None);    site   = f.create_entity("IfcSite",   GlobalId=cguid(), OwnerHistory=owner, Name="Gelaende", ObjectPlacement=site_pl, CompositionType="ELEMENT", RefElevation=0.0)
    bldg_pl  = lp(site_pl); bldg   = f.create_entity("IfcBuilding", GlobalId=cguid(), OwnerHistory=owner, Name="Liegenschaft", ObjectPlacement=bldg_pl, CompositionType="ELEMENT")
    storey_pl= lp(bldg_pl); storey = f.create_entity("IfcBuildingStorey", GlobalId=cguid(), OwnerHistory=owner, Name="EG Aussenanlagen", ObjectPlacement=storey_pl, CompositionType="ELEMENT", Elevation=0.0)
    f.create_entity("IfcRelAggregates", GlobalId=cguid(), OwnerHistory=owner, RelatingObject=project, RelatedObjects=[site])
    f.create_entity("IfcRelAggregates", GlobalId=cguid(), OwnerHistory=owner, RelatingObject=site, RelatedObjects=[bldg])
    f.create_entity("IfcRelAggregates", GlobalId=cguid(), OwnerHistory=owner, RelatingObject=bldg, RelatedObjects=[storey])

    col = f.create_entity("IfcColourRgb", Red=spec["color"][0], Green=spec["color"][1], Blue=spec["color"][2])
    sh  = f.create_entity("IfcSurfaceStyleShading", SurfaceColour=col, Transparency=0.0)
    style = f.create_entity("IfcSurfaceStyle", Side="BOTH", Styles=[sh])

    ext_origin = f.create_entity("IfcCartesianPoint", Coordinates=[0.0,0.0,0.0])
    ext_ax     = f.create_entity("IfcAxis2Placement3D", Location=ext_origin, Axis=zdir, RefDirection=xdir)

    if cls_name == "Gebaeude":
        base_z = 0.0; ext_h = spec["thickness"]
    else:
        base_z = spec["top_z"] - spec["thickness"]; ext_h = spec["thickness"]

    slab_origin_cls = f.create_entity("IfcCartesianPoint", Coordinates=[0.0,0.0,float(base_z)])
    slab_ax_cls     = f.create_entity("IfcAxis2Placement3D", Location=slab_origin_cls, Axis=zdir, RefDirection=xdir)
    slab_pl_cls     = f.create_entity("IfcLocalPlacement", PlacementRelTo=storey_pl, RelativePlacement=slab_ax_cls)

    def ifc_polyline(pts):
        if pts[0] != pts[-1]:
            pts = pts + [pts[0]]
        cps = [f.create_entity("IfcCartesianPoint",
                                 Coordinates=[float(x-bx), float(y-by)]) for (x,y) in pts]
        return f.create_entity("IfcPolyline", Points=cps)

    created = []
    print(f"[{time.time()-t0:.1f}s] Baue Slabs...", flush=True)
    for idx, (label, outer, holes) in enumerate(polys):
        outer_pl = ifc_polyline(outer)
        if holes:
            inners = [ifc_polyline(h) for h in holes]
            profile = f.create_entity("IfcArbitraryProfileDefWithVoids",
                                        ProfileType="AREA", OuterCurve=outer_pl,
                                        InnerCurves=inners)
        else:
            profile = f.create_entity("IfcArbitraryClosedProfileDef",
                                        ProfileType="AREA", OuterCurve=outer_pl)
        solid = f.create_entity("IfcExtrudedAreaSolid",
                                 SweptArea=profile, Position=ext_ax,
                                 ExtrudedDirection=zdir, Depth=float(ext_h))
        shape_rep = f.create_entity("IfcShapeRepresentation",
                                     ContextOfItems=body_ctx,
                                     RepresentationIdentifier="Body",
                                     RepresentationType="SweptSolid", Items=[solid])
        f.create_entity("IfcStyledItem", Item=solid, Styles=[style])
        prod_def = f.create_entity("IfcProductDefinitionShape", Representations=[shape_rep])
        slab = f.create_entity("IfcSlab", GlobalId=cguid(), OwnerHistory=owner,
                                Name=f"{cls_name}_{label}_{idx+1}", ObjectType=cls_name,
                                ObjectPlacement=slab_pl_cls, Representation=prod_def,
                                PredefinedType="USERDEFINED")
        created.append(slab)

    if created:
        f.create_entity("IfcRelContainedInSpatialStructure",
                         GlobalId=cguid(), OwnerHistory=owner,
                         Name=cls_name+"Container", RelatingStructure=storey,
                         RelatedElements=created)

    print(f"[{time.time()-t0:.1f}s] Schreibe {out}...", flush=True)
    f.write(out)
    print(f"[{time.time()-t0:.1f}s] Fertig: {len(created)} Slabs", flush=True)


if __name__ == "__main__":
    main()
