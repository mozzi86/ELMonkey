#!/bin/bash
# ELMonkey-Installer für macOS (Archicad 27/28/29).
# - lädt das passende ELMonkey-Bundle aus dem Release github.com/mozzi86/-archicad (elm-sab-tapir-latest)
# - sichert vorhandene ELMonkey- UND Vanilla-Tapir-Bundles nach ~/ELM_SAB_Backups/<Zeitstempel>/
#   (ELMonkey enthält Tapir vollständig; zwei Add-Ons mit demselben TapirCommand-Namensraum stören sich)
# - entfernt die macOS-Quarantäne
# Aufruf: Doppelklick, oder im Terminal:
#   curl -fsSL https://raw.githubusercontent.com/mozzi86/-archicad/main/installer/ELMonkey-Installer-Mac.command | bash
# Test ohne echte Installation: ELM_AC_ROOT=/pfad/zu/fake-Applications ELM_DRY_RUN=1 bash ELMonkey-Installer-Mac.command
# Aus dem Add-On (Knopf „Jetzt aktualisieren"): ELM_FROM_ADDON=1 ELM_ONLY_AC=29 — nur diese Version, kein
# Abbruch bei laufendem Archicad (Tausch wirkt nach Neustart), kein sudo, keine Rückfrage.
set -u
REL="https://github.com/mozzi86/-archicad/releases/download/elm-sab-tapir-latest"
AC_ROOT="${ELM_AC_ROOT:-/Applications}"
DRY="${ELM_DRY_RUN:-0}"
FROM_ADDON="${ELM_FROM_ADDON:-0}"
ONLY_AC="${ELM_ONLY_AC:-}"
STAMP=$(date +%Y-%m-%d_%H%M%S)
BACKUP="${ELM_BACKUP_ROOT:-$HOME/ELM_SAB_Backups}/$STAMP"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

say () { printf '%s\n' "$*"; }
fail () { say ""; say "FEHLER: $*"; say ""; [ -t 0 ] && read -r -p "Enter zum Schließen …" _; exit 1; }

say "=== ELMonkey-Installer (macOS) ==="
[ "$DRY" = "1" ] && say "(Testlauf – es wird nichts geändert)"

if [ "$DRY" != "1" ] && [ "$FROM_ADDON" != "1" ] && [ -z "${ELM_AC_ROOT:-}" ] && pgrep -x Archicad >/dev/null 2>&1; then
  fail "Archicad läuft noch. Bitte alle Projekte senden/speichern, Archicad mit ⌘Q komplett beenden und den Installer erneut starten."
fi

VER=$(curl -fsSL "$REL/elmonkey-version.txt" 2>/dev/null | tr -d '[:space:]')
[ -n "$VER" ] && say "Aktuelle ELMonkey-Version im Release: $VER" || say "Hinweis: Versionsdatei im Release nicht gefunden (ältere CI) – installiere trotzdem den aktuellen Stand."

found=0; installed=0
for dir in "$AC_ROOT"/[Gg][Rr][Aa][Pp][Hh][Ii][Ss][Oo][Ff][Tt]/Archicad\ 2[789]; do
  [ -d "$dir" ] || continue
  v=$(basename "$dir" | sed 's/[^0-9]//g')
  [ -n "$ONLY_AC" ] && [ "$v" != "$ONLY_AC" ] && continue
  addons="$dir/Add-Ons"
  [ -d "$addons" ] || { say "AC$v: kein Add-Ons-Ordner, übersprungen."; continue; }
  found=$((found+1))
  say ""; say "--- Archicad $v ---"

  zip="$TMP/AC$v.zip"
  if ! curl -fsSL -o "$zip" "$REL/ELM_SAB_Tapir_AC${v}_Mac.zip"; then
    say "AC$v: Download fehlgeschlagen ($REL/ELM_SAB_Tapir_AC${v}_Mac.zip) – übersprungen."; continue
  fi
  mkdir -p "$TMP/AC$v" && ditto -x -k "$zip" "$TMP/AC$v" || { say "AC$v: Entpacken fehlgeschlagen – übersprungen."; continue; }
  new=$(find "$TMP/AC$v" -maxdepth 2 -name "ELM_SAB_AC${v}_Mac.bundle" -type d | head -1)
  [ -n "$new" ] || { say "AC$v: Im Download fehlt ELM_SAB_AC${v}_Mac.bundle – übersprungen."; continue; }

  # Vorhandene ELMonkey- und Tapir-Bundles einsammeln (auch in Unterordnern, max. 2 Ebenen)
  old=()
  while IFS= read -r b; do old+=("$b"); done < <(find "$addons" -maxdepth 3 -type d \( -iname "ELM_SAB*.bundle" -o -iname "Tapir*.bundle" \) -prune 2>/dev/null)

  sudo_cmd=""
  if [ ! -w "$addons" ]; then
    [ "$FROM_ADDON" = "1" ] && fail "Keine Schreibrechte für $addons – bitte den Installer von Hand starten (fragt nach dem Admin-Passwort)."
    say "Für $addons sind Administratorrechte nötig."
    sudo_cmd="sudo"
  fi

  for b in "${old[@]+"${old[@]}"}"; do
    say "Sichere: $b"
    if [ "$DRY" != "1" ]; then
      mkdir -p "$BACKUP/AC$v" && $sudo_cmd mv "$b" "$BACKUP/AC$v/" || fail "Konnte $b nicht sichern."
    fi
  done
  say "Installiere: $addons/ELM_SAB_AC${v}_Mac.bundle"
  if [ "$DRY" != "1" ]; then
    $sudo_cmd ditto "$new" "$addons/ELM_SAB_AC${v}_Mac.bundle" || fail "Kopieren nach $addons fehlgeschlagen."
    $sudo_cmd xattr -dr com.apple.quarantine "$addons/ELM_SAB_AC${v}_Mac.bundle" 2>/dev/null || true
    [ -d "$addons/ELM_SAB_AC${v}_Mac.bundle/Contents/MacOS" ] || fail "Installation in $addons nicht nachweisbar."
  fi
  installed=$((installed+1))
done

say ""
[ "$found" -gt 0 ] || fail "Kein Archicad 27/28/29 unter $AC_ROOT/GRAPHISOFT gefunden."
say "Fertig: ELMonkey ${VER:-?} für $installed von $found Archicad-Version(en) installiert."
[ "$DRY" != "1" ] && [ -d "$BACKUP" ] && say "Alte Bundles gesichert in: $BACKUP"
say "Archicad starten. Kontrolle im Menü: ELMonkey SAB > Über (zeigt ELM_SAB-Version)."
[ -t 0 ] && read -r -p "Enter zum Schließen …" _
exit 0
