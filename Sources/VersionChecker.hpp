#pragma once

#include "Definitions.hpp"
#include "UniString.hpp"

int CompareDottedVersions (const GS::UniString& a, const GS::UniString& b);

class VersionChecker {
public:
    static void CreateInstance (GS::UInt16 acMainVersion);

    static bool IsUsingLatestVersion ();
    static const GS::UniString& LatestVersion ();
    static const GS::UniString& LatestVersionName ();
    static const GS::UniString& LatestVersionDownloadUrl ();
    // ELMonkey 0.9.30: frisch nachfragen (Knopf im Ueber-Dialog) und Release-Seite fuer Installer.
    static bool Refresh ();
    static GS::UInt16 AcMainVersion ();
    static const GS::UniString& ReleasePageUrl ();

private:
    VersionChecker (GS::UInt16 acMainVersionIn);

    const GS::UniString& GetVersionFromGithub ();

private:
    GS::UInt16    acMainVersion;
    GS::UniString latestVersion;
    GS::UniString latestVersionName;
    GS::UniString latestVersionDownloadUrl;
};