// ELM_SAB 0.9.23 (2026-10-13, WiSchu-Tuerenabgleich): FindElements — gemeinsamer Vertrag
// Motor (FindElementsCommand.cpp) <-> Wert-/Beziehungsblaetter (FindElementsLeaves.cpp).
//
// Ein Knoten des where-Baums hat GENAU einen Kriteriums-Schluessel. Operatoren (and/or/not) und
// Beziehungsblaetter (hostedBy/hosts/inZone) tragen ihre Unterknoten in children; Blaetter tragen
// ihre beim Parsen aufgeloesten Daten in leaf (je Blatt eigene LeafData-Ableitung).
#pragma once
#include "ELMCommandBase.hpp"
#include "HashSet.hpp"
#include "HashTable.hpp"
#include <map>
#include <memory>
#include <vector>

namespace FindElementsELM {

enum class NodeKind {
    And, Or, Not,
    Type, Story, Layer, Flags, Guids, InSelection, Teamwork,                    // Task 1 (guenstig)
    Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption,      // Task 2 (Wert)
    HostedBy, Hosts, InZone                                                     // Task 2 (Beziehung, children[0] = Unterbaum)
};

struct LeafData { virtual ~LeafData () = default; };   // je Blatt abgeleitete Struktur, aufgeloest beim Parsen

struct Node {
    NodeKind                            kind = NodeKind::And;
    GS::UniString                       path;          // "where.and[2].not.property"
    GS::UniString                       criterion;     // Schluesselname: "and", "property", ...
    Int32                               cost = 0;      // 0 guenstig .. 3 teuer; Operator = max der Kinder
    UInt32                              statsIndex = 0;
    std::vector<std::unique_ptr<Node>>  children;
    std::unique_ptr<LeafData>           leaf;
};

struct NodeStats {
    GS::UniString path;
    GS::UniString criterion;
    UInt64        checked = 0;
    UInt64        matched = 0;
    UInt64        notAvailable = 0;   // Kriterium nicht anwendbar (Eigenschaft fehlt, kein Bibliotheksteil, kein Wirt ...)
    UInt64        typeMismatch = 0;   // Wert vorhanden, aber Operator passt nicht zum Typ (gdl/ifc)
    bool          evaluated = false;
};

using GuidList = std::vector<API_Guid>;

struct ValueLeafState;                 // definiert in FindElementsLeaves.cpp (Property-Index, Wert-Caches)

struct EvalContext {
    std::vector<NodeStats>                  stats;           // Index = Node::statsIndex (Pre-Order)
    GS::HashTable<API_Guid, API_Elem_Head>  headerCache;
    GS::HashSet<API_Guid>                   selectionAtStart;
    bool                                    isTeamwork = false;
    short                                   myUserId = 0;
    std::vector<GS::UniString>              warnings;
    GS::UniString                           fatalError;      // nicht leer => Auswertung bricht ab; Fehler NACH DB-Rueckschaltung melden
    GSErrCode                               fatalCode = NoError;
    std::shared_ptr<ValueLeafState>         valueState;

    // Task-1-eigene Caches fuer die Ausgabe (Geschoss- und Ebenennamen je Index).
    std::map<Int32, GS::UniString>          storyNames;
    std::map<Int32, GS::UniString>          layerNames;

    bool Header (const API_Guid& guid, API_Elem_Head& out);  // Task 1: lazy ACAPI_Element_GetHeader + Cache; false = nicht lesbar
};

// ---- Task 1 (FindElementsCommand.cpp) ----
void          EvaluateNode (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out);
bool          WildcardMatch (const GS::UniString& text, const GS::UniString& pattern);   // * und ?, ohne Gross/Klein
bool          IsGuidString (const GS::UniString& s);                                      // 8-4-4-4-12 hex
GS::UniString ToUni (const GS::String& s);                                                // GS::UniString (s.ToCStr ())
GS::UniString JoinNames (const std::vector<GS::UniString>& names, UInt32 maxCount);      // "a, b, c (+12 weitere)"

// Zusaetzliche gemeinsame Lese-Helfer (Task 1), damit beide Dateien Eingaben gleich lesen und
// Fehler gleich formulieren.
GS::UniString IntToUni (Int64 value);
GS::UniString NumToUni (double value);                                                    // %g
GS::UniString TypeNameOf (const GS::ObjectState& os, const GS::String& key);             // "Text", "Zahl", "Liste" ...
bool          ReadNumber (const GS::ObjectState& os, const GS::String& key, double& out); // Int/UInt/Real
bool          ReadNumberList (const GS::ObjectState& os, const GS::String& key, std::vector<double>& out);   // einheitliche Zahlenliste
bool          ReadStringOrList (const GS::ObjectState& os, const GS::String& key, std::vector<GS::UniString>& out);   // "a" oder ["a","b"]

// ---- Task 2 (FindElementsLeaves.cpp) ----
std::shared_ptr<ValueLeafState> CreateValueLeafState ();
bool ParseValueLeaf    (NodeKind kind, const GS::ObjectState& nodeOs, const GS::UniString& path,
                        ValueLeafState& vs, Node& node, GS::UniString& error);     // Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption
void EvaluateValueLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out);   // alle Task-2-Arten inkl. HostedBy/Hosts/InZone
bool ParseReturnList   (const GS::ObjectState& parameters, ValueLeafState& vs, GS::UniString& error);
bool HasReturnColumns  (const ValueLeafState& vs);
void AddReturnColumns  (const ValueLeafState& vs, GS::ObjectState& response);
void AddReturnValues   (EvalContext& ctx, const API_Guid& guid, GS::ObjectState& hit);

}   // namespace FindElementsELM
