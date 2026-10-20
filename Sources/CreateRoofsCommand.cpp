#include "CreateRoofsCommand.hpp"
#include "ElmPreflight.hpp"

#include <cmath>

namespace {

bool FindAttrByNameRoofELM (API_AttrTypeID type, const GS::UniString& name, API_AttributeIndex& out)
{
    GS::UniString n (name);
    API_Attr_Head head = {};
    head.typeID = type;
    head.uniStringNamePtr = &n;
    if (ACAPI_Attribute_Search (&head) != NoError)
        return false;
    out = head.index;
    return true;
}

GS::UniString AttrNameRoofELM (API_AttrTypeID type, const API_AttributeIndex& idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = type;
    attr.header.index = idx;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

} // namespace

// ---------------------------------------------------------------------------
// Schema
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> ELMCreateRoofsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "roofsData": {
                "type": "array",
                "description": "Array der zu erzeugenden Ein-Flächen-Dächer (PlaneRoof).",
                "items": {
                    "type": "object",
                    "properties": {
                        "polygonCoordinates": {
                            "type": "array",
                            "description": "Dachflächen-Kontur (erster Punkt NICHT wiederholen; wird automatisch geschlossen).",
                            "minItems": 3,
                            "items": {
                                "type": "object",
                                "properties": {
                                    "x": { "type": "number" },
                                    "y": { "type": "number" }
                                },
                                "required": ["x", "y"]
                            }
                        },
                        "pivotLine": {
                            "type": "object",
                            "description": "Traufkante (Drehachse der Dachebene) als 2D-Strecke.",
                            "properties": {
                                "p1": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "required": ["x", "y"] },
                                "p2": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "required": ["x", "y"] }
                            },
                            "required": ["p1", "p2"]
                        },
                        "angleDeg": {
                            "type": "number",
                            "description": "Dachneigung in Grad (> 0)."
                        },
                        "zCoordinate": {
                            "type": "number",
                            "description": "Absolute Höhe (Projektnull) der Pivot-Linie / Traufkante."
                        },
                        "posSign": {
                            "type": "boolean",
                            "description": "Steig-Seite relativ zur Pivot-Richtung p1→p2. Default true; per Kalibrier-Dach live prüfen."
                        },
                        "floorIndex": {
                            "type": "integer",
                            "description": "Geschoss-Index (Story). Optional, Default: aktuelles Geschoss."
                        },
                        "thickness": {
                            "type": "number",
                            "description": "Dachdicke in Metern (Basis-Aufbau). Steht das Werkzeug auf Mehrschicht, wird auf Basis-Aufbau umgestellt (structureNote in der Antwort). Nicht zusammen mit compositeName."
                        },
                        "buildingMaterialName": {
                            "type": "string",
                            "description": "Basis-Baustoff per Name; setzt den Basis-Aufbau. Optional."
                        },
                        "compositeName": {
                            "type": "string",
                            "description": "Mehrschicht-Aufbau per Name; die Dicke kommt aus dem Aufbau. Nicht zusammen mit thickness/buildingMaterialName."
                        },
                        "layerName": {
                            "type": "string",
                            "description": "Ziel-Ebene per Name (Vorrang vor layerIndex). Optional."
                        },
                        "layerIndex": {
                            "type": "integer",
                            "description": "Ziel-Ebene (Attribut-Index). Optional, Default: Werkzeug-Voreinstellung."
                        }
                    },
                    "required": ["polygonCoordinates", "pivotLine", "angleDeg", "zCoordinate"]
                }
            }
        },
        "required": ["roofsData"]
    })");
}

GS::Optional<GS::UniString> ELMCreateRoofsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["elements"]
    })");
}

// ---------------------------------------------------------------------------
// Ausführung
// ---------------------------------------------------------------------------

GS::ObjectState ELMCreateRoofsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> roofsData;
    parameters.Get ("roofsData", roofsData);

    GS::ObjectState response;
    const auto& elements = response.AddList<GS::ObjectState> ("elements");

    ACAPI_CallUndoableCommand ("ELM_SAB CreateRoofs", [&] () -> GSErrCode {
        for (const GS::ObjectState& roofData : roofsData) {
            GS::Array<GS::ObjectState> coords;
            roofData.Get ("polygonCoordinates", coords);
            const Int32 n = (Int32) coords.GetSize ();
            if (n < 3) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "Mindestens 3 Punkte nötig"));
                continue;
            }

            GS::ObjectState pivot;
            if (!roofData.Get ("pivotLine", pivot)) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "pivotLine fehlt"));
                continue;
            }
            GS::ObjectState p1, p2;
            pivot.Get ("p1", p1);
            pivot.Get ("p2", p2);
            API_Coord c1 = {}, c2 = {};
            p1.Get ("x", c1.x); p1.Get ("y", c1.y);
            p2.Get ("x", c2.x); p2.Get ("y", c2.y);
            if (std::hypot (c2.x - c1.x, c2.y - c1.y) < 0.001) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "pivotLine degeneriert"));
                continue;
            }

            double angleDeg = 0.0;
            roofData.Get ("angleDeg", angleDeg);
            if (angleDeg <= 0.0 || angleDeg >= 89.0) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "angleDeg außerhalb (0, 89)"));
                continue;
            }

            double zCoordinate = 0.0;
            roofData.Get ("zCoordinate", zCoordinate);

            API_Element element = {};
            API_ElementMemo memo = {};

#ifdef ServerMainVers_2600
            element.header.type = API_RoofID;
#else
            element.header.typeID = API_RoofID;
#endif
            GSErrCode err = ACAPI_Element_GetDefaults (&element, &memo);
            if (err != NoError) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (err, "GetDefaults fehlgeschlagen"));
                continue;
            }

            element.roof.roofClass = API_PlaneRoofID;

            Int32 floorIndex = 0;
            if (roofData.Get ("floorIndex", floorIndex))
                element.header.floorInd = (short) floorIndex;

            // Story-Level ermitteln, damit zCoordinate absolut bleibt
            double storyLevel = 0.0;
            {
                API_StoryInfo storyInfo = {};
                if (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) == NoError && storyInfo.data != nullptr) {
                    const short storyCount = storyInfo.lastStory - storyInfo.firstStory + 1;
                    for (short k = 0; k < storyCount; ++k) {
                        if ((*storyInfo.data)[k].index == element.header.floorInd) {
                            storyLevel = (*storyInfo.data)[k].level;
                            break;
                        }
                    }
                    BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
                }
            }

            element.roof.shellBase.level = zCoordinate - storyLevel;

            // 0.9.34: Aufbau. Bei Mehrschicht-Werkzeugstandard ignorierte Archicad thickness still
            // (Demo 20.10.: 34,5 statt 10 cm, Create meldete Erfolg) - jetzt umstellen und melden.
            GS::UniString compositeName, materialName, structureNote;
            const bool hasComposite = roofData.Get ("compositeName", compositeName) && !compositeName.IsEmpty ();
            const bool hasMaterial  = roofData.Get ("buildingMaterialName", materialName) && !materialName.IsEmpty ();
            double thickness = 0.0;
            const bool hasThickness = roofData.Get ("thickness", thickness) && thickness > 0.0;
            if (hasComposite && (hasThickness || hasMaterial)) {
                elements (CreateFailedExecutionResult (APIERR_BADPARS, "compositeName schliesst thickness und buildingMaterialName aus - die Dicke kommt aus dem Mehrschicht-Aufbau"));
                ACAPI_DisposeElemMemoHdls (&memo);
                continue;
            }
            if (hasComposite) {
                API_AttributeIndex idx;
                if (!FindAttrByNameRoofELM (API_CompWallID, compositeName, idx)) {
                    elements (CreateFailedExecutionResult (APIERR_BADNAME, GS::UniString ("[ATTRIBUTE_NOT_FOUND] Mehrschicht-Aufbau '") + compositeName + GS::UniString ("' gibt es nicht")));
                    ACAPI_DisposeElemMemoHdls (&memo);
                    continue;
                }
                element.roof.shellBase.modelElemStructureType = API_CompositeStructure;
                element.roof.shellBase.composite = idx;
            }
            if (hasMaterial) {
                API_AttributeIndex idx;
                if (!FindAttrByNameRoofELM (API_BuildingMaterialID, materialName, idx)) {
                    elements (CreateFailedExecutionResult (APIERR_BADNAME, GS::UniString ("[ATTRIBUTE_NOT_FOUND] Baustoff '") + materialName + GS::UniString ("' gibt es nicht")));
                    ACAPI_DisposeElemMemoHdls (&memo);
                    continue;
                }
                element.roof.shellBase.modelElemStructureType = API_BasicStructure;
                element.roof.shellBase.buildingMaterial = idx;
            }
            if (hasThickness) {
                if (element.roof.shellBase.modelElemStructureType != API_BasicStructure) {
                    if (AttrNameRoofELM (API_BuildingMaterialID, element.roof.shellBase.buildingMaterial).IsEmpty ()) {
                        elements (CreateFailedExecutionResult (APIERR_BADPARS, "[NO_MATERIAL] Werkzeug-Standard ist Mehrschicht ohne gueltigen Basis-Baustoff - buildingMaterialName mitgeben oder compositeName statt thickness"));
                        ACAPI_DisposeElemMemoHdls (&memo);
                        continue;
                    }
                    structureNote = GS::UniString ("Werkzeug-Standard war Mehrschicht '") + AttrNameRoofELM (API_CompWallID, element.roof.shellBase.composite)
                        + GS::UniString ("' - auf Basis-Aufbau umgestellt, damit thickness wirkt");
                    element.roof.shellBase.modelElemStructureType = API_BasicStructure;
                }
                element.roof.shellBase.thickness = thickness;
            }

            GS::UniString layerName;
            Int32 layerIndex = 0;
            if (roofData.Get ("layerName", layerName) && !layerName.IsEmpty ()) {
                API_AttributeIndex li;
                if (!ResolveLayerByNameELM (layerName, li)) {
                    elements (CreateFailedExecutionResult (APIERR_BADNAME, GS::UniString ("[LAYER_NOT_FOUND] Ebene '") + layerName + GS::UniString ("' gibt es nicht")));
                    ACAPI_DisposeElemMemoHdls (&memo);
                    continue;
                }
                element.header.layer = li;
            } else if (roofData.Get ("layerIndex", layerIndex)) {
                element.header.layer = ACAPI_CreateAttributeIndex (layerIndex);
            }

            element.roof.u.planeRoof.baseLine.c1 = c1;
            element.roof.u.planeRoof.baseLine.c2 = c2;
            element.roof.u.planeRoof.angle = angleDeg * 3.14159265358979323846 / 180.0;

            bool posSign = true;
            roofData.Get ("posSign", posSign);
            element.roof.u.planeRoof.posSign = posSign;

            // Polygon in Element + Memo (1-basiert, Endpunkt = Startpunkt)
            element.roof.u.planeRoof.poly.nCoords   = n + 1;
            element.roof.u.planeRoof.poly.nSubPolys = 1;
            element.roof.u.planeRoof.poly.nArcs     = 0;

            memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((element.roof.u.planeRoof.poly.nCoords + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
            memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((element.roof.u.planeRoof.poly.nSubPolys + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
            if (memo.coords == nullptr || memo.pends == nullptr) {
                ACAPI_DisposeElemMemoHdls (&memo);
                elements (CreateFailedExecutionResult (APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"));
                continue;
            }

            Int32 i = 1;
            for (const GS::ObjectState& c : coords) {
                double x = 0.0, y = 0.0;
                c.Get ("x", x);
                c.Get ("y", y);
                (*memo.coords)[i].x = x;
                (*memo.coords)[i].y = y;
                ++i;
            }
            (*memo.coords)[i] = (*memo.coords)[1];
            (*memo.pends)[1] = element.roof.u.planeRoof.poly.nCoords;

            err = ACAPI_Element_Create (&element, &memo);
            ACAPI_DisposeElemMemoHdls (&memo);

            if (err != NoError) {
                elements (CreateFailedExecutionResult (err, "ACAPI_Element_Create fehlgeschlagen"));
                continue;
            }

            GS::ObjectState ok;
            ok.Add ("elementId", CreateGuidObjectStateELM (element.header.guid));
            // 0.9.34: Ruecklese Aufbau + Dicke.
            API_Element check = {};
            check.header.guid = element.header.guid;
            if (ACAPI_Element_Get (&check) == NoError) {
                const API_ShellBaseType& sb = check.roof.shellBase;
                double gotThickness = sb.thickness;
                if (sb.modelElemStructureType == API_CompositeStructure) {
                    ok.Add ("structureType", GS::UniString ("composite"));
                    ok.Add ("compositeName", AttrNameRoofELM (API_CompWallID, sb.composite));
                    API_Attribute comp = {};
                    comp.header.typeID = API_CompWallID;
                    comp.header.index = sb.composite;
                    if (ACAPI_Attribute_Get (&comp) == NoError)
                        gotThickness = comp.compWall.totalThick;
                } else {
                    ok.Add ("structureType", GS::UniString (sb.modelElemStructureType == API_BasicStructure ? "basic" : "profile"));
                    ok.Add ("buildingMaterialName", AttrNameRoofELM (API_BuildingMaterialID, sb.buildingMaterial));
                }
                ok.Add ("thickness", gotThickness);
                if (hasThickness) {
                    const bool matches = std::fabs (gotThickness - thickness) <= 1e-4;
                    ok.Add ("thicknessMatches", matches);
                    if (!matches)
                        ok.Add ("warning", GS::UniString ("[NO_EFFECT] Dicke laut Ruecklese weicht von thickness ab"));
                }
            }
            if (!structureNote.IsEmpty ())
                ok.Add ("structureNote", structureNote);
            elements (ok);
        }
        return NoError;
    });

    return response;
}
