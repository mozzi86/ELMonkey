# ELMonkey-Installer fuer Windows (Archicad 27/28/29/30).
# - laedt das passende ELMonkey-Add-On (.apx) aus dem Release github.com/mozzi86/-archicad (elm-sab-tapir-latest)
# - sichert vorhandene ELMonkey- UND Vanilla-Tapir-Add-Ons nach %USERPROFILE%\ELM_SAB_Backups\<Zeitstempel>\
#   (ELMonkey enthaelt Tapir vollstaendig; zwei Add-Ons mit demselben TapirCommand-Namensraum stoeren sich)
# Start: ELMonkey-Installer-Windows.cmd doppelklicken (holt Administratorrechte).
# Test ohne echte Installation: -AcRoot C:\pfad\zu\fake-ProgramFiles -DryRun
param([string]$AcRoot = $env:ProgramFiles, [switch]$DryRun, [string]$BackupRoot = (Join-Path $env:USERPROFILE "ELM_SAB_Backups"),
      [int]$OnlyAc = 0, [switch]$FromAddOn)
# -FromAddOn -OnlyAc 29: Aufruf aus dem Add-On (Knopf "Jetzt aktualisieren") - nur diese Version, kein Abbruch bei
# laufendem Archicad (geladenes .apx wird umbenannt/verschoben, Tausch wirkt nach Neustart), keine Rueckfrage.
$ErrorActionPreference = "Stop"
$Rel = "https://github.com/mozzi86/-archicad/releases/download/elm-sab-tapir-latest"
$Stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
$Backup = Join-Path $BackupRoot $Stamp
$Tmp = Join-Path $env:TEMP ("elmonkey_" + $Stamp)
New-Item -ItemType Directory -Force -Path $Tmp | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Fail($msg) { Write-Host ""; Write-Host "FEHLER: $msg" -ForegroundColor Red; if (-not $FromAddOn) { Read-Host "Enter zum Schliessen" }; exit 1 }

Write-Host "=== ELMonkey-Installer (Windows) ==="
if ($DryRun) { Write-Host "(Testlauf - es wird nichts geaendert)" }

$isRealTarget = ($AcRoot -eq $env:ProgramFiles)
if ($isRealTarget -and -not $DryRun) {
    $admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $admin) { Fail "Bitte ueber ELMonkey-Installer-Windows.cmd starten (braucht Administratorrechte fuer den Programme-Ordner)." }
    if ((-not $FromAddOn) -and (Get-Process -Name "Archicad*" -ErrorAction SilentlyContinue)) {
        Fail "Archicad laeuft noch. Bitte alle Projekte senden/speichern, Archicad komplett beenden und den Installer erneut starten."
    }
}

$ver = $null
try { $ver = (Invoke-WebRequest -UseBasicParsing -Uri "$Rel/elmonkey-version.txt").Content.Trim() } catch {}
if ($ver) { Write-Host "Aktuelle ELMonkey-Version im Release: $ver" } else { Write-Host "Hinweis: Versionsdatei im Release nicht gefunden - installiere den aktuellen Stand." }

$found = 0; $installed = 0
foreach ($v in 27, 28, 29, 30) {
    if ($OnlyAc -ne 0 -and $v -ne $OnlyAc) { continue }
    $dir = Join-Path $AcRoot "GRAPHISOFT\Archicad $v"
    $addons = Join-Path $dir "Add-Ons"
    if (-not (Test-Path $addons)) { continue }
    $found++
    Write-Host ""; Write-Host "--- Archicad $v ---"
    $apx = Join-Path $Tmp "ELM_SAB_Tapir_AC${v}_Win.apx"
    try { Invoke-WebRequest -UseBasicParsing -Uri "$Rel/ELM_SAB_Tapir_AC${v}_Win.apx" -OutFile $apx }
    catch { Write-Host "AC${v}: Download fehlgeschlagen - uebersprungen."; continue }
    if ((Get-Item $apx).Length -lt 100000) { Write-Host "AC${v}: Download unvollstaendig - uebersprungen."; continue }

    $old = Get-ChildItem -Path $addons -Recurse -Depth 2 -File -Include "ELM_SAB*.apx", "Tapir*.apx" -ErrorAction SilentlyContinue
    foreach ($o in $old) {
        Write-Host "Sichere: $($o.FullName)"
        if (-not $DryRun) {
            $dest = Join-Path $Backup "AC$v"
            New-Item -ItemType Directory -Force -Path $dest | Out-Null
            Move-Item -Force -Path $o.FullName -Destination $dest
        }
    }
    $target = Join-Path $addons "ELM_SAB_AC${v}_Win.apx"
    Write-Host "Installiere: $target"
    if (-not $DryRun) {
        Copy-Item -Force -Path $apx -Destination $target
        Unblock-File -Path $target
        if (-not (Test-Path $target)) { Fail "Installation nach $addons nicht nachweisbar." }
    }
    $installed++
}

Remove-Item -Recurse -Force $Tmp -ErrorAction SilentlyContinue
Write-Host ""
if ($found -eq 0) { Fail "Kein Archicad 27/28/29/30 unter $AcRoot\GRAPHISOFT gefunden." }
Write-Host "Fertig: ELMonkey $ver fuer $installed von $found Archicad-Version(en) installiert."
if ((-not $DryRun) -and (Test-Path $Backup)) { Write-Host "Alte Add-Ons gesichert in: $Backup" }
Write-Host "Archicad starten. Kontrolle: Menue ELMonkey SAB > Ueber (zeigt ELM_SAB-Version, Knopf 'Nach Updates suchen')."
if (-not $FromAddOn) { Read-Host "Enter zum Schliessen" }
exit 0
