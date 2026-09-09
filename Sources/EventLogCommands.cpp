#include "EventLogCommands.hpp"
#include "NotificationCommands.hpp"
#include "MigrationHelper.hpp"
#include "AddOnVersion.hpp"
#include "HashSet.hpp"
#include "HashTable.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>

namespace {

const Int32 kCapacity = 5000;

std::mutex                  gMutex;
std::deque<ELMEventLog::Entry> gEntries;
Int32                       gNextSeq  = 1;
Int32                       gRecorded = 0;
Int32                       gDropped  = 0;
bool                        gInstalled = false;
std::vector<std::pair<GS::UniString, GSErrCode>> gHandlerStatus;

// Zeitstempel als ISO 8601 UTC. Bewusst als std::string: der `since`-Vergleich ist
// ein reiner Zeichenkettenvergleich (ISO-UTC-Strings sortieren lexikografisch wie
// die Zeit selbst), damit hier kein Datumsparser mit Zeitzonenfallen entsteht.
std::string NowIso ()
{
    const auto now = std::chrono::system_clock::now ();
    const std::time_t t = std::chrono::system_clock::to_time_t (now);
    const long long ms =
        (long long) (std::chrono::duration_cast<std::chrono::milliseconds> (now.time_since_epoch ()).count () % 1000);
    std::tm utc = {};
#ifdef _WIN32
    gmtime_s (&utc, &t);
#else
    gmtime_r (&t, &utc);
#endif
    char buf[40];
    std::snprintf (buf, sizeof (buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                   utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                   utc.tm_hour, utc.tm_min, utc.tm_sec, (int) ms);
    return std::string (buf);
}

// Nutzername zu einer Teamwork-Nutzer-ID. Leerer String, wenn nicht auflösbar.
GS::UniString UserName (short userId)
{
    if (userId == 0 || !ACAPI_Teamwork_HasConnection ()) {
        return GS::UniString ();
    }
    GS::UniString name;
    if (ACAPI_Teamwork_GetUsernameFromId (userId, &name) != NoError) {
        return GS::UniString ();
    }
    return name;
}

GS::UniString ElemTypeName (const API_Elem_Head& head)
{
    return GetElementTypeNonLocalizedName (GetElemTypeId (head));
}

GS::UniString ProjectEventName (API_NotifyEventID id)
{
    switch (id) {
        case APINotify_New:                              return "ProjectNew";
        case APINotify_NewAndReset:                      return "ProjectNewAndReset";
        case APINotify_Open:                             return "ProjectOpen";
        case APINotify_PreSave:                          return "ProjectPreSave";
        case APINotify_Save:                             return "ProjectSave";
        case APINotify_Close:                            return "ProjectClose";
        case APINotify_Quit:                             return "ApplicationQuit";
        case APINotify_TempSave:                         return "ProjectTempSave";
        case APINotify_SendChanges:                      return "TeamworkSendChanges";
        case APINotify_ReceiveChanges:                   return "TeamworkReceiveChanges";
        case APINotify_ChangeProjectDB:                  return "ChangeProjectDB";
        case APINotify_ChangeWindow:                     return "ChangeWindow";
        case APINotify_ChangeFloor:                      return "ChangeFloor";
        case APINotify_ChangeLibrary:                    return "ChangeLibrary";
        case APINotify_AllInputFinished:                 return "AllInputFinished";
        case APINotify_UnitChanged:                      return "UnitChanged";
        case APINotify_SideviewCreated:                  return "SideviewCreated";
        case APINotify_SideviewRebuilt:                  return "SideviewRebuilt";
        case APINotify_PropertyVisibilityChanged:        return "PropertyVisibilityChanged";
        case APINotify_ClassificationVisibilityChanged:  return "ClassificationVisibilityChanged";
        case APINotify_ShowIn3DChanged:                  return "ShowIn3DChanged";
        default:                                         return "ProjectEventUnknown";
    }
}

// --- die Handler selbst ------------------------------------------------------

GSErrCode ProjectEventHandler (API_NotifyEventID notifID, Int32 param)
{
    GS::UniString detail;
    if (notifID == APINotify_ChangeLibrary) {
        detail = GS::UniString::Printf ("libPartIndex=%d", param);
    } else if (notifID == APINotify_ChangeProjectDB) {
        detail = (param == 1) ? GS::UniString ("cause=floorsEdited")
               : (param == 2) ? GS::UniString ("cause=merge")
                              : GS::UniString ("cause=other");
    } else if (param != 0) {
        detail = GS::UniString::Printf ("param=%d", param);
    }
    ELMEventLog::Add ("project", ProjectEventName (notifID), APINULLGuid, GS::UniString (),
                      GS::UniString (), detail);
    return NoError;
}

GSErrCode SelectionChangeHandler (const API_Neig* selElemNeig)
{
    API_Guid      guid = APINULLGuid;
    GS::UniString typeName;
    if (selElemNeig != nullptr) {
        guid = selElemNeig->guid;
        API_Elem_Head head = {};
        head.guid = guid;
        if (guid != APINULLGuid && ACAPI_Element_GetHeader (&head) == NoError) {
            typeName = ElemTypeName (head);
        }
    }
    ELMEventLog::Add ("selection", "SelectionChanged", guid, typeName, GS::UniString (), GS::UniString ());
    return NoError;
}

GSErrCode ToolChangeHandler (const API_ToolBoxItem* newToolMode)
{
    GS::UniString typeName;
    if (newToolMode != nullptr) {
        typeName = GetElementTypeNonLocalizedName (GetElemTypeId (newToolMode->type));
    }
    ELMEventLog::Add ("tool", "ToolChanged", APINULLGuid, typeName, GS::UniString (), GS::UniString ());
    return NoError;
}

GSErrCode LockableReservationChangeHandler (const API_Guid& objectId, short ownerId)
{
    ELMEventLog::Add ("reservation", "LockableReservationChanged", objectId, GS::UniString (),
                      UserName (ownerId),
                      GS::UniString::Printf ("ownerId=%d", (int) ownerId));
    return NoError;
}

void LogElementEvent (const API_NotifyElementType* n)
{
    if (n == nullptr) {
        return;
    }
    if (n->notifID == APINotifyElement_BeginEvents || n->notifID == APINotifyElement_EndEvents) {
        return;   // Klammern, keine echten Ereignisse
    }

    GS::UniString type;
    switch (n->notifID) {
        case APINotifyElement_New:                   type = "ElementNew";                break;
        case APINotifyElement_Copy:                  type = "ElementCopied";             break;
        case APINotifyElement_Change:                type = "ElementChanged";            break;
        case APINotifyElement_Edit:                  type = "ElementEdited";             break;
        case APINotifyElement_Delete:                type = "ElementDeleted";            break;
        case APINotifyElement_Undo_Created:          type = "ElementUndoCreated";        break;
        case APINotifyElement_Undo_Modified:         type = "ElementUndoModified";       break;
        case APINotifyElement_Undo_Deleted:          type = "ElementUndoDeleted";        break;
        case APINotifyElement_Redo_Created:          type = "ElementRedoCreated";        break;
        case APINotifyElement_Redo_Modified:         type = "ElementRedoModified";       break;
        case APINotifyElement_Redo_Deleted:          type = "ElementRedoDeleted";        break;
        case APINotifyElement_PropertyValueChange:   type = "ElementPropertyChanged";    break;
        case APINotifyElement_ClassificationChange:  type = "ElementClassificationChanged"; break;
        default:                                     type = "ElementEventUnknown";       break;
    }

    ELMEventLog::Add ("element", type, n->elemHead.guid, ElemTypeName (n->elemHead),
                      UserName (n->elemHead.userId), GS::UniString ());
}

} // namespace

// --- Weichen: ELM_SAB-Log UND Tapirs Notification-Client ---------------------
// Pro Benachrichtigungsart erlaubt das DevKit nur EINEN Handler. Beide Seiten
// registrieren deshalb diese Funktionen; sie protokollieren zuerst und geben
// danach unveraendert an Tapir weiter.

GSErrCode ELMCombinedElementEventHandler (const API_NotifyElementType* elemType)
{
    LogElementEvent (elemType);
    return AddElementNotificationClientCommand::ElementEventHandlerProc (elemType);
}

GSErrCode ELMCombinedReservationChangeHandler (const GS::HashTable<API_Guid, short>& reserved,
                                               const GS::HashSet<API_Guid>&          released,
                                               const GS::HashSet<API_Guid>&          deleted)
{
    for (const auto& kv : reserved) {
#ifdef ServerMainVers_2800
        const API_Guid& guid   = kv.key;
        const short     userId = kv.value;
#else
        const API_Guid& guid   = *kv.key;
        const short     userId = *kv.value;
#endif
        ELMEventLog::Add ("reservation", "ElementReserved", guid, GS::UniString (), UserName (userId),
                          GS::UniString ());
    }
    for (const auto& guid : released) {
        ELMEventLog::Add ("reservation", "ElementReleased", guid, GS::UniString (), GS::UniString (),
                          GS::UniString ());
    }
    for (const auto& guid : deleted) {
        ELMEventLog::Add ("reservation", "ElementDeletedByOther", guid, GS::UniString (), GS::UniString (),
                          GS::UniString ());
    }
    return AddElementNotificationClientCommand::ElementReservationChangeHandler (reserved, released, deleted);
}

// --- Log-Verwaltung ----------------------------------------------------------

namespace ELMEventLog {

void Add (const GS::UniString& category, const GS::UniString& type,
          const API_Guid& elemGuid, const GS::UniString& elemType,
          const GS::UniString& user, const GS::UniString& detail)
{
    std::lock_guard<std::mutex> lock (gMutex);
    Entry e;
    e.seq      = gNextSeq++;
    e.time     = NowIso ();
    e.type     = type;
    e.category = category;
    e.elemGuid = elemGuid;
    e.elemType = elemType;
    e.user     = user;
    e.detail   = detail;
    gEntries.push_back (e);
    ++gRecorded;
    while ((Int32) gEntries.size () > kCapacity) {
        gEntries.pop_front ();
        ++gDropped;
    }
}

std::vector<Entry> Snapshot ()
{
    std::lock_guard<std::mutex> lock (gMutex);
    return std::vector<Entry> (gEntries.begin (), gEntries.end ());
}

Stats GetStats ()
{
    std::lock_guard<std::mutex> lock (gMutex);
    Stats s;
    s.capacity = kCapacity;
    s.stored   = (Int32) gEntries.size ();
    s.recorded = gRecorded;
    s.dropped  = gDropped;
    return s;
}

Int32 Clear ()
{
    std::lock_guard<std::mutex> lock (gMutex);
    const Int32 n = (Int32) gEntries.size ();
    gEntries.clear ();
    gRecorded = 0;
    gDropped  = 0;
    return n;
}

std::vector<std::pair<GS::UniString, GSErrCode>> InstalledHandlers ()
{
    std::lock_guard<std::mutex> lock (gMutex);
    return gHandlerStatus;
}

void InstallHandlers ()
{
    if (gInstalled) {
        return;
    }
    gInstalled = true;

    std::vector<std::pair<GS::UniString, GSErrCode>> status;

    // Projekt-Ereignisse: Maske bewusst aus den EINZELN dokumentierten Enum-Werten
    // zusammengesetzt statt API_AllNotificationMask (0xFFFFFFFF) — dort stecken auch
    // undokumentierte Bits, und was Archicad mit denen tut, ist nicht zugesagt.
    const GSFlags projectEventMask =
        (GSFlags) (APINotify_New | APINotify_NewAndReset | APINotify_Open |
                   APINotify_PreSave | APINotify_Save | APINotify_Close |
                   APINotify_Quit | APINotify_TempSave |
                   APINotify_SendChanges | APINotify_ReceiveChanges |
                   APINotify_ChangeProjectDB | APINotify_ChangeWindow |
                   APINotify_ChangeFloor | APINotify_ChangeLibrary |
                   APINotify_AllInputFinished | APINotify_UnitChanged |
                   APINotify_PropertyVisibilityChanged |
                   APINotify_ClassificationVisibilityChanged |
                   APINotify_ShowIn3DChanged);
    status.push_back ({ "ProjectEvent (ACAPI_ProjectOperation_CatchProjectEvent)",
                        ACAPI_ProjectOperation_CatchProjectEvent (projectEventMask, ProjectEventHandler) });

    status.push_back ({ "SelectionChange (ACAPI_Notification_CatchSelectionChange)",
                        ACAPI_Notification_CatchSelectionChange (SelectionChangeHandler) });

    status.push_back ({ "ToolChange (ACAPI_Notification_CatchToolChange)",
                        ACAPI_Notification_CatchToolChange (ToolChangeHandler) });

    // Neue Elemente global (elemType = nullptr).
    status.push_back ({ "NewElement (ACAPI_Element_CatchNewElement)",
                        ACAPI_Element_CatchNewElement (nullptr, ELMCombinedElementEventHandler) });

    // Aenderung/Loeschung: der Observer-Handler wird hier installiert, greift aber
    // nur an Elementen mit AttachObserver — siehe WatchElements.
    status.push_back ({ "ElementObserver (ACAPI_Element_InstallElementObserver)",
                        ACAPI_Element_InstallElementObserver (ELMCombinedElementEventHandler) });

    status.push_back ({ "ElementReservationChange (ACAPI_Notification_CatchElementReservationChange)",
                        ACAPI_Notification_CatchElementReservationChange (ELMCombinedReservationChangeHandler) });

    status.push_back ({ "LockableReservationChange (ACAPI_Notification_CatchLockableReservationChange)",
                        ACAPI_Notification_CatchLockableReservationChange (LockableReservationChangeHandler) });

    {
        std::lock_guard<std::mutex> lock (gMutex);
        gHandlerStatus = status;
    }

    Add ("project", "EventLogStarted", APINULLGuid, GS::UniString (), GS::UniString (),
         GS::UniString ("ELM_SAB ") + ELM_SAB_VERSION);
}

} // namespace ELMEventLog

// --- GetRecentEvents ---------------------------------------------------------

GS::Optional<GS::UniString> GetRecentEventsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "since": { "type": "string",
                "description": "ISO-8601-Zeitstempel in UTC, z. B. 2026-09-09T07:30:00Z. Nur Ereignisse ab diesem Zeitpunkt. Vergleich ist ein Zeichenkettenvergleich auf denselben Zeitstempeln, die die Antwort liefert." },
            "sinceSeq": { "type": "integer",
                "description": "Nur Ereignisse mit seq groesser als dieser Wert — der stabilere Weg zum Weiterlesen: newestSeq der letzten Antwort hier wieder einsetzen." },
            "categories": { "type": "array", "items": { "type": "string",
                "enum": ["project", "element", "selection", "tool", "reservation"] },
                "description": "Optionaler Filter auf Ereigniskategorien." },
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] },
                "description": "Optionaler Filter: nur Ereignisse zu diesen Elementen." },
            "limit": { "type": "integer", "minimum": 1,
                "description": "Hoechstzahl zurueckgegebener Ereignisse (die JUENGSTEN). Default 200." }
        },
        "additionalProperties": false
    })");
}

GS::Optional<GS::UniString> GetRecentEventsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "events": { "type": "array", "items": { "type": "object" } },
            "eventCount": { "type": "integer" },
            "matchedCount": { "type": "integer" },
            "capacity": { "type": "integer" },
            "storedCount": { "type": "integer" },
            "recordedCount": { "type": "integer" },
            "droppedCount": { "type": "integer" },
            "oldestSeq": { "type": "integer" },
            "newestSeq": { "type": "integer" },
            "serverTime": { "type": "string" },
            "installedHandlers": { "type": "array", "items": { "type": "object" } }
        },
        "required": ["events", "eventCount"]
    })");
}

GS::ObjectState GetRecentEventsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::UniString sinceStr;
    const bool hasSince = parameters.Get ("since", sinceStr);
    const std::string since = hasSince ? std::string (sinceStr.ToCStr ().Get ()) : std::string ();

    Int32 sinceSeq = 0;
    parameters.Get ("sinceSeq", sinceSeq);

    Int32 limit = 200;
    parameters.Get ("limit", limit);
    if (limit < 1) {
        limit = 1;
    }

    GS::Array<GS::UniString> categories;
    parameters.Get ("categories", categories);

    GS::Array<GS::ObjectState> elementFilter;
    parameters.Get ("elements", elementFilter);
    GS::HashSet<API_Guid> wantedGuids;
    for (const GS::ObjectState& item : elementFilter) {
        const API_Guid g = GetGuidFromElementsArrayItem (item);
        if (g != APINULLGuid) {
            wantedGuids.Add (g);
        }
    }

    const std::vector<ELMEventLog::Entry> all = ELMEventLog::Snapshot ();

    std::vector<const ELMEventLog::Entry*> matched;
    for (const ELMEventLog::Entry& e : all) {
        if (hasSince && e.time < since) {
            continue;
        }
        if (sinceSeq > 0 && e.seq <= sinceSeq) {
            continue;
        }
        if (!categories.IsEmpty () && !categories.Contains (e.category)) {
            continue;
        }
        if (!wantedGuids.IsEmpty () && !wantedGuids.Contains (e.elemGuid)) {
            continue;
        }
        matched.push_back (&e);
    }

    const size_t first = (matched.size () > (size_t) limit) ? matched.size () - (size_t) limit : 0;

    GS::ObjectState response;
    const auto& events = response.AddList<GS::ObjectState> ("events");
    for (size_t i = first; i < matched.size (); ++i) {
        const ELMEventLog::Entry& e = *matched[i];
        GS::ObjectState o;
        o.Add ("seq",      e.seq);
        o.Add ("time",     GS::UniString (e.time.c_str ()));
        o.Add ("category", e.category);
        o.Add ("type",     e.type);
        if (e.elemGuid != APINULLGuid) {
            o.Add ("elementId", CreateGuidObjectState (e.elemGuid));
        }
        if (!e.elemType.IsEmpty ()) {
            o.Add ("elementType", e.elemType);
        }
        if (!e.user.IsEmpty ()) {
            o.Add ("user", e.user);
        }
        if (!e.detail.IsEmpty ()) {
            o.Add ("detail", e.detail);
        }
        events (o);
    }

    const ELMEventLog::Stats stats = ELMEventLog::GetStats ();
    response.Add ("eventCount",    (Int32) (matched.size () - first));
    response.Add ("matchedCount",  (Int32) matched.size ());
    response.Add ("capacity",      stats.capacity);
    response.Add ("storedCount",   stats.stored);
    response.Add ("recordedCount", stats.recorded);
    response.Add ("droppedCount",  stats.dropped);
    response.Add ("oldestSeq",     all.empty () ? 0 : all.front ().seq);
    response.Add ("newestSeq",     all.empty () ? 0 : all.back ().seq);
    response.Add ("serverTime",    GS::UniString (NowIso ().c_str ()));

    const auto& handlers = response.AddList<GS::ObjectState> ("installedHandlers");
    for (const auto& kv : ELMEventLog::InstalledHandlers ()) {
        handlers (GS::ObjectState ("notification", kv.first,
                                   "errorCode", (Int32) kv.second,
                                   "installed", kv.second == NoError));
    }

    return response;
}

// --- ClearEvents -------------------------------------------------------------

GS::Optional<GS::UniString> ClearEventsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "clearedCount": { "type": "integer" },
            "success": { "type": "boolean" }
        },
        "required": ["clearedCount", "success"]
    })");
}

GS::ObjectState ClearEventsCommand::Execute (const GS::ObjectState&, GS::ProcessControl&) const
{
    const Int32 cleared = ELMEventLog::Clear ();
    GS::ObjectState response;
    response.Add ("clearedCount", cleared);
    response.Add ("success", true);
    return response;
}

// --- WatchElements -----------------------------------------------------------

GS::Optional<GS::UniString> WatchElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "items": { "type": "object",
                "properties": { "elementId": { "type": "object",
                    "properties": { "guid": { "type": "string" } }, "required": ["guid"] } },
                "required": ["elementId"] } },
            "watch": { "type": "boolean",
                "description": "true (Default) haengt einen Observer an, false nimmt ihn wieder ab." }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> WatchElementsCommand::GetResponseSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "executionResults": { "type": "array", "items": { "type": "object" } },
            "watchedCount": { "type": "integer" }
        },
        "required": ["executionResults"]
    })");
}

GS::ObjectState WatchElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> elements;
    parameters.Get ("elements", elements);

    bool watch = true;
    parameters.Get ("watch", watch);

    Int32 okCount = 0;
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (const GS::ObjectState& item : elements) {
        const API_Guid guid = GetGuidFromElementsArrayItem (item);
        if (guid == APINULLGuid) {
            results (CreateFailedExecutionResult (APIERR_BADPARS, "elementId fehlt oder ist unlesbar."));
            continue;
        }
        const GSErrCode err = watch ? ACAPI_Element_AttachObserver (guid)
                                    : ACAPI_Element_DetachObserver (guid);
        if (err == NoError) {
            ++okCount;
            results (CreateSuccessfulExecutionResult ());
        } else {
            results (CreateFailedExecutionResult (err, watch ? "AttachObserver fehlgeschlagen."
                                                            : "DetachObserver fehlgeschlagen."));
        }
    }
    response.Add ("watchedCount", okCount);
    return response;
}
