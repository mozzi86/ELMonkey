#include "SetPenOfElementsCommand.hpp"

#include <vector>

// ---------------------------------------------------------------------------
// Schema
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> SetPenOfElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Liste der Ziel-Elemente (2D: Hatch, Line, PolyLine, Arc, Circle, Spline, Text).",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": {
                                "guid": { "type": "string" }
                            },
                            "required": ["guid"]
                        }
                    },
                    "required": ["elementId"]
                }
            },
            "contourPen": {
                "type": "integer",
                "description": "Stift-Index für Kontur-/Linienstift. Optional.",
                "minimum": 1,
                "maximum": 255
            },
            "fillForegroundPen": {
                "type": "integer",
                "description": "Stift-Index für Schraffur-Vordergrund (nur Hatch). Optional.",
                "minimum": 1,
                "maximum": 255
            },
            "fillBackgroundPen": {
                "type": "integer",
                "description": "Stift-Index für Schraffur-Hintergrund (nur Hatch). 0 = transparent. Optional.",
                "minimum": 0,
                "maximum": 255
            },
            "clearForegroundRGB": {
                "type": "boolean",
                "description": "true: RGB-Override des Schraffur-Vordergrunds löschen (Stift gilt wieder). Nur Hatch. Optional."
            },
            "clearBackgroundRGB": {
                "type": "boolean",
                "description": "true: RGB-Override des Schraffur-Hintergrunds löschen. Nur Hatch. Optional."
            },
            "databases": {
                "type": "array",
                "description": "Optionale Datenbank-Liste (z.B. Arbeitsblätter). Das Add-On wechselt selbst durch und stellt die Ausgangs-Datenbank wieder her.",
                "items": {
                    "type": "object",
                    "properties": {
                        "databaseId": {
                            "type": "object",
                            "properties": {
                                "guid": { "type": "string" }
                            },
                            "required": ["guid"]
                        }
                    },
                    "required": ["databaseId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetPenOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "success": { "type": "boolean" }
                    },
                    "required": ["success"]
                }
            }
        },
        "required": ["executionResults"]
    })");
}

// ---------------------------------------------------------------------------
// Ausführung
// ---------------------------------------------------------------------------

namespace {

struct PenChangeRequest {
    bool  hasContourPen = false;   Int32 contourPen = 0;
    bool  hasFillFgPen = false;    Int32 fillFgPen = 0;
    bool  hasFillBgPen = false;    Int32 fillBgPen = 0;
    bool  clearFgRGB = false;
    bool  clearBgRGB = false;
};

// Versucht die Änderung an einem Element in der AKTUELLEN Datenbank.
GSErrCode ChangeOneElement (const API_Guid& guid, const PenChangeRequest& req)
{
    API_Element element = {};
    element.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError)
        return err;

    API_Element mask = {};
    ACAPI_ELEMENT_MASK_CLEAR (mask);
    bool changed = false;

    switch (element.header.type.typeID) {
        case API_HatchID:
            if (req.hasContourPen) {
                element.hatch.contPen.penIndex = (short) req.contourPen;
                element.hatch.contPen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_HatchType, contPen);
                changed = true;
            }
            if (req.hasFillFgPen) {
                element.hatch.fillPen.penIndex = (short) req.fillFgPen;
                element.hatch.fillPen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_HatchType, fillPen);
                changed = true;
            }
            if (req.hasFillBgPen) {
                element.hatch.fillBGPen = (short) req.fillBgPen;
                ACAPI_ELEMENT_MASK_SET (mask, API_HatchType, fillBGPen);
                changed = true;
            }
            if (req.clearFgRGB && (element.hatch.hatchFlags & APIHatch_HasFgRGBColor) != 0) {
                element.hatch.hatchFlags &= ~((UInt32) APIHatch_HasFgRGBColor);
                ACAPI_ELEMENT_MASK_SET (mask, API_HatchType, hatchFlags);
                changed = true;
            }
            if (req.clearBgRGB && (element.hatch.hatchFlags & APIHatch_HasBkgRGBColor) != 0) {
                element.hatch.hatchFlags &= ~((UInt32) APIHatch_HasBkgRGBColor);
                ACAPI_ELEMENT_MASK_SET (mask, API_HatchType, hatchFlags);
                changed = true;
            }
            break;

        case API_LineID:
            if (req.hasContourPen) {
                element.line.linePen.penIndex = (short) req.contourPen;
                element.line.linePen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_LineType, linePen);
                changed = true;
            }
            break;

        case API_PolyLineID:
            if (req.hasContourPen) {
                element.polyLine.linePen.penIndex = (short) req.contourPen;
                element.polyLine.linePen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_PolyLineType, linePen);
                changed = true;
            }
            break;

        case API_ArcID:
            if (req.hasContourPen) {
                element.arc.linePen.penIndex = (short) req.contourPen;
                element.arc.linePen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_ArcType, linePen);
                changed = true;
            }
            break;

        case API_CircleID:
            if (req.hasContourPen) {
                element.circle.linePen.penIndex = (short) req.contourPen;
                element.circle.linePen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_CircleType, linePen);
                changed = true;
            }
            break;

        case API_SplineID:
            if (req.hasContourPen) {
                element.spline.linePen.penIndex = (short) req.contourPen;
                element.spline.linePen.colorOverridePenIndex = 0;
                ACAPI_ELEMENT_MASK_SET (mask, API_SplineType, linePen);
                changed = true;
            }
            break;

        case API_TextID:
            if (req.hasContourPen) {
                element.text.pen = (short) req.contourPen;
                ACAPI_ELEMENT_MASK_SET (mask, API_TextType, pen);
                changed = true;
            }
            // Deckung (opaker Text-Hintergrund): fillBackgroundPen setzt
            // usedFill=true + fillPen — SAB-Workflow „Text erbt Schraffur-Farbe".
            if (req.hasFillBgPen) {
                element.text.usedFill = true;
                element.text.fillPen = (short) req.fillBgPen;
                ACAPI_ELEMENT_MASK_SET (mask, API_TextType, usedFill);
                ACAPI_ELEMENT_MASK_SET (mask, API_TextType, fillPen);
                changed = true;
            }
            break;

        default:
            return APIERR_BADELEMENTTYPE;
    }

    if (!changed)
        return APIERR_BADPARS;

    return ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
}

} // namespace

GS::ObjectState SetPenOfElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    PenChangeRequest req;
    req.hasContourPen = parameters.Get ("contourPen", req.contourPen);
    req.hasFillFgPen  = parameters.Get ("fillForegroundPen", req.fillFgPen);
    req.hasFillBgPen  = parameters.Get ("fillBackgroundPen", req.fillBgPen);
    parameters.Get ("clearForegroundRGB", req.clearFgRGB);
    parameters.Get ("clearBackgroundRGB", req.clearBgRGB);

    if (!req.hasContourPen && !req.hasFillFgPen && !req.hasFillBgPen && !req.clearFgRGB && !req.clearBgRGB) {
        return CreateErrorResponse (APIERR_BADPARS, "Mindestens ein Stift-Parameter oder clearForegroundRGB/clearBackgroundRGB muss gesetzt sein.");
    }

    GS::Array<GS::ObjectState> databases;
    parameters.Get ("databases", databases);

    // Element-Guids + Status
    std::vector<API_Guid>  guids;
    std::vector<GSErrCode> status;   // NoError = erledigt
    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        guids.push_back (elementId != nullptr ? GetGuidFromObjectState (*elementId) : APINULLGuid);
        status.push_back (APIERR_GENERAL);
    }

    auto processPending = [&] () {
        ACAPI_CallUndoableCommand ("ELM_SAB SetPenOfElements", [&] () -> GSErrCode {
            for (size_t i = 0; i < guids.size (); ++i) {
                if (status[i] == NoError)
                    continue;
                if (guids[i] == APINULLGuid) {
                    status[i] = APIERR_BADPARS;
                    continue;
                }
                status[i] = ChangeOneElement (guids[i], req);
            }
            return NoError;
        });
    };

    if (databases.IsEmpty ()) {
        processPending ();
    } else {
        API_DatabaseInfo startingDatabase = {};
        ACAPI_Database_GetCurrentDatabase (&startingDatabase);

        for (const GS::ObjectState& dbItem : databases) {
            const GS::ObjectState* dbId = dbItem.Get ("databaseId");
            if (dbId == nullptr)
                continue;
            API_DatabaseInfo targetDb = {};
            targetDb.databaseUnId.elemSetId = GetGuidFromObjectState (*dbId);
            if (ACAPI_Window_GetDatabaseInfo (&targetDb) != NoError)
                continue;
            if (ACAPI_Database_ChangeCurrentDatabase (&targetDb) != NoError)
                continue;
            processPending ();
        }

        ACAPI_Database_ChangeCurrentDatabase (&startingDatabase);
    }

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < guids.size (); ++i) {
        if (status[i] == NoError)
            executionResults (CreateSuccessfulExecutionResult ());
        else
            executionResults (CreateFailedExecutionResult (status[i], "Element konnte nicht geändert werden (in keiner der Datenbanken)"));
    }
    return response;
}
