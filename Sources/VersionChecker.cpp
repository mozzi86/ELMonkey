#include "VersionChecker.hpp"
#include "AddOnVersion.hpp"

#include "HTTP/Client/ClientConnection.hpp"
#include "HTTP/Client/Request.hpp"
#include "HTTP/Client/Response.hpp"
#include "JSON/Value.hpp"
#include "JSON/JDOMParser.hpp"
#include "IBinaryChannelUtilities.hpp"
#include "IOBinProtocolXs.hpp"
#include "IChannelX.hpp"
#include "StringConversion.hpp"

#include <map>
#include <string>

static std::unique_ptr<VersionChecker> Intance;

// "a.b.c" numerisch vergleichen: <0, 0, >0. Nicht-Ziffern beenden eine Stelle.
int CompareDottedVersions (const GS::UniString& a, const GS::UniString& b)
{
    const std::string sa = a.ToCStr ().Get ();
    const std::string sb = b.ToCStr ().Get ();
    size_t ia = 0, ib = 0;
    while (ia < sa.size () || ib < sb.size ()) {
        long va = 0, vb = 0;
        while (ia < sa.size () && sa[ia] != '.') { if (sa[ia] >= '0' && sa[ia] <= '9') va = va * 10 + (sa[ia] - '0'); ++ia; }
        while (ib < sb.size () && sb[ib] != '.') { if (sb[ib] >= '0' && sb[ib] <= '9') vb = vb * 10 + (sb[ib] - '0'); ++ib; }
        if (va != vb) return va < vb ? -1 : 1;
        if (ia < sa.size ()) ++ia;
        if (ib < sb.size ()) ++ib;
    }
    return 0;
}

void VersionChecker::CreateInstance (GS::UInt16 acMainVersion)
{
    Intance.reset (new VersionChecker (acMainVersion));
}

bool VersionChecker::IsUsingLatestVersion ()
{
    if (!Intance) {
        return true;
    }

    // ELMonkey 0.9.30: numerisch gegen die ELMonkey-Version vergleichen (Text-Vergleich hielt
    // "0.9.30" fuer kleiner als "0.9.9"). Unbekannte/leere Release-Version = kein Hinweis.
    return Intance->latestVersion.IsEmpty () || CompareDottedVersions (Intance->latestVersion, ELM_SAB_VERSION) <= 0;
}

const GS::UniString& VersionChecker::LatestVersion ()
{
    if (!Intance) {
        return GS::EmptyUniString;
    }

    return Intance->latestVersion;
}

const GS::UniString& VersionChecker::LatestVersionName ()
{
    if (!Intance) {
        return GS::EmptyUniString;
    }

    return Intance->latestVersionName;
}

bool VersionChecker::Refresh ()
{
    if (!Intance) {
        return false;
    }
    Intance->latestVersion.Clear ();
    Intance->latestVersionName.Clear ();
    Intance->latestVersionDownloadUrl.Clear ();
    Intance->GetVersionFromGithub ();
    return !Intance->latestVersion.IsEmpty ();
}

GS::UInt16 VersionChecker::AcMainVersion ()
{
    return Intance ? Intance->acMainVersion : 0;
}

const GS::UniString& VersionChecker::ReleasePageUrl ()
{
    static const GS::UniString url ("https://github.com/mozzi86/-archicad/releases/tag/elm-sab-tapir-latest");
    return url;
}

const GS::UniString& VersionChecker::LatestVersionDownloadUrl ()
{
    if (!Intance) {
        return GS::EmptyUniString;
    }

    return Intance->latestVersionDownloadUrl;
}

VersionChecker::VersionChecker (GS::UInt16 acMainVersionIn)
    : acMainVersion (acMainVersionIn)
{
    GetVersionFromGithub ();
}

const GS::UniString& VersionChecker::GetVersionFromGithub ()
{
    GS::UniString namePostfix = "_AC" + GS::ValueToUniString (acMainVersion);
#if defined (WINDOWS)
    namePostfix += "_Win";
#else
    namePostfix += "_Mac";
#endif

    try {
        IO::URI::URI connectionUrl ("https://api.github.com");
        HTTP::Client::ClientConnection clientConnection (connectionUrl);
        clientConnection.Connect ();

        HTTP::Client::Request request (HTTP::MessageHeader::Method::Get, "/repos/mozzi86/-archicad/releases/tags/elm-sab-tapir-latest");
        request.GetRequestHeaderFieldCollection ().Add (HTTP::MessageHeader::HeaderFieldName::UserAgent,
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36");
        clientConnection.Send (request);

        HTTP::Client::Response response;
        JSON::JDOMParser parser;
        JSON::ValueRef parsed = parser.Parse (clientConnection.BeginReceive (response));

        if (response.GetStatusCode () == HTTP::MessageHeader::StatusCode::OK) {
            JSON::ObjectValueRef outputObject = GS::DynamicCast<JSON::ObjectValue> (parsed);
            // ELMonkey 0.9.30: Version steht im Release-Namen ("ELMonkey 0.9.30 - AC27/28/29 (CI)"),
            // der rollende Tag selbst traegt keine Nummer. Alter Name ohne Nummer = leer = kein Hinweis.
            JSON::StringValueRef releaseNameValue = GS::DynamicCast<JSON::StringValue> (outputObject->Get ("name"));
            latestVersion.Clear ();
            if (releaseNameValue != nullptr) {
                const std::string releaseName = releaseNameValue->Get ().ToCStr ().Get ();
                const std::string prefix = "ELMonkey ";
                const size_t pos = releaseName.find (prefix);
                if (pos != std::string::npos) {
                    std::string ver;
                    for (size_t k = pos + prefix.size (); k < releaseName.size (); ++k) {
                        const char c = releaseName[k];
                        if ((c >= '0' && c <= '9') || c == '.') ver += c;
                        else break;
                    }
                    latestVersion = GS::UniString (ver.c_str ());
                }
            }

            JSON::ArrayValueRef arrayValue;
            std::map<GS::UniString, GS::UniString> nameDownloadUrlMap;
            outputObject->GetMandatoryMember ("assets", arrayValue);
            for (GS::UIndex i = 0; i < arrayValue->GetSize (); ++i) {
                JSON::ValueRef assetValue = arrayValue->Get (i);
                JSON::ObjectValueRef assetObject = GS::DynamicCast<JSON::ObjectValue> (assetValue);
                JSON::StringValueRef nameValue = GS::DynamicCast<JSON::StringValue> (assetObject->Get ("name"));

                GS::UniString name = nameValue->Get ();
                if (name.Contains (namePostfix)) {
                    JSON::StringValueRef downloadUrlValue = GS::DynamicCast<JSON::StringValue> (assetObject->Get ("browser_download_url"));
                    latestVersionDownloadUrl = downloadUrlValue->Get ();
                    latestVersionName = name;
                    break;
                }
            }
        }

        clientConnection.Close (false);
    } catch (...) {
    }

    return latestVersion;
}