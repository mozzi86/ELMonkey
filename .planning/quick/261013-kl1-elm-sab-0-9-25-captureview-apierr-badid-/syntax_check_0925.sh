#!/usr/bin/env bash
# Lokale Pruefung Quick 261013-kl1 (ELM_SAB 0.9.25, CaptureView Farbtiefe) --
# clang++ -fsyntax-only gegen DevKit 29.3100 plus Struktur-/Schema-/Diff-Pruefungen.
#
# Aufruf aus dem Repo-Wurzelverzeichnis:
#   bash .planning/quick/261013-kl1-elm-sab-0-9-25-captureview-apierr-badid-/syntax_check_0925.sh [basis-rev]
# Default-Basis: a79a36e
#
# Grenzen (bewusst):
#   - Nur der Mac/AC29-Zweig (-Dmacintosh=1) ist kompilierbar. Der Windows-Zweig (TC24) und C++17
#     (AC27/28) werden NUR ueber Diff-Grep und die CI geprueft. Die DevKit-29-Header benutzen
#     `concept`, deshalb wird hier mit -std=c++20 geprueft (siehe syntax_check_0924.sh).
#   - Es wird nichts geschrieben (-fsyntax-only, nur Lesezugriffe auf git), Archicad wird nicht
#     angesprochen.
#
# Stufen: A Syntax, B Plattform-Kontrolle, C Struktur, D Schema, E Diff gegen Basis.
# Jede Stufe meldet "OK ..." bzw. "FEHLER ...". Exit 1 bei mindestens einem Fehler.
set -u

BASIS="${1:-a79a36e}"
DK="/Users/ap/SchwarzPlatform/devkit/API Development Kit 29.3100/Support"
SRC="ELM_SAB_Tapir/Sources"
HPP="$SRC/CaptureViewCommand.hpp"
MAIN="$SRC/AddOnMain.cpp"
VERS="$SRC/AddOnVersion.hpp"

if [ ! -d "$DK/Inc" ]; then
    echo "FEHLER: DevKit nicht gefunden: $DK" >&2
    exit 1
fi
if [ ! -f "$HPP" ]; then
    echo "FEHLER: bitte aus der Repo-Wurzel aufrufen ($HPP nicht gefunden)" >&2
    exit 1
fi

INC=(-isystem "$DK/Inc")
for d in "$DK"/Modules/*/; do
    INC+=(-isystem "$d")
done

# Mac-Flags aus ELM_SAB_Tapir/Tools/CMakeCommon.cmake (Z. 34-53), -fvisibility entfaellt (nur Syntax).
FLAGS=(-std=c++20 -fsyntax-only -Dmacintosh=1 -DACExtension -Wall -Wextra -Werror
    -Wno-multichar -Wno-ctor-dtor-privacy -Wno-invalid-offsetof -Wno-ignored-qualifiers
    -Wno-reorder -Wno-overloaded-virtual -Wno-unused-parameter -Wno-unused-value
    -Wno-unused-private-field -Wno-deprecated -Wno-unknown-pragmas -Wno-missing-braces
    -Wno-missing-field-initializers -Wno-non-c-typedef-for-linkage
    -Wno-uninitialized-const-reference -Wno-shorten-64-to-32 -Wno-sign-compare -Wno-switch)

rc=0
ok()   { echo "OK $*"; }
fail() { echo "FEHLER $*"; rc=1; }

# Zeilen ohne reine Kommentarzeilen (fuer die Struktur-Zaehlungen)
code_lines() { grep -vE '^[[:space:]]*//' "$HPP"; }

echo "== Basis: $BASIS =="

# ---------------------------------------------------------------- A Syntax AC29/Mac
echo "== A Syntax AC29/Mac (AddOnMain.cpp bindet CaptureViewCommand.hpp ein) =="
out=$(clang++ "${FLAGS[@]}" "${INC[@]}" -I "$SRC" "$MAIN" 2>&1)
status=$?
if [ $status -eq 0 ]; then
    ok "A AddOnMain.cpp -fsyntax-only -Werror"
else
    fail "A AddOnMain.cpp (Exit $status)"
    printf '%s\n' "$out" | grep -E 'error:' | head -20
fi

# ---------------------------------------------------------------- B Plattform-Kontrolle
echo "== B Plattform-Kontrolle (MiC nur Mac, TC24 nur Windows) =="
probe() {
    printf '#include "APIEnvir.h"\n#include "ACAPinc.h"\nint probe_value () { return (int) %s; }\n' "$1" \
        | clang++ "${FLAGS[@]}" "${INC[@]}" -I "$SRC" -x c++ - >/dev/null 2>&1
}
if probe APIColorDepth_MiC; then
    ok "B APIColorDepth_MiC kompiliert unter -Dmacintosh=1"
else
    fail "B APIColorDepth_MiC kompiliert NICHT unter -Dmacintosh=1"
fi
if probe APIColorDepth_TC24; then
    fail "B APIColorDepth_TC24 kompilierte unter -Dmacintosh=1 (der Plattform-#if waere nicht noetig)"
else
    ok "B APIColorDepth_TC24 scheitert unter -Dmacintosh=1 (Beleg fuer den Pflicht-#if)"
fi
if probe APIColorDepth_256C; then
    ok "B APIColorDepth_256C kompiliert (plattformunabhaengig)"
else
    fail "B APIColorDepth_256C kompiliert nicht"
fi
HDR="$DK/Inc/APIdefs_Elements.h"
zones=$(sed -n '/APIColorDepth_FromSourceImage = -1/,/} API_ColorDepthID;/p' "$HDR" | awk '
    /^#ifdef macintosh/ { z = "macintosh"; next }
    /^#ifdef WINDOWS/   { z = "WINDOWS"; next }
    /^#endif/           { z = "none"; next }
    /APIColorDepth_MiC,/   { printf "MiC=%s ", z }
    /APIColorDepth_TC24,/  { printf "TC24=%s ", z }
    /APIColorDepth_256C,/  { printf "256C=%s ", z }
    BEGIN { z = "none" }
')
if [ "$zones" = "256C=none MiC=macintosh TC24=WINDOWS " ] || [ "$zones" = "256C=none MiC=macintosh TC24=WINDOWS" ]; then
    ok "B DevKit-Header: $zones"
else
    fail "B DevKit-Header: unerwartete Zonen: '$zones'"
fi

# ---------------------------------------------------------------- C Struktur
echo "== C Struktur CaptureViewCommand.hpp (nur Nicht-Kommentarzeilen) =="
check_count() { # name pattern erwartet-operator erwartet-wert
    local name="$1" pat="$2" op="$3" want="$4" n
    n=$(code_lines | grep -cF -- "$pat")
    if [ "$n" "$op" "$want" ]; then
        ok "C $name: $n ($op $want)"
    else
        fail "C $name: $n, erwartet $op $want"
    fi
}
check_count "APIColorDepth_FromSourceImage" "APIColorDepth_FromSourceImage" -eq 0
check_count "keepSelectionHighlight" "keepSelectionHighlight" -eq 0
check_count "ACAPI_ProjectOperation_Save" "ACAPI_ProjectOperation_Save" -eq 1
check_count "TrySave (" "TrySave (" -eq 3
check_count "APIERR_BADID" "APIERR_BADID" -ge 1
check_count 'response.Add ("colorDepth"' 'response.Add ("colorDepth"' -eq 1
check_count "pict.colorDepth = depth" "pict.colorDepth = depth" -eq 1

pos=$(code_lines | awk '
    /#if defined \(macintosh\)/        { z = "mac" }
    /#elif defined \(WINDOWS\)/        { z = "win" }
    /^[[:space:]]*#else/               { z = "else" }
    /^[[:space:]]*#endif/              { z = "none" }
    /APIColorDepth_MiC/                { mic = mic z ";" }
    /APIColorDepth_TC24/               { tc = tc z ";" }
    /APIColorDepth_256C/               { c256 = c256 z ";" }
    BEGIN { z = "none" }
    END { printf "MiC=%s TC24=%s 256C=%s", mic, tc, c256 }
')
case "$pos" in
    "MiC=mac; TC24=win; 256C="*)
        ok "C Plattformzweige: $pos" ;;
    *)
        fail "C Plattformzweige unerwartet: $pos (MiC nur im macintosh-, TC24 nur im WINDOWS-Zweig)" ;;
esac
# 256C muss im #else-Zweig stehen (Primaer) und als Rueckfall ausserhalb der #if-Kette
case "$pos" in
    *"256C="*"else;"*) ok "C 256C steht im #else-Zweig" ;;
    *) fail "C 256C fehlt im #else-Zweig: $pos" ;;
esac

# ---------------------------------------------------------------- D Schema
echo "== D Schema (Antwortschema, oneOf-Huelle, Eingabeschema byte-gleich) =="
python3 -I - "$BASIS" "$HPP" <<'PY'
import json, re, subprocess, sys

basis, hpp = sys.argv[1], sys.argv[2]
errors = []

def ok(msg):
    print("OK D " + msg)

def fail(msg):
    print("FEHLER D " + msg)
    errors.append(msg)

cur = open(hpp, "rb").read()
base = subprocess.run(["git", "show", "%s:%s" % (basis, hpp)], capture_output=True).stdout

def raw_string(data, anchor):
    m = re.search(anchor + rb' \(\) const override.*?R"\((.*?)\)"', data, re.S)
    return (m.group(1), data[m.end():m.end() + 6]) if m else (None, None)

resp, tail = raw_string(cur, rb'GetResponseSchema')
if resp is None:
    fail("Antwortschema (Raw-String nach GetResponseSchema) nicht gefunden")
else:
    if not tail.startswith(b"));"):
        fail("Raw-String endet nicht vor '));' (Falle: ')\"' im Schematext?): %r" % tail)
    else:
        ok("Raw-String des Antwortschemas endet sauber vor '));'")
    try:
        schema = json.loads(resp.decode("utf-8"))
    except Exception as e:
        schema = None
        fail("Antwortschema ist kein gueltiges JSON: %s" % e)
    if schema is not None:
        props = schema.get("properties", {})
        if set(props.keys()) == {"imagePath", "view2D", "colorDepth"}:
            ok("properties = imagePath, view2D, colorDepth")
        else:
            fail("properties = %s" % sorted(props.keys()))
        if props.get("colorDepth", {}).get("type") == "string":
            ok("colorDepth.type = string")
        else:
            fail("colorDepth.type != string")
        if schema.get("required") == ["imagePath", "view2D"]:
            ok("required = [imagePath, view2D]")
        else:
            fail("required = %s" % schema.get("required"))
        if schema.get("additionalProperties") is False:
            ok("additionalProperties = false")
        else:
            fail("additionalProperties nicht false")
        try:
            resp.decode("ascii")
            ok("Antwortschema ist ASCII")
        except UnicodeDecodeError:
            fail("Antwortschema enthaelt Nicht-ASCII")
        # identisch zu ELMResponseSchemaWithError (ELMCommandBase.hpp) zusammensetzen
        wrapped = ('{"oneOf": [' + resp.decode("utf-8") +
                   ', {"type": "object", "properties": {"error": {"type": "object", "properties": '
                   '{"code": {"type": "integer"}, "message": {"type": "string"}}, '
                   '"additionalProperties": false, "required": ["code", "message"]}}, '
                   '"additionalProperties": false, "required": ["error"]}]}')
        try:
            w = json.loads(wrapped)
            if len(w.get("oneOf", [])) == 2:
                ok("oneOf-Huelle ist gueltiges JSON mit 2 Zweigen")
            else:
                fail("oneOf hat nicht 2 Zweige")
        except Exception as e:
            fail("oneOf-Huelle ist kein gueltiges JSON: %s" % e)

inp_cur, _ = raw_string(cur, rb'GetInputParametersSchema')
inp_base, _ = raw_string(base, rb'GetInputParametersSchema')
if inp_cur is None or inp_base is None:
    fail("Eingabeschema nicht gefunden (aktuell: %s, Basis: %s)" % (inp_cur is not None, inp_base is not None))
elif inp_cur == inp_base:
    ok("Eingabeschema byte-gleich zur Basis (%d Bytes)" % len(inp_cur))
else:
    fail("Eingabeschema weicht von der Basis ab")

sys.exit(1 if errors else 0)
PY
[ $? -eq 0 ] || rc=1

# ---------------------------------------------------------------- E Diff gegen Basis
echo "== E Diff gegen Basis $BASIS (Arbeitsbaum, $SRC) =="
names=$(git diff --name-only "$BASIS" -- "$SRC" | sort | tr '\n' ' ')
want=$(printf '%s\n' "$MAIN" "$HPP" "$VERS" | sort | tr '\n' ' ')
if [ "$names" = "$want" ]; then
    ok "E geaenderte Dateien: $names"
else
    fail "E geaenderte Dateien: '$names', erwartet '$want'"
fi

added=$(git diff -U0 "$BASIS" -- "$SRC" | grep '^+' | grep -v '^+++')
nadded=$(printf '%s\n' "$added" | grep -c .)
echo "   hinzugefuegte Zeilen: $nadded"

banned=$(printf '%s\n' "$added" | grep -nE '<=>|std::format|co_await|co_return|concept |consteval|char8_t|using enum|std::ranges|\{ *\.[A-Za-z_]+ *=' || true)
if [ -z "$banned" ]; then
    ok "E keine C++20-Konstrukte in hinzugefuegten Zeilen"
else
    fail "E C++20-Konstrukt in hinzugefuegten Zeilen:"
    printf '%s\n' "$banned"
fi

badne=""
while IFS= read -r tok; do
    [ -z "$tok" ] && continue
    if ! printf '%s' "$tok" | grep -qE '^!= *(NoError|APIERR_[A-Z]+|APIColorDepth_[A-Za-z0-9]+)$'; then
        badne="$badne [$tok]"
    fi
done < <(printf '%s\n' "$added" | grep -oE '!= *[A-Za-z_0-9:]*')
if [ -z "$badne" ]; then
    ok "E '!=' nur gegen NoError/APIERR_*/APIColorDepth_*"
else
    fail "E '!=' gegen anderes (GS::UniString?):$badne"
fi

# Ausnahme: Die EINE geaenderte Registrierungszeile in AddOnMain.cpp traegt den schon vorhandenen
# Gedankenstrich ihres Altbestands (Plan verlangt: neue Zeile = alte Zeile + Satz). Der angehaengte
# Satz selbst wird unten per python auf ASCII geprueft.
nonascii=$(printf '%s\n' "$added" | grep -vF 'ELM_SAB: Speichert das aktive Fenster (2D oder 3D)' | LC_ALL=C grep -n $'[^ -~\t]' || true)
if [ -z "$nonascii" ]; then
    ok "E alle hinzugefuegten Zeilen sind ASCII (Ausnahme: Altbestand-Gedankenstrich der Registrierungszeile)"
else
    fail "E Nicht-ASCII in hinzugefuegten Zeilen:"
    printf '%s\n' "$nonascii" | head -10
fi

for f in "$MAIN" "$VERS"; do
    ns=$(git diff --numstat "$BASIS" -- "$f" | awk '{print $1 " " $2}')
    if [ "$ns" = "1 1" ]; then
        ok "E $f: genau 1 Zeile geaendert"
    else
        fail "E $f: numstat '$ns', erwartet '1 1'"
    fi
done

if grep -qF '#define ELM_SAB_VERSION "0.9.25"' "$VERS" && grep -qF '#define ADDON_VERSION "1.5.4"' "$VERS"; then
    ok 'E ELM_SAB_VERSION "0.9.25", ADDON_VERSION "1.5.4"'
else
    fail 'E Versionsdefines stimmen nicht (erwartet 0.9.25 / 1.5.4)'
fi
if grep -A1 'RegisterCommand<CaptureViewCommand>' "$MAIN" | grep -qF '"0.9.12"'; then
    ok 'E CaptureView-Registrierung bleibt "0.9.12"'
else
    fail 'E CaptureView-Registrierung nicht mehr "0.9.12"'
fi

python3 -I - "$BASIS" "$MAIN" <<'PY'
import subprocess, sys
basis, main = sys.argv[1], sys.argv[2]
SENT = (" 0.9.25: PNG-Export mit Echtfarbtiefe (Mac MiC, Win TC24) statt FromSourceImage gegen APIERR_BADID, "
        "bei BADID ein Rueckfall auf 256C; die Antwort meldet die benutzte Tiefe in colorDepth.")
old = subprocess.run(["git", "show", "%s:%s" % (basis, main)], capture_output=True, text=True).stdout.split("\n")
new = open(main, encoding="utf-8").read().split("\n")
cands = [l for l in old if l.strip().startswith('"ELM_SAB: Speichert das aktive Fenster')]
if len(cands) != 1:
    print("FEHLER E alte CaptureView-Beschreibungszeile nicht eindeutig gefunden (%d)" % len(cands))
    sys.exit(1)
if not SENT.isascii():
    print("FEHLER E der angehaengte Registrierungssatz ist nicht ASCII")
    sys.exit(1)
o = cands[0].rstrip()
i = o.rindex('"')
expected = o[:i] + SENT + o[i:]
if new.count(expected) == 1:
    print("OK E AddOnMain.cpp: neue Zeile = alte Zeile + 0.9.25-Satz vor dem letzten Anfuehrungszeichen")
else:
    print("FEHLER E AddOnMain.cpp: erwartete Registrierungszeile nicht gefunden")
    sys.exit(1)
PY
[ $? -eq 0 ] || rc=1

echo
if [ $rc -eq 0 ]; then
    echo "ERGEBNIS: alle Stufen OK (A Syntax AC29/Mac c++20, B Plattform, C Struktur, D Schema, E Diff)"
else
    echo "ERGEBNIS: mindestens eine Stufe mit FEHLER"
fi
exit $rc
