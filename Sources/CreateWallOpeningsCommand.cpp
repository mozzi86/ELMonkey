#include "CreateWallOpeningsCommand.hpp"
#include "MigrationHelper.hpp"

#ifdef ServerMainVers_2900
#include "ACAPI/Element/Opening/OpeningDefault.hpp"
#include "ACAPI/Element/Opening/Opening.hpp"
#include "ACAPI/Element/Opening/OpeningExtrusionParameters.hpp"
#include "ACAPI/Element/Opening/OpeningFloorPlanParameters.hpp"
#endif

#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>

// Die SAB-Kennzeichnung. Die GUIDs sind projektspezifisch (SAB-Bürovorlage /
// SAB_Klassifizierung) und stehen hier nur als VORBELEGUNG — jeder Eintrag kann sie
// per Request überschreiben, damit der Befehl in einem anderen Projekt nicht auf
// fremde Definitionen schreibt. Quelle: ~/.scratch/thn-handoff/HANDOFF.md.
#define ELM_SAB_KI_STAMP_PROPERTY_GUID   "E8E0F5CD-6580-6040-ACC4-A705B06E2C57"
#define ELM_SAB_ELEMENT_ID_PROPERTY_GUID "7E221F33-829B-4FBC-A670-E74DABCE6289"
#define ELM_SAB_DURCHBRUCH_ITEM_GUID     "3AFD9241-4490-4846-9771-CB129E21CE84"

GS::Optional<GS::UniString> CreateWallOpeningsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "openings": {
                "type": "array",
                "description": "Je Eintrag eine echte Wand-Oeffnung. Reihenfolge der Antwort entspricht der Eingabe.",
                "items": {
                    "type": "object",
                    "properties": {
                        "sourceObjectGuid": {
                            "type": "string",
                            "description": "Optional: das KI-Durchbruch-Symbol. Der KI-Stempel und - wenn elementId fehlt - die Element-ID werden davon uebernommen. Geloescht wird es nur bei deleteSource=true UND vorhandenem KI-Stempel."
                        },
                        "wallGuid": {
                            "type": "string",
                            "description": "Die Wirtswand (Owner der Oeffnung). Pflicht."
                        },
                        "center": {
                            "type": "object",
                            "description": "Mittelpunkt im Grundriss. Wird auf die Wandachse projiziert.",
                            "properties": { "x": { "type": "number" }, "y": { "type": "number" } },
                            "required": ["x", "y"]
                        },
                        "width":  { "type": "number", "description": "Breite in m. Bei shape=round der DURCHMESSER." },
                        "height": { "type": "number", "description": "Hoehe in m. Bei shape=round ignoriert (= Durchmesser)." },
                        "bottomElevation": { "type": "number", "description": "Unterkante in m relativ zur OKFF des Geschosses der WIRTSWAND." },
                        "topElevation":    { "type": "number", "description": "Alternativ zu bottomElevation: Oberkante relativ zur OKFF des Geschosses der Wirtswand." },
                        "shape": { "type": "string", "enum": ["rect", "round"], "description": "Standard rect." },
                        "elementId": { "type": "string", "description": "Wert fuer die Element-ID-Property." },
                        "propertyValues": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "propertyGuid": { "type": "string" },
                                    "value": {}
                                },
                                "required": ["propertyGuid"]
                            }
                        }
                    },
                    "required": ["wallGuid", "center", "width"]
                },
                "minItems": 1
            },
            "deleteSource": {
                "type": "boolean",
                "description": "Quell-Symbol nach erfolgreicher Oeffnung loeschen - nur wenn es einen KI-Stempel traegt (Standard false)."
            },
            "reserve": {
                "type": "boolean",
                "description": "Teamwork: Wirtswand (und Quellobjekt) vorab reservieren (Standard true) und danach wieder freigeben."
            },
            "classify": {
                "type": "boolean",
                "description": "Klassifikations-Item 'Durchbruch' setzen (Standard true)."
            },
            "classificationItemGuid": { "type": "string", "description": "Ueberschreibt das Item 'Durchbruch'." },
            "kiStampPropertyGuid":    { "type": "string", "description": "Ueberschreibt die Property 'KI generiert'." },
            "elementIdPropertyGuid":  { "type": "string", "description": "Ueberschreibt die Property 'Element-ID'." },
            "kiStampValue":           { "type": "string", "description": "Fester Stempelwert fuer alle Eintraege. Sonst: Wert des Quellobjekts, sonst 'KI generiert <Datum>'." },
            "wrapInUndoScope": {
                "type": "boolean",
                "description": "Den ganzen Durchgang in EINE Undo-Klammer legen (Standard true). OpeningDefault::Place() oeffnet intern eine eigene Undo-Klammer; falls Archicad diese Verschachtelung ablehnt, auf false stellen - dann legt jede Oeffnung ihren eigenen Undo-Eintrag an."
            }
        },
        "required": ["openings"]
    })");
}

GS::Optional<GS::UniString> CreateWallOpeningsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "results":      { "type": "array", "items": { "type": "object" } },
            "undoScope":    { "type": "object" },
            "successCount": { "type": "integer" },
            "openingCount": { "type": "integer" }
        },
        "required": ["results", "undoScope"]
    })");
}

namespace {

struct PropSet {
    API_Guid      guid = APINULLGuid;
    bool          hasString = false;
    GS::UniString stringValue;
    bool          hasNumber = false;
    double        numberValue = 0.0;
    bool          hasBool = false;
    bool          boolValue = false;
};

struct OpeningRequest {
    API_Guid      sourceGuid = APINULLGuid;
    API_Guid      wallGuid   = APINULLGuid;
    double        cx = 0.0, cy = 0.0;
    double        width  = 0.0;
    double        height = 0.0;
    bool          hasBottom = false;
    double        bottomElevation = 0.0;
    bool          hasTop = false;
    double        topElevation = 0.0;
    bool          round = false;
    bool          hasElementId = false;
    GS::UniString elementId;
    std::vector<PropSet> propertyValues;
};

struct OpeningResult {
    bool          success = false;
    API_Guid      openingGuid = APINULLGuid;
    GSErrCode     err = APIERR_GENERAL;
    GS::UniString message = "Nicht ausgefuehrt (Undo-Scope hat das Lambda nicht gestartet)";
    bool          done = false;
    bool          classified = false;
    bool          kiStamped = false;
    bool          elementIdStamped = false;
    UInt32        propertiesSet = 0;
    UInt32        propertiesFailed = 0;
    bool          sourceDeleted = false;
    GS::UniString sourceKeptReason;
    double        projectedX = 0.0, projectedY = 0.0;
    double        centerZ = 0.0;
    double        storyLevel = 0.0;
};

// Alle Helfer ab hier laufen nur im AC29-Zweig. Unter AC27/28 waeren sie
// ungenutzt — und der Build laeuft mit -Werror / /WX.
#ifdef ServerMainVers_2900

// ObjectState::Get(double) scheitert, wenn im JSON eine ganze Zahl steht (1 statt
// 1.0) — genau der Fall bei runden Maßen wie height: 1. Deshalb mit Int32-Rueckfall.
bool GetNumber (const GS::ObjectState& os, const char* name, double& value)
{
    if (os.Get (name, value))
        return true;
    Int32 i = 0;
    if (os.Get (name, i)) {
        value = (double) i;
        return true;
    }
    return false;
}

API_Guid GuidFromString (const GS::UniString& s)
{
    return (s.IsEmpty ()) ? APINULLGuid : APIGuidFromString (s.ToCStr ().Get ());
}

// "KI generiert 2026-09-09" — derselbe Wortlaut wie in den THN-Skripten.
GS::UniString TodayStamp ()
{
    std::time_t now = std::time (nullptr);
    std::tm     tmv = {};
#ifdef WINDOWS
    localtime_s (&tmv, &now);
#else
    localtime_r (&now, &tmv);
#endif
    char buf[32] = {};
    std::snprintf (buf, sizeof (buf), "KI generiert %04d-%02d-%02d",
                   tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    return GS::UniString (buf);
}

bool ReadPropSet (const GS::ObjectState& item, PropSet& ps)
{
    GS::UniString guidStr;
    if (!item.Get ("propertyGuid", guidStr))
        return false;
    ps.guid = GuidFromString (guidStr);
    if (ps.guid == APINULLGuid)
        return false;

    double d = 0.0;
    Int32  i = 0;
    bool   b = false;
    if (item.Get ("value", ps.stringValue)) {
        ps.hasString = true;
    } else if (item.Get ("value", d)) {
        ps.hasNumber = true;
        ps.numberValue = d;
    } else if (item.Get ("value", i)) {
        ps.hasNumber = true;
        ps.numberValue = (double) i;
    } else if (item.Get ("value", b)) {
        ps.hasBool = true;
        ps.boolValue = b;
    }
    return ps.hasString || ps.hasNumber || ps.hasBool;
}

// Liest die Property-Definition am Element (nur so kennt man valueType und
// collectionType), trägt den Wert ein und schreibt zurück.
GSErrCode SetOneProperty (const API_Guid& elemGuid, const PropSet& ps)
{
    API_Property prop = {};
    GSErrCode err = ACAPI_Element_GetPropertyValue (elemGuid, ps.guid, prop);
    if (err != NoError)
        return err;

    if (prop.definition.collectionType != API_PropertySingleCollectionType)
        return APIERR_BADPROPERTY;      // Listen-Properties bewusst nicht unterstützt

    API_Variant& v = prop.value.singleVariant.variant;
    v.type = prop.definition.valueType;

    switch (prop.definition.valueType) {
        case API_PropertyStringValueType:
            if (!ps.hasString && !ps.hasNumber && !ps.hasBool)
                return APIERR_BADPARS;
            if (ps.hasString) {
                v.uniStringValue = ps.stringValue;
            } else if (ps.hasNumber) {
                // GS::UniString::Printf kennt eigene Formatspezifizierer; snprintf ist
                // hier die Variante, die in jedem DevKit-Stand dasselbe tut.
                char numBuf[64] = {};
                std::snprintf (numBuf, sizeof (numBuf), "%g", ps.numberValue);
                v.uniStringValue = GS::UniString (numBuf);
            } else {
                v.uniStringValue = GS::UniString (ps.boolValue ? "true" : "false");
            }
            break;
        case API_PropertyIntegerValueType:
            if (!ps.hasNumber && !ps.hasBool)
                return APIERR_BADPARS;
            v.intValue = ps.hasNumber ? (Int32) ps.numberValue : (ps.boolValue ? 1 : 0);
            break;
        case API_PropertyRealValueType:
            if (!ps.hasNumber)
                return APIERR_BADPARS;
            v.doubleValue = ps.numberValue;
            break;
        case API_PropertyBooleanValueType:
            if (!ps.hasBool && !ps.hasNumber)
                return APIERR_BADPARS;
            v.boolValue = ps.hasBool ? ps.boolValue : (ps.numberValue != 0.0);
            break;
        default:
            return APIERR_BADPROPERTY;  // Guid-/Undefined-Typen: nichts raten
    }

    prop.value.variantStatus = API_VariantStatusNormal;
    prop.status    = API_Property_HasValue;
    prop.isDefault = false;

    return ACAPI_Element_SetProperty (elemGuid, prop);
}

// Rücklese: kommt der geschriebene String wirklich zurück?
bool VerifyStringProperty (const API_Guid& elemGuid, const API_Guid& propGuid, const GS::UniString& expected)
{
    API_Property prop = {};
    if (ACAPI_Element_GetPropertyValue (elemGuid, propGuid, prop) != NoError)
        return false;
    if (prop.definition.valueType != API_PropertyStringValueType)
        return false;
    return prop.value.singleVariant.variant.uniStringValue == expected;
}

bool ReadStringProperty (const API_Guid& elemGuid, const API_Guid& propGuid, GS::UniString& out)
{
    API_Property prop = {};
    if (ACAPI_Element_GetPropertyValue (elemGuid, propGuid, prop) != NoError)
        return false;
    if (prop.definition.valueType != API_PropertyStringValueType)
        return false;
    if (prop.status != API_Property_HasValue)
        return false;
    out = prop.value.singleVariant.variant.uniStringValue;
    return !out.IsEmpty ();
}

// OKFF des Geschosses, auf dem das Element liegt. Rückgabe false = Geschoss nicht
// gefunden; dann ist die Höhenangabe nicht auflösbar und der Eintrag scheitert.
bool GetStoryLevel (short floorInd, double& level)
{
    API_StoryInfo storyInfo = {};
    if (ACAPI_ProjectSetting_GetStorySettings (&storyInfo) != NoError || storyInfo.data == nullptr)
        return false;

    bool found = false;
    const short storyCount = storyInfo.lastStory - storyInfo.firstStory + 1;
    for (short k = 0; k < storyCount; ++k) {
        if ((*storyInfo.data)[k].index == floorInd) {
            level = (*storyInfo.data)[k].level;
            found = true;
            break;
        }
    }
    BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
    return found;
}

// Lotrechte Projektion auf die Wandachse (begC→endC). Gekrümmte Wände: die Achse
// wird als Sehne genommen — siehe Grenzen in reference/mcp-extension.md.
void ProjectOnWallAxis (const API_WallType& wall, double px, double py, double& ox, double& oy)
{
    const double ax = wall.begC.x, ay = wall.begC.y;
    const double bx = wall.endC.x, by = wall.endC.y;
    const double dx = bx - ax,     dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    if (len2 < 1e-12) {
        ox = ax;
        oy = ay;
        return;
    }
    double t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    ox = ax + t * dx;
    oy = ay + t * dy;
}

// Legt EINE Öffnung an und stempelt sie. Schreibt alles nach `res`.
void CreateOne (const OpeningRequest&  req,
                bool                   classify,
                const API_Guid&        classificationItemGuid,
                const API_Guid&        kiStampPropertyGuid,
                const API_Guid&        elementIdPropertyGuid,
                const GS::UniString&   kiStampOverride,
                bool                   deleteSource,
                OpeningResult&         res)
{
    res.done = true;

    if (req.wallGuid == APINULLGuid) {
        res.err = APIERR_BADPARS;
        res.message = "wallGuid fehlt oder ist ungueltig";
        return;
    }
    if (req.width <= 0.0) {
        res.err = APIERR_BADPARS;
        res.message = "width muss > 0 sein";
        return;
    }
    if (!req.round && req.height <= 0.0) {
        res.err = APIERR_BADPARS;
        res.message = "height muss > 0 sein (ausser shape=round)";
        return;
    }
    if (!req.hasBottom && !req.hasTop) {
        res.err = APIERR_BADPARS;
        res.message = "bottomElevation oder topElevation angeben";
        return;
    }

    // 1. Wirtswand lesen und pruefen.
    API_Element wall = {};
    wall.header.guid = req.wallGuid;
    GSErrCode err = ACAPI_Element_Get (&wall);
    if (err != NoError) {
        res.err = err;
        res.message = "Wirtswand nicht lesbar";
        return;
    }
    if (GetElemTypeId (wall.header) != API_WallID) {
        res.err = APIERR_BADELEMENTTYPE;
        res.message = "wallGuid zeigt nicht auf eine Wand";
        return;
    }

    // 2. Hoehe: OKFF des Wand-Geschosses + gewuenschte Unterkante.
    if (!GetStoryLevel (wall.header.floorInd, res.storyLevel)) {
        res.err = APIERR_BADINDEX;
        res.message = "Geschoss der Wirtswand nicht aufloesbar (API_StoryInfo)";
        return;
    }

    const double effHeight = req.round ? req.width : req.height;
    const double bottomRel = req.hasBottom ? req.bottomElevation : (req.topElevation - effHeight);
    res.centerZ = res.storyLevel + bottomRel + effHeight / 2.0;

    // 3. Mittelpunkt auf die Wandachse projizieren. Place() projiziert selbst auf die
    //    naechstliegende Oberflaeche; wir legen den Punkt trotzdem exakt auf die Achse,
    //    damit die Oeffnung mittig in der Wandstaerke sitzt und nicht auf einer Schale.
    ProjectOnWallAxis (wall.wall, req.cx, req.cy, res.projectedX, res.projectedY);

    // 4. Oeffnungs-Default holen und einstellen.
    ACAPI::Result<ACAPI::Element::OpeningDefault> openingDefault = ACAPI::Element::CreateOpeningDefault ();
    if (openingDefault.IsErr ()) {
        res.err = (GSErrCode) openingDefault.UnwrapErr ().kind;
        res.message = GS::UniString ("Oeffnungs-Default nicht verfuegbar: ") +
                      GS::UniString (openingDefault.UnwrapErr ().text.c_str ());
        return;
    }

    GS::UniString setupError;
    ACAPI::Result<void> modifyResult = openingDefault->Modify (
        [&] (ACAPI::Element::OpeningDefault::Modifier& modifier) {
            using namespace ACAPI::Element;

            ModifiableOpeningExtrusionParameters extrusion = modifier.GetExtrusionParameters ();
            extrusion.SetShapeType (req.round ? ShapeType::Circular : ShapeType::Rectangular);

            // Aligned = die Oeffnung richtet sich nach der Wirtswand aus. Damit sitzt
            // die Extrusion senkrecht zur Wandachse, auch bei geneigten Waenden.
            if (extrusion.SetConstraint (Constraint::Aligned).IsErr ())
                setupError = "SetConstraint(Aligned) abgelehnt";

            // "Durch die Wand": Infinite laeuft in beide Richtungen unbegrenzt und
            // schneidet damit alle Schalen einer mehrschichtigen Wand durch.
            extrusion.SetLimitType (LimitType::Infinite);

            // Breite/Hoehe unabhaengig halten, sonst zieht Archicad die Hoehe mit.
            if (extrusion.SetLinkedStatus (LinkedStatus::NotLinked).IsErr ())
                setupError = "SetLinkedStatus(NotLinked) abgelehnt";

            if (extrusion.SetWidth (req.width).IsErr ())
                setupError = "SetWidth abgelehnt (Formtyp passt nicht)";
            if (extrusion.SetHeight (effHeight).IsErr ())
                setupError = "SetHeight abgelehnt (Formtyp passt nicht)";

            // Der Einfuegepunkt ist damit der MITTELPUNKT der Oeffnung — dazu passt
            // centerZ oben. Anker-Hoehe (SetAnchorAltitude) wird NICHT gesetzt: der
            // Bezugshorizont dieses Feldes ist im DevKit-Header nicht dokumentiert.
            extrusion.SetAnchor (APIAnc_MM);

            ModifiableOpeningFloorPlanParameters floorPlan = modifier.GetFloorPlanParameters ();
            floorPlan.SetFloorPlanDisplayMode (OpeningFloorPlanDisplayMode::Symbolic);
        });

    if (modifyResult.IsErr ()) {
        res.err = (GSErrCode) modifyResult.UnwrapErr ().kind;
        res.message = GS::UniString ("Oeffnungs-Default nicht einstellbar: ") +
                      GS::UniString (modifyResult.UnwrapErr ().text.c_str ());
        return;
    }
    if (!setupError.IsEmpty ()) {
        res.err = APIERR_BADPARS;
        res.message = setupError;
        return;
    }

    // 5. Platzieren. Place() legt einen eigenen Undo-Scope an und schliesst ihn selbst.
    const API_Coord3D inputPoint = { res.projectedX, res.projectedY, res.centerZ };
    ACAPI::UniqueID parentId (req.wallGuid, ACAPI_GetToken ());
    ACAPI::Result<ACAPI::UniqueID> placed = openingDefault->Place (parentId, inputPoint);
    if (placed.IsErr ()) {
        res.err = (GSErrCode) placed.UnwrapErr ().kind;
        res.message = GS::UniString ("Platzieren abgelehnt: ") +
                      GS::UniString (placed.UnwrapErr ().text.c_str ());
        return;
    }

    res.openingGuid = GSGuid2APIGuid (placed.Unwrap ().GetGuid ());
    if (res.openingGuid == APINULLGuid) {
        res.err = APIERR_GENERAL;
        res.message = "Place() meldete Erfolg, gab aber keine GUID zurueck";
        return;
    }

    // Die Oeffnung steht. Ab hier zaehlt nur noch die Kennzeichnung — Teilfehler
    // dabei machen die Oeffnung nicht ungueltig, werden aber einzeln gemeldet.
    res.success = true;
    res.err = NoError;
    res.message = "";

    // 6. Klassifikation "Durchbruch".
    if (classify && classificationItemGuid != APINULLGuid)
        res.classified = (ACAPI_Element_AddClassificationItem (res.openingGuid, classificationItemGuid) == NoError);

    // 7. KI-Stempel. Wert: Override > Quellobjekt > "KI generiert <heute>".
    GS::UniString kiValue = kiStampOverride;
    if (kiValue.IsEmpty () && req.sourceGuid != APINULLGuid && kiStampPropertyGuid != APINULLGuid)
        ReadStringProperty (req.sourceGuid, kiStampPropertyGuid, kiValue);
    if (kiValue.IsEmpty ())
        kiValue = TodayStamp ();

    if (kiStampPropertyGuid != APINULLGuid) {
        PropSet ki;
        ki.guid = kiStampPropertyGuid;
        ki.hasString = true;
        ki.stringValue = kiValue;
        if (SetOneProperty (res.openingGuid, ki) == NoError)
            res.kiStamped = VerifyStringProperty (res.openingGuid, kiStampPropertyGuid, kiValue);
    }

    // 8. Element-ID. Nur ueber die Property — die API_Element-Union hat in AC29 kein
    //    opening-Mitglied mehr, API_Elem_Head.id ist fuer Oeffnungen also nicht
    //    erreichbar (dokumentiert in reference/mcp-extension.md).
    GS::UniString idValue = req.hasElementId ? req.elementId : GS::UniString ();
    if (idValue.IsEmpty () && req.sourceGuid != APINULLGuid && elementIdPropertyGuid != APINULLGuid)
        ReadStringProperty (req.sourceGuid, elementIdPropertyGuid, idValue);

    if (!idValue.IsEmpty () && elementIdPropertyGuid != APINULLGuid) {
        PropSet idProp;
        idProp.guid = elementIdPropertyGuid;
        idProp.hasString = true;
        idProp.stringValue = idValue;
        if (SetOneProperty (res.openingGuid, idProp) == NoError)
            res.elementIdStamped = VerifyStringProperty (res.openingGuid, elementIdPropertyGuid, idValue);
    }

    // 9. Weitere Properties aus dem Request.
    for (const PropSet& ps : req.propertyValues) {
        if (SetOneProperty (res.openingGuid, ps) == NoError)
            ++res.propertiesSet;
        else
            ++res.propertiesFailed;
    }

    // 10. Quell-Symbol loeschen — nur mit KI-Stempel. Handgezeichnete Durchbruecke
    //     (ohne Stempel) bleiben unangetastet; das ist eine harte Vorgabe aus dem
    //     THN-SuD-Projekt und keine Komfortoption.
    if (!deleteSource) {
        if (req.sourceGuid != APINULLGuid)
            res.sourceKeptReason = "deleteSource ist false";
        return;
    }
    if (req.sourceGuid == APINULLGuid) {
        res.sourceKeptReason = "kein sourceObjectGuid angegeben";
        return;
    }
    if (kiStampPropertyGuid == APINULLGuid) {
        res.sourceKeptReason = "kiStampPropertyGuid unbekannt - KI-Stempel nicht pruefbar";
        return;
    }
    GS::UniString sourceStamp;
    if (!ReadStringProperty (req.sourceGuid, kiStampPropertyGuid, sourceStamp)) {
        res.sourceKeptReason = "Quellobjekt traegt keinen KI-Stempel (handgezeichnet?) - nicht geloescht";
        return;
    }
    GS::Array<API_Guid> toDelete;
    toDelete.Push (req.sourceGuid);
    if (ACAPI_Element_Delete (toDelete) == NoError)
        res.sourceDeleted = true;
    else
        res.sourceKeptReason = "ACAPI_Element_Delete abgelehnt";
}

#endif // ServerMainVers_2900

} // namespace

GS::ObjectState CreateWallOpeningsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    GS::Array<GS::ObjectState> openings;
    parameters.Get ("openings", openings);
    if (openings.IsEmpty ())
        return CreateErrorResponse (APIERR_BADPARS, "openings ist leer.");

#ifndef ServerMainVers_2900
    return CreateErrorResponse (APIERR_NOTSUPPORTED,
        "CreateWallOpenings braucht die ACAPI::Element::Opening-API und damit Archicad 29. "
        "Unter AC27/28 liegt API_OpeningType noch in der API_Element-Union, die Felder fuer "
        "Anker, Grenze und Grundrissdarstellung sind im vorliegenden DevKit aber nicht "
        "nachlesbar - deshalb wird hier nichts geraten.");
#else

    bool deleteSource = false;
    parameters.Get ("deleteSource", deleteSource);
    bool reserve = true;
    parameters.Get ("reserve", reserve);
    bool classify = true;
    parameters.Get ("classify", classify);
    bool wrapInUndoScope = true;
    parameters.Get ("wrapInUndoScope", wrapInUndoScope);

    GS::UniString s;
    API_Guid classificationItemGuid = GuidFromString (ELM_SAB_DURCHBRUCH_ITEM_GUID);
    if (parameters.Get ("classificationItemGuid", s))
        classificationItemGuid = GuidFromString (s);
    API_Guid kiStampPropertyGuid = GuidFromString (ELM_SAB_KI_STAMP_PROPERTY_GUID);
    if (parameters.Get ("kiStampPropertyGuid", s))
        kiStampPropertyGuid = GuidFromString (s);
    API_Guid elementIdPropertyGuid = GuidFromString (ELM_SAB_ELEMENT_ID_PROPERTY_GUID);
    if (parameters.Get ("elementIdPropertyGuid", s))
        elementIdPropertyGuid = GuidFromString (s);
    GS::UniString kiStampOverride;
    parameters.Get ("kiStampValue", kiStampOverride);

    const size_t n = openings.GetSize ();
    std::vector<OpeningRequest> requests (n);
    std::vector<OpeningResult>  results (n);

    for (size_t i = 0; i < n; ++i) {
        // MSVC behandelt die size_t->GS::UIndex-Verengung in GS::Array::operator[]
        // als Fehler (C4267 + /WX) - deshalb der explizite Cast.
        const GS::ObjectState& item = openings[(GS::UIndex) i];
        OpeningRequest& req = requests[i];

        GS::UniString g;
        if (item.Get ("wallGuid", g))
            req.wallGuid = GuidFromString (g);
        if (item.Get ("sourceObjectGuid", g))
            req.sourceGuid = GuidFromString (g);

        const GS::ObjectState* center = item.Get ("center");
        if (center != nullptr) {
            GetNumber (*center, "x", req.cx);
            GetNumber (*center, "y", req.cy);
        }

        GetNumber (item, "width", req.width);
        GetNumber (item, "height", req.height);
        req.hasBottom = GetNumber (item, "bottomElevation", req.bottomElevation);
        req.hasTop    = GetNumber (item, "topElevation", req.topElevation);

        GS::UniString shape;
        if (item.Get ("shape", shape))
            req.round = (shape == "round");

        req.hasElementId = item.Get ("elementId", req.elementId) && !req.elementId.IsEmpty ();

        GS::Array<GS::ObjectState> props;
        if (item.Get ("propertyValues", props)) {
            for (const GS::ObjectState& p : props) {
                PropSet ps;
                if (ReadPropSet (p, ps))
                    req.propertyValues.push_back (ps);
            }
        }
    }

    // Teamwork: Wirtswaende (und bei deleteSource die Quellobjekte) einmal fuer alle
    // reservieren. Konflikte werden je Eintrag gemeldet, nicht global.
    GS::HashTable<API_Guid, short> conflicts;
    const bool teamwork = ACAPI_Teamwork_HasConnection ();
    GS::Array<API_Guid> reserved;
    if (reserve && teamwork) {
        GS::HashTable<API_Guid, bool> seen;
        for (size_t i = 0; i < n; ++i) {
            const API_Guid candidates[2] = { requests[i].wallGuid,
                                             deleteSource ? requests[i].sourceGuid : APINULLGuid };
            for (const API_Guid& candidate : candidates) {
                if (candidate == APINULLGuid || seen.ContainsKey (candidate))
                    continue;
                seen.Add (candidate, true);
                reserved.Push (candidate);
            }
        }
        if (!reserved.IsEmpty ())
            ACAPI_Teamwork_ReserveElements (reserved, &conflicts, false);
    }

    for (size_t i = 0; i < n; ++i) {
        const API_Guid& wallGuid = requests[i].wallGuid;
        if (wallGuid != APINULLGuid && conflicts.ContainsKey (wallGuid)) {
            results[i].err = APIERR_NOACCESSRIGHT;
            results[i].message = "Wirtswand nicht reservierbar (anderer Nutzer, ausgeblendete Ebene oder Hotlink)";
            results[i].done = true;
        }
    }

    auto runAll = [&] () {
        for (size_t i = 0; i < n; ++i) {
            if (!results[i].done) {
                CreateOne (requests[i], classify, classificationItemGuid, kiStampPropertyGuid,
                           elementIdPropertyGuid, kiStampOverride, deleteSource, results[i]);
            }
        }
    };

    // Ergebnisliste liegt AUSSERHALB des Lambdas: startet Archicad das Lambda nicht,
    // wird das gemeldet statt eine leere Liste ohne Fehler zurueckzugeben.
    //
    // Zur Verschachtelung: OpeningDefault::Place() oeffnet laut Header selbst eine
    // Undo-Klammer. Die aeussere Klammer haelt den ganzen Lauf samt Stempeln in EINEM
    // Undo-Schritt zusammen - falls Archicad die Verschachtelung ablehnt, schaltet
    // `wrapInUndoScope: false` sie ab, ohne dass neu gebaut werden muss.
    bool      lambdaRan = false;
    GSErrCode undoErr   = NoError;
    if (wrapInUndoScope) {
        undoErr = ACAPI_CallUndoableCommand ("ELM_SAB CreateWallOpenings", [&] () -> GSErrCode {
            lambdaRan = true;
            runAll ();
            return NoError;
        });
    } else {
        lambdaRan = true;
        runAll ();
    }

    if (reserve && teamwork && !reserved.IsEmpty ())
        ACAPI_Teamwork_ReleaseElements (reserved, false);

    GS::ObjectState response;
    const auto& resultList = response.AddList<GS::ObjectState> ("results");
    UInt32 successCount = 0;
    for (size_t i = 0; i < n; ++i) {
        const OpeningResult& r = results[i];
        GS::ObjectState entry;
        entry.Add ("success", r.success);
        if (r.success) {
            ++successCount;
            entry.Add ("openingGuid", APIGuidToString (r.openingGuid));
            entry.Add ("classified", r.classified);
            entry.Add ("kiStamped", r.kiStamped);
            entry.Add ("elementIdStamped", r.elementIdStamped);
            entry.Add ("propertiesSet", (Int32) r.propertiesSet);
            entry.Add ("propertiesFailed", (Int32) r.propertiesFailed);
            entry.Add ("sourceDeleted", r.sourceDeleted);
            if (!r.sourceKeptReason.IsEmpty ())
                entry.Add ("sourceKeptReason", r.sourceKeptReason);
            GS::ObjectState placement;
            placement.Add ("x", r.projectedX);
            placement.Add ("y", r.projectedY);
            placement.Add ("centerZ", r.centerZ);
            placement.Add ("storyLevel", r.storyLevel);
            entry.Add ("placement", placement);
        } else {
            GS::ObjectState error;
            error.Add ("code", (Int32) r.err);
            error.Add ("message", r.message);
            entry.Add ("error", error);
        }
        resultList (entry);
    }

    GS::ObjectState undoScope;
    undoScope.Add ("executed", lambdaRan);
    undoScope.Add ("errorCode", (Int32) undoErr);
    undoScope.Add ("mode", GS::UniString (!wrapInUndoScope ? "perOpening"
                                                             : (lambdaRan ? "undoable" : "notExecuted")));
    if (!lambdaRan && wrapInUndoScope) {
        undoScope.Add ("hint", GS::UniString ("ACAPI_CallUndoableCommand hat das Lambda nicht gestartet - "
                                              "es wurde NICHTS angelegt. Archicad-Sitzung pruefen (offener "
                                              "Dialog, aktives Werkzeug, Teamwork-Schreibrecht). Kein "
                                              "Wiederholungsdurchgang ohne Undo-Klammer: eine Oeffnung ohne "
                                              "Undo-Eintrag laesst sich nicht zurueckrollen."));
    }
    response.Add ("undoScope", undoScope);
    response.Add ("successCount", (Int32) successCount);
    response.Add ("openingCount", (Int32) n);

    return response;
#endif
}
