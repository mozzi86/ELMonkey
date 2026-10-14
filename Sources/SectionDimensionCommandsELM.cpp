#include "SectionDimensionCommandsELM.hpp"
#include "MigrationHelper.hpp"

#include <functional>

namespace {

// Führt fn in der angegebenen Datenbank aus (oder in der aktuellen, wenn keine Id kommt).
// Gibt einen Fehlertext zurück, wenn der Wechsel scheitert.
GS::Optional<GS::UniString> RunInDatabase (const GS::Optional<API_Guid>& databaseId, const std::function<GSErrCode ()>& fn)
{
    if (!databaseId.HasValue ()) {
        const GSErrCode err = fn ();
        if (err != NoError) {
            return GS::UniString::Printf ("Action failed (%d).", (int) err);
        }
        return GS::NoValue;
    }
    GS::Optional<GS::UniString> failure;
    GS::Array<API_Guid> ids;
    ids.Push (databaseId.Get ());
    const GSErrCode err = ExecuteActionForEachDatabase (ids, fn,
        [] () {},
        [&] (GSErrCode code, const GS::UniString& message) {
            failure = message + GS::UniString::Printf (" (%d)", (int) code);
        });
    if (failure.HasValue ()) {
        return failure;
    }
    if (err != NoError) {
        return GS::UniString::Printf ("Database switch failed (%d).", (int) err);
    }
    return GS::NoValue;
}

GS::Optional<API_Guid> GetOptionalDatabaseId (const GS::ObjectState& parameters, const char* fieldName)
{
    const GS::ObjectState* dbOs = parameters.Get (fieldName);
    if (dbOs == nullptr) {
        return GS::NoValue;
    }
    return GetGuidFromObjectState (*dbOs);
}

GS::ObjectState CreateBoxObjectState (const API_Box3D& box)
{
    GS::ObjectState os;
    os.Add ("xMin", box.xMin);
    os.Add ("yMin", box.yMin);
    os.Add ("zMin", box.zMin);
    os.Add ("xMax", box.xMax);
    os.Add ("yMax", box.yMax);
    os.Add ("zMax", box.zMax);
    return os;
}

GS::ObjectState CreateNoteObjectState (const API_NoteType& note)
{
    GS::ObjectState os;
    os.Add ("pen", (Int32) note.notePen);
    os.Add ("font", (Int32) note.noteFont);
    os.Add ("size", note.noteSize);
    os.Add ("angle", note.noteAngle);
    os.Add ("contentType", note.contentType == API_NoteContent_Custom ? GS::UniString ("Custom") : GS::UniString ("Measured"));
    if (note.contentUStr != nullptr) {
        os.Add ("text", *note.contentUStr);
    } else {
        os.Add ("text", GS::UniString (note.content));
    }
    os.Add ("fixPos", note.fixPos);
    return os;
}

void ResetNoteText (API_NoteType& note)
{
    note.contentType = API_NoteContent_Measured;
    note.contentUStr = nullptr;
    note.content[0] = '\0';
}

}

// -----------------------------------------------------------------------------
// GetSectionElementsELM
// -----------------------------------------------------------------------------

GS::ObjectState GetSectionElementsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> databases;
    parameters.Get ("databases", databases);
    bool withBounds = true;
    parameters.Get ("withBounds", withBounds);

    GS::ObjectState response;
    const auto& result = response.AddList<GS::ObjectState> ("databases");

    auto collect = [&] (GS::ObjectState& dbResult) -> GSErrCode {
        GS::Array<API_Guid> elemList;
        GSErrCode err = ACAPI_Element_GetElemList (API_SectElemID, &elemList);
        if (err != NoError) {
            return err;
        }
        const auto& items = dbResult.AddList<GS::ObjectState> ("sectionElements");
        for (const API_Guid& guid : elemList) {
            API_Element element = {};
            element.header.guid = guid;
            if (ACAPI_Element_Get (&element) != NoError) {
                continue;
            }
            GS::ObjectState item;
            item.Add ("sectionElementId", CreateGuidObjectStateELM (guid));
            item.Add ("parentId", CreateGuidObjectStateELM (element.sectElem.parentGuid));
            item.Add ("parentType", GetElementTypeNonLocalizedName (GetElemTypeId (element.sectElem.parentType)));
            item.Add ("ownerId", CreateGuidObjectStateELM (element.sectElem.ownerGuid));
            item.Add ("layerIndex", GetAttributeIndex (element.header.layer));
            if (withBounds) {
                API_Box3D box = {};
                if (ACAPI_Element_CalcBounds (&element.header, &box) == NoError) {
                    item.Add ("bounds", CreateBoxObjectState (box));
                }
            }
            items (item);
        }
        return NoError;
    };

    if (databases.IsEmpty ()) {
        GS::ObjectState dbResult;
        const GSErrCode err = collect (dbResult);
        if (err != NoError) {
            return CreateErrorResponse (err, "Section elements of current database not readable.");
        }
        result (dbResult);
        return response;
    }

    for (const GS::ObjectState& dbItem : databases) {
        const API_Guid dbId = GetGuidFromDatabaseArrayItem (dbItem);
        GS::ObjectState dbResult;
        dbResult.Add ("databaseId", CreateGuidObjectStateELM (dbId));
        auto failure = RunInDatabase (dbId, [&] () { return collect (dbResult); });
        if (failure.HasValue ()) {
            dbResult.Add ("error", failure.Get ());
        }
        result (dbResult);
    }
    return response;
}

// -----------------------------------------------------------------------------
// GetDimensionDetailsELM
// -----------------------------------------------------------------------------

GS::ObjectState GetDimensionDetailsELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);
    const GS::Optional<API_Guid> databaseId = GetOptionalDatabaseId (parameters, "databaseId");

    GS::ObjectState response;
    const auto& result = response.AddList<GS::ObjectState> ("dimensions");

    auto failure = RunInDatabase (databaseId, [&] () -> GSErrCode {
        for (const GS::ObjectState& item : elements) {
            const API_Guid guid = GetGuidFromElementsArrayItem (item);
            API_Element element = {};
            element.header.guid = guid;
            GSErrCode err = ACAPI_Element_Get (&element);
            if (err != NoError || GetElemTypeId (element.header) != API_DimensionID) {
                result (CreateErrorResponse (APIERR_BADID, "Not a linear dimension in this database."));
                continue;
            }
            API_ElementMemo memo = {};
            err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_All);
            const GS::OnExit cleanup ([&] () { ACAPI_DisposeElemMemoHdls (&memo); });

            const API_DimensionType& dim = element.dimension;
            GS::ObjectState os;
            os.Add ("elementId", CreateGuidObjectStateELM (guid));
            os.Add ("layerIndex", GetAttributeIndex (element.header.layer));
            os.Add ("floorIndex", (Int32) element.header.floorInd);
            os.Add ("linPen", (Int32) dim.linPen);
            os.Add ("textPos", (Int32) dim.textPos);
            os.Add ("textWay", (Int32) dim.textWay);
            os.Add ("dimAppear", (Int32) dim.dimAppear);
            os.Add ("defStaticDim", dim.defStaticDim);
            os.Add ("defWitnessForm", (Int32) dim.defWitnessForm);
            os.Add ("defWitnessVal", dim.defWitnessVal);
            os.Add ("defNote", CreateNoteObjectState (dim.defNote));
            os.Add ("refC", Create2DCoordinateObjectState (dim.refC));
            os.Add ("direction", Create2DCoordinateObjectState (API_Coord {dim.direction.x, dim.direction.y}));
            os.Add ("horizontalText", dim.horizontalText);
            os.Add ("nDimElem", (Int32) dim.nDimElem);

            const auto& points = os.AddList<GS::ObjectState> ("witnessPoints");
            if (err == NoError && memo.dimElems != nullptr) {
                for (Int32 i = 0; i < dim.nDimElem; ++i) {
                    const API_DimElem& de = (*memo.dimElems)[i];
                    GS::ObjectState p;
                    p.Add ("elementType", GetElementTypeNonLocalizedName (GetElemTypeId (de.base.base.type)));
                    p.Add ("elementId", CreateGuidObjectStateELM (de.base.base.guid));
                    p.Add ("line", de.base.base.line);
                    p.Add ("special", (Int32) de.base.base.special);
                    p.Add ("inIndex", (Int32) de.base.base.inIndex);
                    p.Add ("nodeId", (double) de.base.base.node_id);
                    p.Add ("nodeStatus", (Int32) de.base.base.node_status);
                    p.Add ("nodeType", (Int32) de.base.base.node_typ);
                    p.Add ("loc", Create2DCoordinateObjectState (de.base.loc));
                    p.Add ("pos", Create2DCoordinateObjectState (de.pos));
                    p.Add ("dimVal", de.dimVal);
                    p.Add ("witnessVal", de.witnessVal);
                    p.Add ("witnessForm", (Int32) de.witnessForm);
                    p.Add ("fixedPos", de.fixedPos);
                    p.Add ("note", CreateNoteObjectState (de.note));
                    points (p);
                }
            }
            result (os);
        }
        return NoError;
    });
    if (failure.HasValue ()) {
        return CreateErrorResponse (APIERR_GENERAL, failure.Get ());
    }
    return response;
}

// -----------------------------------------------------------------------------
// CreateDimensionsLikeTemplateELM
// -----------------------------------------------------------------------------

GS::ObjectState CreateDimensionsLikeTemplateELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    const GS::ObjectState* templateIdOs = parameters.Get ("templateDimensionId");
    if (templateIdOs == nullptr) {
        return CreateErrorResponse (APIERR_BADPARS, "templateDimensionId missing.");
    }
    const API_Guid templateGuid = GetGuidFromObjectState (*templateIdOs);
    const GS::Optional<API_Guid> templateDbId = GetOptionalDatabaseId (parameters, "templateDatabaseId");
    const GS::Optional<API_Guid> targetDbId = GetOptionalDatabaseId (parameters, "targetDatabaseId");

    GS::Array<GS::ObjectState> dimensionsData;
    parameters.Get ("dimensionsData", dimensionsData);

    // 1) Vorlage lesen (Element + erster Messpunkt als Muster für Text/Hilfslinie).
    API_Element templ = {};
    API_DimElem templPoint = {};
    bool hasTemplPoint = false;
    auto readFailure = RunInDatabase (templateDbId, [&] () -> GSErrCode {
        templ.header.guid = templateGuid;
        GSErrCode err = ACAPI_Element_Get (&templ);
        if (err != NoError) {
            return err;
        }
        if (GetElemTypeId (templ.header) != API_DimensionID) {
            return APIERR_BADID;
        }
        API_ElementMemo memo = {};
        if (ACAPI_Element_GetMemo (templateGuid, &memo, APIMemoMask_All) == NoError && memo.dimElems != nullptr && templ.dimension.nDimElem > 0) {
            templPoint = (*memo.dimElems)[0];
            hasTemplPoint = true;
        }
        ACAPI_DisposeElemMemoHdls (&memo);
        return NoError;
    });
    if (readFailure.HasValue ()) {
        return CreateErrorResponse (APIERR_BADID, GS::UniString ("Template dimension not readable: ") + readFailure.Get ());
    }
    // Zeiger aus dem bereits freigegebenen Vorlage-Memo nie weiterreichen.
    ResetNoteText (templ.dimension.defNote);
    if (hasTemplPoint) {
        ResetNoteText (templPoint.note);
    }

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("elements");

    // 2) In der Zieldatenbank anlegen.
    auto createFailure = RunInDatabase (targetDbId, [&] () -> GSErrCode {
        return ACAPI_CallUndoableCommand ("ELM_SAB Dimensions like template", [&] () -> GSErrCode {
            for (const GS::ObjectState& data : dimensionsData) {
                const GS::ObjectState* refOs = data.Get ("referencePoint");
                const GS::ObjectState* dirOs = data.Get ("direction");
                GS::Array<GS::ObjectState> pointsData;
                data.Get ("witnessPoints", pointsData);
                if (refOs == nullptr || dirOs == nullptr || pointsData.GetSize () < 1) {
                    results (CreateErrorResponse (APIERR_BADPARS, "referencePoint, direction and witnessPoints are required."));
                    continue;
                }

                API_Element element = templ;
                element.header.guid = APINULLGuid;
                element.dimension.refC = Get2DCoordinateFromObjectState (*refOs);
                const API_Coord dir = Get2DCoordinateFromObjectState (*dirOs);
                element.dimension.direction = {dir.x, dir.y};
                element.dimension.nDimElem = (Int32) pointsData.GetSize ();
                bool staticDim = templ.dimension.defStaticDim;
                data.Get ("staticDimension", staticDim);
                element.dimension.defStaticDim = staticDim;

                API_ElementMemo memo = {};
                const GS::OnExit cleanup ([&] () { ACAPI_DisposeElemMemoHdls (&memo); });
                memo.dimElems = reinterpret_cast<API_DimElem**> (BMhAllClear (element.dimension.nDimElem * sizeof (API_DimElem)));
                if (memo.dimElems == nullptr || *memo.dimElems == nullptr) {
                    results (CreateErrorResponse (APIERR_MEMFULL, "Out of memory for witness points."));
                    continue;
                }

                GS::Optional<GS::UniString> pointError;
                for (UIndex i = 0; i < pointsData.GetSize (); ++i) {
                    const GS::ObjectState& pd = pointsData[i];
                    API_DimElem& de = (*memo.dimElems)[i];
                    if (hasTemplPoint) {
                        de.note = templPoint.note;
                        de.witnessForm = templPoint.witnessForm;
                        de.witnessVal = templPoint.witnessVal;
                    } else {
                        de.note = templ.dimension.defNote;
                        de.witnessForm = templ.dimension.defWitnessForm;
                        de.witnessVal = templ.dimension.defWitnessVal;
                    }
                    double witnessVal = 0.0;
                    if (pd.Get ("witnessVal", witnessVal)) {
                        de.witnessVal = witnessVal;
                    }
                    Int32 witnessForm = 0;
                    if (pd.Get ("witnessForm", witnessForm)) {
                        de.witnessForm = (API_WitnessID) witnessForm;
                    }

                    const GS::ObjectState* coordOs = pd.Get ("coordinate");
                    API_Coord coord = {0.0, 0.0};
                    if (coordOs != nullptr) {
                        coord = Get2DCoordinateFromObjectState (*coordOs);
                    }
                    de.base.loc = coord;
                    de.base.loc3D = {coord.x, coord.y, 0.0};

                    const GS::ObjectState* idOs = pd.Get ("elementId");
                    if (idOs != nullptr) {
                        const API_Guid baseGuid = GetGuidFromObjectState (*idOs);
                        API_Elem_Head head = {};
                        if (!LoadElementHeaderByGuid (baseGuid, head)) {
                            pointError = GS::UniString::Printf ("Witness %d: base element not in this database.", (int) i);
                            break;
                        }
                        de.base.base.type = API_ElemType (GetElemTypeId (head));
                        de.base.base.guid = baseGuid;
                        bool line = false;
                        pd.Get ("line", line);
                        de.base.base.line = line;
                        Int32 v = 0;
                        if (pd.Get ("special", v)) { de.base.base.special = (char) v; }
                        if (pd.Get ("inIndex", v)) { de.base.base.inIndex = v; }
                        if (pd.Get ("nodeStatus", v)) { de.base.base.node_status = (short) v; }
                        if (pd.Get ("nodeType", v)) { de.base.base.node_typ = (short) v; }
                        double nodeId = 0.0;
                        if (pd.Get ("nodeId", nodeId)) { de.base.base.node_id = (UInt32) nodeId; }
                    } else {
                        if (coordOs == nullptr) {
                            pointError = GS::UniString::Printf ("Witness %d: neither elementId nor coordinate.", (int) i);
                            break;
                        }
                        de.base.base.type = API_ElemType (API_ZombieElemID);
                        de.base.base.guid = APINULLGuid;
                    }

                    GS::UniString customText;
                    if (pd.Get ("customText", customText) && !customText.IsEmpty ()) {
                        de.note.contentType = API_NoteContent_Custom;
                        de.note.contentUStr = new GS::UniString (customText);
                    }
                }
                if (pointError.HasValue ()) {
                    results (CreateErrorResponse (APIERR_BADPARS, pointError.Get ()));
                    continue;
                }

                GSErrCode err = ACAPI_Element_Create (&element, &memo);
                if (err != NoError) {
                    results (CreateErrorResponse (err, "Failed to create dimension."));
                    continue;
                }

                // Rücklese: Anzahl Messpunkte und Bezüge der neuen Kette.
                GS::ObjectState ok = CreateElementIdObjectState (element.header.guid);
                API_Element check = {};
                check.header.guid = element.header.guid;
                if (ACAPI_Element_Get (&check) == NoError) {
                    ok.Add ("witnessCount", (Int32) check.dimension.nDimElem);
                    API_ElementMemo checkMemo = {};
                    if (ACAPI_Element_GetMemo (check.header.guid, &checkMemo, APIMemoMask_All) == NoError && checkMemo.dimElems != nullptr) {
                        Int32 associated = 0;
                        for (Int32 k = 0; k < check.dimension.nDimElem; ++k) {
                            if ((*checkMemo.dimElems)[k].base.base.guid != APINULLGuid) {
                                ++associated;
                            }
                        }
                        ok.Add ("associatedCount", associated);
                    }
                    ACAPI_DisposeElemMemoHdls (&checkMemo);
                }
                results (ok);
            }
            return NoError;
        });
    });
    if (createFailure.HasValue ()) {
        return CreateErrorResponse (APIERR_GENERAL, createFailure.Get ());
    }
    return response;
}

// -----------------------------------------------------------------------------
// SetDimensionAppearanceELM
// -----------------------------------------------------------------------------

GS::ObjectState SetDimensionAppearanceELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> dimensions;
    parameters.Get ("dimensions", dimensions);
    const GS::Optional<API_Guid> databaseId = GetOptionalDatabaseId (parameters, "databaseId");

    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");

    auto failure = RunInDatabase (databaseId, [&] () -> GSErrCode {
        return ACAPI_CallUndoableCommand ("ELM_SAB Dimension appearance", [&] () -> GSErrCode {
            for (const GS::ObjectState& item : dimensions) {
                const GS::ObjectState* idOs = item.Get ("elementId");
                if (idOs == nullptr) {
                    results (CreateErrorResponse (APIERR_BADPARS, "elementId missing."));
                    continue;
                }
                const API_Guid guid = GetGuidFromObjectState (*idOs);
                API_Element element = {};
                element.header.guid = guid;
                GSErrCode err = ACAPI_Element_Get (&element);
                if (err != NoError || GetElemTypeId (element.header) != API_DimensionID) {
                    results (CreateErrorResponse (APIERR_BADID, "Not a linear dimension in this database."));
                    continue;
                }
                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_All);
                const GS::OnExit cleanup ([&] () { ACAPI_DisposeElemMemoHdls (&memo); });
                if (err != NoError || memo.dimElems == nullptr) {
                    results (CreateErrorResponse (err, "Dimension memo not readable."));
                    continue;
                }

                bool hasCurtainWallNodes = false;
                for (Int32 i = 0; i < element.dimension.nDimElem; ++i) {
                    const API_Base& b = (*memo.dimElems)[i].base.base;
                    if (b.guid != APINULLGuid && b.node_typ == 2101) {
                        hasCurtainWallNodes = true;
                    }
                }
                if (hasCurtainWallNodes) {
                    results (CreateErrorResponse (APIERR_REFUSEDPAR, "Dimension references curtain wall nodes (2101); Archicad would delete it on change. Refused."));
                    continue;
                }

                API_Element mask;
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                double noteSize = 0.0;
                const bool setSize = item.Get ("noteSize", noteSize) && noteSize > 0.0;
                if (setSize) {
                    element.dimension.defNote.noteSize = noteSize;
                    ACAPI_ELEMENT_MASK_SET (mask, API_DimensionType, defNote.noteSize);
                    for (Int32 i = 0; i < element.dimension.nDimElem; ++i) {
                        (*memo.dimElems)[i].note.noteSize = noteSize;
                    }
                }
                GS::Array<GS::ObjectState> witnessValues;
                item.Get ("witnessValues", witnessValues);
                bool badIndex = false;
                for (const GS::ObjectState& wv : witnessValues) {
                    Int32 index = -1;
                    double value = 0.0;
                    if (!wv.Get ("index", index) || !wv.Get ("witnessVal", value) || index < 0 || index >= element.dimension.nDimElem) {
                        badIndex = true;
                        break;
                    }
                    (*memo.dimElems)[index].witnessVal = value;
                }
                GS::Array<GS::ObjectState> witnessCoordinates;
                item.Get ("witnessCoordinates", witnessCoordinates);
                for (const GS::ObjectState& wc : witnessCoordinates) {
                    Int32 index = -1;
                    double x = 0.0, y = 0.0;
                    if (!wc.Get ("index", index) || !wc.Get ("x", x) || !wc.Get ("y", y) || index < 0 || index >= element.dimension.nDimElem
                        || (*memo.dimElems)[index].base.base.guid != APINULLGuid) {
                        badIndex = true;
                        break;
                    }
                    API_DimElem& de = (*memo.dimElems)[index];
                    de.base.loc = {x, y};
                    de.base.loc3D = {x, y, 0.0};
                }
                if (badIndex) {
                    results (CreateErrorResponse (APIERR_BADPARS, "Invalid witness index/value, or coordinate change requested for an associative point."));
                    continue;
                }

                err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_All, true);
                if (err != NoError) {
                    results (CreateErrorResponse (err, "Failed to change dimension."));
                    continue;
                }

                // Rücklese
                GS::ObjectState ok = CreateElementIdObjectState (guid);
                API_Element check = {};
                check.header.guid = guid;
                API_ElementMemo checkMemo = {};
                if (ACAPI_Element_Get (&check) == NoError && ACAPI_Element_GetMemo (guid, &checkMemo, APIMemoMask_All) == NoError && checkMemo.dimElems != nullptr) {
                    ok.Add ("defNoteSize", check.dimension.defNote.noteSize);
                    const auto& sizes = ok.AddList<double> ("noteSizes");
                    const auto& vals = ok.AddList<double> ("witnessVals");
                    const auto& locs = ok.AddList<GS::ObjectState> ("locs");
                    for (Int32 k = 0; k < check.dimension.nDimElem; ++k) {
                        locs (Create2DCoordinateObjectState ((*checkMemo.dimElems)[k].base.loc));
                    }
                    Int32 associated = 0;
                    for (Int32 k = 0; k < check.dimension.nDimElem; ++k) {
                        sizes ((*checkMemo.dimElems)[k].note.noteSize);
                        vals ((*checkMemo.dimElems)[k].witnessVal);
                        if ((*checkMemo.dimElems)[k].base.base.guid != APINULLGuid) {
                            ++associated;
                        }
                    }
                    ok.Add ("associatedCount", associated);
                }
                ACAPI_DisposeElemMemoHdls (&checkMemo);
                results (ok);
            }
            return NoError;
        });
    });
    if (failure.HasValue ()) {
        return CreateErrorResponse (APIERR_GENERAL, failure.Get ());
    }
    return response;
}

// -----------------------------------------------------------------------------
// Interior elevations: vertical range
// -----------------------------------------------------------------------------

namespace {

GS::ObjectState CreateSegmentRangeObjectState (const API_SectionSegment& s)
{
    GS::ObjectState os;
    os.Add ("vertRange", s.vertRange == APIVerRange_Limited ? GS::UniString ("Limited") : (s.vertRange == APIVerRange_FitToZoneRange ? GS::UniString ("FitToZoneRange") : GS::UniString ("Infinite")));
    os.Add ("vertMin", s.vertMin);
    os.Add ("vertMax", s.vertMax);
    os.Add ("relativeToStory", s.relativeToStory);
    os.Add ("storyBase", (Int32) s.verticalRangeStoryBaseNumber);
    os.Add ("id", GS::UniString (s.cutPlIdStr));
    os.Add ("name", GS::UniString (s.cutPlName));
    os.Add ("databaseElemSetId", CreateGuidObjectStateELM (s.databaseID.elemSetId));
    return os;
}

GS::Optional<GS::UniString> RunInFloorPlan (const std::function<GSErrCode ()>& fn)
{
    API_DatabaseInfo start = {};
    ACAPI_Database_GetCurrentDatabase (&start);
    API_DatabaseInfo plan = {};
    plan.typeID = APIWind_FloorPlanID;
    GSErrCode err = ACAPI_Database_ChangeCurrentDatabase (&plan);
    if (err != NoError) {
        return GS::UniString ("Floor plan database not reachable.");
    }
    err = fn ();
    ACAPI_Database_ChangeCurrentDatabase (&start);
    if (err != NoError) {
        return GS::UniString::Printf ("Action failed (%d).", (int) err);
    }
    return GS::NoValue;
}

}

GS::ObjectState GetInteriorElevationsELMCommand::Execute (const GS::ObjectState&, GS::ProcessControl&) const
{
    GS::ObjectState response;
    const auto& list = response.AddList<GS::ObjectState> ("interiorElevations");
    auto failure = RunInFloorPlan ([&] () -> GSErrCode {
        GS::Array<API_Guid> elemList;
        GSErrCode err = ACAPI_Element_GetElemList (API_InteriorElevationID, &elemList);
        if (err != NoError) {
            return err;
        }
        for (const API_Guid& guid : elemList) {
            API_Element element = {};
            element.header.guid = guid;
            if (ACAPI_Element_Get (&element) != NoError) {
                continue;
            }
            GS::ObjectState os;
            os.Add ("elementId", CreateGuidObjectStateELM (guid));
            os.Add ("floorIndex", (Int32) element.header.floorInd);
            os.Add ("nSegments", (Int32) element.interiorElevation.nSegments);
            os.Add ("common", CreateSegmentRangeObjectState (element.interiorElevation.segment));
            const auto& segs = os.AddList<GS::ObjectState> ("segments");
            API_ElementMemo memo = {};
            if (ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_SectionSegments) == NoError && memo.intElevSegments != nullptr) {
                for (UInt32 i = 0; i < element.interiorElevation.nSegments; ++i) {
                    segs (CreateSegmentRangeObjectState (memo.intElevSegments[i]));
                }
            }
            ACAPI_DisposeElemMemoHdls (&memo);
            list (os);
        }
        return NoError;
    });
    if (failure.HasValue ()) {
        return CreateErrorResponse (APIERR_GENERAL, failure.Get ());
    }
    return response;
}

GS::ObjectState SetInteriorElevationVerticalRangeELMCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    parameters.Get ("interiorElevations", items);
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("results");

    auto failure = RunInFloorPlan ([&] () -> GSErrCode {
        return ACAPI_CallUndoableCommand ("ELM_SAB Interior elevation vertical range", [&] () -> GSErrCode {
            for (const GS::ObjectState& item : items) {
                const GS::ObjectState* idOs = item.Get ("elementId");
                double vertMax = 0.0;
                if (idOs == nullptr || !item.Get ("vertMax", vertMax)) {
                    results (CreateErrorResponse (APIERR_BADPARS, "elementId and vertMax are required."));
                    continue;
                }
                const API_Guid guid = GetGuidFromObjectState (*idOs);
                API_Element element = {};
                element.header.guid = guid;
                GSErrCode err = ACAPI_Element_Get (&element);
                if (err != NoError || GetElemTypeId (element.header) != API_InteriorElevationID) {
                    results (CreateErrorResponse (APIERR_BADID, "Not an interior elevation."));
                    continue;
                }
                double vertMin = 0.0;
                const bool setMin = item.Get ("vertMin", vertMin);

                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_SectionSegments);
                const GS::OnExit cleanup ([&] () { ACAPI_DisposeElemMemoHdls (&memo); });
                if (err != NoError) {
                    results (CreateErrorResponse (err, "Segments not readable."));
                    continue;
                }

                auto apply = [&] (API_SectionSegment& s) {
                    s.vertRange = APIVerRange_Limited;
                    s.vertMax = vertMax;
                    if (setMin) {
                        s.vertMin = vertMin;
                    }
                };
                apply (element.interiorElevation.segment);
                if (memo.intElevSegments != nullptr) {
                    for (UInt32 i = 0; i < element.interiorElevation.nSegments; ++i) {
                        apply (memo.intElevSegments[i]);
                    }
                }

                API_Element mask;
                ACAPI_ELEMENT_MASK_CLEAR (mask);
                ACAPI_ELEMENT_MASK_SET (mask, API_InteriorElevationType, segment.vertRange);
                ACAPI_ELEMENT_MASK_SET (mask, API_InteriorElevationType, segment.vertMax);
                if (setMin) {
                    ACAPI_ELEMENT_MASK_SET (mask, API_InteriorElevationType, segment.vertMin);
                }
                err = ACAPI_Element_Change (&element, &mask, &memo, APIMemoMask_SectionSegments, true);
                if (err != NoError) {
                    results (CreateErrorResponse (err, "Failed to change interior elevation."));
                    continue;
                }

                GS::ObjectState ok = CreateElementIdObjectState (guid);
                API_Element check = {};
                check.header.guid = guid;
                if (ACAPI_Element_Get (&check) == NoError) {
                    ok.Add ("common", CreateSegmentRangeObjectState (check.interiorElevation.segment));
                    API_ElementMemo checkMemo = {};
                    if (ACAPI_Element_GetMemo (guid, &checkMemo, APIMemoMask_SectionSegments) == NoError && checkMemo.intElevSegments != nullptr) {
                        const auto& segs = ok.AddList<GS::ObjectState> ("segments");
                        for (UInt32 i = 0; i < check.interiorElevation.nSegments; ++i) {
                            segs (CreateSegmentRangeObjectState (checkMemo.intElevSegments[i]));
                        }
                    }
                    ACAPI_DisposeElemMemoHdls (&checkMemo);
                }
                results (ok);
            }
            return NoError;
        });
    });
    if (failure.HasValue ()) {
        return CreateErrorResponse (APIERR_GENERAL, failure.Get ());
    }
    return response;
}
