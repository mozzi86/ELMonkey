#include "SetAddParsCommand.hpp"

#include <cstring>

GS::Optional<GS::UniString> SetAddParsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Ziel-Elemente mit zu setzenden GDL-Parametern (Object, Lamp, Label, Zone/Stempel).",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        },
                        "parameters": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "name": { "type": "string" },
                                    "stringValue": { "type": "string" },
                                    "numberValue": { "type": "number" }
                                },
                                "required": ["name"]
                            }
                        }
                    },
                    "required": ["elementId", "parameters"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetAddParsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["executionResults"]
    })");
}

namespace {

struct ParChange {
    GS::UniString name;
    bool          hasString = false;
    GS::UniString stringValue;
    bool          hasNumber = false;
    double        numberValue = 0.0;
};

// Wendet die Änderungen auf das AddPars-Handle an. Liefert die Anzahl der
// tatsächlich gefundenen Parameter.
UInt32 ApplyToParams (API_AddParType** params, const GS::Array<ParChange>& changes)
{
    if (params == nullptr)
        return 0;
    const GSSize nParams = BMGetHandleSize (reinterpret_cast<GSHandle> (params)) / sizeof (API_AddParType);
    UInt32 found = 0;
    for (const ParChange& ch : changes) {
        for (GSIndex i = 0; i < nParams; ++i) {
            API_AddParType& p = (*params)[i];
            if (ch.name != GS::UniString (p.name))
                continue;
            if (p.typeMod != API_ParSimple)
                break; // Arrays nicht unterstuetzt
            if (p.typeID == APIParT_CString) {
                if (ch.hasString) {
                    GS::UniString value = ch.stringValue;
                    // uStr ist ein festes Array (API_UAddParStrLen) — kappen.
                    const USize maxLen = (USize) (sizeof (p.value.uStr) / sizeof (p.value.uStr[0])) - 1;
                    GS::ucscpy (p.value.uStr, value.ToUStr (0, GS::Min (value.GetLength (), maxLen)).Get ());
                    ++found;
                }
            } else {
                if (ch.hasNumber) {
                    p.value.real = ch.numberValue;
                    ++found;
                }
            }
            break;
        }
    }
    return found;
}

// Vergleicht nach dem Schreiben zurückgelesene Werte mit den Sollwerten.
bool VerifyParams (API_AddParType** params, const GS::Array<ParChange>& changes)
{
    if (params == nullptr)
        return false;
    const GSSize nParams = BMGetHandleSize (reinterpret_cast<GSHandle> (params)) / sizeof (API_AddParType);
    for (const ParChange& ch : changes) {
        for (GSIndex i = 0; i < nParams; ++i) {
            API_AddParType& p = (*params)[i];
            if (ch.name != GS::UniString (p.name) || p.typeMod != API_ParSimple)
                continue;
            if (p.typeID == APIParT_CString) {
                if (ch.hasString) {
                    GS::UniString readBack (p.value.uStr);
                    const USize maxLen = (USize) (sizeof (p.value.uStr) / sizeof (p.value.uStr[0])) - 1;
                    GS::UniString expected = ch.stringValue.GetSubstring (0, GS::Min (ch.stringValue.GetLength (), maxLen));
                    if (readBack != expected)
                        return false;
                }
            } else if (ch.hasNumber) {
                if (p.value.real < ch.numberValue - 1e-9 || p.value.real > ch.numberValue + 1e-9)
                    return false;
            }
            break;
        }
    }
    return true;
}

} // namespace

GS::ObjectState SetAddParsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");

    ACAPI_CallUndoableCommand ("ELM_SAB SetAddParsOfElements", [&] () -> GSErrCode {
        for (const GS::ObjectState& elementItem : elements) {
            const GS::ObjectState* elementId = elementItem.Get ("elementId");
            GS::Array<GS::ObjectState> parItems;
            elementItem.Get ("parameters", parItems);
            if (elementId == nullptr || parItems.IsEmpty ()) {
                executionResults (CreateFailedExecutionResult (APIERR_BADPARS, "elementId oder parameters fehlt"));
                continue;
            }

            GS::Array<ParChange> changes;
            for (const GS::ObjectState& parItem : parItems) {
                ParChange ch;
                if (!parItem.Get ("name", ch.name)) {
                    continue;
                }
                ch.hasString = parItem.Get ("stringValue", ch.stringValue);
                ch.hasNumber = parItem.Get ("numberValue", ch.numberValue);
                changes.Push (ch);
            }

            API_Element element = {};
            element.header.guid = GetGuidFromObjectState (*elementId);
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "Element nicht gefunden"));
                continue;
            }

            const API_ElemTypeID typeId = element.header.type.typeID;
            if (typeId != API_ObjectID && typeId != API_LampID && typeId != API_LabelID && typeId != API_ZoneID) {
                executionResults (CreateFailedExecutionResult (APIERR_BADELEMENTTYPE, "Nur Object, Lamp, Label oder Zone"));
                continue;
            }

            API_ElementMemo memo = {};
            err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_AddPars);
            if (err != NoError || memo.params == nullptr) {
                ACAPI_DisposeElemMemoHdls (&memo);
                executionResults (CreateFailedExecutionResult (err != NoError ? err : APIERR_GENERAL, "AddPars-Memo nicht lesbar"));
                continue;
            }

            const UInt32 found = ApplyToParams (memo.params, changes);

            API_Element mask = {};
            ACAPI_ELEMENT_MASK_CLEAR (mask);
            err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_AddPars, true);
            ACAPI_DisposeElemMemoHdls (&memo);

            if (err != NoError) {
                executionResults (CreateFailedExecutionResult (err, "AddPars konnten nicht geändert werden"));
                continue;
            }

            // Rücklese-Verifikation (Lektion: NoError beweist nichts).
            API_ElementMemo checkMemo = {};
            bool verified = false;
            if (ACAPI_Element_GetMemo (element.header.guid, &checkMemo, APIMemoMask_AddPars) == NoError) {
                verified = VerifyParams (checkMemo.params, changes);
            }
            ACAPI_DisposeElemMemoHdls (&checkMemo);

            if (!verified) {
                executionResults (CreateFailedExecutionResult (APIERR_GENERAL, "Änderung nicht wirksam (Rücklese-Verifikation fehlgeschlagen)"));
                continue;
            }

            GS::ObjectState ok;
            ok.Add ("success", true);
            ok.Add ("parametersSet", (Int32) found);
            executionResults (ok);
        }
        return NoError;
    });

    return response;
}
