// ELM_SAB CaptureView — speichert den Inhalt des aktiven Fensters als PNG-Datei.
//
// Warum es den Befehl braucht: Tapir (bis 1.5.7 geprueft) hat keinen Befehl, der das
// aktive Fenster als Bild liefert — GetElementPreviewImage/GetRoomImage rendern nur
// Einzelelemente bzw. Raeume. Fuer den Agenten-Workflow ist der komplette
// Fensterinhalt aber die Ruecklese schlechthin: HighlightElements setzen,
// FitInWindow, CaptureView, PNG lesen. Idee aus revit-mcp-python (get_revit_view),
// Abwaegung in ~/.scratch/elmonkey/pyrevit-vergleich.md.
//
// Header-only wie GetVersionCommandELM, damit kein CMake-Eingriff noetig ist.
// Kein C++20 (AC27/28 bauen nach C++17). ACAPI_ProjectOperation_Save mit Parametern
// und ACAPI_Window_GetCurrentWindow existieren ab AC27 nativ (MigrationHelper shimt
// nur <2700, betrifft uns nicht).
//
// ELM_SAB 0.9.23 (2026-10-13, WiSchu Layout FR_00-06.1): Fehlerantworten verletzten das
// Antwortschema (additionalProperties:false + required imagePath/view2D) -> der Client sah nur
// 4009, nie die Ursache. Jetzt oneOf Erfolg|Fehler (ELMResponseSchemaWithError), Fenstertyp +
// echter Archicad-Code im Text. Der Export wird weiter fuer JEDEN Fenstertyp versucht (keine
// Typsperre); Layout/MasterLayout bekommen bei Fehlschlag einen Hinweis auf Zeichnung/Grundriss
// bzw. PDF-Publizieren — das DevKit (API_SavePars_Picture) kennt keinen eigenen Layout-Weg.
#pragma once

#include "ELMCommandBase.hpp"
#include "FileSystem.hpp"

class CaptureViewCommand : public ELMCommandBase
{
public:
    CaptureViewCommand () = default;

    virtual GS::String GetName () const override
    {
        return "CaptureView";
    }

    virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override
    {
        return R"({
    "type": "object",
    "properties": {
        "outputPath": {
            "type": "string",
            "description": "Absoluter Zielpfad der PNG-Datei. Eine vorhandene Datei wird überschrieben."
        },
        "crop": {
            "type": "boolean",
            "description": "Auf den aktuellen Zoom beschneiden (nur 2D-Fenster wirksam). Default: true."
        }
    },
    "additionalProperties": false,
    "required": [
        "outputPath"
    ]
})";
    }

    virtual GS::Optional<GS::UniString> GetResponseSchema () const override
    {
        // Erfolgszweig unveraendert; ELMResponseSchemaWithError erlaubt zusaetzlich {error:{code,message}}.
        return ELMResponseSchemaWithError (GS::UniString (R"({
    "type": "object",
    "properties": {
        "imagePath": {
            "type": "string",
            "description": "Pfad der geschriebenen PNG-Datei."
        },
        "view2D": {
            "type": "boolean",
            "description": "true = 2D-Fensterinhalt gespeichert, false = 3D-Fenster."
        }
    },
    "additionalProperties": false,
    "required": [
        "imagePath",
        "view2D"
    ]
})"));
    }

    virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
    {
        GS::UniString outputPath;
        if (!parameters.Get ("outputPath", outputPath) || outputPath.IsEmpty ()) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("CaptureView: outputPath fehlt oder ist leer"));
        }

        bool crop = true;
        parameters.Get ("crop", crop);

        // 2D/3D am aktiven Fenster erkennen — ein falsch gesetztes view2D liefert
        // leere oder veraltete Bilder statt eines Fehlers.
        API_WindowInfo windowInfo = {};
        GSErrCode err = ACAPI_Window_GetCurrentWindow (&windowInfo);
        if (err != NoError) {
            return CreateErrorResponseELM (err, GS::UniString::Printf ("CaptureView: aktives Fenster nicht ermittelbar (Fehler %d)", (int) err));
        }
        // Fenstertyp steht ab hier in JEDER Meldung (0.9.23).
        const GS::UniString windowType = WindowTypeNameELM (windowInfo.typeID);
        const bool is3D = (windowInfo.typeID == APIWind_3DModelID);

        IO::Location fileLocation (outputPath);
        API_FileSavePars fsp = {};
        fsp.fileTypeID = APIFType_PNGFile;
        fsp.file = &fileLocation;

        API_SavePars_Picture pict = {};
        // Zero-Init waere APIColorDepth_BW (= 0, Schwarzweiss!) — deshalb explizit.
        pict.colorDepth = APIColorDepth_FromSourceImage;
        pict.dithered = false;
        pict.view2D = !is3D;
        pict.crop = crop;

        err = ACAPI_ProjectOperation_Save (&fsp, &pict);
        if (err != NoError) {
            if (windowInfo.typeID == APIWind_LayoutID || windowInfo.typeID == APIWind_MasterLayoutID) {
                return CreateErrorResponseELM (err, GS::UniString::Printf (
                    "CaptureView: Layout-Fenster ('%T'): PNG-Export per API nicht moeglich (Fehler %d) - Zeichnung/Grundriss oeffnen oder Layout als PDF publizieren",
                    windowType.ToPrintf (), (int) err));
            }
            return CreateErrorResponseELM (err, GS::UniString::Printf (
                "CaptureView: Fensterinhalt (Fenstertyp '%T') nicht als PNG speicherbar (Fehler %d) - Fenstertyp exportierbar? Pfad beschreibbar?",
                windowType.ToPrintf (), (int) err));
        }

        // Ruecklese: NoError beweist nichts (mcp-extension.md) — die Datei muss
        // tatsaechlich existieren, bevor wir ihren Pfad als Erfolg melden.
        bool exists = false;
        if (IO::fileSystem.Contains (fileLocation, &exists) != NoError || !exists) {
            return CreateErrorResponseELM (APIERR_COMMANDFAILED, GS::UniString::Printf (
                "CaptureView: Save meldete Erfolg (Fenstertyp '%T'), aber die PNG-Datei existiert nicht (Ruecklese)",
                windowType.ToPrintf ()));
        }

        GS::ObjectState response;
        response.Add ("imagePath", outputPath);
        response.Add ("view2D", !is3D);
        return response;
    }
};
