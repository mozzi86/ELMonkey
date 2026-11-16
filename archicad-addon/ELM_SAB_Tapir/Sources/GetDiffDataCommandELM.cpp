/**
 * GetDiffDataELM — Elementdaten für den Diff-Checker Archicad ↔ Plattform (ELM_SAB 0.11.13).
 *
 * Wozu: Der Abgleich mit der Plattform lief bisher über einen Komplett-IFC-Export (langsam,
 * Datenmüll auf der Platte). Dieser Befehl liest dieselben Informationen direkt aus dem Modell;
 * verglichen wird im Python-Monkey (elmonkey.diff_plattform), nicht hier.
 *
 * Welche Elemente: alle Elemente der Grundriss-Datenbank (ACAPI_Element_GetElemList ohne
 * Filter) — unabhängig von Umbau-Filter, ausgeblendeten Ebenen und Geschossanzeige. Reine
 * 2D-Werkzeuge (Linien, Texte, Bemaßungen, Schraffuren, Marker, Zeichnungen …) werden
 * übersprungen, ebenso Elemente ohne IFC-Klasse. Keine feste Klassenliste.
 *
 * Eigenschaften: über die IFC-API (ELMGetIFCData aus IFCCommands.cpp) — IFC-Klasse, Attribute
 * (Name, Description, ObjectType, Tag, PredefinedType …) und die Vorschau-Eigenschaften des
 * aktiven IFC-Übersetzers, also das, was ein Export in die Psets schreiben würde.
 *
 * Geometrie: Box aus den Körpern des 3D-Modells des Elements (ACAPI_ModelAccess_Get3DInfo +
 * Körper-Boxen body.xmin..zmax, Weltkoordinaten). Laut DevKit rechnet Archicad das Element dafür
 * unabhängig vom 3D-Fenster in 3D um — mit den Verschneidungen/Anschlüssen, ohne 3D-Schnittebenen.
 * Alle Körper eines Elements (bei Mehrschicht-Bauteilen je Schicht einer) gehen in EINE Box, wie
 * die zusammengefassten IfcBuildingElementPart-Teile im IFC. Treppe/Geländer/Fassade: Box über
 * die Körper ihrer Teilelemente. Ob die Box der Box im exportierten IFC entspricht, ist NICHT
 * live belegt — der erste Lauf vergleicht beides (siehe skills/archicad/reference/diff-plattform.md).
 * Ohne 3D-Körper (z. B. Ebene ausgeblendet: APIERR_HIDDENLAY) fällt der Befehl auf
 * ACAPI_Element_CalcBounds zurück (rohe Box, Quelle "calcBounds") — außer bei Decke, Dach und Zone:
 * CalcBounds stürzt bei Decken ohne Zeichenumgebung ab (#686), dort bleibt die Geometrie leer.
 * Die Quelle steht je Element in geometry.source; der Vergleich wertet nur "3dModel" als Beleg.
 *
 * Ändert nichts: keine Reservierung, kein Undo-Schritt, keine Ebenen/Filter/Ansichten.
 */

#include "GetDiffDataCommandELM.hpp"
#include "IFCCommands.hpp"       // ELMGetIFCData
#include "MigrationHelper.hpp"   // GetElemTypeId, GetAttributeIndex, ACAPI_Database_* (AC27)
#include "AddOnVersion.hpp"      // ELM_SAB_VERSION

#ifdef ServerMainVers_2800
#include "ACAPI/IFCObjectAccessor.hpp"
#include "ACAPI/IFCObjectID.hpp"
#endif

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr Int32 kDefaultLimit = 500;
constexpr Int32 kMaxLimit = 5000;

#if defined(ServerMainVers_3000)
constexpr Int32 kArchicadVersion = 30;
#elif defined(ServerMainVers_2900)
constexpr Int32 kArchicadVersion = 29;
#elif defined(ServerMainVers_2800)
constexpr Int32 kArchicadVersion = 28;
#else
constexpr Int32 kArchicadVersion = 27;
#endif

// ---------------------------------------------------------------------------
// Typen
// ---------------------------------------------------------------------------

// Reine 2D-/Dokumentationswerkzeuge: haben kein IFC-Bauteil und kosten nur Zeit.
bool IsDocumentationType (API_ElemTypeID t)
{
    switch (t) {
        case API_DimensionID:
        case API_RadialDimensionID:
        case API_LevelDimensionID:
        case API_AngleDimensionID:
        case API_TextID:
        case API_LabelID:
        case API_HatchID:
        case API_LineID:
        case API_PolyLineID:
        case API_ArcID:
        case API_CircleID:
        case API_SplineID:
        case API_HotspotID:
        case API_CutPlaneID:
        case API_CameraID:
        case API_CamSetID:
        case API_SectElemID:
        case API_DrawingID:
        case API_PictureID:
        case API_DetailID:
        case API_ElevationID:
        case API_InteriorElevationID:
        case API_WorksheetID:
        case API_HotlinkID:
        case API_ChangeMarkerID:
            return true;
        default:
            return false;
    }
}

GS::UniString RenovationText (API_RenovationStatusType s)
{
    switch (s) {
        case API_ExistingStatus:   return "Existing";
        case API_NewStatus:        return "New";
        case API_DemolishedStatus: return "ToBeDemolished";
        default:                   return "Undefined";
    }
}

bool GuidLess (const API_Guid& a, const API_Guid& b)
{
    return std::memcmp (&a, &b, sizeof (API_Guid)) < 0;
}

// Schlüssel für std::map: die 16 Bytes der GUID.
std::string GuidKey (const API_Guid& g)
{
    return std::string (reinterpret_cast<const char*> (&g), sizeof (API_Guid));
}

// Kleine Namensliste ohne Doppel (GS::UniString hat keinen garantierten operator< für std::set).
void AddUnique (GS::Array<GS::UniString>& list, const GS::UniString& name)
{
    if (!name.IsEmpty () && !list.Contains (name)) {
        list.Push (name);
    }
}

// ---------------------------------------------------------------------------
// Attribute (Name über uniStringNamePtr — das 8-Bit-Feld verliert Umlaute)
// ---------------------------------------------------------------------------

GS::UniString AttributeNameDiffELM (API_AttrTypeID type, const API_AttributeIndex& index)
{
    GS::UniString name;
    API_Attribute attr = {};
    attr.header.typeID = type;
    attr.header.index = index;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError) {
        return GS::UniString ();
    }
    return name;
}

struct LayerInfo {
    GS::UniString name;
    bool hidden = false;
};

class Lookups {
public:
    LayerInfo Layer (const API_AttributeIndex& index)
    {
        const Int32 key = GetAttributeIndex (index);
        auto it = layers.find (key);
        if (it != layers.end ()) {
            return it->second;
        }
        LayerInfo info;
        GS::UniString name;
        API_Attribute attr = {};
        attr.header.typeID = API_LayerID;
        attr.header.index = index;
        attr.header.uniStringNamePtr = &name;
        if (ACAPI_Attribute_Get (&attr) == NoError) {
            info.name = name;
            info.hidden = (attr.header.flags & APILay_Hidden) != 0;
        }
        layers[key] = info;
        return info;
    }

    GS::UniString Surface (const API_AttributeIndex& index)
    {
        const Int32 key = GetAttributeIndex (index);
        auto it = surfaces.find (key);
        if (it != surfaces.end ()) {
            return it->second;
        }
        const GS::UniString name = AttributeNameDiffELM (API_MaterialID, index);
        surfaces[key] = name;
        return name;
    }

    void LoadStories ()
    {
        API_StoryInfo storyInfo = {};
        if (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) != NoError || storyInfo.data == nullptr) {
            return;
        }
        const Int32 count = (Int32) (BMGetHandleSize ((GSHandle) storyInfo.data) / (GSSize) sizeof (API_StoryType));
        for (Int32 i = 0; i < count; ++i) {
            const API_StoryType& story = (*storyInfo.data)[i];
            stories[(Int32) story.index] = GS::UniString (story.uName);
        }
        BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
    }

    GS::UniString StoryName (short floorInd) const
    {
        auto it = stories.find ((Int32) floorInd);
        return it != stories.end () ? it->second : GS::UniString ();
    }

    void LoadClassificationSystems ()
    {
        ACAPI_Classification_GetClassificationSystems (classificationSystems);
    }

    // Öffnung (Wandöffnungs-Werkzeug) → Wirt; API_OpeningID hat in AC29 kein owner-Feld mehr.
    API_Guid OpeningHost (const API_Guid& opening)
    {
        if (!openingMapBuilt) {
            openingMapBuilt = true;
            const API_ElemTypeID hostTypes[] = { API_WallID, API_SlabID, API_RoofID, API_ShellID, API_BeamID, API_ColumnID };
            for (const API_ElemTypeID hostType : hostTypes) {
                GS::Array<API_Guid> hosts;
                if (ACAPI_Element_GetElemList (hostType, &hosts) != NoError) {
                    continue;
                }
                for (const API_Guid& host : hosts) {
                    GS::Array<API_Guid> openings;
                    if (ACAPI_Grouping_GetConnectedElements (host, API_OpeningID, &openings) != NoError) {
                        continue;
                    }
                    for (const API_Guid& o : openings) {
                        openingToHost.emplace (GuidKey (o), host);
                    }
                }
            }
        }
        auto it = openingToHost.find (GuidKey (opening));
        return it != openingToHost.end () ? it->second : APINULLGuid;
    }

    GS::Array<API_ClassificationSystem> classificationSystems;

private:
    std::map<Int32, LayerInfo> layers;
    std::map<Int32, GS::UniString> surfaces;
    std::map<Int32, GS::UniString> stories;
    std::map<std::string, API_Guid> openingToHost;
    bool openingMapBuilt = false;
};

// ---------------------------------------------------------------------------
// Geometrie
// ---------------------------------------------------------------------------

struct BoxAcc {
    double xMin = 1e30, xMax = -1e30, yMin = 1e30, yMax = -1e30, zMin = 1e30, zMax = -1e30;
    Int32 bodies = 0;

    void Add (double x0, double y0, double z0, double x1, double y1, double z1)
    {
        xMin = (std::min) (xMin, x0); yMin = (std::min) (yMin, y0); zMin = (std::min) (zMin, z0);
        xMax = (std::max) (xMax, x1); yMax = (std::max) (yMax, y1); zMax = (std::max) (zMax, z1);
    }
};

// Körper des Elements in die Box; Oberflächen der Polygone in `surfaceNames` (optional).
// Indizes von Polygonen/Materialtabelle gelten erst, nachdem der Körper geholt wurde (wie in
// CollectSolidBodies, ElementCommands.cpp).
GSErrCode AccumulateBodies (const API_Elem_Head& head, BoxAcc& box, Lookups& lookups, GS::Array<GS::UniString>* surfaceNames)
{
    API_ElemInfo3D info3D = {};
    GSErrCode err = ACAPI_ModelAccess_Get3DInfo (head, &info3D);
    if (err != NoError) {
        return err;
    }
    for (Int32 iBody = info3D.fbody; iBody <= info3D.lbody; ++iBody) {
        API_Component3D component = {};
        component.header.typeID = API_BodyID;
        component.header.index = iBody;
        if (ACAPI_ModelAccess_GetComponent (&component) != NoError) {
            continue;
        }
        if (component.body.nPgon == 0) {   // kein Volumenkörper
            continue;
        }
        const API_BodyType body = component.body;
        // body.xmin..zmax ist schon in Weltkoordinaten (siehe AccumulateSolidBodyBounds, #563).
        box.Add (body.xmin, body.ymin, body.zmin, body.xmax, body.ymax, body.zmax);
        box.bodies += 1;

        if (surfaceNames == nullptr) {
            continue;
        }
        std::vector<Int32> seenUmat;
        for (Int32 iPgon = 1; iPgon <= body.nPgon; ++iPgon) {
            component.header.typeID = API_PgonID;
            component.header.index = iPgon;
            if (ACAPI_ModelAccess_GetComponent (&component) != NoError) {
                continue;
            }
            const Int32 iumat = component.pgon.iumat;
            if (iumat <= 0 || std::find (seenUmat.begin (), seenUmat.end (), iumat) != seenUmat.end ()) {
                continue;
            }
            seenUmat.push_back (iumat);
            component.header.typeID = API_UmatID;
            component.header.index = iumat;
            if (ACAPI_ModelAccess_GetComponent (&component) == NoError) {
                AddUnique (*surfaceNames, lookups.Surface (component.umat.mater.head.index));
            }
        }
    }
    return NoError;
}

template<typename APIElemType>
void AccumulateSubelements (APIElemType* subelems, BoxAcc& box, Lookups& lookups, GS::Array<GS::UniString>* surfaceNames)
{
    if (subelems == nullptr) {
        return;
    }
    const GSSize n = BMGetPtrSize (reinterpret_cast<GSPtr> (subelems)) / (GSSize) sizeof (APIElemType);
    for (GSIndex i = 0; i < n; ++i) {
        AccumulateBodies (subelems[i].head, box, lookups, surfaceNames);
    }
}

// Treppe, Geländer, Fassade tragen ihre Körper in den Teilelementen (wie CollectBodiesOfSubelements).
bool AccumulateHierarchical (const API_Elem_Head& head, API_ElemTypeID typeID, BoxAcc& box, Lookups& lookups,
                             GS::Array<GS::UniString>* surfaceNames)
{
    if (typeID != API_StairID && typeID != API_RailingID && typeID != API_CurtainWallID) {
        return false;
    }
    API_ElementMemo memo = {};
    if (ACAPI_Element_GetMemo (head.guid, &memo, APIMemoMask_All) == NoError) {
        AccumulateSubelements (memo.stairRisers, box, lookups, surfaceNames);
        AccumulateSubelements (memo.stairTreads, box, lookups, surfaceNames);
        AccumulateSubelements (memo.stairStructures, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingPosts, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingInnerPosts, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingBalusters, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingPanels, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingRails, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingToprails, box, lookups, surfaceNames);
        AccumulateSubelements (memo.railingHandrails, box, lookups, surfaceNames);
        AccumulateSubelements (memo.cWallFrames, box, lookups, surfaceNames);
        AccumulateSubelements (memo.cWallPanels, box, lookups, surfaceNames);
        AccumulateSubelements (memo.cWallJunctions, box, lookups, surfaceNames);
        AccumulateSubelements (memo.cWallAccessories, box, lookups, surfaceNames);
    }
    ACAPI_DisposeElemMemoHdls (&memo);
    return true;
}

struct Counters {
    Int32 geometry3DModel = 0;
    Int32 geometryCalcBounds = 0;
    Int32 geometryNone = 0;
    Int32 skippedDocumentation = 0;
    Int32 skippedNoIfcType = 0;
    Int32 skippedNotFound = 0;
    std::map<Int32, Int32> no3DReasons;   // Archicad-Code von Get3DInfo → Anzahl
};

GS::ObjectState GeometryOS (const API_Elem_Head& head, API_ElemTypeID typeID, Lookups& lookups, bool wantSurfaces,
                            Counters& counters, GS::Array<GS::UniString>& surfacesOut)
{
    BoxAcc box;
    GS::Array<GS::UniString> surfaceNames;
    GS::Array<GS::UniString>* surf = wantSurfaces ? &surfaceNames : nullptr;
    GSErrCode err = AccumulateBodies (head, box, lookups, surf);
    const bool hierarchical = AccumulateHierarchical (head, typeID, box, lookups, surf);
    if (hierarchical && box.bodies > 0) {
        err = NoError;
    }
    for (const GS::UniString& s : surfaceNames) {
        surfacesOut.Push (s);
    }

    GS::ObjectState geometry;
    if (err == NoError && box.bodies > 0) {
        counters.geometry3DModel += 1;
        geometry.Add ("source", GS::UniString ("3dModel"));
        geometry.Add ("bodies", box.bodies);
    } else {
        const Int32 reason = (err == NoError) ? 0 : (Int32) err;
        counters.no3DReasons[reason] += 1;
        geometry.Add ("no3DModelCode", reason);
        API_Box3D raw = {};
        const bool calcBoundsAllowed = typeID != API_SlabID && typeID != API_RoofID && typeID != API_ZoneID &&
                                       typeID != API_OpeningID;
        if (calcBoundsAllowed && ACAPI_Element_CalcBounds (&head, &raw) == NoError) {
            counters.geometryCalcBounds += 1;
            geometry.Add ("source", GS::UniString ("calcBounds"));
            box = BoxAcc ();
            box.Add (raw.xMin, raw.yMin, raw.zMin, raw.xMax, raw.yMax, raw.zMax);
        } else {
            counters.geometryNone += 1;
            geometry.Add ("source", GS::UniString ("none"));
            return geometry;
        }
    }
    const auto& b = geometry.AddList<double> ("box");
    b (box.xMin); b (box.xMax); b (box.yMin); b (box.yMax); b (box.zMin); b (box.zMax);
    return geometry;
}

// ---------------------------------------------------------------------------
// Aufbau, Klassifizierung, Element-ID
// ---------------------------------------------------------------------------

GS::ObjectState StructureOS (const API_Element& e, API_ElemTypeID typeID, bool& has)
{
    has = true;
    GS::ObjectState os;
    auto fromBase = [&] (API_ModelElemStructureType t, const API_AttributeIndex& mat, const API_AttributeIndex& comp,
                         const API_AttributeIndex* prof) {
        if (t == API_CompositeStructure) {
            os.Add ("type", GS::UniString ("composite"));
            os.Add ("name", AttributeNameDiffELM (API_CompWallID, comp));
        } else if (t == API_ProfileStructure && prof != nullptr) {
            os.Add ("type", GS::UniString ("profile"));
            os.Add ("name", AttributeNameDiffELM (API_ProfileID, *prof));
        } else {
            os.Add ("type", GS::UniString ("basic"));
            os.Add ("name", AttributeNameDiffELM (API_BuildingMaterialID, mat));
        }
    };
    switch (typeID) {
        case API_WallID:
            fromBase (e.wall.modelElemStructureType, e.wall.buildingMaterial, e.wall.composite, &e.wall.profileAttr);
            break;
        case API_SlabID:
            fromBase (e.slab.modelElemStructureType, e.slab.buildingMaterial, e.slab.composite, nullptr);
            break;
        case API_RoofID:
            fromBase (e.roof.shellBase.modelElemStructureType, e.roof.shellBase.buildingMaterial, e.roof.shellBase.composite, nullptr);
            break;
        case API_ShellID:
            fromBase (e.shell.shellBase.modelElemStructureType, e.shell.shellBase.buildingMaterial, e.shell.shellBase.composite, nullptr);
            break;
        case API_MorphID:
            os.Add ("type", GS::UniString ("basic"));
            os.Add ("name", AttributeNameDiffELM (API_BuildingMaterialID, e.morph.buildingMaterial));
            break;
        default:
            has = false;
            break;
    }
    return os;
}

bool NeedsElement (API_ElemTypeID t)
{
    return t == API_WallID || t == API_SlabID || t == API_RoofID || t == API_ShellID || t == API_MorphID ||
           t == API_ZoneID || t == API_DoorID || t == API_WindowID || t == API_SkylightID;
}

GS::UniString ElementIdText (const API_Guid& guid)
{
    GS::UniString text;
    API_ElementMemo memo = {};
    if (ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_ElemInfoString) == NoError && memo.elemInfoString != nullptr) {
        text = *memo.elemInfoString;
    }
    ACAPI_DisposeElemMemoHdls (&memo);
    return text;
}

void AddClassifications (GS::ObjectState& os, const API_Guid& guid, const Lookups& lookups)
{
    const auto& list = os.AddList<GS::ObjectState> ("classifications");
    for (const API_ClassificationSystem& system : lookups.classificationSystems) {
        API_ClassificationItem item;
        if (ACAPI_Element_GetClassificationInSystem (guid, system.guid, item) != NoError || item.guid == APINULLGuid) {
            continue;
        }
        ACAPI_Classification_GetClassificationItem (item);
        list (GS::ObjectState ("system", system.name, "id", item.id, "name", item.name));
    }
}

// ---------------------------------------------------------------------------
// Ein Element
// ---------------------------------------------------------------------------

enum class Outcome { Added, Documentation, NoIfcType, NotFound };

Outcome ReadElement (const API_Guid& guid, Lookups& lookups, bool wantProperties, bool wantSurfaces,
                     Counters& counters, GS::ObjectState& out)
{
    API_Elem_Head head = {};
    head.guid = guid;
    if (ACAPI_Element_GetHeader (&head) != NoError) {
        return Outcome::NotFound;
    }
    const API_ElemTypeID typeID = GetElemTypeId (head);
    if (IsDocumentationType (typeID)) {
        return Outcome::Documentation;
    }

    GS::UniString ifcType;
    GS::Array<ELMIFCText> ifcProperties;
    GS::Array<ELMIFCText> ifcAttributes;
    const GSErrCode ifcErr = ELMGetIFCData (head, wantProperties, true, ifcType, ifcProperties, ifcAttributes);
    if (ifcErr != NoError || ifcType.IsEmpty ()) {
        return Outcome::NoIfcType;
    }

    out.Add ("elementId", CreateGuidObjectStateELM (guid));
    out.Add ("type", GetElementTypeNonLocalizedName (typeID));
    out.Add ("ifcType", ifcType);

#ifdef ServerMainVers_2800
    {
        auto accessor = IFCAPI::GetObjectAccessor ();
        auto objectID = accessor.CreateElementObjectID (head);
        if (objectID.IsOk ()) {
            auto globalId = accessor.GetGlobalId (objectID.Unwrap ());
            if (globalId.IsOk ()) {
                out.Add ("ifcGlobalId", globalId.Unwrap ());
            }
        }
    }
#else
    {
        API_Guid ifcId = APINULLGuid;
        API_Guid externalIfcId = APINULLGuid;
        if (ACAPI_Element_GetIFCIdentifier (guid, ifcId, externalIfcId) == NoError) {
            out.Add ("ifcGlobalId", APIGuidToString (ifcId));   // AC27: unkomprimierte GUID, der Kern wandelt
        }
    }
#endif

    {
        const auto& attrs = out.AddList<GS::ObjectState> ("ifcAttributes");
        for (const ELMIFCText& a : ifcAttributes) {
            if (a.hasValue) {
                attrs (GS::ObjectState ("name", a.name, "value", a.value));
            }
        }
    }
    if (wantProperties) {
        const auto& props = out.AddList<GS::ObjectState> ("ifcProperties");
        for (const ELMIFCText& p : ifcProperties) {
            props (GS::ObjectState ("pset", p.propertySetName, "name", p.name, "value", p.value));
        }
    }

    out.Add ("floorIndex", (Int32) head.floorInd);
    out.Add ("storyName", lookups.StoryName (head.floorInd));
    out.Add ("renovationStatus", RenovationText (head.renovationStatus));
    out.Add ("elementIdText", ElementIdText (guid));
    {
        const LayerInfo layer = lookups.Layer (head.layer);
        out.Add ("layer", layer.name);
        out.Add ("layerHidden", layer.hidden);
    }
    AddClassifications (out, guid, lookups);

    if (NeedsElement (typeID)) {
        API_Element element = {};
        element.header.guid = guid;
        if (ACAPI_Element_Get (&element) == NoError) {
            bool hasStructure = false;
            GS::ObjectState structure = StructureOS (element, typeID, hasStructure);
            if (hasStructure) {
                out.Add ("structure", structure);
            }
            if (typeID == API_ZoneID) {
                out.Add ("zone", GS::ObjectState ("name", GS::UniString (element.zone.roomName),
                                                  "number", GS::UniString (element.zone.roomNoStr)));
            } else if (typeID == API_DoorID || typeID == API_WindowID) {
                if (element.window.owner != APINULLGuid) {
                    out.Add ("hostElementId", CreateGuidObjectStateELM (element.window.owner));
                }
            } else if (typeID == API_SkylightID) {
                if (element.skylight.owner != APINULLGuid) {
                    out.Add ("hostElementId", CreateGuidObjectStateELM (element.skylight.owner));
                }
            }
        }
    }
    if (typeID == API_OpeningID) {
        const API_Guid host = lookups.OpeningHost (guid);
        if (host != APINULLGuid) {
            out.Add ("hostElementId", CreateGuidObjectStateELM (host));
        }
    }

    GS::Array<GS::UniString> surfaces;
    out.Add ("geometry", GeometryOS (head, typeID, lookups, wantSurfaces, counters, surfaces));
    if (wantSurfaces) {
        const auto& s = out.AddList<GS::UniString> ("surfaces");
        for (const GS::UniString& name : surfaces) {
            s (name);
        }
    }
    return Outcome::Added;
}

}   // namespace

// ---------------------------------------------------------------------------
// Befehl
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> GetDiffDataCommandELM::GetInputParametersSchema () const
{
    return R"({
        "type": "object",
        "properties": {
            "elements": {
                "type": "array",
                "description": "Optional: nur diese Elemente lesen ({elementId: {guid}}). Ohne Angabe: alle Elemente der Grundriss-Datenbank, seitenweise (offset/limit).",
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
            },
            "offset": { "type": "integer", "minimum": 0, "description": "Startposition in der (nach GUID sortierten) Elementliste. Standard 0." },
            "limit": { "type": "integer", "minimum": 1, "maximum": 5000, "description": "Hoechstens so viele Listeneintraege je Aufruf pruefen (Standard 500). Weiter mit nextOffset." },
            "includeIfcProperties": { "type": "boolean", "description": "IFC-Eigenschaften (Vorschau des aktiven IFC-Uebersetzers) mitliefern. Standard true." },
            "includeSurfaces": { "type": "boolean", "description": "Oberflaechen aus den Polygonen des 3D-Modells mitliefern. Standard true." }
        },
        "additionalProperties": false
    })";
}

GS::Optional<GS::UniString> GetDiffDataCommandELM::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (R"({
        "type": "object",
        "properties": {
            "format": { "type": "string" },
            "readOnly": { "type": "boolean" },
            "total": { "type": "integer" },
            "offset": { "type": "integer" },
            "nextOffset": { "type": "integer" },
            "elements": { "type": "array", "items": { "type": "object" } },
            "notFound": { "type": "array", "items": { "type": "object" } },
            "skipped": { "type": "object" },
            "context": { "type": "object" }
        },
        "required": [ "format", "readOnly", "elements", "context" ]
    })");
}

GS::ObjectState GetDiffDataCommandELM::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    Int32 offset = 0;
    Int32 limit = kDefaultLimit;
    bool wantProperties = true;
    bool wantSurfaces = true;
    parameters.Get ("offset", offset);
    parameters.Get ("limit", limit);
    parameters.Get ("includeIfcProperties", wantProperties);
    parameters.Get ("includeSurfaces", wantSurfaces);
    if (offset < 0) {
        offset = 0;
    }
    if (limit < 1 || limit > kMaxLimit) {
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("limit muss zwischen 1 und 5000 liegen"));
    }

    // Grundriss als aktuelle Datenbank — sonst liefert GetElemList die Elemente des aktiven
    // Schnitts/Layouts. Umschalten ohne Fenster nach vorn, am Ende zurück.
    API_DatabaseInfo startingDatabase = {};
    bool databaseSwitched = false;
    if (ACAPI_Database_GetCurrentDatabase (&startingDatabase) == NoError && startingDatabase.typeID != APIWind_FloorPlanID) {
        API_DatabaseInfo floorPlan = {};
        floorPlan.typeID = APIWind_FloorPlanID;
        ACAPI_Window_GetDatabaseInfo (&floorPlan);
        if (ACAPI_Database_ChangeCurrentDatabase (&floorPlan) != NoError) {
            return CreateErrorResponseELM (APIERR_BADDATABASE, GS::UniString ("Grundriss-Datenbank nicht erreichbar — bitte den Grundriss aktiv machen"));
        }
        databaseSwitched = true;
    }

    GS::Array<API_Guid> guids;
    bool explicitList = false;
    GS::Array<GS::ObjectState> requested;
    if (parameters.Get ("elements", requested)) {
        explicitList = true;
        for (const GS::ObjectState& r : requested) {
            const GS::ObjectState* id = r.Get ("elementId");
            if (id != nullptr) {
                guids.Push (GetGuidFromObjectState (*id));
            }
        }
    } else {
        ACAPI_Element_GetElemList (API_ZombieElemID, &guids, APIFilt_None);
        std::vector<API_Guid> sorted;
        sorted.reserve (guids.GetSize ());
        for (const API_Guid& g : guids) {
            sorted.push_back (g);
        }
        std::sort (sorted.begin (), sorted.end (), GuidLess);
        guids.Clear ();
        for (const API_Guid& g : sorted) {
            guids.Push (g);
        }
    }

    Lookups lookups;
    lookups.LoadStories ();
    lookups.LoadClassificationSystems ();
    Counters counters;

    GS::ObjectState response;
    response.Add ("format", GS::UniString ("elm-diff-daten/1"));
    response.Add ("readOnly", true);
    const Int32 total = (Int32) guids.GetSize ();
    response.Add ("total", total);
    response.Add ("offset", explicitList ? (Int32) 0 : offset);

    const Int32 begin = explicitList ? 0 : (std::min) (offset, total);
    const Int32 end = explicitList ? total : (std::min) (total, begin + limit);
    const auto& elements = response.AddList<GS::ObjectState> ("elements");
    const auto& notFound = response.AddList<GS::ObjectState> ("notFound");
    for (Int32 i = begin; i < end; ++i) {
        const API_Guid guid = guids[(UIndex) i];
        GS::ObjectState element;
        switch (ReadElement (guid, lookups, wantProperties, wantSurfaces, counters, element)) {
            case Outcome::Added:         elements (element); break;
            case Outcome::Documentation: counters.skippedDocumentation += 1; break;
            case Outcome::NoIfcType:     counters.skippedNoIfcType += 1; break;
            case Outcome::NotFound:
                counters.skippedNotFound += 1;
                if (explicitList) {
                    notFound (CreateGuidObjectStateELM (guid));
                }
                break;
        }
    }
    if (!explicitList && end < total) {
        response.Add ("nextOffset", end);
    }

    response.Add ("skipped", GS::ObjectState ("documentation", counters.skippedDocumentation,
                                              "noIfcType", counters.skippedNoIfcType,
                                              "notFound", counters.skippedNotFound));

    GS::ObjectState context;
    context.Add ("elmSabVersion", GS::UniString (ELM_SAB_VERSION));
    context.Add ("archicadVersion", kArchicadVersion);
    context.Add ("ifcGlobalIdForm", GS::UniString (kArchicadVersion >= 28 ? "ifc22" : "guid36"));
    context.Add ("databaseSwitched", databaseSwitched);
    context.Add ("geometry3DModel", counters.geometry3DModel);
    context.Add ("geometryCalcBounds", counters.geometryCalcBounds);
    context.Add ("geometryNone", counters.geometryNone);
    {
        const auto& reasons = context.AddList<GS::ObjectState> ("no3DModelCodes");
        for (const auto& r : counters.no3DReasons) {
            reasons (GS::ObjectState ("code", r.first, "count", r.second));
        }
    }
    {
        // 3D-Schnittebenen wirken laut DevKit nicht auf Get3DInfo — nur zur Einordnung gemeldet.
        API_3DCutPlanesInfo cutInfo = {};
        if (ACAPI_View_Get3DCuttingPlanes (&cutInfo) == NoError) {
            context.Add ("cutPlanesActive", cutInfo.isCutPlanes);
            if (cutInfo.shapes != nullptr) {
                BMKillHandle (reinterpret_cast<GSHandle*> (&cutInfo.shapes));
            }
        }
    }

    if (databaseSwitched) {
        const bool restored = ACAPI_Database_ChangeCurrentDatabase (&startingDatabase) == NoError;
        context.Add ("databaseRestored", restored);
    }
    response.Add ("context", context);
    return response;
}
