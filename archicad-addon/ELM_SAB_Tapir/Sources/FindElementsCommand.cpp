// ELM_SAB 0.9.23 (2026-10-13, WiSchu-Tuerenabgleich) — FindElements: Motor, Suchbereich,
// guenstige Kriterien, Ausgabe, Auswahl.
//
// Anlass: Tapir GetElementsByType/GetAllElements lesen die Datenbank des AKTIVEN Fensters. Mit
// einem Layout vorn lieferten sie am 13.10. 0 Tueren statt 267; "Tueren mit T.-Nummer, nicht
// Abbruch, je Geschoss" brauchte 4 Aufrufe + Python. FindElements macht das in EINEM Aufruf.
//
// Leitregeln:
//  * Ausgewertet wird IMMER die Grundriss-Datenbank (alle Geschosse) ueber
//    ExecuteActionForEachDatabase — das schaltet am Ende auf JEDEM Pfad auf die Ausgangs-Datenbank
//    zurueck (CommandBase.cpp). Alle Parameter- und Namensfehler werden VOR dem Wechsel gemeldet,
//    Laufzeitfehler erst NACH der Rueckschaltung (ctx.fatalError).
//  * Kein stiller Nullbefund: unbekannte Schluessel/Namen = Fehler mit Namen und Pfad;
//    criteriaStats je Knoten macht jede "0" erklaerbar.
//  * Nur lesen: keine Modellaenderung, keine Undo-Klammer. select:true ersetzt nur die Auswahl.
//
// Entwurfsentscheidungen (Planer, 261013-fel):
//  * where im Eingabeschema nur {"type":"object"} — ELMCommandBase hat keine Schema-Definitionen
//    ($ref), und die strenge Pruefung hier liefert sprechende Fehler mit Pfad statt anonymer
//    Schemafehler. Aus demselben Grund kein additionalProperties:false auf oberster Ebene.
//  * databases ist optional umgesetzt: ohne databases laeuft genau derselbe Weg mit der
//    Grundriss-ID; select zusammen mit databases = Fehler.

#include "FindElementsCommand.hpp"
#include "FindElementsInternal.hpp"
#include "MigrationHelper.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

// ELMonkey 0.10.0: DevKit 30 hat GS::UniString::ToLowerCase durch GetLowerCased ersetzt (so loest es
// auch Tapir 1.7.2 in TapirPalette.cpp). Nur in dieser Datei und erst nach allen Includes umbenannt.
#ifdef ServerMainVers_3000
#define ToLowerCase GetLowerCased
#endif

// =====================================================================================
// Vertragsfunktionen (Task 1) und gemeinsame Lese-Helfer
// =====================================================================================

namespace FindElementsELM {

bool EvalContext::Header (const API_Guid& guid, API_Elem_Head& out)
{
    if (headerCache.ContainsKey (guid)) {
        out = headerCache.Get (guid);
        return true;
    }
    API_Elem_Head head = {};
    head.guid = guid;
    if (ACAPI_Element_GetHeader (&head) != NoError) {
        return false;
    }
    headerCache.Add (guid, head);
    out = head;
    return true;
}

bool WildcardMatch (const GS::UniString& textIn, const GS::UniString& patternIn)
{
    // Iterativ mit Ruecksprung auf den letzten Stern — kein exponentielles Backtracking.
    const GS::UniString text    = textIn.ToLowerCase ();
    const GS::UniString pattern = patternIn.ToLowerCase ();
    const GS::UniChar   star ('*');
    const GS::UniChar   question ('?');
    const UInt32 n = text.GetLength ();
    const UInt32 m = pattern.GetLength ();
    UInt32 t = 0;
    UInt32 p = 0;
    bool   haveStar = false;
    UInt32 starP = 0;
    UInt32 starT = 0;
    while (t < n) {
        if (p < m && (pattern[p] == question || pattern[p] == text[t])) {
            ++t;
            ++p;
        } else if (p < m && pattern[p] == star) {
            haveStar = true;
            starP = p;
            starT = t;
            ++p;
        } else if (haveStar) {
            p = starP + 1;
            ++starT;
            t = starT;
        } else {
            return false;
        }
    }
    while (p < m && pattern[p] == star) {
        ++p;
    }
    return p == m;
}

bool IsGuidString (const GS::UniString& s)
{
    const std::string str (s.ToCStr ().Get ());
    if (str.size () != 36) {
        return false;
    }
    for (size_t i = 0; i < str.size (); ++i) {
        const char c = str[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return false;
            }
            continue;
        }
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

GS::UniString ToUni (const GS::String& s)
{
    return GS::UniString (s.ToCStr ());
}

GS::UniString IntToUni (Int64 value)
{
    char buffer[32] = {};
    std::snprintf (buffer, sizeof (buffer), "%lld", (long long) value);
    return GS::UniString (buffer);
}

GS::UniString NumToUni (double value)
{
    char buffer[64] = {};
    std::snprintf (buffer, sizeof (buffer), "%g", value);
    return GS::UniString (buffer);
}

GS::UniString JoinNames (const std::vector<GS::UniString>& names, UInt32 maxCount)
{
    GS::UniString result;
    const UInt32 count = (UInt32) names.size ();
    const UInt32 shown = (count < maxCount) ? count : maxCount;
    for (UInt32 i = 0; i < shown; ++i) {
        if (i > 0) {
            result += GS::UniString (", ");
        }
        result += names[i];
    }
    if (count > shown) {
        result += GS::UniString (" (+") + IntToUni ((Int64) (count - shown)) + GS::UniString (" weitere)");
    }
    return result;
}

GS::UniString TypeNameOf (const GS::ObjectState& os, const GS::String& key)
{
    if (!os.Contains (key)) {
        return GS::UniString ("nichts");
    }
    switch (os.GetType (key)) {
        case GS::ObjectState::Bool:   return GS::UniString ("Wahrheitswert");
        case GS::ObjectState::UInt:
        case GS::ObjectState::Int:
        case GS::ObjectState::Real:   return GS::UniString ("Zahl");
        case GS::ObjectState::String: return GS::UniString ("Text");
        case GS::ObjectState::List:   return GS::UniString ("Liste");
        case GS::ObjectState::Object: return GS::UniString ("Objekt");
        default:                      return GS::UniString ("unbekannt");
    }
}

bool ReadNumber (const GS::ObjectState& os, const GS::String& key, double& out)
{
    if (!os.Contains (key)) {
        return false;
    }
    const GS::ObjectState::Type type = os.GetType (key);
    if (type == GS::ObjectState::Real) {
        return os.Get (key, out);
    }
    if (type == GS::ObjectState::Int) {
        Int64 v = 0;
        if (!os.Get (key, v)) {
            return false;
        }
        out = (double) v;
        return true;
    }
    if (type == GS::ObjectState::UInt) {
        UInt64 v = 0;
        if (!os.Get (key, v)) {
            return false;
        }
        out = (double) v;
        return true;
    }
    return false;
}

bool ReadNumberList (const GS::ObjectState& os, const GS::String& key, std::vector<double>& out)
{
    out.clear ();
    if (!os.Contains (key) || os.GetType (key) != GS::ObjectState::List) {
        return false;
    }
    // Die JSON-Zahlen kommen je nach Schreibweise als Real oder Int an; eine einheitliche Liste
    // wird in einer der drei Lesarten gelesen.
    GS::Array<double> reals;
    if (os.Get (key, reals)) {
        for (const double v : reals) {
            out.push_back (v);
        }
        return true;
    }
    GS::Array<Int64> ints;
    if (os.Get (key, ints)) {
        for (const Int64 v : ints) {
            out.push_back ((double) v);
        }
        return true;
    }
    GS::Array<UInt64> uints;
    if (os.Get (key, uints)) {
        for (const UInt64 v : uints) {
            out.push_back ((double) v);
        }
        return true;
    }
    return false;
}

bool ReadStringOrList (const GS::ObjectState& os, const GS::String& key, std::vector<GS::UniString>& out)
{
    out.clear ();
    if (!os.Contains (key)) {
        return false;
    }
    const GS::ObjectState::Type type = os.GetType (key);
    if (type == GS::ObjectState::String) {
        GS::UniString s;
        if (!os.Get (key, s)) {
            return false;
        }
        out.push_back (s);
        return true;
    }
    if (type == GS::ObjectState::List) {
        GS::Array<GS::UniString> list;
        if (!os.Get (key, list)) {
            return false;
        }
        for (const GS::UniString& s : list) {
            out.push_back (s);
        }
        return true;
    }
    return false;
}

}   // namespace FindElementsELM

// =====================================================================================
// Interna: Kriterien-Tabelle, guenstige Blaetter, Parser
// =====================================================================================

namespace {

using namespace FindElementsELM;

struct KindInfo {
    const char* name;
    NodeKind    kind;
    Int32       cost;
};

// Kosten laut Plan: 0 guenstig (Header) .. 3 teuer (Property/GDL/IFC je Element).
const KindInfo kKinds[] = {
    { "and",            NodeKind::And,            0 },
    { "or",             NodeKind::Or,             0 },
    { "not",            NodeKind::Not,            0 },
    { "type",           NodeKind::Type,           0 },
    { "story",          NodeKind::Story,          0 },
    { "layer",          NodeKind::Layer,          0 },
    { "flags",          NodeKind::Flags,          1 },
    { "guids",          NodeKind::Guids,          0 },
    { "inSelection",    NodeKind::InSelection,    0 },
    { "teamwork",       NodeKind::Teamwork,       0 },
    { "property",       NodeKind::Property,       3 },
    { "classification", NodeKind::Classification, 2 },
    { "libPart",        NodeKind::LibPart,        2 },
    { "gdl",            NodeKind::Gdl,            3 },
    { "ifc",            NodeKind::Ifc,            3 },
    { "hostedBy",       NodeKind::HostedBy,       2 },
    { "hosts",          NodeKind::Hosts,          2 },
    { "inZone",         NodeKind::InZone,         2 },
    { "bbox",           NodeKind::BBox,           2 },
    { "near",           NodeKind::Near,           2 },
    { "designOption",   NodeKind::DesignOption,   2 }
};

const char* const kModifiers[] = { "op", "value", "caseSensitive", "includeChildren", "unclassified" };

struct FlagInfo {
    const char*         name;
    API_ElemFilterFlags flag;
};

// Eigene Kopie der 12 Zuordnungen aus ElementCommands.cpp (ConvertFilterStringToFlag ist dort static).
const FlagInfo kFlags[] = {
    { "IsEditable",                  APIFilt_IsEditable },
    { "IsVisibleByLayer",            APIFilt_OnVisLayer },
    { "IsVisibleByRenovation",       APIFilt_IsVisibleByRenovation },
    { "IsVisibleByStructureDisplay", APIFilt_IsInStructureDisplay },
    { "IsVisibleIn3D",               APIFilt_In3D },
    { "OnActualFloor",               APIFilt_OnActFloor },
    { "OnActualLayout",              APIFilt_OnActLayout },
    { "InMyWorkspace",               APIFilt_InMyWorkspace },
    { "IsIndependent",               APIFilt_IsIndependent },
    { "InCroppedView",               APIFilt_InCroppedView },
    { "HasAccessRight",              APIFilt_HasAccessRight },
    { "IsOverriddenByRenovation",    APIFilt_IsOverridden }
};

GS::UniString AllCriteriaNames ()
{
    std::vector<GS::UniString> names;
    for (const KindInfo& k : kKinds) {
        names.push_back (GS::UniString (k.name));
    }
    return JoinNames (names, 100);
}

bool FindKind (const GS::UniString& name, KindInfo& out)
{
    for (const KindInfo& k : kKinds) {
        if (name == GS::UniString (k.name)) {
            out = k;
            return true;
        }
    }
    return false;
}

bool IsModifier (const GS::UniString& name)
{
    for (const char* m : kModifiers) {
        if (name == GS::UniString (m)) {
            return true;
        }
    }
    return false;
}

bool ModifierAllowed (NodeKind kind, const GS::UniString& m)
{
    switch (kind) {
        case NodeKind::Property:
        case NodeKind::Gdl:
        case NodeKind::Ifc:
            return m == GS::UniString ("op") || m == GS::UniString ("value") || m == GS::UniString ("caseSensitive");
        case NodeKind::Classification:
            return m == GS::UniString ("includeChildren") || m == GS::UniString ("unclassified");
        default:
            return false;
    }
}

GS::UniString AllowedModifiersText (NodeKind kind)
{
    switch (kind) {
        case NodeKind::Property:
        case NodeKind::Gdl:
        case NodeKind::Ifc:
            return GS::UniString ("erlaubt: op, value, caseSensitive");
        case NodeKind::Classification:
            return GS::UniString ("erlaubt: includeChildren, unclassified");
        default:
            return GS::UniString ("dieses Kriterium hat keine Zusatzschluessel");
    }
}

bool EqualsNoCase (const GS::UniString& a, const GS::UniString& b)
{
    return a.ToLowerCase () == b.ToLowerCase ();
}

// ---- Projektlisten (einmal je Aufruf) -----------------------------------------------------

struct StoryEntry {
    Int32         index = 0;
    GS::UniString name;
};

struct LayerEntry {
    Int32         index = 0;
    short         flags = 0;
    GS::UniString name;
};

struct ParseEnv {
    std::vector<StoryEntry> stories;
    std::vector<LayerEntry> layers;
};

GS::UniString AttributeNameOf (API_AttrTypeID typeID, const API_AttributeIndex& index)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = typeID;
    attr.header.index = index;
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError) {
        return GS::UniString ();
    }
    return name;
}

GSErrCode LoadStories (std::vector<StoryEntry>& out)
{
    API_StoryInfo storyInfo = {};
    const GSErrCode err = ACAPI_ProjectSetting_GetStorySettings (&storyInfo);
    if (err == NoError && storyInfo.data != nullptr) {
        const Int32 count = (Int32) storyInfo.lastStory - (Int32) storyInfo.firstStory + 1;
        for (Int32 i = 0; i < count; ++i) {
            const API_StoryType& story = (*storyInfo.data)[i];
            GS::UniString uName = story.uName;
            StoryEntry entry;
            entry.index = (Int32) story.index;
            entry.name = uName;
            out.push_back (entry);
        }
    }
    BMKillHandle (reinterpret_cast<GSHandle*> (&storyInfo.data));
    return err;
}

void LoadLayers (std::vector<LayerEntry>& out)
{
    GS::Array<API_Attribute> attrs;
    ACAPI_Attribute_GetAttributesByType (API_LayerID, attrs);
    for (API_Attribute& attr : attrs) {
        LayerEntry entry;
        entry.index = GetAttributeIndex (attr.header.index);
        entry.flags = attr.header.flags;
        // Name ueber uniStringNamePtr (Umlaute sicher); header.name nur als Rueckfall.
        entry.name = AttributeNameOf (API_LayerID, attr.header.index);
        if (entry.name.IsEmpty ()) {
            entry.name = GS::UniString (attr.header.name);
        }
        out.push_back (entry);
        DisposeAttribute (attr);
    }
}

GS::UniString StoryListText (const ParseEnv& env)
{
    std::vector<GS::UniString> items;
    for (const StoryEntry& s : env.stories) {
        items.push_back (IntToUni (s.index) + GS::UniString (":") + s.name);
    }
    return JoinNames (items, 60);
}

// ---- Leaf-Daten der guenstigen Blaetter -------------------------------------------------------

struct TypeLeaf : LeafData {
    GS::HashSet<Int32> types;
};

struct StoryLeaf : LeafData {
    GS::HashSet<Int32> floors;
};

struct LayerLeaf : LeafData {
    GS::HashSet<Int32> layers;
};

struct FlagsLeaf : LeafData {
    API_ElemFilterFlags flags = APIFilt_None;
};

struct GuidsLeaf : LeafData {
    GS::HashSet<API_Guid> guids;
};

struct SelectionLeaf : LeafData {
    bool inside = true;
};

enum class TeamworkMode { Free, Mine, Others };

struct TeamworkLeaf : LeafData {
    TeamworkMode mode = TeamworkMode::Free;
};

// ---- Parser der guenstigen Blaetter -----------------------------------------------------------

bool ParseTypeLeaf (const GS::ObjectState& os, const GS::UniString& path, TypeLeaf& leaf, GS::UniString& error)
{
    std::vector<GS::UniString> names;
    if (!ReadStringOrList (os, "type", names) || names.empty ()) {
        error = path + GS::UniString (": erwartet Typname oder nicht-leere Liste von Typnamen (z. B. \"Door\" oder [\"Door\",\"Window\"]), erhalten: ") + TypeNameOf (os, "type");
        return false;
    }
    for (const GS::UniString& name : names) {
        const API_ElemTypeID typeId = GetElementTypeFromNonLocalizedName (name);
        if (typeId != API_ZombieElemID) {
            leaf.types.Add ((Int32) typeId);
            continue;
        }
        // Bekannte Namen fuer die Meldung sammeln (Tapir-Typnamen, Gross/Klein zaehlt).
        std::vector<GS::UniString> known;
        GS::UniString suggestion;
        for (Int32 t = 1; t < 128; ++t) {
            const GS::UniString candidate = GetElementTypeNonLocalizedName ((API_ElemTypeID) t);
            if (candidate == GS::UniString ("Unknown") || GetElementTypeFromNonLocalizedName (candidate) == API_ZombieElemID) {
                continue;
            }
            known.push_back (candidate);
            if (EqualsNoCase (candidate, name)) {
                suggestion = candidate;
            }
        }
        error = path + GS::UniString (": unbekannter Elementtyp '") + name + GS::UniString ("'");
        if (!suggestion.IsEmpty ()) {
            error += GS::UniString (" - gemeint '") + suggestion + GS::UniString ("'? (Gross/Klein zaehlt)");
        }
        error += GS::UniString (" - erlaubt: ") + JoinNames (known, 200);
        return false;
    }
    return true;
}

bool AddStoryByIndex (const ParseEnv& env, double value, const GS::UniString& path, StoryLeaf& leaf, GS::UniString& error)
{
    if (value < -32768.0 || value > 32767.0) {
        error = path + GS::UniString (": Geschoss-Index ausserhalb des Bereichs: ") + NumToUni (value);
        return false;
    }
    const Int32 index = (Int32) value;
    if ((double) index != value) {
        error = path + GS::UniString (": Geschoss-Index muss ganzzahlig sein, erhalten ") + NumToUni (value);
        return false;
    }
    for (const StoryEntry& s : env.stories) {
        if (s.index == index) {
            leaf.floors.Add (index);
            return true;
        }
    }
    error = path + GS::UniString (": Geschoss-Index ") + IntToUni (index) + GS::UniString (" gibt es nicht - vorhanden: ") + StoryListText (env);
    return false;
}

bool AddStoryByName (const ParseEnv& env, const GS::UniString& name, const GS::UniString& path, StoryLeaf& leaf, GS::UniString& error)
{
    bool found = false;
    for (const StoryEntry& s : env.stories) {
        if (EqualsNoCase (s.name, name)) {
            leaf.floors.Add (s.index);
            found = true;
        }
    }
    if (!found) {
        error = path + GS::UniString (": unbekanntes Geschoss '") + name + GS::UniString ("' - vorhanden: ") + StoryListText (env);
    }
    return found;
}

bool ParseStoryLeaf (const GS::ObjectState& os, const GS::UniString& path, const ParseEnv& env, StoryLeaf& leaf, GS::UniString& error)
{
    const GS::String key ("story");
    const GS::ObjectState::Type type = os.GetType (key);
    if (type == GS::ObjectState::Int || type == GS::ObjectState::UInt || type == GS::ObjectState::Real) {
        double value = 0.0;
        if (!ReadNumber (os, key, value)) {
            error = path + GS::UniString (": Zahl nicht lesbar");
            return false;
        }
        return AddStoryByIndex (env, value, path, leaf, error);
    }
    if (type == GS::ObjectState::String) {
        GS::UniString name;
        os.Get (key, name);
        return AddStoryByName (env, name, path, leaf, error);
    }
    const GS::ObjectState* so = (type == GS::ObjectState::Object) ? os.Get (key) : nullptr;
    if (so == nullptr) {
        error = path + GS::UniString (": erwartet Index, Name oder Objekt {index | indices | name | names | from+to}, erhalten: ") + TypeNameOf (os, key);
        return false;
    }
    const GS::HashSet<GS::String> fields = so->GetFieldNames ();
    if (fields.IsEmpty ()) {
        error = path + GS::UniString (": leeres Objekt - erwartet index, indices, name, names oder from+to");
        return false;
    }
    bool hasFrom = false;
    bool hasTo = false;
    for (const GS::String& field : fields) {
        const GS::UniString f = ToUni (field);
        const GS::UniString fieldPath = path + GS::UniString (".") + f;
        if (f == GS::UniString ("index")) {
            double value = 0.0;
            if (!ReadNumber (*so, field, value)) {
                error = fieldPath + GS::UniString (": erwartet Zahl, erhalten: ") + TypeNameOf (*so, field);
                return false;
            }
            if (!AddStoryByIndex (env, value, fieldPath, leaf, error)) {
                return false;
            }
        } else if (f == GS::UniString ("indices")) {
            std::vector<double> values;
            if (!ReadNumberList (*so, field, values) || values.empty ()) {
                error = fieldPath + GS::UniString (": erwartet nicht-leere Liste von Zahlen, erhalten: ") + TypeNameOf (*so, field);
                return false;
            }
            for (const double v : values) {
                if (!AddStoryByIndex (env, v, fieldPath, leaf, error)) {
                    return false;
                }
            }
        } else if (f == GS::UniString ("name") || f == GS::UniString ("names")) {
            std::vector<GS::UniString> names;
            if (!ReadStringOrList (*so, field, names) || names.empty ()) {
                error = fieldPath + GS::UniString (": erwartet Name oder Liste von Namen, erhalten: ") + TypeNameOf (*so, field);
                return false;
            }
            for (const GS::UniString& n : names) {
                if (!AddStoryByName (env, n, fieldPath, leaf, error)) {
                    return false;
                }
            }
        } else if (f == GS::UniString ("from")) {
            hasFrom = true;
        } else if (f == GS::UniString ("to")) {
            hasTo = true;
        } else {
            error = fieldPath + GS::UniString (": unbekannter Schluessel - erlaubt: index, indices, name, names, from, to");
            return false;
        }
    }
    if (hasFrom != hasTo) {
        error = path + GS::UniString (": from und to nur gemeinsam angeben");
        return false;
    }
    if (hasFrom) {
        double from = 0.0;
        double to = 0.0;
        if (!ReadNumber (*so, "from", from) || !ReadNumber (*so, "to", to)) {
            error = path + GS::UniString (": from/to muessen Zahlen sein");
            return false;
        }
        Int32 added = 0;
        for (const StoryEntry& s : env.stories) {
            if ((double) s.index >= from && (double) s.index <= to) {
                leaf.floors.Add (s.index);
                ++added;
            }
        }
        if (added == 0) {
            error = path + GS::UniString (": Bereich ") + NumToUni (from) + GS::UniString ("..") + NumToUni (to) +
                    GS::UniString (" enthaelt kein Geschoss - vorhanden: ") + StoryListText (env);
            return false;
        }
    }
    return true;
}

void IntersectInto (GS::HashSet<Int32>& current, bool& initialized, const GS::HashSet<Int32>& next)
{
    if (!initialized) {
        current = next;
        initialized = true;
        return;
    }
    GS::HashSet<Int32> result;
    for (const Int32 v : current) {
        if (next.Contains (v)) {
            result.Add (v);
        }
    }
    current = result;
}

bool LayerIndexByName (const ParseEnv& env, const GS::UniString& name, const GS::UniString& path, GS::HashSet<Int32>& out, GS::UniString& error)
{
    bool found = false;
    for (const LayerEntry& l : env.layers) {
        if (EqualsNoCase (l.name, name)) {
            out.Add (l.index);
            found = true;
        }
    }
    if (found) {
        return true;
    }
    std::vector<GS::UniString> similar;
    const GS::UniString needle = name.ToLowerCase ();
    for (const LayerEntry& l : env.layers) {
        if (similar.size () >= 10) {
            break;
        }
        if (!needle.IsEmpty () && l.name.ToLowerCase ().Contains (needle)) {
            similar.push_back (l.name);
        }
    }
    error = path + GS::UniString (": unbekannte Ebene '") + name + GS::UniString ("'");
    if (similar.empty ()) {
        error += GS::UniString (" - keine aehnliche Ebene (Teilstring) gefunden");
    } else {
        error += GS::UniString (" - aehnlich: ") + JoinNames (similar, 10);
    }
    return false;
}

bool LayersVisibleInCombination (const GS::UniString& comboName, const GS::UniString& path, GS::HashSet<Int32>& out, GS::UniString& error)
{
    GS::Array<API_Attribute> combos;
    ACAPI_Attribute_GetAttributesByType (API_LayerCombID, combos);
    std::vector<GS::UniString> names;
    bool  found = false;
    Int32 comboIndex = 0;
    for (API_Attribute& combo : combos) {
        const GS::UniString name = AttributeNameOf (API_LayerCombID, combo.header.index);
        names.push_back (name);
        if (!found && EqualsNoCase (name, comboName)) {
            found = true;
            comboIndex = GetAttributeIndex (combo.header.index);
        }
        DisposeAttribute (combo);
    }
    if (!found) {
        error = path + GS::UniString (": unbekannte Ebenen-Kombination '") + comboName + GS::UniString ("' - vorhanden: ") + JoinNames (names, 60);
        return false;
    }

    API_AttributeDef def = {};
    const GSErrCode err = ACAPI_Attribute_GetDef (API_LayerCombID, ACAPI_CreateAttributeIndex (comboIndex), &def);
    if (err != NoError || def.layer_statItems == nullptr) {
        ACAPI_DisposeAttrDefsHdls (&def);
        error = path + GS::UniString (": Ebenen-Kombination '") + comboName + GS::UniString ("' nicht lesbar (Fehler ") + IntToUni (err) + GS::UniString (")");
        return false;
    }
    // Versions-Split wie AttributeCommands.cpp (AC28+: kv.key/kv.value, AC27: Zeiger).
    for (const auto& kv : *def.layer_statItems) {
#ifdef ServerMainVers_2800
        const API_AttributeIndex& layerIndex = kv.key;
        const API_LayerStat&      layerStat  = kv.value;
#else
        const API_AttributeIndex& layerIndex = *kv.key;
        const API_LayerStat&      layerStat  = *kv.value;
#endif
        if ((layerStat.lFlags & APILay_Hidden) == 0) {
            out.Add (GetAttributeIndex (layerIndex));
        }
    }
    ACAPI_DisposeAttrDefsHdls (&def);
    return true;
}

bool ParseLayerLeaf (const GS::ObjectState& os, const GS::UniString& path, const ParseEnv& env,
                     EvalContext& ctx, LayerLeaf& leaf, GS::UniString& error)
{
    const GS::String key ("layer");
    const GS::ObjectState::Type type = os.GetType (key);
    if (type == GS::ObjectState::String) {
        GS::UniString name;
        os.Get (key, name);
        return LayerIndexByName (env, name, path, leaf.layers, error);
    }
    const GS::ObjectState* lo = (type == GS::ObjectState::Object) ? os.Get (key) : nullptr;
    if (lo == nullptr) {
        error = path + GS::UniString (": erwartet Ebenenname oder Objekt {name, names, pattern, visible, locked, inCombination}, erhalten: ") + TypeNameOf (os, key);
        return false;
    }
    const GS::HashSet<GS::String> fields = lo->GetFieldNames ();
    if (fields.IsEmpty ()) {
        error = path + GS::UniString (": leeres Objekt - erwartet name, names, pattern, visible, locked oder inCombination");
        return false;
    }
    GS::HashSet<Int32> current;
    bool initialized = false;
    for (const GS::String& field : fields) {
        const GS::UniString f = ToUni (field);
        const GS::UniString fieldPath = path + GS::UniString (".") + f;
        GS::HashSet<Int32> part;
        if (f == GS::UniString ("name") || f == GS::UniString ("names")) {
            std::vector<GS::UniString> names;
            if (!ReadStringOrList (*lo, field, names) || names.empty ()) {
                error = fieldPath + GS::UniString (": erwartet Name oder Liste von Namen, erhalten: ") + TypeNameOf (*lo, field);
                return false;
            }
            for (const GS::UniString& n : names) {
                if (!LayerIndexByName (env, n, fieldPath, part, error)) {
                    return false;
                }
            }
        } else if (f == GS::UniString ("pattern")) {
            std::vector<GS::UniString> patterns;
            if (!ReadStringOrList (*lo, field, patterns) || patterns.empty ()) {
                error = fieldPath + GS::UniString (": erwartet Muster (* und ?) oder Liste von Mustern, erhalten: ") + TypeNameOf (*lo, field);
                return false;
            }
            for (const GS::UniString& pattern : patterns) {
                bool any = false;
                for (const LayerEntry& l : env.layers) {
                    if (WildcardMatch (l.name, pattern)) {
                        part.Add (l.index);
                        any = true;
                    }
                }
                if (!any) {
                    ctx.warnings.push_back (fieldPath + GS::UniString (": keine Ebene passt auf '") + pattern + GS::UniString ("'"));
                }
            }
        } else if (f == GS::UniString ("visible") || f == GS::UniString ("locked")) {
            if (lo->GetType (field) != GS::ObjectState::Bool) {
                error = fieldPath + GS::UniString (": erwartet true/false, erhalten: ") + TypeNameOf (*lo, field);
                return false;
            }
            bool wanted = false;
            lo->Get (field, wanted);
            const bool isVisible = (f == GS::UniString ("visible"));
            for (const LayerEntry& l : env.layers) {
                const bool state = isVisible ? ((l.flags & APILay_Hidden) == 0) : ((l.flags & APILay_Locked) != 0);
                if (state == wanted) {
                    part.Add (l.index);
                }
            }
        } else if (f == GS::UniString ("inCombination")) {
            GS::UniString comboName;
            if (lo->GetType (field) != GS::ObjectState::String || !lo->Get (field, comboName)) {
                error = fieldPath + GS::UniString (": erwartet Name der Ebenen-Kombination, erhalten: ") + TypeNameOf (*lo, field);
                return false;
            }
            if (!LayersVisibleInCombination (comboName, fieldPath, part, error)) {
                return false;
            }
        } else {
            error = fieldPath + GS::UniString (": unbekannter Schluessel - erlaubt: name, names, pattern, visible, locked, inCombination");
            return false;
        }
        IntersectInto (current, initialized, part);
    }
    leaf.layers = current;
    return true;
}

bool ParseFlagsLeaf (const GS::ObjectState& os, const GS::UniString& path, FlagsLeaf& leaf, GS::UniString& error)
{
    std::vector<GS::UniString> names;
    std::vector<GS::UniString> allowed;
    for (const FlagInfo& fi : kFlags) {
        allowed.push_back (GS::UniString (fi.name));
    }
    if (!ReadStringOrList (os, "flags", names) || names.empty ()) {
        error = path + GS::UniString (": erwartet Flag-Name oder nicht-leere Liste - erlaubt: ") + JoinNames (allowed, 20) + GS::UniString (", erhalten: ") + TypeNameOf (os, "flags");
        return false;
    }
    for (const GS::UniString& name : names) {
        if (name == GS::UniString ("IncludeSubElemObjects")) {
            error = path + GS::UniString (": 'IncludeSubElemObjects' ist kein Filter-Flag (nur fuer GetElementsByType)");
            return false;
        }
        bool found = false;
        for (const FlagInfo& fi : kFlags) {
            if (name == GS::UniString (fi.name)) {
                leaf.flags |= fi.flag;
                found = true;
                break;
            }
        }
        if (!found) {
            error = path + GS::UniString (": unbekanntes Flag '") + name + GS::UniString ("' - erlaubt: ") + JoinNames (allowed, 20);
            return false;
        }
    }
    return true;
}

bool ParseGuidsLeaf (const GS::ObjectState& os, const GS::UniString& path, GuidsLeaf& leaf, GS::UniString& error)
{
    const GS::String key ("guids");
    if (os.GetType (key) != GS::ObjectState::List) {
        error = path + GS::UniString (": erwartet Liste aus \"GUID\", {guid} oder {elementId:{guid}}, erhalten: ") + TypeNameOf (os, key);
        return false;
    }
    std::vector<GS::UniString> strings;
    GS::Array<GS::UniString> stringList;
    GS::Array<GS::ObjectState> objectList;
    if (os.Get (key, stringList)) {
        for (const GS::UniString& s : stringList) {
            strings.push_back (s);
        }
    } else if (os.Get (key, objectList)) {
        for (UInt32 i = 0; i < objectList.GetSize (); ++i) {
            const GS::ObjectState& item = objectList[i];
            GS::UniString g;
            const GS::ObjectState* elementId = item.Get ("elementId");
            if (elementId != nullptr) {
                elementId->Get ("guid", g);
            } else {
                item.Get ("guid", g);
            }
            if (g.IsEmpty ()) {
                error = path + GS::UniString ("[") + IntToUni (i) + GS::UniString ("]: erwartet {guid} oder {elementId:{guid}}");
                return false;
            }
            strings.push_back (g);
        }
    } else {
        error = path + GS::UniString (": Liste nicht lesbar - einheitlich GUID-Strings oder Objekte {guid}/{elementId:{guid}} angeben");
        return false;
    }
    for (const GS::UniString& s : strings) {
        if (!IsGuidString (s)) {
            error = path + GS::UniString (": ungueltige GUID '") + s + GS::UniString ("'");
            return false;
        }
        leaf.guids.Add (APIGuidFromString (s.ToCStr ().Get ()));
    }
    return true;
}

bool ParseSelectionLeaf (const GS::ObjectState& os, const GS::UniString& path, SelectionLeaf& leaf, GS::UniString& error)
{
    if (os.GetType ("inSelection") != GS::ObjectState::Bool) {
        error = path + GS::UniString (": erwartet true/false, erhalten: ") + TypeNameOf (os, "inSelection");
        return false;
    }
    os.Get ("inSelection", leaf.inside);
    return true;
}

bool ParseTeamworkLeaf (const GS::ObjectState& os, const GS::UniString& path, const EvalContext& ctx, TeamworkLeaf& leaf, GS::UniString& error)
{
    GS::UniString value;
    if (os.GetType ("teamwork") != GS::ObjectState::String || !os.Get ("teamwork", value)) {
        error = path + GS::UniString (": erwartet \"frei\", \"vonMir\" oder \"vonAnderen\", erhalten: ") + TypeNameOf (os, "teamwork");
        return false;
    }
    if (!ctx.isTeamwork) {
        error = path + GS::UniString (": Projekt ist kein Teamwork-Projekt");
        return false;
    }
    const GS::UniString v = value.ToLowerCase ();
    if (v == GS::UniString ("frei") || v == GS::UniString ("free")) {
        leaf.mode = TeamworkMode::Free;
    } else if (v == GS::UniString ("vonmir") || v == GS::UniString ("mine")) {
        leaf.mode = TeamworkMode::Mine;
    } else if (v == GS::UniString ("vonanderen") || v == GS::UniString ("others")) {
        leaf.mode = TeamworkMode::Others;
    } else {
        error = path + GS::UniString (": unbekannter Wert '") + value + GS::UniString ("' - erlaubt: frei, vonMir, vonAnderen (auch free, mine, others)");
        return false;
    }
    return true;
}

// ---- Knoten-Parser ----------------------------------------------------------------------------

std::unique_ptr<Node> ParseNode (EvalContext& ctx, const ParseEnv& env, const GS::ObjectState& os,
                                 const GS::UniString& basePath, GS::UniString& error)
{
    const GS::HashSet<GS::String> fields = os.GetFieldNames ();
    if (fields.IsEmpty ()) {
        error = basePath + GS::UniString (": leerer Knoten - erwartet genau ein Kriterium (") + AllCriteriaNames () + GS::UniString (")");
        return nullptr;
    }
    std::vector<GS::String>    criteria;
    std::vector<GS::UniString> modifiers;
    for (const GS::String& field : fields) {
        const GS::UniString f = ToUni (field);
        KindInfo info = {};
        if (FindKind (f, info)) {
            criteria.push_back (field);
        } else if (IsModifier (f)) {
            modifiers.push_back (f);
        } else {
            error = basePath + GS::UniString (": unbekanntes Kriterium '") + f + GS::UniString ("' - erlaubt: ") + AllCriteriaNames ();
            return nullptr;
        }
    }
    if (criteria.empty ()) {
        error = basePath + GS::UniString (": nur Zusatzschluessel (") + JoinNames (modifiers, 10) +
                GS::UniString (") ohne Kriterium - erlaubt: ") + AllCriteriaNames ();
        return nullptr;
    }
    if (criteria.size () > 1) {
        std::vector<GS::UniString> names;
        for (const GS::String& c : criteria) {
            names.push_back (ToUni (c));
        }
        error = basePath + GS::UniString (": mehrere Kriterien in einem Knoten (") + JoinNames (names, 10) +
                GS::UniString (") - je Knoten genau eines, mit and/or verbinden");
        return nullptr;
    }

    const GS::String key = criteria[0];
    KindInfo info = {};
    FindKind (ToUni (key), info);
    for (const GS::UniString& m : modifiers) {
        if (!ModifierAllowed (info.kind, m)) {
            error = basePath + GS::UniString (".") + m + GS::UniString (": bei '") + ToUni (key) +
                    GS::UniString ("' nicht erlaubt - ") + AllowedModifiersText (info.kind);
            return nullptr;
        }
    }

    std::unique_ptr<Node> node (new Node ());
    node->kind = info.kind;
    node->criterion = ToUni (key);
    node->path = basePath + GS::UniString (".") + node->criterion;
    node->cost = info.cost;
    node->statsIndex = (UInt32) ctx.stats.size ();
    {
        NodeStats stats;
        stats.path = node->path;
        stats.criterion = node->criterion;
        ctx.stats.push_back (stats);
    }

    switch (info.kind) {
        case NodeKind::And:
        case NodeKind::Or: {
            GS::Array<GS::ObjectState> items;
            if (os.GetType (key) != GS::ObjectState::List || !os.Get (key, items) || items.IsEmpty ()) {
                error = node->path + GS::UniString (": erwartet nicht-leere Liste von Knoten (Objekten), erhalten: ") + TypeNameOf (os, key);
                return nullptr;
            }
            Int32 cost = 0;
            for (UInt32 i = 0; i < items.GetSize (); ++i) {
                const GS::UniString childBase = node->path + GS::UniString ("[") + IntToUni (i) + GS::UniString ("]");
                std::unique_ptr<Node> child = ParseNode (ctx, env, items[i], childBase, error);
                if (child == nullptr) {
                    return nullptr;
                }
                cost = (std::max) (cost, child->cost);
                node->children.push_back (std::move (child));
            }
            node->cost = cost;
            break;
        }
        case NodeKind::Not:
        case NodeKind::HostedBy:
        case NodeKind::Hosts:
        case NodeKind::InZone: {
            const GS::ObjectState* childOs = (os.GetType (key) == GS::ObjectState::Object) ? os.Get (key) : nullptr;
            if (childOs == nullptr) {
                error = node->path + GS::UniString (": erwartet Knoten (Objekt mit genau einem Kriterium), erhalten: ") + TypeNameOf (os, key);
                return nullptr;
            }
            std::unique_ptr<Node> child = ParseNode (ctx, env, *childOs, node->path, error);
            if (child == nullptr) {
                return nullptr;
            }
            node->cost = (info.kind == NodeKind::Not) ? child->cost : (std::max) ((Int32) 2, child->cost);
            node->children.push_back (std::move (child));
            break;
        }
        case NodeKind::Type: {
            std::unique_ptr<TypeLeaf> leaf (new TypeLeaf ());
            if (!ParseTypeLeaf (os, node->path, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::Story: {
            std::unique_ptr<StoryLeaf> leaf (new StoryLeaf ());
            if (!ParseStoryLeaf (os, node->path, env, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::Layer: {
            std::unique_ptr<LayerLeaf> leaf (new LayerLeaf ());
            if (!ParseLayerLeaf (os, node->path, env, ctx, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::Flags: {
            std::unique_ptr<FlagsLeaf> leaf (new FlagsLeaf ());
            if (!ParseFlagsLeaf (os, node->path, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::Guids: {
            std::unique_ptr<GuidsLeaf> leaf (new GuidsLeaf ());
            if (!ParseGuidsLeaf (os, node->path, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::InSelection: {
            std::unique_ptr<SelectionLeaf> leaf (new SelectionLeaf ());
            if (!ParseSelectionLeaf (os, node->path, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        case NodeKind::Teamwork: {
            std::unique_ptr<TeamworkLeaf> leaf (new TeamworkLeaf ());
            if (!ParseTeamworkLeaf (os, node->path, ctx, *leaf, error)) {
                return nullptr;
            }
            node->leaf = std::move (leaf);
            break;
        }
        default: {
            // Property, Classification, LibPart, Gdl, Ifc, BBox, Near, DesignOption -> Task 2.
            if (!ParseValueLeaf (info.kind, os, node->path, *ctx.valueState, *node, error)) {
                return nullptr;
            }
            node->cost = info.cost;
            break;
        }
    }
    return node;
}

bool TreeContains (const Node& node, NodeKind kind)
{
    if (node.kind == kind) {
        return true;
    }
    for (const std::unique_ptr<Node>& child : node.children) {
        if (TreeContains (*child, kind)) {
            return true;
        }
    }
    return false;
}

// ---- Auswertung der guenstigen Blaetter -------------------------------------------------------

bool MatchHeaderLeaf (const EvalContext& ctx, const Node& node, const API_Elem_Head& head)
{
    switch (node.kind) {
        case NodeKind::Type:
            return static_cast<const TypeLeaf*> (node.leaf.get ())->types.Contains ((Int32) GetElemTypeId (head));
        case NodeKind::Story:
            return static_cast<const StoryLeaf*> (node.leaf.get ())->floors.Contains ((Int32) head.floorInd);
        case NodeKind::Layer:
            return static_cast<const LayerLeaf*> (node.leaf.get ())->layers.Contains (GetAttributeIndex (head.layer));
        case NodeKind::Teamwork: {
            // In Teamwork ist userId ("owner") die Nutzer-ID des Reservierenden, frei = 0; lockId bleibt 0
            // (live THN 29.10.2026, ELM 0.10.3).
            const TeamworkMode mode = static_cast<const TeamworkLeaf*> (node.leaf.get ())->mode;
            const short ownerId = head.userId;
            if (mode == TeamworkMode::Free) {
                return ownerId == 0;
            }
            if (mode == TeamworkMode::Mine) {
                return ownerId != 0 && ownerId == ctx.myUserId;
            }
            return ownerId != 0 && ownerId != ctx.myUserId;
        }
        default:
            return false;
    }
}

void EvaluateCheapLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out)
{
    if (node.kind == NodeKind::Guids) {
        const GuidsLeaf* leaf = static_cast<const GuidsLeaf*> (node.leaf.get ());
        for (const API_Guid& guid : in) {
            if (leaf->guids.Contains (guid)) {
                out.push_back (guid);
            }
        }
        return;
    }
    if (node.kind == NodeKind::InSelection) {
        const bool inside = static_cast<const SelectionLeaf*> (node.leaf.get ())->inside;
        for (const API_Guid& guid : in) {
            if (ctx.selectionAtStart.Contains (guid) == inside) {
                out.push_back (guid);
            }
        }
        return;
    }
    if (node.kind == NodeKind::Flags) {
        const API_ElemFilterFlags flags = static_cast<const FlagsLeaf*> (node.leaf.get ())->flags;
        for (const API_Guid& guid : in) {
            if (ACAPI_Element_Filter (guid, flags)) {
                out.push_back (guid);
            }
        }
        return;
    }
    UInt64 notAvailable = 0;
    for (const API_Guid& guid : in) {
        API_Elem_Head head = {};
        if (!ctx.Header (guid, head)) {
            ++notAvailable;
            continue;
        }
        if (MatchHeaderLeaf (ctx, node, head)) {
            out.push_back (guid);
        }
    }
    ctx.stats[node.statsIndex].notAvailable += notAvailable;
}

// ---- Auswahl ----------------------------------------------------------------------------------

// Kopie aus ElementCommands.cpp (dort static): Schnitt-/Ansichtselemente auf das Modellelement abbilden.
API_Guid ParentOfSectElem (const API_Guid& elemGuid)
{
    API_Element element = {};
    element.header.guid = elemGuid;
    if (ACAPI_Element_GetHeader (&element.header) != NoError ||
        GetElemTypeId (element.header) != API_SectElemID ||
        ACAPI_Element_Get (&element) != NoError) {
        return elemGuid;
    }
    return element.sectElem.parentGuid;
}

GSErrCode ReadSelection (GS::HashSet<API_Guid>& out)
{
    API_SelectionInfo selectionInfo = {};
    GS::Array<API_Neig> selectedNeigs;
    GSErrCode err = ACAPI_Selection_Get (&selectionInfo, &selectedNeigs, false);
    // Marquee-Region gehoert dem Aufrufer (DevKit-Doku zu API_SelectionInfo).
    BMKillHandle (reinterpret_cast<GSHandle*> (&selectionInfo.marquee.coords));
    if (err == APIERR_NOSEL) {
        return NoError;
    }
    if (err != NoError) {
        return err;
    }
    for (const API_Neig& neig : selectedNeigs) {
        out.Add (ParentOfSectElem (neig.guid));
    }
    return NoError;
}

GS::ObjectState SelectHits (const GuidList& hits)
{
    GS::ObjectState selection;
    selection.Add ("requested", (Int32) hits.size ());
    if (hits.empty ()) {
        // 0 Treffer: Auswahl des Nutzers bleibt unveraendert.
        selection.Add ("changed", false);
        return selection;
    }
    ACAPI_Selection_DeselectAll ();
    GS::Array<API_Neig> neigs;
    for (const API_Guid& guid : hits) {
        neigs.Push (API_Neig (guid));
    }
    const GSErrCode selectErr = ACAPI_Selection_Select (neigs, true);

    GS::HashSet<API_Guid> nowSelected;
    const GSErrCode readErr = ReadSelection (nowSelected);
    Int32 selected = 0;
    Int32 notSelectedCount = 0;
    std::vector<GS::UniString> notSelected;
    for (const API_Guid& guid : hits) {
        if (nowSelected.Contains (guid)) {
            ++selected;
        } else {
            ++notSelectedCount;
            if (notSelected.size () < 500) {
                notSelected.push_back (APIGuidToString (guid));
            }
        }
    }
    selection.Add ("changed", true);
    selection.Add ("selected", selected);
    selection.Add ("notSelectedCount", notSelectedCount);
    const auto& notSelectedOut = selection.AddList<GS::UniString> ("notSelected");
    for (const GS::UniString& g : notSelected) {
        notSelectedOut (g);
    }
    const Int32 totalNow = (Int32) nowSelected.GetSize ();
    selection.Add ("extraSelectedCount", (totalNow > selected) ? (totalNow - selected) : (Int32) 0);
    if (selectErr != NoError) {
        selection.Add ("selectErr", (Int32) selectErr);
    }
    if (readErr != NoError) {
        selection.Add ("readBackErr", (Int32) readErr);
    }
    return selection;
}

// ---- Ausgabe ----------------------------------------------------------------------------------

GS::UniString LayerNameCached (EvalContext& ctx, Int32 layerIndex)
{
    const auto it = ctx.layerNames.find (layerIndex);
    if (it != ctx.layerNames.end ()) {
        return it->second;
    }
    const GS::UniString name = AttributeNameOf (API_LayerID, ACAPI_CreateAttributeIndex (layerIndex));
    ctx.layerNames[layerIndex] = name;
    return name;
}

GS::ObjectState BuildHit (EvalContext& ctx, const API_Guid& guid, bool withValues, const API_Guid* databaseId)
{
    GS::ObjectState hit = CreateElementIdObjectState (guid);
    API_Elem_Head head = {};
    if (ctx.Header (guid, head)) {
        hit.Add ("type", GetElementTypeNonLocalizedName (GetElemTypeId (head)));
        hit.Add ("floorIndex", (Int32) head.floorInd);
        const auto storyIt = ctx.storyNames.find ((Int32) head.floorInd);
        hit.Add ("storyName", (storyIt != ctx.storyNames.end ()) ? storyIt->second : GS::UniString ());
        hit.Add ("layerName", LayerNameCached (ctx, GetAttributeIndex (head.layer)));
    }
    if (withValues) {
        AddReturnValues (ctx, guid, hit);
    }
    if (databaseId != nullptr) {
        hit.Add ("databaseId", CreateGuidObjectState (*databaseId));
    }
    return hit;
}

struct PerDatabase {
    API_Guid      id = APINULLGuid;
    Int32         scopeCount = 0;
    Int32         matchCount = 0;
    bool          failed = false;
    GSErrCode     err = NoError;
    GS::UniString message;
};

}   // namespace

// =====================================================================================
// Motor (Vertrag)
// =====================================================================================

namespace FindElementsELM {

void EvaluateNode (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out)
{
    out.clear ();
    ctx.stats[node.statsIndex].evaluated = true;
    ctx.stats[node.statsIndex].checked += (UInt64) in.size ();
    if (in.empty () || !ctx.fatalError.IsEmpty ()) {
        return;
    }

    switch (node.kind) {
        case NodeKind::And: {
            // Guenstige Kinder zuerst; bei gleichen Kosten Reihenfolge des Nutzers (stable_sort).
            std::vector<const Node*> order;
            for (const std::unique_ptr<Node>& child : node.children) {
                order.push_back (child.get ());
            }
            std::stable_sort (order.begin (), order.end (), [] (const Node* a, const Node* b) { return a->cost < b->cost; });
            GuidList current = in;
            GuidList next;
            for (const Node* child : order) {
                EvaluateNode (ctx, *child, current, next);
                current.swap (next);
                if (current.empty () || !ctx.fatalError.IsEmpty ()) {
                    break;
                }
            }
            out.swap (current);
            break;
        }
        case NodeKind::Or: {
            std::vector<const Node*> order;
            for (const std::unique_ptr<Node>& child : node.children) {
                order.push_back (child.get ());
            }
            std::stable_sort (order.begin (), order.end (), [] (const Node* a, const Node* b) { return a->cost < b->cost; });
            GS::HashSet<API_Guid> hit;
            GuidList open = in;
            GuidList result;
            for (const Node* child : order) {
                // Jedes Kind nur auf die noch NICHT getroffenen Elemente anwenden (Kurzschluss).
                EvaluateNode (ctx, *child, open, result);
                if (!ctx.fatalError.IsEmpty ()) {
                    break;
                }
                for (const API_Guid& guid : result) {
                    hit.Add (guid);
                }
                GuidList stillOpen;
                for (const API_Guid& guid : open) {
                    if (!hit.Contains (guid)) {
                        stillOpen.push_back (guid);
                    }
                }
                open.swap (stillOpen);
                if (open.empty ()) {
                    break;
                }
            }
            for (const API_Guid& guid : in) {
                if (hit.Contains (guid)) {
                    out.push_back (guid);
                }
            }
            break;
        }
        case NodeKind::Not: {
            GuidList childOut;
            EvaluateNode (ctx, *node.children[0], in, childOut);
            GS::HashSet<API_Guid> excluded;
            for (const API_Guid& guid : childOut) {
                excluded.Add (guid);
            }
            for (const API_Guid& guid : in) {
                if (!excluded.Contains (guid)) {
                    out.push_back (guid);
                }
            }
            break;
        }
        case NodeKind::Type:
        case NodeKind::Story:
        case NodeKind::Layer:
        case NodeKind::Flags:
        case NodeKind::Guids:
        case NodeKind::InSelection:
        case NodeKind::Teamwork:
            EvaluateCheapLeaf (ctx, node, in, out);
            break;
        default:
            EvaluateValueLeaf (ctx, node, in, out);
            break;
    }

    ctx.stats[node.statsIndex].matched += (UInt64) out.size ();
}

}   // namespace FindElementsELM

// =====================================================================================
// Befehl
// =====================================================================================

GS::Optional<GS::UniString> FindElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "description": "Elementsuche wie 'Suchen & aktivieren'. Liest IMMER die Grundriss-Datenbank (alle Geschosse), nie das aktive Fenster. where = Kriterienbaum, je Knoten GENAU ein Schluessel: and/or (Liste von Knoten), not (Knoten), type, story, layer, flags, guids, inSelection, teamwork, property, classification, libPart, gdl, ifc, hostedBy/hosts/inZone (Unterbaum), bbox, near, designOption. Vergleiche bei property/gdl/ifc ueber Geschwister op/value/caseSensitive (op: = != contains startsWith endsWith regex < <= > >= between in hasValue isEmpty). Ohne where: alle Elemente des Suchbereichs. Nur lesen (select aendert nur die Auswahl).",
        "properties": {
            "where": {
                "type": "object",
                "description": "Kriterienbaum, z. B. {\"and\":[{\"type\":\"Door\"},{\"story\":{\"index\":2}},{\"not\":{\"property\":\"Umbau-Status\",\"op\":\"=\",\"value\":\"ToBeDemolished\"}}]}. Fehler nennen den Pfad (where.and[2].not...)."
            },
            "scope": {
                "type": "string",
                "description": "Suchbereich: projekt (Vorgabe, alle Geschosse inkl. ausgeblendeter Ebenen) | wie3D (sichtbare Ebenen + in 3D) | sichtbar (sichtbare Ebenen inkl. 2D) | aktuellesGeschoss."
            },
            "return": {
                "type": "array",
                "description": "Eigenschaften, deren Werte je Treffer mitgeliefert werden: \"Name\", \"Gruppe/Name\", \"GUID\", {guid} oder {group, name}.",
                "items": { "oneOf": [ { "type": "string" }, { "type": "object" } ] }
            },
            "countOnly": { "type": "boolean", "description": "Nur zaehlen, keine Elementliste." },
            "select": { "type": "boolean", "description": "Treffer in Archicad auswaehlen (ersetzt die Auswahl), mit Ruecklese. Nicht zusammen mit databases." },
            "limit": { "type": "integer", "description": "Hoechstzahl gelieferter Treffer (Vorgabe 20000, 1..200000); matchCount zaehlt immer alle." },
            "databases": {
                "type": "array",
                "description": "Optional statt Grundriss: diese Datenbanken auswerten.",
                "items": {
                    "type": "object",
                    "properties": {
                        "databaseId": {
                            "type": "object",
                            "properties": { "guid": { "type": "string" } },
                            "required": [ "guid" ]
                        }
                    },
                    "required": [ "databaseId" ]
                }
            }
        }
    })");
}

GS::Optional<GS::UniString> FindElementsCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (GS::UniString (R"({
        "type": "object",
        "properties": {
            "database": { "type": "string" },
            "startDatabaseType": { "type": "string" },
            "scope": { "type": "string" },
            "scopeCount": { "type": "integer" },
            "matchCount": { "type": "integer" },
            "returnedCount": { "type": "integer" },
            "truncated": { "type": "boolean" },
            "elements": { "type": "array", "items": { "type": "object" } },
            "returnColumns": { "type": "array", "items": { "type": "object" } },
            "criteriaStats": { "type": "array", "items": { "type": "object" } },
            "selection": { "type": "object" },
            "warnings": { "type": "array", "items": { "type": "string" } },
            "perDatabase": { "type": "array", "items": { "type": "object" } },
            "elapsedMs": { "type": "integer" }
        },
        "required": [ "scopeCount", "matchCount", "criteriaStats" ]
    })"));
}

GS::ObjectState FindElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    using namespace FindElementsELM;
    const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now ();

    // ---- 1. Schluessel der obersten Ebene ----
    {
        const char* const allowedTop[] = { "where", "scope", "return", "countOnly", "select", "limit", "databases" };
        const GS::HashSet<GS::String> topFields = parameters.GetFieldNames ();
        for (const GS::String& field : topFields) {
            bool known = false;
            for (const char* a : allowedTop) {
                if (ToUni (field) == GS::UniString (a)) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("FindElements: unbekannter Parameter '") + ToUni (field) +
                    GS::UniString ("' - erlaubt: where, scope, return, countOnly, select, limit, databases"));
            }
        }
    }

    // ---- 2. scope, limit, countOnly, select, databases ----
    GS::UniString scope ("projekt");
    API_ElemFilterFlags scopeFlags = APIFilt_None;
    if (parameters.Contains ("scope")) {
        if (parameters.GetType ("scope") != GS::ObjectState::String) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("scope: erwartet Text (projekt, wie3D, sichtbar, aktuellesGeschoss), erhalten: ") + TypeNameOf (parameters, "scope"));
        }
        parameters.Get ("scope", scope);
    }
    {
        const GS::UniString s = scope.ToLowerCase ();
        if (s == GS::UniString ("projekt")) {
            scope = GS::UniString ("projekt");
            scopeFlags = APIFilt_None;
        } else if (s == GS::UniString ("wie3d")) {
            scope = GS::UniString ("wie3D");
            scopeFlags = APIFilt_OnVisLayer;
            scopeFlags |= APIFilt_In3D;
        } else if (s == GS::UniString ("sichtbar")) {
            scope = GS::UniString ("sichtbar");
            scopeFlags = APIFilt_OnVisLayer;
        } else if (s == GS::UniString ("aktuellesgeschoss")) {
            scope = GS::UniString ("aktuellesGeschoss");
            scopeFlags = APIFilt_OnActFloor;
        } else {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("scope: unbekannter Suchbereich '") + scope +
                GS::UniString ("' - erlaubt: projekt, wie3D, sichtbar, aktuellesGeschoss"));
        }
    }

    Int32 limit = 20000;
    if (parameters.Contains ("limit")) {
        double value = 0.0;
        if (!ReadNumber (parameters, "limit", value) || value < 1.0 || value > 200000.0 || value != (double) (Int64) value) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("limit: erwartet ganze Zahl 1..200000"));
        }
        limit = (Int32) value;
    }

    bool countOnly = false;
    bool doSelect = false;
    if (parameters.Contains ("countOnly")) {
        if (parameters.GetType ("countOnly") != GS::ObjectState::Bool) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("countOnly: erwartet true/false"));
        }
        parameters.Get ("countOnly", countOnly);
    }
    if (parameters.Contains ("select")) {
        if (parameters.GetType ("select") != GS::ObjectState::Bool) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("select: erwartet true/false"));
        }
        parameters.Get ("select", doSelect);
    }

    const bool hasDatabases = parameters.Contains ("databases");
    GS::Array<API_Guid> databaseIds;
    if (hasDatabases) {
        GS::Array<GS::ObjectState> dbItems;
        if (parameters.GetType ("databases") != GS::ObjectState::List || !parameters.Get ("databases", dbItems) || dbItems.IsEmpty ()) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("databases: erwartet nicht-leere Liste von {databaseId:{guid}}"));
        }
        for (UInt32 i = 0; i < dbItems.GetSize (); ++i) {
            const API_Guid id = GetGuidFromDatabaseArrayItem (dbItems[i]);
            if (id == APINULLGuid) {
                return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("databases[") + IntToUni (i) + GS::UniString ("]: databaseId.guid fehlt oder ungueltig"));
            }
            databaseIds.Push (id);
        }
        if (doSelect) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("select ist nur ohne databases moeglich (Auswahl gilt fuer den Grundriss)"));
        }
    }

    // ---- 3. Kontext, return, Projekt-Info, Listen, where ----
    EvalContext ctx;
    ctx.valueState = CreateValueLeafState ();
    {
        GS::UniString returnError;
        if (!ParseReturnList (parameters, *ctx.valueState, returnError)) {
            return CreateErrorResponseELM (APIERR_BADPARS, returnError);
        }
    }
    {
        API_ProjectInfo projectInfo = {};
        if (ACAPI_ProjectOperation_Project (&projectInfo) == NoError) {
            ctx.isTeamwork = projectInfo.teamwork;
            ctx.myUserId = projectInfo.userId;
        }
        // API_ProjectInfo-Zeiger werden wie im uebrigen Tapir-Code nicht freigegeben.
    }
    ParseEnv env;
    LoadStories (env.stories);
    LoadLayers (env.layers);
    for (const StoryEntry& s : env.stories) {
        ctx.storyNames[s.index] = s.name;
    }
    for (const LayerEntry& l : env.layers) {
        ctx.layerNames[l.index] = l.name;
    }

    std::unique_ptr<Node> root;
    if (parameters.Contains ("where")) {
        const GS::ObjectState* whereOs = (parameters.GetType ("where") == GS::ObjectState::Object) ? parameters.Get ("where") : nullptr;
        if (whereOs == nullptr) {
            return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("where: erwartet Objekt (Kriterienbaum), erhalten: ") + TypeNameOf (parameters, "where"));
        }
        GS::UniString parseError;
        root = ParseNode (ctx, env, *whereOs, GS::UniString ("where"), parseError);
        if (root == nullptr) {
            return CreateErrorResponseELM (APIERR_BADPARS, parseError);
        }
    }

    // ---- 4. Auswahl im AUSGANGS-Fenster lesen (nur wenn inSelection vorkommt) ----
    if (root != nullptr && TreeContains (*root, NodeKind::InSelection)) {
        const GSErrCode selErr = ReadSelection (ctx.selectionAtStart);
        if (selErr != NoError) {
            return CreateErrorResponseELM (selErr, GS::UniString ("inSelection: Auswahl nicht lesbar (Fehler ") + IntToUni (selErr) + GS::UniString (")"));
        }
    }

    // ---- 5. Ausgangs-Datenbank ----
    GS::UniString startDatabaseType ("unbekannt");
    {
        API_DatabaseInfo startDb = {};
        if (ACAPI_Database_GetCurrentDatabase (&startDb) == NoError) {
            startDatabaseType = WindowTypeNameELM (startDb.typeID);
        }
    }

    // ---- 6. Datenbanken auswerten ----
    if (!hasDatabases) {
        API_DatabaseInfo floorPlan = {};
        floorPlan.typeID = APIWind_FloorPlanID;
        databaseIds.Push (DatabaseIdResolver::Instance ().GetIdOfDatabase (floorPlan));
    }

    const bool withValues = HasReturnColumns (*ctx.valueState);
    std::vector<PerDatabase> perDb (databaseIds.GetSize ());
    for (UInt32 i = 0; i < databaseIds.GetSize (); ++i) {
        perDb[i].id = databaseIds[i];
    }
    UInt32 dbPos = 0;
    Int64  scopeCount = 0;
    Int64  matchCount = 0;
    Int32  returnedCount = 0;
    std::vector<GS::ObjectState> hitObjects;
    GS::ObjectState selection;
    bool selectionDone = false;

    const GSErrCode dbErr = ExecuteActionForEachDatabase (databaseIds,
        [&] () -> GSErrCode {
            PerDatabase& current = perDb[dbPos];
            GS::Array<API_Guid> list;
            const GSErrCode listErr = ACAPI_Element_GetElemList (API_ZombieElemID, &list, scopeFlags);
            if (listErr != NoError) {
                return listErr;
            }
            GuidList candidates;
            candidates.reserve (list.GetSize ());
            for (const API_Guid& guid : list) {
                candidates.push_back (guid);
            }
            GuidList hits;
            if (root != nullptr) {
                EvaluateNode (ctx, *root, candidates, hits);
            } else {
                hits = candidates;
            }
            if (!ctx.fatalError.IsEmpty ()) {
                return APIERR_GENERAL;
            }
            current.scopeCount = (Int32) candidates.size ();
            current.matchCount = (Int32) hits.size ();
            scopeCount += (Int64) candidates.size ();
            matchCount += (Int64) hits.size ();
            if (!countOnly) {
                for (const API_Guid& guid : hits) {
                    if (returnedCount >= limit) {
                        break;
                    }
                    hitObjects.push_back (BuildHit (ctx, guid, withValues, hasDatabases ? &current.id : nullptr));
                    ++returnedCount;
                }
                if (!ctx.fatalError.IsEmpty ()) {
                    return APIERR_GENERAL;
                }
            }
            if (doSelect && !hasDatabases) {
                selection = SelectHits (hits);
                selectionDone = true;
            }
            return NoError;
        },
        [&] () {
            ++dbPos;
        },
        [&] (GSErrCode err, const GS::UniString& message) {
            if (dbPos < perDb.size ()) {
                perDb[dbPos].failed = true;
                perDb[dbPos].err = err;
                perDb[dbPos].message = message;
            }
            ++dbPos;
        });

    // ---- 7. Nach der Rueckschaltung: Fehler melden ----
    if (dbErr != NoError) {
        return CreateErrorResponseELM (dbErr, GS::UniString ("FindElements: Ausgangs-Datenbank nicht ermittelbar/nicht wiederhergestellt (Fehler ") +
            IntToUni (dbErr) + GS::UniString (") - aktives Fenster pruefen"));
    }
    if (!ctx.fatalError.IsEmpty ()) {
        return CreateErrorResponseELM ((ctx.fatalCode != NoError) ? ctx.fatalCode : (GSErrCode) APIERR_GENERAL, ctx.fatalError);
    }
    if (!hasDatabases && !perDb.empty () && perDb[0].failed) {
        return CreateErrorResponseELM (perDb[0].err, GS::UniString ("FindElements: Grundriss-Datenbank nicht erreichbar: ") + perDb[0].message +
            GS::UniString (" (Fehler ") + IntToUni (perDb[0].err) + GS::UniString (")"));
    }

    // ---- 8. Antwort ----
    GS::ObjectState response;
    response.Add ("database", GS::UniString (hasDatabases ? "databases" : "FloorPlan"));
    response.Add ("startDatabaseType", startDatabaseType);
    response.Add ("scope", scope);
    response.Add ("scopeCount", (Int32) scopeCount);
    response.Add ("matchCount", (Int32) matchCount);
    response.Add ("returnedCount", returnedCount);
    response.Add ("truncated", !countOnly && (Int64) returnedCount < matchCount);
    if (!countOnly) {
        const auto& elementsOut = response.AddList<GS::ObjectState> ("elements");
        for (const GS::ObjectState& hit : hitObjects) {
            elementsOut (hit);
        }
    }
    if (withValues) {
        AddReturnColumns (*ctx.valueState, response);
    }
    {
        const auto& statsOut = response.AddList<GS::ObjectState> ("criteriaStats");
        for (const NodeStats& st : ctx.stats) {
            GS::ObjectState s;
            s.Add ("path", st.path);
            s.Add ("criterion", st.criterion);
            s.Add ("checked", (Int32) st.checked);
            s.Add ("matched", (Int32) st.matched);
            s.Add ("evaluated", st.evaluated);
            if (st.notAvailable > 0) {
                s.Add ("notAvailable", (Int32) st.notAvailable);
            }
            if (st.typeMismatch > 0) {
                s.Add ("typeMismatch", (Int32) st.typeMismatch);
            }
            statsOut (s);
        }
    }
    if (selectionDone) {
        response.Add ("selection", selection);
    }
    if (!ctx.warnings.empty ()) {
        const auto& warningsOut = response.AddList<GS::UniString> ("warnings");
        for (const GS::UniString& w : ctx.warnings) {
            warningsOut (w);
        }
    }
    if (hasDatabases) {
        const auto& perDbOut = response.AddList<GS::ObjectState> ("perDatabase");
        for (const PerDatabase& p : perDb) {
            GS::ObjectState d;
            d.Add ("databaseId", CreateGuidObjectState (p.id));
            d.Add ("scopeCount", p.scopeCount);
            d.Add ("matchCount", p.matchCount);
            if (p.failed) {
                const GS::UniString dbError = p.message + GS::UniString (" (Fehler ") + IntToUni (p.err) + GS::UniString (")");
                d.Add ("error", dbError);
            }
            perDbOut (d);
        }
    }
    const Int64 elapsed = (Int64) std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now () - startTime).count ();
    response.Add ("elapsedMs", (Int32) elapsed);
    return response;
}
