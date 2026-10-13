#!/usr/bin/env bash
# Lokale Syntaxpruefung Quick 261013-jom (ELM_SAB 0.9.24) -- clang++ -fsyntax-only gegen DevKit 29.3100.
#
# Aufruf aus dem Repo-Wurzelverzeichnis:  bash <Quick-Ordner>/syntax_check_0924.sh
#
# Grenzen (bewusst):
#   - Nur der AC29-Zweig ist pruefbar. C++17 (AC27/28) NICHT: die DevKit-29-Header benutzen
#     `concept` (HashCommon.hpp), deshalb wird hier mit -std=c++20 geprueft. Die C++17-Sicherheit
#     kommt ueber den Diff-Grep auf C++20-Konstrukte (siehe Plan, Task 1 verify).
#   - Der AC27/28-Zweig `#ifndef ServerMainVers_2900` in CreateWallOpenings wird nicht kompiliert
#     (ACAPinc.h definiert das Makro).
#   - Es wird nichts geschrieben (-fsyntax-only), Archicad wird nicht angesprochen.
set -u

DK="/Users/ap/SchwarzPlatform/devkit/API Development Kit 29.3100/Support"
SRC="ELM_SAB_Tapir/Sources"

if [ ! -d "$DK/Inc" ]; then
    echo "FEHLER: DevKit nicht gefunden: $DK" >&2
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

FILES=(
    CreateWallOpeningsCommand.cpp
    SetLayerCommand.cpp
    SetObjectParametersForceCommand.cpp
    SetPenOfElementsCommand.cpp
    StoryVisibilityCommands.cpp
    SetTextSizeCommand.cpp
    AddOnMain.cpp
)

rc=0
for f in "${FILES[@]}"; do
    out=$(clang++ "${FLAGS[@]}" "${INC[@]}" -I "$SRC" "$SRC/$f" 2>&1)
    status=$?
    if [ $status -eq 0 ]; then
        echo "OK $f"
    else
        echo "FEHLER $f (Exit $status)"
        printf '%s\n' "$out" | grep -E 'error:' | head -20
        rc=1
    fi
done

if [ $rc -eq 0 ]; then
    echo "ERGEBNIS: alle 7 Dateien syntaktisch OK (AC29-Zweig, c++20)"
else
    echo "ERGEBNIS: Syntaxfehler"
fi
exit $rc
