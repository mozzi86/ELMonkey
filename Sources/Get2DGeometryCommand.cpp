#include "Get2DGeometryCommand.hpp"

GS::Optional<GS::UniString> Get2DGeometryCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "elementId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": ["guid"]
                        }
                    },
                    "required": ["elementId"]
                }
            }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> Get2DGeometryCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "geometryOfElements": {
                "type": "array",
                "items": { "type": "object" }
            }
        },
        "required": ["geometryOfElements"]
    })");
}

static GS::ObjectState CoordOS (double x, double y)
{
    GS::ObjectState os;
    os.Add ("x", x);
    os.Add ("y", y);
    return os;
}

// ---- 0.9.19: ShapePrims-Fallback fuer alle uebrigen Elementtypen ------------------
// Liefert die Zeichenprimitive, wie sie im Grundriss gezeichnet werden (Tuer-/Fenster-
// Aufschlag als "arc", Symbolstriche als "line" usw.). Muster: CollectCutFillPolygons in
// ElementCommands.cpp. Eigene Namen im anonymen namespace.
namespace {

struct ElmGeoPrimCtx {
    GS::Array<GS::ObjectState> prims;
    Int32 skipped = 0;
    Int32 hatchBorderDepth = 0;
};

thread_local ElmGeoPrimCtx* tl_elmGeoPrimCtx = nullptr;

GSErrCode ElmGeoCollectPrims (const API_PrimElement* primElem, const void* par1, const void* par2, const void* par3)
{
    ElmGeoPrimCtx* ctx = tl_elmGeoPrimCtx;
    if (ctx == nullptr || primElem == nullptr)
        return NoError;

    switch (primElem->header.typeID) {
        case API_PrimCtrl_HatchBorderBegID:
            ++ctx->hatchBorderDepth;
            break;
        case API_PrimCtrl_HatchBorderEndID:
            if (ctx->hatchBorderDepth > 0) --ctx->hatchBorderDepth;
            break;
        case API_PrimPointID: {
            GS::ObjectState o;
            o.Add ("kind", GS::UniString ("point"));
            o.Add ("coordinate", CoordOS (primElem->point.loc.x, primElem->point.loc.y));
            ctx->prims.Push (o);
            break;
        }
        case API_PrimLineID: {
            GS::ObjectState o;
            o.Add ("kind", GS::UniString ("line"));
            o.Add ("begCoordinate", CoordOS (primElem->line.c1.x, primElem->line.c1.y));
            o.Add ("endCoordinate", CoordOS (primElem->line.c2.x, primElem->line.c2.y));
            o.Add ("penIndex", (Int32) primElem->line.head.pen.penIndex);
            ctx->prims.Push (o);
            break;
        }
        case API_PrimArcID: {
            GS::ObjectState o;
            o.Add ("kind", GS::UniString ("arc"));
            o.Add ("origin", CoordOS (primElem->arc.orig.x, primElem->arc.orig.y));
            o.Add ("radius", primElem->arc.r);
            o.Add ("begAngle", primElem->arc.begAng);
            o.Add ("endAngle", primElem->arc.endAng);
            o.Add ("ratio", primElem->arc.ratio);
            o.Add ("angle", primElem->arc.angle);
            o.Add ("whole", primElem->arc.whole);
            o.Add ("penIndex", (Int32) primElem->arc.head.pen.penIndex);
            ctx->prims.Push (o);
            break;
        }
        case API_PrimTextID: {
            GS::ObjectState o;
            o.Add ("kind", GS::UniString ("text"));
            o.Add ("location", CoordOS (primElem->text.loc.x, primElem->text.loc.y));
            if (par2 != nullptr) {
                // par2 = Zeiger auf UniCode-Text (APIdefs_Callback.h); Terminator innerhalb der
                // Blockgroesse suchen, damit kein Lesen ueber das Ende hinaus passiert.
                const GS::uchar_t* chars = static_cast<const GS::uchar_t*> (par2);
                const GSSize bytes = BMGetPtrSize (reinterpret_cast<GSPtr> (const_cast<void*> (par2)));
                const GSSize maxChars = bytes / (GSSize) sizeof (GS::uchar_t);
                bool terminated = false;
                for (GSSize i = 0; i < maxChars; ++i) {
                    if (chars[i] == 0) { terminated = true; break; }
                }
                if (terminated)
                    o.Add ("content", GS::UniString (chars));
            }
            ctx->prims.Push (o);
            break;
        }
        case API_PrimPLineID:
        case API_PrimPolyID: {
            const bool isPoly = (primElem->header.typeID == API_PrimPolyID);
            const Int32 nCoords = isPoly ? primElem->poly.nCoords : primElem->pline.nCoords;
            const Int32 nArcs   = isPoly ? primElem->poly.nArcs   : primElem->pline.nArcs;
            const API_Coord* coords = static_cast<const API_Coord*> (par1);
            if (coords == nullptr || nCoords <= 0)
                break;
            GS::ObjectState o;
            o.Add ("kind", GS::UniString ("poly"));
            o.Add ("closed", isPoly);
            const auto& cl = o.AddList<GS::ObjectState> ("coordinates");
            for (Int32 i = 1; i <= nCoords; ++i)            // Koordinaten sind 1-basiert
                cl (CoordOS (coords[i].x, coords[i].y));
            if (isPoly && par2 != nullptr) {
                const Int32* pends = static_cast<const Int32*> (par2);
                const auto& el = o.AddList<GS::ObjectState> ("subPolyEnds");
                for (Int32 s = 1; s <= primElem->poly.nSubPolys; ++s)
                    el (GS::ObjectState ("end", pends[s]));
            }
            const API_PolyArc* parcs = static_cast<const API_PolyArc*> (par3);
            if (parcs != nullptr && nArcs > 0) {
                const auto& al = o.AddList<GS::ObjectState> ("arcs");
                for (Int32 a = 0; a < nArcs; ++a) {         // Bogenfeld 0-basiert (wie Memo-parcs)
                    GS::ObjectState ao;
                    ao.Add ("begIndex", (Int32) parcs[a].begIndex);
                    ao.Add ("endIndex", (Int32) parcs[a].endIndex);
                    ao.Add ("arcAngle", parcs[a].arcAngle);
                    al (ao);
                }
            }
            o.Add ("insideHatchBorder", ctx->hatchBorderDepth > 0);
            ctx->prims.Push (o);
            break;
        }
        case API_PrimCtrl_BegID:
        case API_PrimCtrl_EndID:
        case API_PrimCtrl_HatchLinesBegID:
        case API_PrimCtrl_HatchLinesEndID:
        case API_PrimCtrl_ElementRefID:
            break;      // Steuercodes, keine Geometrie
        default:
            ++ctx->skipped;
            break;
    }
    return NoError;
}

GSErrCode CollectShapePrims (const API_Guid& guid, ElmGeoPrimCtx& ctx)
{
    tl_elmGeoPrimCtx = &ctx;
    const GS::OnExit guard ([] () { tl_elmGeoPrimCtx = nullptr; });

    API_Elem_Head elemHead = {};
    elemHead.guid = guid;

    API_ShapePrimsParams params = {};
    params.dontClip   = true;
    params.allStories = true;
    params.polygon    = nullptr;

#if defined(ServerMainVers_2700)
    return ACAPI_DrawingPrimitive_ShapePrimsExt (elemHead, ElmGeoCollectPrims, &params);
#else
    return ACAPI_Element_ShapePrimsExt (elemHead, ElmGeoCollectPrims, &params);
#endif
}

} // namespace

GS::ObjectState Get2DGeometryCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    GS::ObjectState response;
    const auto& geometryOfElements = response.AddList<GS::ObjectState> ("geometryOfElements");

    for (const GS::ObjectState& elementItem : elements) {
        const GS::ObjectState* elementId = elementItem.Get ("elementId");
        if (elementId == nullptr) {
            geometryOfElements (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt"));
            continue;
        }

        API_Element element = {};
        element.header.guid = GetGuidFromObjectState (*elementId);
        GSErrCode err = ACAPI_Element_Get (&element);
        if (err != NoError) {
            geometryOfElements (CreateFailedExecutionResult (err, "Element nicht gefunden"));
            continue;
        }

        GS::ObjectState geo;
        geo.Add ("success", true);
        geo.Add ("layerIndex", (Int32) element.header.layer.ToInt32_Deprecated ());
        geo.Add ("floorIndex", (Int32) element.header.floorInd);

        switch (element.header.type.typeID) {
            case API_LineID: {
                geo.Add ("elementType", "Line");
                geo.Add ("begCoordinate", CoordOS (element.line.begC.x, element.line.begC.y));
                geo.Add ("endCoordinate", CoordOS (element.line.endC.x, element.line.endC.y));
                geo.Add ("ltypeIndex", (Int32) element.line.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.line.linePen.penIndex);
                break;
            }
            case API_ArcID:
            case API_CircleID: {
                const bool isCircle = (element.header.type.typeID == API_CircleID);
                geo.Add ("elementType", isCircle ? "Circle" : "Arc");
                geo.Add ("origin", CoordOS (element.arc.origC.x, element.arc.origC.y));
                geo.Add ("radius", element.arc.r);
                geo.Add ("ratio", element.arc.ratio);
                geo.Add ("angle", element.arc.angle);
                geo.Add ("begAngle", element.arc.begAng);
                geo.Add ("endAngle", element.arc.endAng);
                geo.Add ("reflected", element.arc.reflected);
                geo.Add ("whole", element.arc.whole);
                geo.Add ("ltypeIndex", (Int32) element.arc.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.arc.linePen.penIndex);
                break;
            }
            case API_PolyLineID: {
                geo.Add ("elementType", "PolyLine");
                geo.Add ("ltypeIndex", (Int32) element.polyLine.ltypeInd.ToInt32_Deprecated ());
                geo.Add ("penIndex", (Int32) element.polyLine.linePen.penIndex);
                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_Polygon);
                if (err != NoError || memo.coords == nullptr) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    geometryOfElements (CreateFailedExecutionResult (err, "Polygon-Memo nicht lesbar"));
                    continue;
                }
                const auto& coords = geo.AddList<GS::ObjectState> ("coordinates");
                const Int32 n = element.polyLine.poly.nCoords;
                for (Int32 i = 1; i <= n; ++i)
                    coords (CoordOS ((*memo.coords)[i].x, (*memo.coords)[i].y));
                const auto& arcs = geo.AddList<GS::ObjectState> ("arcs");
                if (memo.parcs != nullptr) {
                    for (Int32 i = 0; i < element.polyLine.poly.nArcs; ++i) {
                        GS::ObjectState a;
                        a.Add ("begIndex", (Int32) (*memo.parcs)[i].begIndex);
                        a.Add ("endIndex", (Int32) (*memo.parcs)[i].endIndex);
                        a.Add ("arcAngle", (*memo.parcs)[i].arcAngle);
                        arcs (a);
                    }
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                break;
            }
            case API_HatchID: {
                geo.Add ("elementType", "Hatch");
                API_ElementMemo memo = {};
                err = ACAPI_Element_GetMemo (element.header.guid, &memo, APIMemoMask_Polygon);
                if (err != NoError || memo.coords == nullptr) {
                    ACAPI_DisposeElemMemoHdls (&memo);
                    geometryOfElements (CreateFailedExecutionResult (err, "Polygon-Memo nicht lesbar"));
                    continue;
                }
                const auto& coords = geo.AddList<GS::ObjectState> ("coordinates");
                const Int32 n = element.hatch.poly.nCoords;
                for (Int32 i = 1; i <= n; ++i)
                    coords (CoordOS ((*memo.coords)[i].x, (*memo.coords)[i].y));
                const auto& arcs = geo.AddList<GS::ObjectState> ("arcs");
                if (memo.parcs != nullptr) {
                    for (Int32 i = 0; i < element.hatch.poly.nArcs; ++i) {
                        GS::ObjectState a;
                        a.Add ("begIndex", (Int32) (*memo.parcs)[i].begIndex);
                        a.Add ("endIndex", (Int32) (*memo.parcs)[i].endIndex);
                        a.Add ("arcAngle", (*memo.parcs)[i].arcAngle);
                        arcs (a);
                    }
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                break;
            }
            default: {
                // 0.9.19: ShapePrims-Fallback (Zeichenprimitive wie im Grundriss).
                ElmGeoPrimCtx ctx;
                const GSErrCode primErr = CollectShapePrims (element.header.guid, ctx);
                if (primErr != NoError) {
                    geometryOfElements (CreateFailedExecutionResult (primErr, "ShapePrims fehlgeschlagen"));
                    continue;
                }
                geo.Add ("elementType", GetElementTypeNonLocalizedName (element.header.type.typeID));
                geo.Add ("source", GS::UniString ("shapePrims"));
                const auto& primList = geo.AddList<GS::ObjectState> ("primitives");
                for (const GS::ObjectState& pr : ctx.prims)
                    primList (pr);
                geo.Add ("skippedPrimitiveCount", ctx.skipped);
                break;
            }
        }

        geometryOfElements (geo);
    }

    return response;
}
