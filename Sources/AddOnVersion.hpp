#pragma once

// ADDON_VERSION ist die Version des ZUGRUNDELIEGENDEN Tapir-Stands — sie wandert nur
// mit, wenn Upstream-Tapir nachgezogen wird. TapirCommand.GetAddOnVersion meldet sie.
// ELMonkey 0.10.0: Tapir 1.7.2 eingemischt (Drei-Wege-Merge, Basis Upstream 18734069).
#define ADDON_VERSION "1.7.2"

// ELM_SAB_VERSION ist die Version UNSERES Bundles. Sie muss bei jeder Änderung an den
// ELM_SAB-Befehlen hoch — sonst ist am laufenden Archicad nicht feststellbar, welcher
// Build geladen ist, und ein veraltetes Bundle sieht aus wie ein frisches (genau die
// Update-Stolperfalle aus reference/mcp-extension.md).
#define ELM_SAB_VERSION "0.10.1"

// Identifiziert den konkreten Build — die CI baut aus einem rollenden Tag, der Tag allein
// ist also kein Beleg dafür, welcher Stand im Bundle steckt.
#define ELM_SAB_BUILD_STAMP __DATE__ " " __TIME__
