#include "LabelCommandsELM.hpp"
#include <vector>
#include <memory>
#include <cmath>

namespace {

// ---- Helfer (Ablesen) ---------------------------------------------------------

GS::UniString LibPartNameOf (Int32 libInd)
{
    if (libInd <= 0)
        return GS::UniString ();
    API_LibPart lp = {};
    lp.index = libInd;
    GS::UniString name;
    if (ACAPI_LibraryPart_Get (&lp) == NoError) {
        name = GS::UniString (lp.docu_UName);
        delete lp.location;           // von ACAPI_LibraryPart_Get angelegt (siehe ElementCreationCommands)
        lp.location = nullptr;
    }
    return name;
}

GS::UniString LayerNameOf (const API_AttributeIndex& idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = API_LayerID;
    attr.header.index = idx;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

// Ebenenname mit Cache (Index -> Name), damit ein Massenlauf nicht je Etikett nachfragt.
const GS::UniString& CachedLayerName (GS::HashTable<Int32, GS::UniString>& cache, const API_AttributeIndex& idx)
{
    const Int32 key = (Int32) idx.ToInt32_Deprecated ();
    if (!cache.ContainsKey (key))
        cache.Add (key, LayerNameOf (idx));
    return cache.Get (key);
}

bool NearEqual (const API_Coord& a, const API_Coord& b)
{
    return std::fabs (a.x - b.x) <= 0.001 && std::fabs (a.y - b.y) <= 0.001;   // API-Einheit Meter => 1 mm
}

// Gemeinsame Felder eines gelesenen Etiketts.
void FillLabelCommon (GS::ObjectState& o, const API_Element& e, GS::HashTable<Int32, GS::UniString>& layerCache)
{
    o.Add ("floorInd", (Int32) e.header.floorInd);
    o.Add ("layerIndex", (Int32) e.header.layer.ToInt32_Deprecated ());
    o.Add ("layerName", CachedLayerName (layerCache, e.header.layer));
    o.Add ("begCoordinate", Create2DCoordinateObjectState (e.label.begC));
    o.Add ("midCoordinate", Create2DCoordinateObjectState (e.label.midC));
    o.Add ("endCoordinate", Create2DCoordinateObjectState (e.label.endC));
    o.Add ("hasLeaderLine", e.label.hasLeaderLine);
}

// ---- CreateLabels: Vorbereitung je Eintrag ---------------------------------------

struct LabelPrep {
    GSErrCode        err = NoError;
    GS::UniString    message;
    API_Guid         parent = APINULLGuid;
    API_Guid         created = APINULLGuid;
    API_Element      element = {};
    API_ElementMemo  memo = {};
    bool             hasBeg = false, hasMid = false, hasEnd = false;
    API_Coord        beg = {}, mid = {}, end = {};
    Int32            wantFloor = 0;
};

void FailPrep (LabelPrep& p, GSErrCode err, const GS::UniString& msg)
{
    p.err = err;
    p.message = msg;
}

void PrepareOne (const GS::ObjectState& item, LabelPrep& p)
{
    const GS::ObjectState* parentId = item.Get ("parentElementId");
    p.parent = (parentId != nullptr) ? GetGuidFromObjectState (*parentId) : APINULLGuid;
    if (p.parent == APINULLGuid) { FailPrep (p, APIERR_BADPARS, "parentElementId fehlt"); return; }

    API_Elem_Head parentHead = {};
    parentHead.guid = p.parent;
    if (ACAPI_Element_GetHeader (&parentHead) != NoError) { FailPrep (p, APIERR_BADPARS, "Parent nicht gefunden"); return; }

    const GS::ObjectState* begOS = item.Get ("begCoordinate");
    const GS::ObjectState* midOS = item.Get ("midCoordinate");
    const GS::ObjectState* endOS = item.Get ("endCoordinate");
    if (begOS != nullptr) { p.hasBeg = true; p.beg = Get2DCoordinateFromObjectState (*begOS); }
    if (midOS != nullptr) { p.hasMid = true; p.mid = Get2DCoordinateFromObjectState (*midOS); }
    if (endOS != nullptr) { p.hasEnd = true; p.end = Get2DCoordinateFromObjectState (*endOS); }

    // Soll-Geschoss: Eingabe gewinnt, Parent-Geschoss nur als Rueckfall.
    Int32 floorIn = 0;
    const bool hasFloor = item.Get ("floorInd", floorIn);
    p.wantFloor = hasFloor ? floorIn : (Int32) parentHead.floorInd;

    // Vorlage oder Werkzeug-Default.
    const GS::ObjectState* tmplOS = item.Get ("templateLabelId");
    const API_Guid tmplGuid = (tmplOS != nullptr) ? GetGuidFromObjectState (*tmplOS) : APINULLGuid;
    if (tmplGuid != APINULLGuid) {
        API_Element tmpl = {};
        tmpl.header.guid = tmplGuid;
        if (ACAPI_Element_Get (&tmpl) != NoError) { FailPrep (p, APIERR_BADPARS, "Vorlage nicht gefunden"); return; }
        if (tmpl.header.type.typeID != API_LabelID || tmpl.label.labelClass != APILblClass_Symbol) {
            FailPrep (p, APIERR_BADPARS, "Vorlage ist kein Symbol-Etikett");
            return;
        }
        p.element = tmpl;
        // Die Vorlage wird nur gelesen. AddPars-Memo mitnehmen.
        const GSErrCode memoErr = ACAPI_Element_GetMemo (tmplGuid, &p.memo, APIMemoMask_AddPars);
        if (memoErr != NoError) { FailPrep (p, memoErr, "AddPars der Vorlage nicht lesbar"); return; }
        p.element.header.guid = APINULLGuid;
    } else {
        p.element.header.type = API_LabelID;
        p.element.label.parentType = parentHead.type;     // Default passend zum Elementtyp holen
        GSErrCode defErr = ACAPI_Element_GetDefaults (&p.element, &p.memo);
        if (defErr != NoError) {
            ACAPI_DisposeElemMemoHdls (&p.memo);
            p.memo = {};
            p.element = {};
            p.element.header.type = API_LabelID;
            defErr = ACAPI_Element_GetDefaults (&p.element, &p.memo);
        }
        if (defErr != NoError) { FailPrep (p, defErr, "Werkzeug-Default fuer Etikett nicht lesbar"); return; }
        if (p.element.label.labelClass != APILblClass_Symbol) {
            FailPrep (p, APIERR_BADPARS, "Werkzeug-Default ist Text-Etikett - templateLabelId angeben");
            return;
        }
    }

    p.element.label.parent = p.parent;
    p.element.label.parentType = parentHead.type;
    p.element.header.floorInd = (short) p.wantFloor;

    if (p.hasBeg) {
        p.element.label.begC = p.beg;
    } else {
        API_Box3D box = {};
        ACAPI_Element_CalcBounds (&parentHead, &box);
        p.element.label.begC.x = (box.xMin + box.xMax) / 2.0;
        p.element.label.begC.y = (box.yMin + box.yMax) / 2.0;
    }

    if (p.hasEnd) {
        p.element.label.createAtDefaultPosition = false;
        p.element.label.endC = p.end;
        p.element.label.midC = p.hasMid ? p.mid : p.element.label.begC;   // gerade Zeigerlinie, nichts ableiten
    } else {
        p.element.label.createAtDefaultPosition = true;
    }
}

} // namespace

// ---- CreateLabels ----------------------------------------------------------------

GS::Optional<GS::UniString> CreateLabelsELMCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "labelsData": { "type": "array", "items": { "type": "object",
                "properties": {
                    "parentElementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
                    "floorInd": { "type": "integer",
                        "description": "Geschoss des Etiketts. Wird NICHT durch das Parent-Geschoss ueberschrieben; nur wenn es fehlt, gilt das Parent-Geschoss." },
                    "begCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                    "midCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                    "endCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                    "templateLabelId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"],
                        "description": "Optional: Symbol-Etikett als Vorlage (libPart, Ebene, AddPars, Zeigerlinie)." }
                },
                "required": ["parentElementId"] } }
        },
        "required": ["labelsData"]
    })");
}

GS::Optional<GS::UniString> CreateLabelsELMCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "undoScope": { "type": "object" }
        },
        "required": ["executionResults"]
    })");
}

GS::ObjectState CreateLabelsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    parameters.Get ("labelsData", items);

    // 1) Vorbereitung VOR dem Undo-Lambda.
    std::vector<std::unique_ptr<LabelPrep>> preps;
    for (const GS::ObjectState& item : items) {
        preps.push_back (std::unique_ptr<LabelPrep> (new LabelPrep ()));
        PrepareOne (item, *preps.back ());
    }

    // 2) Anlegen IM Lambda; Status liegt ausserhalb (Lehre aus 0.9.16).
    std::vector<GSErrCode> createErr (preps.size (), NoError);
    bool lambdaStarted = false;
    ACAPI_CallUndoableCommand ("ELM_SAB CreateLabels", [&] () -> GSErrCode {
        lambdaStarted = true;
        for (size_t i = 0; i < preps.size (); ++i) {
            LabelPrep& p = *preps[i];
            if (p.err != NoError) continue;
            createErr[i] = ACAPI_Element_Create (&p.element, &p.memo);
            if (createErr[i] == NoError)
                p.created = p.element.header.guid;
        }
        return NoError;
    });

    for (auto& p : preps)
        ACAPI_DisposeElemMemoHdls (&p->memo);

    // 3) Ruecklese + Antwort NACH dem Lambda.
    GS::HashTable<Int32, GS::UniString> layerCache;
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < preps.size (); ++i) {
        const LabelPrep& p = *preps[i];
        if (p.err != NoError) {
            results (CreateFailedExecutionResult (p.err, p.message));
            continue;
        }
        if (!lambdaStarted) {
            results (CreateFailedExecutionResult (APIERR_GENERAL, "Undo-Lambda wurde nicht gestartet - nichts angelegt"));
            continue;
        }
        if (createErr[i] != NoError) {
            results (CreateFailedExecutionResult (createErr[i], "Etikett konnte nicht angelegt werden"));
            continue;
        }
        API_Element check = {};
        check.header.guid = p.created;
        if (ACAPI_Element_Get (&check) != NoError) {
            results (CreateFailedExecutionResult (APIERR_GENERAL, "Ruecklese fehlgeschlagen (Etikett nicht lesbar)"));
            continue;
        }

        GS::ObjectState ok = CreateSuccessfulExecutionResult ();
        ok.Add ("elementId", CreateGuidObjectState (p.created));
        ok.Add ("parentElementId", CreateGuidObjectState (p.parent));
        FillLabelCommon (ok, check, layerCache);
        ok.Add ("libPartName", LibPartNameOf (check.label.u.symbol.libInd));
        ok.Add ("floorIndRespected", (Int32) check.header.floorInd == p.wantFloor);

        bool match = true;
        if (p.hasBeg && !NearEqual (check.label.begC, p.beg)) match = false;
        if (p.hasMid && !NearEqual (check.label.midC, p.mid)) match = false;
        if (p.hasEnd && !NearEqual (check.label.endC, p.end)) match = false;
        ok.Add ("coordinatesMatch", match);
        results (ok);
    }

    GS::ObjectState undoScope;
    undoScope.Add ("lambdaStarted", lambdaStarted);
    response.Add ("undoScope", undoScope);
    return response;
}

// ---- GetLabelsOfElements ---------------------------------------------------------

GS::Optional<GS::UniString> GetLabelsOfElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "description": "Liefert je Owner-Element seine Etiketten mit EINEM Durchlauf ueber alle Etiketten. Liest nur die aktuelle Datenbank (Grundriss), Etiketten anderer Datenbanken (Schnitte, Ansichten) werden nicht gefunden.",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] } }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> GetLabelsOfElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "labelsOfElements": { "type": "array", "items": { "type": "object" } },
            "scannedLabelCount": { "type": "integer" }
        },
        "required": ["labelsOfElements"]
    })");
}

GS::ObjectState GetLabelsOfElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    std::vector<API_Guid> owners;
    std::vector<bool>     valid;
    GS::HashTable<API_Guid, UInt32> ownerPos;       // Owner -> Position in labelsPerOwner
    std::vector<std::vector<GS::ObjectState>> labelsPerOwner;

    for (const GS::ObjectState& item : elements) {
        const GS::ObjectState* id = item.Get ("elementId");
        const API_Guid g = (id != nullptr) ? GetGuidFromObjectState (*id) : APINULLGuid;
        owners.push_back (g);
        API_Elem_Head head = {};
        head.guid = g;
        const bool ok = (g != APINULLGuid) && (ACAPI_Element_GetHeader (&head) == NoError);
        valid.push_back (ok);
        if (ok && !ownerPos.ContainsKey (g)) {
            ownerPos.Add (g, (UInt32) labelsPerOwner.size ());
            labelsPerOwner.push_back (std::vector<GS::ObjectState> ());
        }
    }

    // EIN Durchlauf ueber alle Etiketten.
    GS::Array<API_Guid> labelGuids;
    ACAPI_Element_GetElemList (API_LabelID, &labelGuids);

    GS::HashTable<Int32, GS::UniString> layerCache;
    GS::HashTable<Int32, GS::UniString> libCache;
    for (const API_Guid& lg : labelGuids) {
        API_Element label = {};
        label.header.guid = lg;
        if (ACAPI_Element_Get (&label) != NoError)
            continue;
        if (!ownerPos.ContainsKey (label.label.parent))
            continue;

        GS::ObjectState o;
        o.Add ("guid", APIGuidToString (lg));
        const bool isSymbol = (label.label.labelClass == APILblClass_Symbol);
        o.Add ("labelClass", GS::UniString (isSymbol ? "Symbol" : "Text"));
        if (isSymbol) {
            const Int32 libInd = (Int32) label.label.u.symbol.libInd;
            if (!libCache.ContainsKey (libInd))
                libCache.Add (libInd, LibPartNameOf (libInd));
            o.Add ("libPartName", libCache.Get (libInd));
        }
        FillLabelCommon (o, label, layerCache);
        labelsPerOwner[ownerPos.Get (label.label.parent)].push_back (o);
    }

    GS::ObjectState response;
    const auto& out = response.AddList<GS::ObjectState> ("labelsOfElements");
    for (size_t i = 0; i < owners.size (); ++i) {
        GS::ObjectState entry;
        if (!valid[i]) {
            entry.Add ("elementId", CreateGuidObjectState (owners[i]));
            entry.Add ("error", GS::UniString ("Element nicht gefunden"));
            out (entry);
            continue;
        }
        entry.Add ("elementId", CreateGuidObjectState (owners[i]));
        const auto& labels = entry.AddList<GS::ObjectState> ("labels");
        for (const GS::ObjectState& l : labelsPerOwner[ownerPos.Get (owners[i])])
            labels (l);
        out (entry);
    }
    response.Add ("scannedLabelCount", (Int32) labelGuids.GetSize ());
    return response;
}
