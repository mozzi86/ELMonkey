#include "AboutDialog.hpp"

// ELMonkey 0.10.0: Tapir-1.7-MigrationHelper braucht die API-Header VOR sich.
#include "APIEnvir.h"
#include "ACAPinc.h"
#include "AddOnVersion.hpp"
#include "ResourceIds.hpp"
#include "MigrationHelper.hpp"
#include "VersionChecker.hpp"

#include <cstdlib>

AboutDialog::AboutDialog () :
    DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ABOUT_DIALOG, ACAPI_GetOwnResModule ()),
    okButton (GetReference (), 1),
    updateButton (GetReference (), 6),
    versionText (GetReference (), 4)
{
    AttachToAllItems (*this);
    Attach (*this);

    GS::UShort portNumber = 0;
    ACAPI_Command_GetHttpConnectionPort (&portNumber);

    GS::UniString versionTextContent = versionText.GetText ();
    GS::UniString versionTextNewContent = GS::UniString::SPrintf (
        versionTextContent,
        GS::UniString (ELM_SAB_VERSION).ToPrintf (),
        GS::UniString (ADDON_VERSION).ToPrintf (),
        GS::ValueToUniString (portNumber).ToPrintf ()
    );
    versionText.SetText (versionTextNewContent);
}

void AboutDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
    if (ev.GetSource () == &okButton) {
        PostCloseRequest (DG::ModalDialog::Accept);
    } else if (ev.GetSource () == &updateButton) {
        CheckForUpdate ();
    }
}

// ELMonkey 0.9.30: Update-Pruefung gegen das eigene Release (github.com/mozzi86/-archicad,
// Tag elm-sab-tapir-latest). Installiert NICHTS selbst - ein Add-On kann sich im laufenden
// Archicad nicht ersetzen; installiert wird per Installer bei beendetem Archicad.
void AboutDialog::CheckForUpdate ()
{
    const GS::UniString installed (ELM_SAB_VERSION);
    if (!VersionChecker::Refresh ()) {
        DGAlert (DG_WARNING, GS::UniString ("Nach Updates suchen"),
            GS::UniString ("Die Update-Pruefung hat keine Version geliefert."),
            GS::UniString ("Keine Internetverbindung zu github.com, oder das Release nennt noch keine ELMonkey-Version. Installiert: ") + installed,
            GS::UniString ("OK"));
        return;
    }
    const GS::UniString latest = VersionChecker::LatestVersion ();
    if (VersionChecker::IsUsingLatestVersion ()) {
        DGAlert (DG_INFORMATION, GS::UniString ("Nach Updates suchen"),
            GS::UniString ("ELMonkey ist aktuell."),
            GS::UniString ("Installiert: ") + installed + GS::UniString ("  -  Neueste: ") + latest,
            GS::UniString ("OK"));
        return;
    }
    const short answer = DGAlert (DG_INFORMATION, GS::UniString ("Nach Updates suchen"),
        GS::UniString ("ELMonkey ") + latest + GS::UniString (" ist verfuegbar (installiert: ") + installed + GS::UniString (")."),
        GS::UniString ("'Jetzt aktualisieren' tauscht das Add-On aus (altes wird in ELM_SAB_Backups gesichert). Wirksam nach dem naechsten Archicad-Neustart."),
        GS::UniString ("Jetzt aktualisieren"), GS::UniString ("Download-Seite"), GS::UniString ("Spaeter"));
    if (answer == DG_OK) {
        RunSelfUpdate (latest);
    } else if (answer == DG_CANCEL) {
        const GS::UniString command = GS::UniString::Printf (
#ifdef WINDOWS
            "start \"\" \"%T\"",
#else
            "open \"%T\"",
#endif
            VersionChecker::ReleasePageUrl ().ToPrintf ());
        std::system (command.ToCStr ().Get ());
    }
}

// Startet den ELMonkey-Installer aus dem Release im Add-On-Modus: nur die laufende Archicad-
// Version, kein Abbruch bei laufendem Archicad, altes Bundle wird gesichert (Mac: verschoben,
// Windows: geladenes .apx wird verschoben - beides erlaubt das Betriebssystem). Wirkt nach Neustart.
void AboutDialog::RunSelfUpdate (const GS::UniString& latest)
{
    const GS::UniString ac = GS::ValueToUniString (VersionChecker::AcMainVersion ());
    const GS::UniString base ("https://github.com/mozzi86/-archicad/releases/download/elm-sab-tapir-latest/");
#ifdef WINDOWS
    const GS::UniString command = GS::UniString ("powershell -NoProfile -ExecutionPolicy Bypass -Command \"")
        + GS::UniString ("$p = Join-Path $env:TEMP 'ELMonkey-Installer-Windows.ps1'; ")
        + GS::UniString ("[Net.ServicePointManager]::SecurityProtocol = 'Tls12'; ")
        + GS::UniString ("Invoke-WebRequest -UseBasicParsing -Uri '") + base + GS::UniString ("ELMonkey-Installer-Windows.ps1' -OutFile $p; ")
        + GS::UniString ("$r = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File \\\"' + $p + '\\\" -FromAddOn -OnlyAc ") + ac + GS::UniString ("'); exit $r.ExitCode\"");
    const GS::UniString logHint ("%USERPROFILE%\\ELM_SAB_Backups");
#else
    const GS::UniString command = GS::UniString ("/bin/mkdir -p \"$HOME/ELM_SAB_Backups\" && /usr/bin/curl -fsSL '")
        + base + GS::UniString ("ELMonkey-Installer-Mac.command' | ELM_FROM_ADDON=1 ELM_ONLY_AC=") + ac
        + GS::UniString (" /bin/bash > \"$HOME/ELM_SAB_Backups/letztes_update.log\" 2>&1");
    const GS::UniString logHint ("~/ELM_SAB_Backups/letztes_update.log");
#endif
    const int rc = std::system (command.ToCStr ().Get ());
    if (rc == 0) {
        DGAlert (DG_INFORMATION, GS::UniString ("ELMonkey aktualisiert"),
            GS::UniString ("ELMonkey ") + latest + GS::UniString (" ist installiert."),
            GS::UniString ("Bitte Archicad beenden und neu starten - erst dann laeuft die neue Version. Protokoll: ") + logHint,
            GS::UniString ("OK"));
    } else {
        DGAlert (DG_WARNING, GS::UniString ("ELMonkey-Update fehlgeschlagen"),
            GS::UniString ("Das Update konnte nicht installiert werden (Code ") + GS::ValueToUniString (rc) + GS::UniString (")."),
            GS::UniString ("Nichts wurde veraendert oder das alte Add-On liegt in ELM_SAB_Backups. Bitte Archicad beenden und den Installer von der Download-Seite starten. Protokoll: ") + logHint,
            GS::UniString ("OK"));
    }
}
