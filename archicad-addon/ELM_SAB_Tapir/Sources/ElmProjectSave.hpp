// ELMonkey — offenes Projekt am Ort sichern (fuer ELM_SAB.SaveProjectCopy).
// 0.9.34 lieferte die parameterlose Form ACAPI_ProjectOperation_Save () live APIERR_READONLY; der
// damalige Fix (API_FileSavePars auf den eigenen Pfad) hat einen Haken, den Tapir 1.7.x (#681)
// gemessen hat: er ueberschreibt auch ein schreibgeschuetzt geoeffnetes Projekt (veraltete Sperre,
// anderer Nutzer), wo Ablage > Sichern ablehnt. Die eigentliche Ursache: das parameterlose Sichern
// wirkt auf das AKTIVE Fenster - aus 3D/Schnitt/Ansicht gibt es nichts zu sichern -> READONLY.
// 0.10.0: wie Tapirs SaveProject Grundriss kurz aktivieren, normal sichern, Fenster zurueckstellen.
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "OnExit.hpp"

#include <string>

inline GSErrCode SaveProjectInPlaceELM ()
{
    API_WindowInfo previousWindow = {};
    bool windowSwitched = false;
    if (ACAPI_Window_GetCurrentWindow (&previousWindow) == NoError &&
        previousWindow.typeID != APIWind_FloorPlanID) {
        API_WindowInfo floorPlanWindow = {};
        floorPlanWindow.typeID = APIWind_FloorPlanID;
        windowSwitched = ACAPI_Window_ChangeWindow (&floorPlanWindow) == NoError;
    }
    const GS::OnExit restoreWindow ([&] () {
        if (windowSwitched) {
            ACAPI_Window_ChangeWindow (&previousWindow);
        }
    });
    return ACAPI_ProjectOperation_Save ();
}

// Lesbarer Name der haeufigsten Speicher-Fehler fuer die Fehlermeldung.
inline GS::UniString SaveErrorNameELM (GSErrCode err)
{
    switch (err) {
        case APIERR_READONLY:     return GS::UniString ("APIERR_READONLY - Projekt schreibgeschuetzt geoeffnet?");
        case APIERR_REFUSEDCMD:   return GS::UniString ("APIERR_REFUSEDCMD");
        case APIERR_REFUSEDPAR:   return GS::UniString ("APIERR_REFUSEDPAR");
        case APIERR_BADPARS:      return GS::UniString ("APIERR_BADPARS");
        case APIERR_NOTSUPPORTED: return GS::UniString ("APIERR_NOTSUPPORTED");
        default:                  return GS::UniString ("Code ") + GS::UniString (std::to_string ((long) err).c_str ());
    }
}
