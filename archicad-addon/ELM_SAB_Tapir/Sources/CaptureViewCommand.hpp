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
//
// ELM_SAB 0.9.25 (2026-10-13, WiSchu, AC29 Build 5101): ACAPI_ProjectOperation_Save lieferte live
// APIERR_BADID (-2130313115) fuer Grundriss UND Layout, ohne dass eine Datei entstand. CaptureView
// hat seit 0.9.12 nie ein Bild geliefert. Ursache (DevKit 29.3100, nicht live belegt):
// APIColorDepth_FromSourceImage (-1) ist nur fuer die Bildkonvertierung definiert
// (APIdefs_Goodies.h Z. 280, API_MimePicture::outDepth); beim Speichern eines Fensters gibt es
// kein Quellbild. Das einzige DevKit-Beispiel fuer API_SavePars_Picture, Do_Save_TiffFile
// (Examples/Automate_Functions/Src/Automate_Functions.cpp), nimmt APIColorDepth_256C. Jetzt gilt die
// Echtfarbtiefe je Plattform (API_ColorDepthID ist plattformgetrennt, APIdefs_Elements.h Z. 10893
// und 10900): Mac MiC, Windows TC24, sonst 256C. Liefert der erste Save APIERR_BADID, folgt genau
// EIN Rueckfall auf 256C; die Antwort meldet die benutzte Tiefe in colorDepth. Bewusst unveraendert
// (nur eine Ursache pro Version): Dateityp PNG, dithered, view2D/crop, Fenstertyp-Erkennung,
// Ruecklese und keepSelectionHighlight (bleibt false). Stand: NICHT live verifiziert.
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
        // Erfolgszweig: 0.9.25 + optionales colorDepth (additionalProperties:false -> es MUSS in properties
        // stehen, sonst 4009); ELMResponseSchemaWithError erlaubt zusaetzlich {error:{code,message}}.
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
        },
        "colorDepth": {
            "type": "string",
            "description": "Tatsaechlich benutzte Farbtiefe des PNG-Exports: MiC = macOS, TC24 = Windows, 256C = Rueckfall bei APIERR_BADID, ab 0.9.25."
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

        // 0.9.25: Primaertiefe je Plattform, bei APIERR_BADID genau EIN Rueckfall auf 256C.
        API_ColorDepthID primaryDepth = APIColorDepth_256C;
        GS::UniString usedDepthName;
        GetPrimaryColorDepth (primaryDepth, usedDepthName);

        err = TrySave (fileLocation, primaryDepth, !is3D, crop);
        GS::UniString attempts = GS::UniString::Printf ("Farbtiefe %T: Fehler %d", usedDepthName.ToPrintf (), (int) err);
        if (err == APIERR_BADID && primaryDepth != APIColorDepth_256C) {
            usedDepthName = GS::UniString ("256C");
            err = TrySave (fileLocation, APIColorDepth_256C, !is3D, crop);
            attempts += GS::UniString::Printf (", Rueckfall 256C: Fehler %d", (int) err);
        }
        if (err != NoError) {
            if (windowInfo.typeID == APIWind_LayoutID || windowInfo.typeID == APIWind_MasterLayoutID) {
                return CreateErrorResponseELM (err, GS::UniString::Printf (
                    "CaptureView: Layout-Fenster ('%T'): PNG-Export per API nicht moeglich (%T) - Zeichnung/Grundriss oeffnen oder Layout als PDF publizieren",
                    windowType.ToPrintf (), attempts.ToPrintf ()));
            }
            return CreateErrorResponseELM (err, GS::UniString::Printf (
                "CaptureView: Fensterinhalt (Fenstertyp '%T') nicht als PNG speicherbar (%T) - Fenstertyp exportierbar? Pfad beschreibbar?",
                windowType.ToPrintf (), attempts.ToPrintf ()));
        }

        // Ruecklese: NoError beweist nichts (mcp-extension.md) — die Datei muss
        // tatsaechlich existieren, bevor wir ihren Pfad als Erfolg melden.
        // Schwaeche (bekannt): geprueft wird nur die Existenz, eine Altdatei am Zielpfad taeuscht Erfolg vor.
        bool exists = false;
        if (IO::fileSystem.Contains (fileLocation, &exists) != NoError || !exists) {
            return CreateErrorResponseELM (APIERR_COMMANDFAILED, GS::UniString::Printf (
                "CaptureView: Save meldete Erfolg (Fenstertyp '%T', Farbtiefe %T), aber die PNG-Datei existiert nicht (Ruecklese)",
                windowType.ToPrintf (), usedDepthName.ToPrintf ()));
        }

        GS::ObjectState response;
        response.Add ("imagePath", outputPath);
        response.Add ("view2D", !is3D);
        response.Add ("colorDepth", usedDepthName);
        return response;
    }

private:
    // Primaere Farbtiefe je Plattform. API_ColorDepthID ist plattformgetrennt (APIdefs_Elements.h):
    // MiC steht nur im macintosh-Block, TC24 nur im WINDOWS-Block, 256C ist gemeinsam. Der #if ist
    // Pflicht, APIColorDepth_TC24 ist im Mac-Build undeklariert.
    static void GetPrimaryColorDepth (API_ColorDepthID& depth, GS::UniString& name)
    {
#if defined (macintosh)
        depth = APIColorDepth_MiC;
        name = GS::UniString ("MiC");
#elif defined (WINDOWS)
        depth = APIColorDepth_TC24;
        name = GS::UniString ("TC24");
#else
        depth = APIColorDepth_256C;
        name = GS::UniString ("256C");
#endif
    }

    // Ein Save-Versuch mit frisch aufgebauten Parametern.
    static GSErrCode TrySave (IO::Location& file, API_ColorDepthID depth, bool view2D, bool crop)
    {
        API_FileSavePars fsp = {};
        fsp.fileTypeID = APIFType_PNGFile;
        fsp.file = &file;

        API_SavePars_Picture pict = {};
        // Zero-Init waere APIColorDepth_BW (= 0, Schwarzweiss!) - deshalb wird colorDepth immer
        // explizit gesetzt. FromSourceImage ist hier NICHT gueltig (siehe Kopfkommentar 0.9.25).
        pict.colorDepth = depth;
        pict.dithered = false;
        pict.view2D = view2D;
        pict.crop = crop;

        return ACAPI_ProjectOperation_Save (&fsp, &pict);
    }
};
