// ELM_SAB GetSolidBounds (0.11.13) — Box der 3D-Koerper eines Bauteils, so wie sie nach Anschluessen und
// Verschneidungen im 3D-Modell stehen (und damit im IFC herauskaemen).
//
// Warum es den Befehl braucht: TapirCommand.Get3DBoundingBoxes nimmt fuer Waende ACAPI_Element_CalcBounds,
// die rohe Huellbox. Die weicht bei angeschlossenen Waenden um 20-35 cm von der geschnittenen Geometrie ab.
// Im Abgleich Archicad <-> Plattform (AFWC, 16.11.2026) sah deshalb fast jede Wand geaendert aus; der
// Ausweg war ein Komplett-IFC. Mit dieser Box vergleicht der Diff-Checker (elmonkey.diff) ohne IFC-Datei.
//
// Antwort je Element: boundingBox3D, source ("solid" = aus den Koerpern, "calcBounds" = rohe Huellbox als
// Rueckfall, nicht vergleichbar) und bodies (Zahl der Koerper). Ein Bauteil ohne Koerper ist meist im 3D
// ausgeblendet (Umbau-Filter, Ebene). Fuer Decken gibt es dann keinen Rueckfall: CalcBounds stuerzt dort
// ab, wenn die Decke im aktiven Fenster nicht gezeichnet ist (Tapir #686).
//
// Liest nur. Header-only, kein C++20 (AC27/28 bauen nach C++17).
#pragma once

#include "ELMCommandBase.hpp"
#include "CommandBase.hpp"
#include "MigrationHelper.hpp"

class GetSolidBoundsCommandELM : public ELMCommandBase
{
public:
    GetSolidBoundsCommandELM () = default;

    virtual GS::String GetName () const override
    {
        return "GetSolidBounds";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override
    {
        return R"({
    "type": "object",
    "properties": {
        "elements": {
            "type": "array",
            "description": "Elemente als [{elementId: {guid}}].",
            "items": {
                "type": "object",
                "properties": {
                    "elementId": {
                        "type": "object",
                        "properties": { "guid": { "type": "string" } },
                        "required": [ "guid" ]
                    }
                },
                "required": [ "elementId" ]
            }
        }
    },
    "additionalProperties": false,
    "required": [ "elements" ]
})";
    }

    virtual GS::Optional<GS::UniString> GetResponseSchema () const override
    {
        return ELMResponseSchemaWithError (R"({
    "type": "object",
    "properties": {
        "solidBounds": {
            "type": "array",
            "description": "Je Eingabe-Element ein Eintrag, gleiche Reihenfolge.",
            "items": { "type": "object" }
        }
    },
    "additionalProperties": false,
    "required": [ "solidBounds" ]
})");
    }

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
    {
        GS::Array<GS::ObjectState> elements;
        if (!parameters.Get ("elements", elements)) {
            return CreateErrorResponseELM (APIERR_BADPARS, "elements fehlt");
        }

        GS::ObjectState response;
        const auto& solidBounds = response.AddList<GS::ObjectState> ("solidBounds");

        for (const GS::ObjectState& element : elements) {
            const GS::ObjectState* elementId = element.Get ("elementId");
            if (elementId == nullptr) {
                solidBounds (CreateErrorResponseELM (APIERR_BADPARS, "elementId fehlt"));
                continue;
            }

            API_Elem_Head elemHead = {};
            elemHead.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_GetHeader (&elemHead);
            if (err != NoError) {
                solidBounds (CreateErrorResponseELM (err, "Element nicht gefunden"));
                continue;
            }

            API_Box3D box = {};
            box.xMin = box.yMin = box.zMin = 1e30;
            box.xMax = box.yMax = box.zMax = -1e30;
            Int32 bodies = 0;

            // body.xmin..zmax ist die Box des Koerpers in Weltkoordinaten (tranmat schon angewandt) -
            // wie in AccumulateSolidBodyBounds in ElementCommands.cpp, dort an Tapir #563 belegt.
            API_ElemInfo3D info3D = {};
            if (ACAPI_ModelAccess_Get3DInfo (elemHead, &info3D) == NoError) {
                for (Int32 iBody = info3D.fbody; iBody <= info3D.lbody; ++iBody) {
                    API_Component3D bodyComp = {};
                    bodyComp.header.typeID = API_BodyID;
                    bodyComp.header.index = iBody;
                    if (ACAPI_ModelAccess_GetComponent (&bodyComp) != NoError || bodyComp.body.nPgon == 0) {
                        continue;
                    }
                    ++bodies;
                    if (bodyComp.body.xmin < box.xMin) box.xMin = bodyComp.body.xmin;
                    if (bodyComp.body.ymin < box.yMin) box.yMin = bodyComp.body.ymin;
                    if (bodyComp.body.zmin < box.zMin) box.zMin = bodyComp.body.zmin;
                    if (bodyComp.body.xmax > box.xMax) box.xMax = bodyComp.body.xmax;
                    if (bodyComp.body.ymax > box.yMax) box.yMax = bodyComp.body.ymax;
                    if (bodyComp.body.zmax > box.zMax) box.zMax = bodyComp.body.zmax;
                }
            }

            GS::UniString source ("solid");
            if (bodies == 0) {
                if (GetElemTypeId (elemHead) == API_SlabID) {
                    solidBounds (CreateErrorResponseELM (APIERR_GENERAL,
                        "Decke ohne 3D-Koerper (im 3D ausgeblendet?) - kein Rueckfall, CalcBounds stuerzt hier ab"));
                    continue;
                }
                err = ACAPI_Element_CalcBounds (&elemHead, &box);
                if (err != NoError) {
                    solidBounds (CreateErrorResponseELM (err, "Kein 3D-Koerper und keine Huellbox"));
                    continue;
                }
                source = "calcBounds";
            }

            GS::ObjectState boundingBox3D ("xMin", box.xMin, "xMax", box.xMax,
                                           "yMin", box.yMin, "yMax", box.yMax,
                                           "zMin", box.zMin, "zMax", box.zMax);
            GS::ObjectState entry ("boundingBox3D", boundingBox3D);
            entry.Add ("source", source);
            entry.Add ("bodies", bodies);
            solidBounds (entry);
        }

        return response;
    }
};
