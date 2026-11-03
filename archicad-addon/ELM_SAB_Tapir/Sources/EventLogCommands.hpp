// ELM_SAB Stufe 2 „Auge" — Ereignis-Log.
//
// Zweck: Ein Agent, der Archicad fernsteuert, sieht bisher nur, was er selbst
// zurueckliest. Alles, was der NUTZER zwischendurch tut (Projekt schliessen,
// Teamwork-Receive, Bibliothek neu laden, Elemente loeschen), bleibt unsichtbar —
// und genau daraus entstehen die stillen Fehlschlaege aus reference/mcp-extension.md.
// Dieses Log fuellt sich passiv aus den ACAPI-Benachrichtigungen und wird per
// `ELM_SAB.GetRecentEvents` abgefragt.
//
// WAS DAS DEVKIT HERGIBT (AC27–29, geprueft in ACAPinc.h / APIdefs_Callback.h):
//   * ACAPI_ProjectOperation_CatchProjectEvent  — Open/Save/Close/Quit/Send/
//     Receive/ChangeWindow/ChangeFloor/ChangeLibrary/AllInputFinished u. a.
//   * ACAPI_Notification_CatchSelectionChange   — Selektionswechsel
//   * ACAPI_Notification_CatchToolChange        — Werkzeugwechsel
//   * ACAPI_Element_CatchNewElement             — NEUE Elemente (global, elemType=nullptr)
//   * ACAPI_Element_InstallElementObserver      — Aenderung/Loeschung, ABER nur fuer
//     Elemente, an denen vorher ACAPI_Element_AttachObserver haengt.
//   * ACAPI_Notification_CatchElementReservationChange  — Teamwork-Reservierungen
//   * ACAPI_Notification_CatchLockableReservationChange — Reservierung von
//     Attributen/Favoriten/Modellansichten
//
// WAS ES NICHT GIBT (nicht erfunden, sondern dokumentiert): eine globale
// „irgendein Element wurde geaendert/geloescht"-Benachrichtigung. Aenderung und
// Loeschung kommen ausschliesslich elementweise ueber AttachObserver. Deshalb
// gibt es `ELM_SAB.WatchElements`: erst damit landen Change/Delete-Ereignisse fuer
// die genannten Elemente im Log. Ein Rundum-Attach ueber das ganze Modell waere am
// THN (>100k Elemente) zu teuer und wird bewusst NICHT gemacht.
//
// Ein weiterer DevKit-Zwang: pro Benachrichtigungsart darf nur EIN Handler
// installiert sein. Tapirs SetElementNotificationClient installiert seine eigenen
// Element- und Reservierungs-Handler. Damit sich die beiden nicht gegenseitig
// abschalten, gehen beide Seiten ueber die Weichen ELMCombinedElementEventHandler /
// ELMCombinedReservationChangeHandler (siehe NotificationCommands.cpp).
#pragma once

#include "ELMCommandBase.hpp"

#include <string>
#include <vector>

namespace ELMEventLog {

struct Entry {
    Int32          seq       = 0;
    std::string    time;                    // ISO 8601 UTC, millisekundengenau
    GS::UniString  type;                    // z. B. "ProjectOpen", "ElementDeleted"
    GS::UniString  category;                // project | element | selection | tool | reservation
    API_Guid       elemGuid  = APINULLGuid;
    GS::UniString  elemType;
    GS::UniString  user;
    GS::UniString  detail;
};

struct Stats {
    Int32 capacity  = 0;
    Int32 stored    = 0;
    Int32 recorded  = 0;   // insgesamt seit Start bzw. seit ClearEvents
    Int32 dropped   = 0;   // vom Ringpuffer verworfen
};

// Wird EINMAL aus Initialize() gerufen. Meldet pro Benachrichtigungsart, ob die
// Registrierung geklappt hat — abrufbar ueber GetRecentEvents.installed.
void InstallHandlers ();

// Benachrichtigungsarten und ihr Registrierungsergebnis (Name -> GSErrCode).
std::vector<std::pair<GS::UniString, GSErrCode>> InstalledHandlers ();

void  Add (const GS::UniString& category, const GS::UniString& type,
           const API_Guid& elemGuid, const GS::UniString& elemType,
           const GS::UniString& user, const GS::UniString& detail);

std::vector<Entry> Snapshot ();
Stats GetStats ();
Int32 Clear ();

} // namespace ELMEventLog

// Weichen fuer die Handler, die sich ELM_SAB und Tapir teilen muessen.
GSErrCode ELMCombinedElementEventHandler (const API_NotifyElementType* elemType);
GSErrCode ELMCombinedReservationChangeHandler (const GS::HashTable<API_Guid, short>& reserved,
                                               const GS::HashSet<API_Guid>&          released,
                                               const GS::HashSet<API_Guid>&          deleted);

class GetRecentEventsCommand : public ELMCommandBase
{
public:
    GetRecentEventsCommand () = default;
    virtual GS::String GetName () const override { return "GetRecentEvents"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

class ClearEventsCommand : public ELMCommandBase
{
public:
    ClearEventsCommand () = default;
    virtual GS::String GetName () const override { return "ClearEvents"; }
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};

// Haengt Element-Observer an bzw. ab — nur so entstehen Change/Delete-Ereignisse.
class WatchElementsCommand : public ELMCommandBase
{
public:
    WatchElementsCommand () = default;
    virtual GS::String GetName () const override { return "WatchElements"; }
    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override;
    virtual GS::Optional<GS::UniString> GetResponseSchema () const override;
    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& processControl) const override;
};
