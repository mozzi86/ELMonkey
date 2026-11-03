#pragma once

#include "Definitions.hpp"
#include "UniString.hpp"
#include "Thread.hpp"

#include <atomic>
#include <functional>
#include <memory>

// Looks up the latest Tapir release on GitHub.
// ELMonkey 0.10.0: Tapirs Hintergrund-Pruefung (1.7.1) uebernommen, aber gegen das ELMonkey-Release
// (mozzi86/-archicad, Tag elm-sab-tapir-latest, Version im Release-Namen "ELMonkey X.Y.Z") und gegen
// ELM_SAB_VERSION statt ADDON_VERSION. Dazu die ELMonkey-Zugriffe fuer den Update-Knopf im Ueber-Dialog.
// The lookup runs on its own thread (StartCheck) with a bounded timeout, so Archicad never waits for GitHub
// while it starts or opens a project (#798). Until the lookup completes, or when it fails, the accessors
// report the current version as the latest one.
class VersionChecker {
public:
    static void CreateInstance (GS::UInt16 acMainVersion);
    // Starts the lookup on a background thread. onCompleted runs on Archicad's message loop (main thread)
    // once the result is available. Does nothing when the lookup was already started.
    static void StartCheck (const std::function<void ()>& onCompleted);

    static bool IsCheckCompleted ();
    static bool IsUsingLatestVersion ();
    static bool IsNewerVersionWithoutAddOn ();
    static const GS::UniString& LatestVersion ();
    static const GS::UniString& LatestVersionName ();
    static const GS::UniString& LatestInstallerDownloadUrl ();
    static GS::UInt16           ArchicadMainVersion ();

    // ELMonkey: Download-Adresse des Bundles dieser Archicad-Version, synchrone Neupruefung fuer den
    // Knopf "Nach Updates suchen" (der Nutzer wartet dort bewusst), Archicad-Version und Release-Seite.
    static const GS::UniString& LatestVersionDownloadUrl ();
    static bool                 Refresh ();
    static GS::UInt16           AcMainVersion ();
    static const GS::UniString& ReleasePageUrl ();

private:
    // Written by the checker thread before completed is set, never changed afterwards: so once completed is
    // true (acquire), the strings may be read from the main thread without a lock.
    struct Result {
        std::atomic<bool> completed { false };
        GS::UniString     latestVersion;
        GS::UniString     latestVersionName;
        GS::UniString     latestVersionDownloadUrl;
        GS::UniString     latestInstallerDownloadUrl;
    };
    class CheckTask;

    VersionChecker (GS::UInt16 acMainVersionIn);

    static void          GetVersionFromGithub (GS::UInt16 acMainVersion, Result& result);
    static const Result* GetCompletedResult ();

private:
    GS::UInt16              acMainVersion;
    std::shared_ptr<Result> result;   // shared with the CheckTask, so the thread never writes into a destroyed object
    bool                    checkStarted = false;
    GS::Thread              checkerThread;
};
