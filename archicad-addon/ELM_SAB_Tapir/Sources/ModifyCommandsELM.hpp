// ELM_SAB 0.10.2 — "Aendern statt loeschen und neu anlegen" (Recherche ueber 45 Sitzungen, 2026-11-02)
//
//   ModifyMeshAppearance – Netze/Gelaende: Oberflaechen oben/Seite/unten, Verkettung, Baustoff,
//                          Deckschraffur (an/aus, Typ, Stifte, 3D-Schraffur), Linien, Stifte
//   MoveOpenings         – Wandoeffnungen (API_OpeningID) verschieben (Tapir: "completely immovable")
//   SetObjectsReflected  – Objekte/Lampen spiegeln bzw. Spiegelung aufheben
//   ModifySurfaces       – Oberflaechen-Attribute wirklich ueberschreiben (Farbe, Reflexion,
//                          Transparenz, Glanz, Texturgroesse und -drehwinkel)
// Alle: Teamwork-Reservierung, ACAPI_CallUndoableCommand, Ruecklese je Element.
// Header-only, AC29.
#pragma once

#include "ELMCommandBase.hpp"
#include "CommandBase.hpp"
#ifdef ServerMainVers_2900
#include "ACAPI/Element/Opening/Opening.hpp"
#include "ACAPI/Element/Opening/OpeningGeometry.hpp"
#include "Polygon2D.hpp"
#endif

#ifdef ServerMainVers_2900

namespace ElmModify {

inline API_Guid ElemGuid (const GS::ObjectState& item)
{
    const GS::ObjectState* id = item.Get ("elementId");
    return id != nullptr ? GetGuidFromObjectState (*id) : APINULLGuid;
}

inline GS::HashTable<API_Guid, short> Reserve (const GS::Array<API_Guid>& guids)
{
    GS::HashTable<API_Guid, short> conflicts;
    if (ACAPI_Teamwork_HasConnection () && !guids.IsEmpty ())
        ACAPI_Teamwork_ReserveElements (guids, &conflicts, false);
    return conflicts;
}

inline GS::ObjectState Fail (const API_Guid& g, GSErrCode err, const GS::UniString& msg)
{
    GS::ObjectState r;
    r.Add ("elementId", CreateGuidObjectStateELM (g));
    r.Add ("success", false);
    r.Add ("code", (Int32) err);
    r.Add ("error", msg);
    return r;
}

inline void SetOverridden (API_OverriddenAttribute& a, Int32 index)
{
    a.hasValue = index >= 0;
    a.value = ACAPI_CreateAttributeIndex (index >= 0 ? index : 0);
}

inline Int32 OverriddenIndex (const API_OverriddenAttribute& a)
{
    return a.hasValue ? a.value.ToInt32_Deprecated () : -1;
}

} // namespace ElmModify


// ------------------------------------------------------------------------------------------
class ModifyMeshAppearanceCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "ModifyMeshAppearance"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmModify;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        GS::Array<API_Guid> guids;
        for (const auto& it : items) guids.Push (ElemGuid (it));
        const auto conflicts = Reserve (guids);

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        ACAPI_CallUndoableCommand ("ELM_SAB ModifyMeshAppearance", [&] () -> GSErrCode {
            for (const auto& it : items) {
                const API_Guid g = ElemGuid (it);
                if (conflicts.ContainsKey (g)) { results (Fail (g, APIERR_NOACCESSRIGHT, "Reservierung abgelehnt")); continue; }
                API_Element el = {}; el.header.guid = g;
                GSErrCode err = ACAPI_Element_Get (&el);
                if (err != NoError || el.header.type.typeID != API_MeshID) { results (Fail (g, err != NoError ? err : APIERR_BADID, "Kein Netz")); continue; }
                API_Element mask = {}; ACAPI_ELEMENT_MASK_CLEAR (mask);
                Int32 i = 0; bool b = false;
                if (it.Get ("topSurfaceIndex", i))    { SetOverridden (el.mesh.topMat, i);  ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, topMat); }
                if (it.Get ("sideSurfaceIndex", i))   { SetOverridden (el.mesh.sideMat, i); ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, sideMat); }
                if (it.Get ("bottomSurfaceIndex", i)) { SetOverridden (el.mesh.botMat, i);  ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, botMat); }
                if (it.Get ("materialsChained", b))   { el.mesh.materialsChained = b;     ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, materialsChained); }
                if (it.Get ("buildingMaterialIndex", i)) { el.mesh.buildingMaterial = ACAPI_CreateAttributeIndex (i); ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, buildingMaterial); }
                if (it.Get ("useFloorFill", b))       { el.mesh.useFloorFill = b;         ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, useFloorFill); }
                if (it.Get ("floorFillIndex", i))     { el.mesh.floorFillInd = ACAPI_CreateAttributeIndex (i); ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, floorFillInd); }
                if (it.Get ("floorFillPen", i))       { el.mesh.floorFillPen = (short) i; ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, floorFillPen); }
                if (it.Get ("floorFillBackgroundPen", i)) { el.mesh.floorFillBGPen = (short) i; ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, floorFillBGPen); }
                if (it.Get ("use3DHatching", b))      { el.mesh.use3DHatching = b;        ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, use3DHatching); }
                if (it.Get ("showLines", i))          { el.mesh.showLines = (short) i;    ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, showLines); }
                if (it.Get ("contourPen", i))         { el.mesh.contPen = (short) i;      ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, contPen); }
                if (it.Get ("levelLinePen", i))       { el.mesh.levelPen = (short) i;     ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, levelPen); }
                if (it.Get ("lineTypeIndex", i))      { el.mesh.ltypeInd = ACAPI_CreateAttributeIndex (i); ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, ltypeInd); }
                if (it.Get ("smoothRidges", b))       { el.mesh.smoothRidges = b ? 1 : 0; ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, smoothRidges); }
                double d = 0;
                if (it.Get ("fillAngle", d))          { el.mesh.fillAngle = d;            ACAPI_ELEMENT_MASK_SET (mask, API_MeshType, fillAngle); }
                err = ACAPI_Element_Change (&el, &mask, nullptr, 0, true);
                if (err != NoError) { results (Fail (g, err, "ACAPI_Element_Change fehlgeschlagen (ausgeblendete Ebene? Gruppe?)")); continue; }
                API_Element bk = {}; bk.header.guid = g; ACAPI_Element_Get (&bk);
                GS::ObjectState r; r.Add ("elementId", CreateGuidObjectStateELM (g)); r.Add ("success", true);
                GS::ObjectState now;
                now.Add ("topSurfaceIndex", OverriddenIndex (bk.mesh.topMat));
                now.Add ("sideSurfaceIndex", OverriddenIndex (bk.mesh.sideMat));
                now.Add ("bottomSurfaceIndex", OverriddenIndex (bk.mesh.botMat));
                now.Add ("materialsChained", bk.mesh.materialsChained);
                now.Add ("buildingMaterialIndex", bk.mesh.buildingMaterial.ToInt32_Deprecated ());
                now.Add ("useFloorFill", bk.mesh.useFloorFill);
                now.Add ("floorFillIndex", bk.mesh.floorFillInd.ToInt32_Deprecated ());
                now.Add ("floorFillPen", (Int32) bk.mesh.floorFillPen);
                now.Add ("floorFillBackgroundPen", (Int32) bk.mesh.floorFillBGPen);
                now.Add ("use3DHatching", bk.mesh.use3DHatching);
                now.Add ("showLines", (Int32) bk.mesh.showLines);
                now.Add ("contourPen", (Int32) bk.mesh.contPen);
                now.Add ("levelLinePen", (Int32) bk.mesh.levelPen);
                r.Add ("readback", now);
                results (r);
            }
            return NoError;
        });
        return response;
    }
};

// ------------------------------------------------------------------------------------------
// AC29: Wandoeffnungen haben kein API_Element-Feld mehr, sondern ACAPI::Element::Opening. Die
// Geometrie ist ein Polygon in der lokalen Ebene (vectorX/vectorY ab Ankerpunkt); verschoben
// wird, indem das Polygon in dieser Ebene versetzt wird. Bewegung entlang der Extrusionsrichtung
// (quer zur Wand) ist nicht moeglich und wird gemeldet. Ruecklese: Ankerpunkt + Polygon-Box.
class MoveOpeningsCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "MoveOpenings"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmModify;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        GS::Array<API_Guid> guids;
        for (const auto& it : items) guids.Push (ElemGuid (it));
        const auto conflicts = Reserve (guids);

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        for (const auto& it : items) {
            const API_Guid g = ElemGuid (it);
            if (conflicts.ContainsKey (g)) { results (Fail (g, APIERR_NOACCESSRIGHT, "Reservierung abgelehnt")); continue; }
            const GS::ObjectState* mv = it.Get ("moveVector");
            if (mv == nullptr) { results (Fail (g, APIERR_BADPARS, "moveVector fehlt")); continue; }
            double vx = 0, vy = 0, vz = 0; mv->Get ("x", vx); mv->Get ("y", vy); mv->Get ("z", vz);
            auto op = ACAPI::Element::Opening::Get (ACAPI::UniqueID (g, ACAPI_GetToken ()));
            if (op.IsErr ()) { results (Fail (g, APIERR_BADID, "Keine Wandoeffnung (ACAPI::Element::Opening)")); continue; }
            auto opening = op.Unwrap ();
            const auto geo = opening.GetOpeningGeometry ();
            auto poly = geo.GetPolygon ();
            if (!poly.has_value ()) { results (Fail (g, APIERR_GENERAL, "Oeffnung ohne Polygon")); continue; }
            const API_Vector3D ex = geo.GetVectorX (), ey = geo.GetVectorY (), ed = geo.GetExtrusionDirection ();
            const double dx = vx * ex.x + vy * ex.y + vz * ex.z;
            const double dy = vx * ey.x + vy * ey.y + vz * ey.z;
            const double dn = vx * ed.x + vy * ed.y + vz * ed.z;
            const Box2DData b0 = poly->GetBoundBox ();
            Geometry::Polygon2D moved = *poly;
            moved.Move (Vector2D (dx, dy));
            const auto res = opening.Modify ([&] (ACAPI::Element::Opening::Modifier& m) {
                m.GetOpeningGeometry ().SetPolygon (moved);
            }, "ELM_SAB MoveOpenings");
            if (res.IsErr ()) { results (Fail (g, APIERR_GENERAL, GS::UniString (res.UnwrapErr ().text.c_str ()))); continue; }
            auto op2 = ACAPI::Element::Opening::Get (ACAPI::UniqueID (g, ACAPI_GetToken ()));
            GS::ObjectState r; r.Add ("elementId", CreateGuidObjectStateELM (g));
            bool ok = false;
            if (op2.IsOk ()) {
                const auto geo2 = op2.Unwrap ().GetOpeningGeometry ();
                const auto p2 = geo2.GetPolygon ();
                const API_Coord3D a0 = geo.GetAnchorPoint (), a1 = geo2.GetAnchorPoint ();
                if (p2.has_value ()) {
                    const Box2DData b1 = p2->GetBoundBox ();
                    // Wirkung: Polygon versetzt ODER Ankerpunkt um denselben Betrag gewandert
                    const double pmx = b1.xMin - b0.xMin, pmy = b1.yMin - b0.yMin;
                    const double amx = (a1.x - a0.x) * ex.x + (a1.y - a0.y) * ex.y + (a1.z - a0.z) * ex.z;
                    const double amy = (a1.x - a0.x) * ey.x + (a1.y - a0.y) * ey.y + (a1.z - a0.z) * ey.z;
                    ok = (fabs (pmx + amx - dx) < 1e-4) && (fabs (pmy + amy - dy) < 1e-4);
                    GS::ObjectState d; d.Add ("alongX", pmx + amx); d.Add ("alongY", pmy + amy); r.Add ("movedLocal", d);
                }
            }
            r.Add ("success", ok);
            GS::ObjectState want; want.Add ("alongX", dx); want.Add ("alongY", dy); r.Add ("requestedLocal", want);
            if (fabs (dn) > 1e-6) r.Add ("ignoredAlongExtrusion", dn);
            if (!ok) r.Add ("error", GS::UniString ("Ruecklese: Oeffnung nicht wie verlangt verschoben"));
            results (r);
        }
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class SetObjectsReflectedCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "SetObjectsReflected"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        using namespace ElmModify;
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        GS::Array<API_Guid> guids;
        for (const auto& it : items) guids.Push (ElemGuid (it));
        const auto conflicts = Reserve (guids);

        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        ACAPI_CallUndoableCommand ("ELM_SAB SetObjectsReflected", [&] () -> GSErrCode {
            for (const auto& it : items) {
                const API_Guid g = ElemGuid (it);
                if (conflicts.ContainsKey (g)) { results (Fail (g, APIERR_NOACCESSRIGHT, "Reservierung abgelehnt")); continue; }
                bool refl = true; it.Get ("reflected", refl);
                API_Element el = {}; el.header.guid = g;
                GSErrCode err = ACAPI_Element_Get (&el);
                const API_ElemTypeID t = el.header.type.typeID;
                if (err != NoError || (t != API_ObjectID && t != API_LampID)) { results (Fail (g, err != NoError ? err : APIERR_BADID, "Kein Objekt/keine Lampe")); continue; }
                API_Element mask = {}; ACAPI_ELEMENT_MASK_CLEAR (mask);
                el.object.reflected = refl;
                ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, reflected);
                err = ACAPI_Element_Change (&el, &mask, nullptr, 0, true);
                if (err != NoError) { results (Fail (g, err, "ACAPI_Element_Change fehlgeschlagen")); continue; }
                API_Element bk = {}; bk.header.guid = g; ACAPI_Element_Get (&bk);
                GS::ObjectState r; r.Add ("elementId", CreateGuidObjectStateELM (g));
                r.Add ("success", bk.object.reflected == refl); r.Add ("reflected", bk.object.reflected);
                results (r);
            }
            return NoError;
        });
        return response;
    }
};

// ------------------------------------------------------------------------------------------
class ModifySurfacesCommandELM : public ELMCommandBase {
public:
    virtual GS::String GetName () const override { return "ModifySurfaces"; }
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const override
    {
        GS::Array<GS::ObjectState> items;
        parameters.Get ("items", items);
        GS::ObjectState response;
        const auto& results = response.AddList<GS::ObjectState> ("executionResults");
        ACAPI_CallUndoableCommand ("ELM_SAB ModifySurfaces", [&] () -> GSErrCode {
            for (const auto& it : items) {
                Int32 index = -1; it.Get ("index", index);
                GS::ObjectState r; r.Add ("index", index);
                API_Attribute a = {};
                a.header.typeID = API_MaterialID;
                a.header.index = ACAPI_CreateAttributeIndex (index);
                GSErrCode err = ACAPI_Attribute_Get (&a);
                if (err != NoError) { r.Add ("success", false); r.Add ("code", (Int32) err); r.Add ("error", GS::UniString ("Oberflaeche nicht gefunden")); results (r); continue; }
                const GS::ObjectState* c = it.Get ("surfaceColor");
                if (c != nullptr) { c->Get ("red", a.material.surfaceRGB.f_red); c->Get ("green", a.material.surfaceRGB.f_green); c->Get ("blue", a.material.surfaceRGB.f_blue); }
                Int32 v = 0;
                if (it.Get ("ambientReflection", v))  a.material.ambientPc  = (short) v;
                if (it.Get ("diffuseReflection", v))  a.material.diffusePc  = (short) v;
                if (it.Get ("specularReflection", v)) a.material.specularPc = (short) v;
                if (it.Get ("transparency", v))       a.material.transpPc   = (short) v;
                if (it.Get ("shine", v))              a.material.shine      = (short) v;
                const GS::ObjectState* tx = it.Get ("texture");
                if (tx != nullptr) {
                    tx->Get ("xSize", a.material.texture.xSize);
                    tx->Get ("ySize", a.material.texture.ySize);
                    tx->Get ("rotationAngle", a.material.texture.rotAng);
                }
                err = ACAPI_Attribute_Modify (&a, nullptr);
                if (err != NoError) { r.Add ("success", false); r.Add ("code", (Int32) err); r.Add ("error", GS::UniString ("ACAPI_Attribute_Modify fehlgeschlagen (Teamwork: Attribut reserviert?)")); results (r); continue; }
                API_Attribute b = {}; b.header.typeID = API_MaterialID; b.header.index = a.header.index;
                ACAPI_Attribute_Get (&b);
                const bool ok = fabs (b.material.surfaceRGB.f_red - a.material.surfaceRGB.f_red) < 1e-3 && b.material.shine == a.material.shine &&
                                b.material.transpPc == a.material.transpPc && fabs (b.material.texture.rotAng - a.material.texture.rotAng) < 1e-6 &&
                                fabs (b.material.texture.xSize - a.material.texture.xSize) < 1e-6;
                r.Add ("success", ok);
                r.Add ("name", GS::UniString (b.header.name));
                GS::ObjectState t; t.Add ("xSize", b.material.texture.xSize); t.Add ("ySize", b.material.texture.ySize); t.Add ("rotationAngle", b.material.texture.rotAng);
                r.Add ("texture", t);
                if (!ok) r.Add ("error", GS::UniString ("Ruecklese weicht ab"));
                results (r);
            }
            return NoError;
        });
        return response;
    }
};

#endif // ServerMainVers_2900
