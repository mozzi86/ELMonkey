#include "SetObjectParametersForceCommand.hpp"
#include "MigrationHelper.hpp"

#include <vector>

GS::Optional<GS::UniString> SetObjectParametersForceCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Ziel-Elemente. gdlParameters je Element schlaegt die globale Liste.",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        },
                        "gdlParameters": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "name":        { "type": "string" },
                                    "value":       {},
                                    "numberValue": { "type": "number" },
                                    "stringValue": { "type": "string" },
                                    "boolValue":   { "type": "boolean" }
                                },
                                "required": ["name"]
                            }
                        }
                    },
                    "required": ["elementId"]
                },
                "minItems": 1
            },
            "gdlParameters": {
                "type": "array",
                "description": "Fuer alle Elemente geltende Parameter, wenn das Element keine eigenen mitbringt.",
                "items": {
                    "type": "object",
                    "properties": {
                        "name":        { "type": "string" },
                        "value":       {},
                        "numberValue": { "type": "number" },
                        "stringValue": { "type": "string" },
                        "boolValue":   { "type": "boolean" }
                    },
                    "required": ["name"]
                }
            },
            "reserve": {
                "type": "boolean",
                "description": "Teamwork: vorab reservieren (Standard true) und erfolgreiche Elemente wieder freigeben."
            },
            "syncObjectRatios": {
                "type": "boolean",
                "description": "Bei Objekten A/B zusaetzlich in xRatio/yRatio spiegeln, damit die Masse wirklich umspringen (Standard true)."
            },
            "allowWithoutUndoScope": {
                "type": "boolean",
                "description": "Wenn ACAPI_CallUndoableCommand das Lambda nicht ausfuehrt, denselben Durchgang ohne Undo-Klammer wiederholen (Standard true)."
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> SetObjectParametersForceCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "undoScope": { "type": "object" }
        },
        "required": ["executionResults", "undoScope"]
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

// Liest einen Parameter-Eintrag. Akzeptiert value (Zahl/String/Bool) genauso wie
// die expliziten Felder numberValue/stringValue/boolValue.
bool ReadParChange (const GS::ObjectState& item, ParChange& ch)
{
    if (!item.Get ("name", ch.name) || ch.name.IsEmpty ())
        return false;

    ch.hasNumber = item.Get ("numberValue", ch.numberValue);
    ch.hasString = item.Get ("stringValue", ch.stringValue);

    bool boolValue = false;
    if (!ch.hasNumber && item.Get ("boolValue", boolValue)) {
        ch.hasNumber = true;
        ch.numberValue = boolValue ? 1.0 : 0.0;
    }

    if (!ch.hasNumber && !ch.hasString) {
        double  d = 0.0;
        Int32   i = 0;
        if (item.Get ("value", d)) {
            ch.hasNumber = true;
            ch.numberValue = d;
        } else if (item.Get ("value", i)) {
            ch.hasNumber = true;
            ch.numberValue = (double) i;
        } else if (item.Get ("value", boolValue)) {
            ch.hasNumber = true;
            ch.numberValue = boolValue ? 1.0 : 0.0;
        } else if (item.Get ("value", ch.stringValue)) {
            ch.hasString = true;
        }
    }

    return ch.hasNumber || ch.hasString;
}

void ReadParChanges (const GS::Array<GS::ObjectState>& items, std::vector<ParChange>& out)
{
    for (const GS::ObjectState& item : items) {
        ParChange ch;
        if (ReadParChange (item, ch))
            out.push_back (ch);
    }
}

GSSize CountParams (API_AddParType** params)
{
    if (params == nullptr)
        return 0;
    return BMGetHandleSize (reinterpret_cast<GSHandle> (params)) / sizeof (API_AddParType);
}

// Traegt die Aenderungen ins AddPars-Handle ein. notFound zaehlt Parameter, die es
// im Bibliotheksteil gar nicht gibt — die sollen nicht als Erfolg durchgehen.
UInt32 ApplyToParams (API_AddParType** params, const std::vector<ParChange>& changes, UInt32& notFound)
{
    const GSSize nParams = CountParams (params);
    UInt32 applied = 0;
    notFound = 0;

    for (const ParChange& ch : changes) {
        bool hit = false;
        for (GSIndex i = 0; i < nParams; ++i) {
            API_AddParType& p = (*params)[i];
            if (ch.name != GS::UniString (p.name))
                continue;
            hit = true;
            if (p.typeMod != API_ParSimple)
                break;                                  // Arrays: bewusst nicht unterstuetzt
            if (p.typeID == APIParT_CString) {
                if (ch.hasString) {
                    const USize maxLen = (USize) (sizeof (p.value.uStr) / sizeof (p.value.uStr[0])) - 1;
                    GS::ucscpy (p.value.uStr, ch.stringValue.ToUStr (0, GS::Min (ch.stringValue.GetLength (), maxLen)).Get ());
                    ++applied;
                }
            } else if (ch.hasNumber) {
                p.value.real = ch.numberValue;
                ++applied;
            }
            break;
        }
        if (!hit)
            ++notFound;
    }
    return applied;
}

// Ruecklese: NoError von ACAPI_Element_Change beweist nichts.
bool VerifyParams (API_AddParType** params, const std::vector<ParChange>& changes)
{
    const GSSize nParams = CountParams (params);
    if (nParams == 0)
        return false;

    for (const ParChange& ch : changes) {
        for (GSIndex i = 0; i < nParams; ++i) {
            API_AddParType& p = (*params)[i];
            if (ch.name != GS::UniString (p.name) || p.typeMod != API_ParSimple)
                continue;
            if (p.typeID == APIParT_CString) {
                if (ch.hasString) {
                    const USize maxLen = (USize) (sizeof (p.value.uStr) / sizeof (p.value.uStr[0])) - 1;
                    const GS::UniString readBack (p.value.uStr);
                    const GS::UniString expected = ch.stringValue.GetSubstring (0, GS::Min (ch.stringValue.GetLength (), maxLen));
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

bool FindNumber (const std::vector<ParChange>& changes, const char* name, double& value)
{
    for (const ParChange& ch : changes) {
        if (ch.hasNumber && ch.name == GS::UniString (name)) {
            value = ch.numberValue;
            return true;
        }
    }
    return false;
}

struct ElemResult {
    GSErrCode     err = APIERR_GENERAL;
    GS::UniString message = "Nicht ausgefuehrt (Undo-Scope hat das Lambda nicht gestartet)";
    UInt32        applied = 0;
    UInt32        notFound = 0;
    bool          done = false;      // schon abgearbeitet -> im zweiten Durchgang ueberspringen
};

// Setzt die Parameter an EINEM Element und liest zurueck.
void ApplyToOne (const API_Guid&                guid,
                 const std::vector<ParChange>&  changes,
                 bool                           syncObjectRatios,
                 ElemResult&                    result)
{
    result.done = true;

    if (guid == APINULLGuid) {
        result.err = APIERR_BADPARS;
        result.message = "elementId/guid fehlt oder ist ungueltig";
        return;
    }
    if (changes.empty ()) {
        result.err = APIERR_BADPARS;
        result.message = "Keine verwertbaren gdlParameters";
        return;
    }

    API_Element element = {};
    element.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get (&element);
    if (err != NoError) {
        result.err = err;
        result.message = "Element nicht lesbar";
        return;
    }

    const API_ElemTypeID typeId = GetElemTypeId (element.header);
    if (typeId != API_ObjectID && typeId != API_LampID && typeId != API_LabelID && typeId != API_ZoneID) {
        result.err = APIERR_BADELEMENTTYPE;
        result.message = "Nur Object, Lamp, Label oder Zone";
        return;
    }

    API_ElementMemo memo = {};
    err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_AddPars);
    if (err != NoError || memo.params == nullptr) {
        ACAPI_DisposeElemMemoHdls (&memo);
        result.err = (err != NoError) ? err : APIERR_GENERAL;
        result.message = "AddPars-Memo nicht lesbar";
        return;
    }

    result.applied = ApplyToParams (memo.params, changes, result.notFound);

    API_Element mask = {};
    ACAPI_ELEMENT_MASK_CLEAR (mask);

    // A/B sind bei Objekten zugleich xRatio/yRatio. Wer nur die AddPars schreibt,
    // aendert die Parameterliste, aber nicht die Masse des platzierten Objekts.
    if (syncObjectRatios && typeId == API_ObjectID) {
        double a = 0.0, b = 0.0;
        if (FindNumber (changes, "A", a)) {
            element.object.xRatio = a;
            ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, xRatio);
        }
        if (FindNumber (changes, "B", b)) {
            element.object.yRatio = b;
            ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, yRatio);
        }
    }

    err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_AddPars, true);
    ACAPI_DisposeElemMemoHdls (&memo);

    if (err != NoError) {
        result.err = err;
        result.message = "ACAPI_Element_Change abgelehnt";
        return;
    }

    API_ElementMemo checkMemo = {};
    bool verified = false;
    if (ACAPI_Element_GetMemo (guid, &checkMemo, APIMemoMask_AddPars) == NoError)
        verified = VerifyParams (checkMemo.params, changes);
    ACAPI_DisposeElemMemoHdls (&checkMemo);

    if (!verified) {
        result.err = APIERR_GENERAL;
        result.message = "Aenderung nicht wirksam (Ruecklese-Verifikation fehlgeschlagen)";
        return;
    }

    result.err = NoError;
    result.message = "";
}

} // namespace

GS::ObjectState SetObjectParametersForceCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);
    if (elements.IsEmpty ())
        return CreateErrorResponse (APIERR_BADPARS, "elements ist leer.");

    std::vector<ParChange> globalChanges;
    GS::Array<GS::ObjectState> globalItems;
    if (parameters.Get ("gdlParameters", globalItems))
        ReadParChanges (globalItems, globalChanges);

    bool reserve = true;
    parameters.Get ("reserve", reserve);
    bool syncObjectRatios = true;
    parameters.Get ("syncObjectRatios", syncObjectRatios);
    bool allowWithoutUndoScope = true;
    parameters.Get ("allowWithoutUndoScope", allowWithoutUndoScope);

    const size_t n = elements.GetSize ();
    std::vector<API_Guid>               guids (n, APINULLGuid);
    std::vector<std::vector<ParChange>> changes (n);
    std::vector<ElemResult>             results (n);

    for (size_t i = 0; i < n; ++i) {
        const GS::ObjectState* id = elements[i].Get ("elementId");
        if (id != nullptr)
            guids[i] = GetGuidFromObjectState (*id);

        GS::Array<GS::ObjectState> items;
        if (elements[i].Get ("gdlParameters", items))
            ReadParChanges (items, changes[i]);
        if (changes[i].empty ())
            changes[i] = globalChanges;
    }

    // Teamwork: einmal fuer alle reservieren, Konflikte je Element melden.
    GS::HashTable<API_Guid, short> conflicts;
    const bool teamwork = ACAPI_Teamwork_HasConnection ();
    if (reserve && teamwork) {
        GS::Array<API_Guid> toReserve;
        for (size_t i = 0; i < n; ++i)
            if (guids[i] != APINULLGuid)
                toReserve.Push (guids[i]);
        if (!toReserve.IsEmpty ())
            ACAPI_Teamwork_ReserveElements (toReserve, &conflicts, false);
    }

    for (size_t i = 0; i < n; ++i) {
        if (guids[i] != APINULLGuid && conflicts.ContainsKey (guids[i])) {
            results[i].err = APIERR_NOACCESSRIGHT;
            results[i].message = "Reservierung abgelehnt (anderer Nutzer, ausgeblendete Ebene oder Hotlink)";
            results[i].done = true;
        }
    }

    // Durchgang 1 in der Undo-Klammer. `lambdaRan` deckt genau den Fall auf, an dem
    // Tapirs Befehle still scheitern: Archicad startet das Lambda nicht.
    bool lambdaRan = false;
    const GSErrCode undoErr = ACAPI_CallUndoableCommand ("ELM_SAB SetObjectParametersForce", [&] () -> GSErrCode {
        lambdaRan = true;
        for (size_t i = 0; i < n; ++i) {
            if (!results[i].done)
                ApplyToOne (guids[i], changes[i], syncObjectRatios, results[i]);
        }
        return NoError;
    });

    // Durchgang 2 ohne Undo-Klammer — lieber eine Aenderung ohne Undo-Eintrag als
    // gar keine. Der Fehlercode von ACAPI_Element_Change wird dann sichtbar.
    GS::UniString mode = lambdaRan ? "undoable" : "notExecuted";
    if (!lambdaRan && allowWithoutUndoScope) {
        mode = "direct";
        for (size_t i = 0; i < n; ++i) {
            if (!results[i].done)
                ApplyToOne (guids[i], changes[i], syncObjectRatios, results[i]);
        }
    }

    if (reserve && teamwork) {
        GS::Array<API_Guid> toRelease;
        for (size_t i = 0; i < n; ++i)
            if (results[i].err == NoError)
                toRelease.Push (guids[i]);
        if (!toRelease.IsEmpty ())
            ACAPI_Teamwork_ReleaseElements (toRelease, false);
    }

    GS::ObjectState response;
    const auto& executionResults = response.AddList<GS::ObjectState> ("executionResults");
    UInt32 successCount = 0;
    for (size_t i = 0; i < n; ++i) {
        if (results[i].err == NoError) {
            ++successCount;
            GS::ObjectState ok = CreateSuccessfulExecutionResult ();
            ok.Add ("elementId", CreateGuidObjectStateELM (guids[i]));
            ok.Add ("parametersSet", (Int32) results[i].applied);
            ok.Add ("parametersNotInLibPart", (Int32) results[i].notFound);
            executionResults (ok);
        } else {
            GS::ObjectState failed = CreateFailedExecutionResult (results[i].err, results[i].message);
            failed.Add ("elementId", CreateGuidObjectStateELM (guids[i]));
            executionResults (failed);
        }
    }

    GS::ObjectState undoScope;
    undoScope.Add ("executed", lambdaRan);
    undoScope.Add ("errorCode", (Int32) undoErr);
    undoScope.Add ("mode", mode);
    if (!lambdaRan) {
        undoScope.Add ("hint", GS::UniString ("ACAPI_CallUndoableCommand hat das Lambda nicht gestartet — "
                                              "genau hier liefern Tapirs SetGDLParametersOfElements/MoveElements "
                                              "still eine leere executionResults-Liste. Archicad-Sitzung pruefen "
                                              "(offener Dialog, aktives Werkzeug, Teamwork-Schreibrecht) bzw. neu starten."));
    }
    response.Add ("undoScope", undoScope);
    response.Add ("successCount", (Int32) successCount);
    response.Add ("elementCount", (Int32) n);

    return response;
}
