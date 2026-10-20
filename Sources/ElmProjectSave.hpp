// ELMonkey 0.9.34 — offenes Projekt am Ort sichern (fuer ELM_SAB.SaveProjectCopy und Tapir SaveProject).
// Die parameterlose Form ACAPI_ProjectOperation_Save () lieferte live (AC29, 20.10.2026, lokale .pln,
// eigene .lck, Cmd+S speichert ohne Meldung) APIERR_READONLY - auch Tapirs SaveProject scheiterte so.
// Laut DevKit kommt READONLY auch, wenn das Projekt "nie gesichert" wurde und fileSavePars fehlt.
// Deshalb wie bei CaptureView: API_FileSavePars mit PLN-Typ und dem Pfad des Originals. Teamwork und
// unbenannte Projekte haben keinen lokalen Pfad - dort bleibt der alte Weg.
#pragma once

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "Location.hpp"

#include <string>

inline GSErrCode SaveProjectInPlaceELM ()
{
    API_ProjectInfo info = {};
    const GSErrCode infoErr = ACAPI_ProjectOperation_Project (&info);
    if (infoErr != NoError)
        return infoErr;
    if (info.teamwork || info.untitled || info.location == nullptr)
        return ACAPI_ProjectOperation_Save ();

    IO::Location file = *info.location;
    API_FileSavePars fsp = {};
    fsp.fileTypeID = APIFType_PlanFile;
    fsp.file = &file;
    return ACAPI_ProjectOperation_Save (&fsp);
}

// Lesbarer Name der haeufigsten Speicher-Fehler fuer die Fehlermeldung.
inline GS::UniString SaveErrorNameELM (GSErrCode err)
{
    switch (err) {
        case APIERR_READONLY:   return GS::UniString ("APIERR_READONLY");
        case APIERR_REFUSEDCMD: return GS::UniString ("APIERR_REFUSEDCMD");
        case APIERR_REFUSEDPAR: return GS::UniString ("APIERR_REFUSEDPAR");
        case APIERR_BADPARS:    return GS::UniString ("APIERR_BADPARS");
        case APIERR_NOTSUPPORTED: return GS::UniString ("APIERR_NOTSUPPORTED");
        default:                return GS::UniString ("Code ") + GS::UniString (std::to_string ((long) err).c_str ());
    }
}
