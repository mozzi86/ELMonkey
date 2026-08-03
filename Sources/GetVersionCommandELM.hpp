// ELM_SAB GetAddOnVersion — meldet, welcher ELM_SAB-Build tatsaechlich geladen ist.
//
// Warum es den Befehl braucht: TapirCommand.GetAddOnVersion liefert ADDON_VERSION, also
// die Version des Tapir-Unterbaus (1.5.4). Sie bleibt gleich, egal welcher ELM_SAB-Stand
// im Bundle steckt. Zusammen mit dem rollenden CI-Tag `elm-sab-tapir-latest` war damit am
// laufenden Archicad nicht feststellbar, ob das installierte Bundle aktuell ist — ein
// veraltetes Bundle antwortete identisch zu einem frischen.
//
// Header-only gehalten, damit kein CMake-Eingriff noetig ist. Kein C++20 (AC27/28 bauen
// nach C++17).
#pragma once

#include "ELMCommandBase.hpp"
#include "AddOnVersion.hpp"

class ELMGetAddOnVersionCommand : public ELMCommandBase
{
public:
    ELMGetAddOnVersionCommand () = default;

    virtual GS::String GetName () const override
    {
        return "GetAddOnVersion";
    }

    virtual GS::Optional<GS::UniString> GetResponseSchema () const override
    {
        return R"({
    "type": "object",
    "properties": {
        "version": {
            "type": "string",
            "description": "Version des ELM_SAB-Bundles."
        },
        "tapirBaseVersion": {
            "type": "string",
            "description": "Version des zugrundeliegenden Tapir-Stands."
        },
        "buildStamp": {
            "type": "string",
            "description": "Zeitstempel des Builds — identifiziert den konkreten Bundle-Build."
        }
    },
    "additionalProperties": false,
    "required": [
        "version",
        "tapirBaseVersion",
        "buildStamp"
    ]
})";
    }

    virtual GS::ObjectState Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const override
    {
        GS::ObjectState response;
        response.Add ("version", GS::UniString (ELM_SAB_VERSION));
        response.Add ("tapirBaseVersion", GS::UniString (ADDON_VERSION));
        response.Add ("buildStamp", GS::UniString (ELM_SAB_BUILD_STAMP));
        return response;
    }
};
