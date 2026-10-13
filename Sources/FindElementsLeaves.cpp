// ELM_SAB 0.9.23 (2026-10-13, WiSchu-Tuerenabgleich) — FindElements: Wert- und Beziehungskriterien
// (property, classification, libPart, gdl, ifc, bbox, near, designOption, hostedBy, hosts, inZone)
// und die return-Werte je Treffer. Vertrag mit dem Motor: FindElementsInternal.hpp.
//
// Eigenschaften werden je Element GEBUENDELT gelesen: EIN ACAPI_Element_GetPropertyValuesByGuid-
// Aufruf mit ALLEN im Baum + return benoetigten Eigenschaften, nur fuer die noch offene
// Kandidatenmenge, Ergebnis gecacht (das DevKit kennt keine Mehr-Element-Property-API).
// Gespeichert werden nur die extrahierten Werte, nicht API_Property (Speicher).
//
// Semantik (Planer 261013-fel): Wert nicht verfuegbar -> Blatt false + notAvailable; undefiniert/leer
// -> "=" false, "!=" true, isEmpty true, hasValue false; Aufzaehlungen passen auf nonLocalizedValue
// ODER Anzeige-String; Text ohne Gross/Klein (caseSensitive:true schaltet um); Zahlen in API-Einheiten,
// Gleichheit mit Toleranz 1e-6; "not" ueber ein Wert-Blatt schliesst Elemente OHNE die Eigenschaft ein.

#include "FindElementsInternal.hpp"
#include "MigrationHelper.hpp"
#include "IFCCommands.hpp"

#include <cmath>
#include <locale>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef ServerMainVers_2900
#include "ACAPI/Result.hpp"
#include "ACAPI_Navigator.h"
#include "ACAPI/View.hpp"
#include "ACAPI/DesignOptionManager.hpp"
// Namespace-Alias statt using-Direktive (DesignOptionCommands.cpp): die DesignOptions-Namen
// (DesignOption, ...) sollen nicht mit den FindElements-Namen kollidieren.
namespace ELMDesignOptions = ACAPI::DesignOptions;
#endif

namespace FindElementsELM {

namespace {

const double kTolerance = 1e-6;

// ---- Text-/Zahl-Helfer ----------------------------------------------------------------------

std::string ToUtf8 (const GS::UniString& s)
{
    return std::string (s.ToCStr (CC_UTF8).Get ());
}

GS::UniString FromUtf8 (const std::string& s)
{
    return GS::UniString (s.c_str (), (USize) s.size (), CC_UTF8);
}

std::string TrimAscii (const std::string& s)
{
    const size_t begin = s.find_first_not_of (" \t");
    if (begin == std::string::npos) {
        return std::string ();
    }
    const size_t end = s.find_last_not_of (" \t");
    return s.substr (begin, end - begin + 1);
}

// Zahl aus Text, unabhaengig vom Gebietsschema (Archicad laeuft mit deutschem Locale);
// Dezimalkomma wird akzeptiert.
bool ParseNumberText (const GS::UniString& text, double& out)
{
    std::string s = TrimAscii (ToUtf8 (text));
    if (s.empty ()) {
        return false;
    }
    for (char& c : s) {
        if (c == ',') {
            c = '.';
        }
    }
    std::istringstream stream (s);
    stream.imbue (std::locale::classic ());
    double value = 0.0;
    stream >> value;
    if (stream.fail ()) {
        return false;
    }
    stream >> std::ws;
    if (!stream.eof ()) {
        return false;
    }
    out = value;
    return true;
}

GS::UniString NumberText (double value)
{
    std::ostringstream stream;
    stream.imbue (std::locale::classic ());
    stream.precision (15);
    stream << value;
    return GS::UniString (stream.str ().c_str ());
}

std::string LowerAscii (const char* s)
{
    std::string result (s);
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') {
            c = (char) (c - 'A' + 'a');
        }
    }
    return result;
}

GS::UniString UniFromUtf8Literal (const char* bytes, USize length)
{
    return GS::UniString (bytes, length, CC_UTF8);
}

// ---- Vergleich -------------------------------------------------------------------------------

enum class Op { Eq, Ne, Contains, StartsWith, EndsWith, Regex, Lt, Le, Gt, Ge, Between, In, HasValue, IsEmpty };

struct OpName {
    const char* name;
    Op          op;
};

const OpName kOpNames[] = {
    { "=",          Op::Eq },
    { "==",         Op::Eq },
    { "!=",         Op::Ne },
    { "<>",         Op::Ne },
    { "contains",   Op::Contains },
    { "startswith", Op::StartsWith },
    { "endswith",   Op::EndsWith },
    { "regex",      Op::Regex },
    { "<",          Op::Lt },
    { "<=",         Op::Le },
    { ">",          Op::Gt },
    { ">=",         Op::Ge },
    { "between",    Op::Between },
    { "in",         Op::In },
    { "hasvalue",   Op::HasValue },
    { "isempty",    Op::IsEmpty }
};

const char* const kOpListText = "=, !=, contains, startsWith, endsWith, regex, <, <=, >, >=, between, in, hasValue, isEmpty (auch Unicode-Formen fuer ungleich/kleiner-gleich/groesser-gleich)";

bool ParseOp (const GS::UniString& raw, Op& op)
{
    // Unicode-Formen als UTF-8-Bytes, damit die Quelle ASCII bleibt.
    if (raw == UniFromUtf8Literal ("\xE2\x89\xA0", 3)) {   // ungleich
        op = Op::Ne;
        return true;
    }
    if (raw == UniFromUtf8Literal ("\xE2\x89\xA4", 3)) {   // kleiner-gleich
        op = Op::Le;
        return true;
    }
    if (raw == UniFromUtf8Literal ("\xE2\x89\xA5", 3)) {   // groesser-gleich
        op = Op::Ge;
        return true;
    }
    const GS::UniString lower = raw.ToLowerCase ();
    for (const OpName& entry : kOpNames) {
        if (lower == GS::UniString (entry.name)) {
            op = entry.op;
            return true;
        }
    }
    return false;
}

bool IsTextOp (Op op)
{
    return op == Op::Contains || op == Op::StartsWith || op == Op::EndsWith || op == Op::Regex;
}

bool IsNumberOp (Op op)
{
    return op == Op::Lt || op == Op::Le || op == Op::Gt || op == Op::Ge || op == Op::Between;
}

struct CmpValue {
    bool          isNumber = false;
    bool          isBool = false;
    bool          isText = false;
    double        number = 0.0;
    bool          boolValue = false;
    GS::UniString text;
    GS::UniString textLower;
};

struct Comparison {
    Op                          op = Op::HasValue;
    GS::UniString               opName;
    std::vector<CmpValue>       values;
    bool                        caseSensitive = false;
    std::shared_ptr<std::regex> regex;
};

void SetText (CmpValue& cv, const GS::UniString& text)
{
    cv.isNumber = false;
    cv.isBool = false;
    cv.isText = true;
    cv.text = text;
    cv.textLower = text.ToLowerCase ();
}

void SetNumber (CmpValue& cv, double number)
{
    cv.isNumber = true;
    cv.isBool = false;
    cv.isText = false;
    cv.number = number;
}

bool ReadScalar (const GS::ObjectState& os, const GS::String& key, CmpValue& cv)
{
    if (!os.Contains (key)) {
        return false;
    }
    const GS::ObjectState::Type type = os.GetType (key);
    if (type == GS::ObjectState::String) {
        GS::UniString text;
        if (!os.Get (key, text)) {
            return false;
        }
        SetText (cv, text);
        return true;
    }
    if (type == GS::ObjectState::Bool) {
        bool b = false;
        if (!os.Get (key, b)) {
            return false;
        }
        cv.isBool = true;
        cv.boolValue = b;
        return true;
    }
    double number = 0.0;
    if (ReadNumber (os, key, number)) {
        SetNumber (cv, number);
        return true;
    }
    return false;
}

bool ReadValueList (const GS::ObjectState& os, const GS::String& key, std::vector<CmpValue>& values)
{
    values.clear ();
    if (!os.Contains (key) || os.GetType (key) != GS::ObjectState::List) {
        return false;
    }
    GS::Array<GS::UniString> texts;
    if (os.Get (key, texts)) {
        for (const GS::UniString& t : texts) {
            CmpValue cv;
            SetText (cv, t);
            values.push_back (cv);
        }
        return true;
    }
    std::vector<double> numbers;
    if (ReadNumberList (os, key, numbers)) {
        for (const double n : numbers) {
            CmpValue cv;
            SetNumber (cv, n);
            values.push_back (cv);
        }
        return true;
    }
    GS::Array<bool> bools;
    if (os.Get (key, bools)) {
        for (const bool b : bools) {
            CmpValue cv;
            cv.isBool = true;
            cv.boolValue = b;
            values.push_back (cv);
        }
        return true;
    }
    return false;
}

// Liest op/value/caseSensitive (Geschwister des Kriteriums) und normalisiert die Werte:
// Text-Operatoren bekommen Text, Zahl-Operatoren Zahlen. Regex wird hier kompiliert.
bool ParseComparison (const GS::ObjectState& nodeOs, const GS::UniString& path, Comparison& cmp, GS::UniString& error)
{
    if (nodeOs.Contains ("caseSensitive")) {
        if (nodeOs.GetType ("caseSensitive") != GS::ObjectState::Bool) {
            error = path + GS::UniString (".caseSensitive: erwartet true/false, erhalten: ") + TypeNameOf (nodeOs, "caseSensitive");
            return false;
        }
        nodeOs.Get ("caseSensitive", cmp.caseSensitive);
    }
    const bool hasValue = nodeOs.Contains ("value");
    GS::UniString opRaw;
    if (nodeOs.Contains ("op")) {
        if (nodeOs.GetType ("op") != GS::ObjectState::String || !nodeOs.Get ("op", opRaw)) {
            error = path + GS::UniString (".op: erwartet Text, erhalten: ") + TypeNameOf (nodeOs, "op");
            return false;
        }
    } else {
        opRaw = GS::UniString (hasValue ? "=" : "hasValue");
    }
    if (!ParseOp (opRaw, cmp.op)) {
        error = path + GS::UniString (".op: unbekannter Operator '") + opRaw + GS::UniString ("' - erlaubt: ") + GS::UniString (kOpListText);
        return false;
    }
    cmp.opName = opRaw;

    if (cmp.op == Op::HasValue || cmp.op == Op::IsEmpty) {
        if (hasValue) {
            error = path + GS::UniString (".value: bei '") + opRaw + GS::UniString ("' nicht erlaubt");
            return false;
        }
        return true;
    }
    if (!hasValue) {
        error = path + GS::UniString (".value: fehlt (Operator '") + opRaw + GS::UniString ("')");
        return false;
    }

    if (cmp.op == Op::Between) {
        std::vector<double> numbers;
        if (!ReadNumberList (nodeOs, "value", numbers) || numbers.size () != 2) {
            error = path + GS::UniString (".value: 'between' erwartet [a, b] mit zwei Zahlen, erhalten: ") + TypeNameOf (nodeOs, "value");
            return false;
        }
        if (numbers[0] > numbers[1]) {
            error = path + GS::UniString (".value: 'between' erwartet a <= b, erhalten [") + NumberText (numbers[0]) +
                    GS::UniString (", ") + NumberText (numbers[1]) + GS::UniString ("]");
            return false;
        }
        for (const double n : numbers) {
            CmpValue cv;
            SetNumber (cv, n);
            cmp.values.push_back (cv);
        }
        return true;
    }
    if (cmp.op == Op::In) {
        if (!ReadValueList (nodeOs, "value", cmp.values) || cmp.values.empty ()) {
            error = path + GS::UniString (".value: 'in' erwartet eine nicht-leere, einheitliche Liste (Texte, Zahlen oder Wahrheitswerte), erhalten: ") + TypeNameOf (nodeOs, "value");
            return false;
        }
        return true;
    }

    CmpValue cv;
    if (!ReadScalar (nodeOs, "value", cv)) {
        error = path + GS::UniString (".value: erwartet Text, Zahl oder Wahrheitswert, erhalten: ") + TypeNameOf (nodeOs, "value");
        return false;
    }
    if (IsTextOp (cmp.op)) {
        if (cv.isNumber) {
            SetText (cv, NumberText (cv.number));
        } else if (cv.isBool) {
            error = path + GS::UniString (".value: Operator '") + opRaw + GS::UniString ("' erwartet Text");
            return false;
        }
        if (cmp.op == Op::Regex) {
            try {
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (!cmp.caseSensitive) {
                    flags |= std::regex::icase;
                }
                cmp.regex = std::make_shared<std::regex> (ToUtf8 (cv.text), flags);
            } catch (const std::exception& e) {
                error = path + GS::UniString (".value: ungueltiger regulaerer Ausdruck '") + cv.text +
                        GS::UniString ("' (") + GS::UniString (e.what ()) + GS::UniString (")");
                return false;
            }
        }
    } else if (IsNumberOp (cmp.op)) {
        if (cv.isText) {
            double number = 0.0;
            if (!ParseNumberText (cv.text, number)) {
                error = path + GS::UniString (".value: Operator '") + opRaw + GS::UniString ("' erwartet Zahl, erhalten '") + cv.text + GS::UniString ("'");
                return false;
            }
            SetNumber (cv, number);
        } else if (cv.isBool) {
            error = path + GS::UniString (".value: Operator '") + opRaw + GS::UniString ("' erwartet Zahl");
            return false;
        }
    }
    cmp.values.push_back (cv);
    return true;
}

// ---- Werte eines Elements --------------------------------------------------------------------

struct ValItem {
    bool                       hasNumber = false;
    double                     number = 0.0;
    bool                       hasBool = false;
    bool                       boolValue = false;
    std::vector<GS::UniString> texts;      // Aufzaehlung: [nonLocalizedValue,] Anzeige
};

enum class ValStatus { NotAvailable, Undefined, Normal, TypeMismatch };

struct ExtractedValue {
    ValStatus            status = ValStatus::NotAvailable;
    std::vector<ValItem> items;
    bool                 isEnum = false;
    bool                 isList = false;
    GS::UniString        display;          // nur fuer return-Spalten gefuellt
};

enum class MatchResult { No, Yes, Mismatch };

bool IsEmptyValue (const ExtractedValue& v)
{
    if (v.status == ValStatus::Undefined || v.items.empty ()) {
        return true;
    }
    for (const ValItem& item : v.items) {
        if (item.hasNumber || item.hasBool) {
            return false;
        }
        for (const GS::UniString& t : item.texts) {
            if (!t.IsEmpty ()) {
                return false;
            }
        }
    }
    return true;
}

bool BoolFromText (const GS::UniString& text, bool& out)
{
    const GS::UniString lower = text.ToLowerCase ();
    if (lower == GS::UniString ("true") || lower == GS::UniString ("wahr") || lower == GS::UniString ("on") || lower == GS::UniString ("1")) {
        out = true;
        return true;
    }
    if (lower == GS::UniString ("false") || lower == GS::UniString ("falsch") || lower == GS::UniString ("off") || lower == GS::UniString ("0")) {
        out = false;
        return true;
    }
    return false;
}

bool NumberOfItem (const ValItem& item, bool parseText, double& out)
{
    if (item.hasNumber) {
        out = item.number;
        return true;
    }
    if (parseText && !item.texts.empty ()) {
        return ParseNumberText (item.texts[0], out);
    }
    return false;
}

MatchResult ItemEquals (const ValItem& item, const CmpValue& cv, bool caseSensitive, bool parseText)
{
    if (cv.isNumber) {
        double x = 0.0;
        if (NumberOfItem (item, parseText, x)) {
            return (std::fabs (x - cv.number) <= kTolerance) ? MatchResult::Yes : MatchResult::No;
        }
        if (item.hasBool) {
            return ((item.boolValue ? 1.0 : 0.0) == cv.number) ? MatchResult::Yes : MatchResult::No;
        }
        return MatchResult::Mismatch;
    }
    if (cv.isBool) {
        if (item.hasBool) {
            return (item.boolValue == cv.boolValue) ? MatchResult::Yes : MatchResult::No;
        }
        if (item.hasNumber) {
            return ((item.number != 0.0) == cv.boolValue) ? MatchResult::Yes : MatchResult::No;
        }
        for (const GS::UniString& t : item.texts) {
            bool b = false;
            if (BoolFromText (t, b)) {
                return (b == cv.boolValue) ? MatchResult::Yes : MatchResult::No;
            }
        }
        return MatchResult::Mismatch;
    }
    // Text
    if (!item.texts.empty ()) {
        for (const GS::UniString& t : item.texts) {
            const bool equal = caseSensitive ? (t == cv.text) : (t.ToLowerCase () == cv.textLower);
            if (equal) {
                return MatchResult::Yes;
            }
        }
        return MatchResult::No;
    }
    if (item.hasNumber) {
        double d = 0.0;
        if (ParseNumberText (cv.text, d)) {
            return (std::fabs (item.number - d) <= kTolerance) ? MatchResult::Yes : MatchResult::No;
        }
        return MatchResult::Mismatch;
    }
    if (item.hasBool) {
        bool b = false;
        if (BoolFromText (cv.text, b)) {
            return (b == item.boolValue) ? MatchResult::Yes : MatchResult::No;
        }
        return MatchResult::Mismatch;
    }
    return MatchResult::No;
}

MatchResult ItemTextOp (EvalContext& ctx, const ValItem& item, const Comparison& cmp)
{
    if (item.texts.empty ()) {
        return MatchResult::Mismatch;
    }
    const CmpValue& cv = cmp.values[0];
    for (const GS::UniString& t : item.texts) {
        bool hit = false;
        switch (cmp.op) {
            case Op::Contains:
                hit = cmp.caseSensitive ? t.Contains (cv.text) : t.ToLowerCase ().Contains (cv.textLower);
                break;
            case Op::StartsWith:
                hit = cmp.caseSensitive ? t.BeginsWith (cv.text) : t.ToLowerCase ().BeginsWith (cv.textLower);
                break;
            case Op::EndsWith:
                hit = cmp.caseSensitive ? t.EndsWith (cv.text) : t.ToLowerCase ().EndsWith (cv.textLower);
                break;
            case Op::Regex:
                try {
                    hit = cmp.regex != nullptr && std::regex_search (ToUtf8 (t), *cmp.regex);
                } catch (const std::exception& e) {
                    if (ctx.fatalError.IsEmpty ()) {
                        ctx.fatalError = GS::UniString ("regex: Auswertung abgebrochen (") + GS::UniString (e.what ()) + GS::UniString (")");
                        ctx.fatalCode = APIERR_GENERAL;
                    }
                    return MatchResult::No;
                }
                break;
            default:
                break;
        }
        if (hit) {
            return MatchResult::Yes;
        }
    }
    return MatchResult::No;
}

MatchResult ItemNumberOp (const ValItem& item, const Comparison& cmp, bool parseText)
{
    double x = 0.0;
    if (!NumberOfItem (item, parseText, x)) {
        return MatchResult::Mismatch;
    }
    const double v = cmp.values[0].number;
    bool hit = false;
    switch (cmp.op) {
        case Op::Lt:      hit = x < v - kTolerance; break;
        case Op::Le:      hit = x <= v + kTolerance; break;
        case Op::Gt:      hit = x > v + kTolerance; break;
        case Op::Ge:      hit = x >= v - kTolerance; break;
        case Op::Between: hit = x >= v - kTolerance && x <= cmp.values[1].number + kTolerance; break;
        default:          break;
    }
    return hit ? MatchResult::Yes : MatchResult::No;
}

// Vergleich eines (verfuegbaren) Werts. Listen/Mehrfachauswahl: irgendein Eintrag passt;
// "!=" = keiner gleich. Mismatch nur, wenn KEIN Eintrag typgerecht vergleichbar war.
MatchResult MatchValue (EvalContext& ctx, const ExtractedValue& v, const Comparison& cmp, bool parseText)
{
    const bool empty = IsEmptyValue (v);
    if (cmp.op == Op::HasValue) {
        return empty ? MatchResult::No : MatchResult::Yes;
    }
    if (cmp.op == Op::IsEmpty) {
        return empty ? MatchResult::Yes : MatchResult::No;
    }
    if (empty) {
        return (cmp.op == Op::Ne) ? MatchResult::Yes : MatchResult::No;
    }
    bool anyYes = false;
    bool anyNo = false;
    for (const ValItem& item : v.items) {
        if (cmp.op == Op::Eq || cmp.op == Op::Ne || cmp.op == Op::In) {
            for (const CmpValue& cv : cmp.values) {
                const MatchResult r = ItemEquals (item, cv, cmp.caseSensitive, parseText);
                if (r == MatchResult::Yes) {
                    anyYes = true;
                } else if (r == MatchResult::No) {
                    anyNo = true;
                }
            }
        } else {
            const MatchResult r = IsTextOp (cmp.op) ? ItemTextOp (ctx, item, cmp) : ItemNumberOp (item, cmp, parseText);
            if (r == MatchResult::Yes) {
                anyYes = true;
            } else if (r == MatchResult::No) {
                anyNo = true;
            }
        }
    }
    if (cmp.op == Op::Ne) {
        if (anyYes) {
            return MatchResult::No;
        }
        return anyNo ? MatchResult::Yes : MatchResult::Mismatch;
    }
    if (anyYes) {
        return MatchResult::Yes;
    }
    return anyNo ? MatchResult::No : MatchResult::Mismatch;
}

// ---- Eigenschafts-Index --------------------------------------------------------------------------

struct EnumEntry {
    API_Guid      key = APINULLGuid;
    GS::UniString display;
    GS::UniString nonLocalized;
    bool          hasNonLocalized = false;
    bool          displayIsNumber = false;
    double        displayNumber = 0.0;
};

struct PropDef {
    API_Guid                   guid = APINULLGuid;
    GS::UniString              group;
    GS::UniString              name;
    GS::UniString              groupLower;
    GS::UniString              nameLower;
    API_PropertyCollectionType collectionType = API_PropertyUndefinedCollectionType;
    API_VariantType            valueType = API_PropertyUndefinedValueType;
    std::vector<EnumEntry>     enums;      // nur bei Aufzaehlungen (possibleEnumValues)
};

struct ReturnColumn {
    GS::UniString requested;
    UInt32        slot = 0;
};

struct IfcEntry {
    GSErrCode               err = NoError;
    GS::UniString           type;
    GS::Array<ELMIFCText>   properties;
    GS::Array<ELMIFCText>   attributes;
};

struct BoundsEntry {
    bool      ok = false;
    API_Box3D box = {};
};

struct HostEntry {
    bool     applicable = false;
    API_Guid host = APINULLGuid;
};

struct OpeningsEntry {
    bool                  applicable = false;
    std::vector<API_Guid> openings;
};

// ---- Blatt-Daten ---------------------------------------------------------------------------------

struct PropertyLeaf : LeafData {
    UInt32     slot = 0;
    Comparison cmp;
};

struct ClassificationLeaf : LeafData {
    API_Guid              systemGuid = APINULLGuid;
    GS::HashSet<API_Guid> items;
    bool                  unclassified = false;
};

struct LibPartLeaf : LeafData {
    bool          usePattern = false;
    GS::UniString nameLower;
    GS::UniString pattern;
};

struct GdlLeaf : LeafData {
    UInt32     slot = 0;
    Comparison cmp;
};

enum class IfcMode { Type, Property, Attribute };

struct IfcLeaf : LeafData {
    IfcMode                    mode = IfcMode::Type;
    std::vector<GS::UniString> typesLower;
    GS::UniString              psetLower;
    GS::UniString              nameLower;
    Comparison                 cmp;
};

struct BoxLeaf : LeafData {
    double minX = 0.0, minY = 0.0, minZ = 0.0;
    double maxX = 0.0, maxY = 0.0, maxZ = 0.0;
    bool   hasMinZ = false;
    bool   hasMaxZ = false;
    bool   inside = false;
};

struct NearLeaf : LeafData {
    double x = 0.0, y = 0.0, z = 0.0;
    bool   hasZ = false;
    double radius = 0.0;
};

#ifdef ServerMainVers_2900
struct DesignOptionLeaf : LeafData {
    bool                  mainModel = false;
    std::vector<GS::Guid> options;
};
#endif

}   // namespace

// ---- Zustand (Vertrag: Vorwaertsdeklaration in FindElementsInternal.hpp) -----------------------

struct ValueLeafState {
    // Eigenschafts-Index (einmal, beim ersten Bedarf)
    bool                                                  propIndexLoaded = false;
    std::vector<PropDef>                                  propDefs;
    GS::HashTable<API_Guid, UInt32>                       propDefByGuid;
    // gebuendelte Eigenschaften (Blaetter + return, Parse-Reihenfolge, ohne Doppelte)
    GS::Array<API_Guid>                                   neededProps;
    std::vector<UInt32>                                   neededDef;       // Slot -> propDefs-Index
    std::vector<bool>                                     neededDisplay;   // Slot braucht Anzeige-String (return)
    GS::HashTable<API_Guid, UInt32>                       slotByGuid;
    GS::HashTable<API_Guid, std::vector<ExtractedValue>>  propCache;       // Element -> Werte je Slot
    std::vector<ReturnColumn>                             returnColumns;
    // gdl
    std::vector<std::string>                              gdlNamesLower;
    GS::HashTable<API_Guid, std::vector<ExtractedValue>>  gdlCache;
    // ifc
    bool                                                  ifcWantProperties = false;
    bool                                                  ifcWantAttributes = false;
    GS::HashTable<API_Guid, IfcEntry>                     ifcCache;
    // Klassifizierung: System -> (Element -> Eintrag, APINULLGuid = keiner)
    GS::HashTable<API_Guid, GS::HashTable<API_Guid, API_Guid>> classCache;
    // Bibliotheksteil
    GS::HashTable<API_Guid, Int32>                        libIndCache;
    std::map<Int32, GS::UniString>                        libNameCache;
    // Bounds, Wirt, Oeffnungen
    GS::HashTable<API_Guid, BoundsEntry>                  boundsCache;
    GS::HashTable<API_Guid, HostEntry>                    hostCache;
    bool                                                  openingMapBuilt = false;
    GS::HashTable<API_Guid, API_Guid>                     openingToHost;
    GS::HashTable<API_Guid, OpeningsEntry>                openingsCache;
    // Zonen-Mitglieder je inZone-Knoten (statsIndex)
    std::map<UInt32, GS::HashSet<API_Guid>>               zoneMembers;
};

namespace {

// ---- Eigenschaften: Index, Adresse, Slots ---------------------------------------------------------

bool IsEnumCollection (API_PropertyCollectionType type)
{
    return type == API_PropertySingleChoiceEnumerationCollectionType || type == API_PropertyMultipleChoiceEnumerationCollectionType;
}

void EnsurePropertyIndex (ValueLeafState& vs)
{
    if (vs.propIndexLoaded) {
        return;
    }
    vs.propIndexLoaded = true;
    GS::Array<API_PropertyGroup> groups;
    ACAPI_Property_GetPropertyGroups (groups);
    for (const API_PropertyGroup& group : groups) {
        GS::Array<API_PropertyDefinition> definitions;
        if (ACAPI_Property_GetPropertyDefinitions (group.guid, definitions) != NoError) {
            continue;
        }
        for (const API_PropertyDefinition& definition : definitions) {
            if (vs.propDefByGuid.ContainsKey (definition.guid)) {
                continue;
            }
            PropDef def;
            def.guid = definition.guid;
            def.group = group.name;
            def.name = definition.name;
            def.groupLower = group.name.ToLowerCase ();
            def.nameLower = definition.name.ToLowerCase ();
            def.collectionType = definition.collectionType;
            def.valueType = definition.valueType;
            if (IsEnumCollection (definition.collectionType)) {
                for (const API_SingleEnumerationVariant& variant : definition.possibleEnumValues) {
                    EnumEntry entry;
                    entry.key = variant.keyVariant.guidValue;
                    entry.display = variant.displayVariant.uniStringValue;
                    entry.hasNonLocalized = variant.nonLocalizedValue.HasValue ();
                    if (entry.hasNonLocalized) {
                        entry.nonLocalized = *variant.nonLocalizedValue;
                    }
                    if (variant.displayVariant.type == API_PropertyIntegerValueType) {
                        entry.displayIsNumber = true;
                        entry.displayNumber = (double) variant.displayVariant.intValue;
                    } else if (variant.displayVariant.type == API_PropertyRealValueType) {
                        entry.displayIsNumber = true;
                        entry.displayNumber = variant.displayVariant.doubleValue;
                    }
                    def.enums.push_back (entry);
                }
            }
            vs.propDefByGuid.Add (definition.guid, (UInt32) vs.propDefs.size ());
            vs.propDefs.push_back (def);
        }
    }
}

GS::UniString DefLabel (const PropDef& def)
{
    return def.group + GS::UniString ("/") + def.name;
}

GS::UniString DefLabelWithGuid (const PropDef& def)
{
    return DefLabel (def) + GS::UniString (" {") + APIGuidToString (def.guid) + GS::UniString ("}");
}

void AddUnique (std::vector<UInt32>& list, UInt32 value)
{
    for (const UInt32 v : list) {
        if (v == value) {
            return;
        }
    }
    list.push_back (value);
}

bool FinishPropertyMatch (const ValueLeafState& vs, const std::vector<UInt32>& found, const GS::UniString& text,
                          const GS::UniString& needleLower, const GS::UniString& path, UInt32& defIndex, GS::UniString& error)
{
    if (found.size () == 1) {
        defIndex = found[0];
        return true;
    }
    if (found.empty ()) {
        std::vector<GS::UniString> suggestions;
        for (const PropDef& def : vs.propDefs) {
            if (suggestions.size () >= 10) {
                break;
            }
            if (!needleLower.IsEmpty () && def.nameLower.Contains (needleLower)) {
                suggestions.push_back (DefLabel (def));
            }
        }
        error = path + GS::UniString (": unbekannte Eigenschaft '") + text + GS::UniString ("'");
        if (suggestions.empty ()) {
            error += GS::UniString (" - keine aehnliche Eigenschaft gefunden");
        } else {
            error += GS::UniString (" - aehnlich: ") + JoinNames (suggestions, 10);
        }
        return false;
    }
    std::vector<GS::UniString> candidates;
    for (const UInt32 index : found) {
        candidates.push_back (DefLabelWithGuid (vs.propDefs[index]));
    }
    error = path + GS::UniString (": Eigenschaft '") + text + GS::UniString ("' mehrdeutig: ") + JoinNames (candidates, 20) +
            GS::UniString (" - GUID oder {group, name} angeben");
    return false;
}

// "Name", "Gruppe/Name" (jede "/"-Position wird probiert — Gruppen- und Eigenschaftsnamen
// enthalten selbst "/", z. B. "Balken / Traeger") oder GUID-String.
bool ResolvePropertyString (ValueLeafState& vs, const GS::UniString& text, const GS::UniString& path, UInt32& defIndex, GS::UniString& error)
{
    EnsurePropertyIndex (vs);
    if (IsGuidString (text)) {
        const API_Guid guid = APIGuidFromString (text.ToCStr ().Get ());
        if (!vs.propDefByGuid.ContainsKey (guid)) {
            error = path + GS::UniString (": Eigenschaft {") + text + GS::UniString ("} nicht im Projekt");
            return false;
        }
        defIndex = vs.propDefByGuid.Get (guid);
        return true;
    }
    const GS::UniString wholeLower = text.ToLowerCase ();
    std::vector<UInt32> found;
    for (UInt32 i = 0; i < (UInt32) vs.propDefs.size (); ++i) {
        if (vs.propDefs[i].nameLower == wholeLower) {
            AddUnique (found, i);
        }
    }
    const std::string utf8 = ToUtf8 (text);
    GS::UniString lastNameLower = wholeLower;
    for (size_t pos = 0; pos < utf8.size (); ++pos) {
        if (utf8[pos] != '/') {
            continue;
        }
        const GS::UniString groupLower = FromUtf8 (TrimAscii (utf8.substr (0, pos))).ToLowerCase ();
        const GS::UniString nameLower = FromUtf8 (TrimAscii (utf8.substr (pos + 1))).ToLowerCase ();
        lastNameLower = nameLower;
        for (UInt32 i = 0; i < (UInt32) vs.propDefs.size (); ++i) {
            if (vs.propDefs[i].groupLower == groupLower && vs.propDefs[i].nameLower == nameLower) {
                AddUnique (found, i);
            }
        }
    }
    return FinishPropertyMatch (vs, found, text, found.empty () ? lastNameLower : wholeLower, path, defIndex, error);
}

bool ResolvePropertyObject (ValueLeafState& vs, const GS::ObjectState& os, const GS::UniString& path,
                            UInt32& defIndex, GS::UniString& requested, GS::UniString& error)
{
    EnsurePropertyIndex (vs);
    GS::UniString guidText;
    if (os.Contains ("guid")) {
        if (os.GetType ("guid") != GS::ObjectState::String || !os.Get ("guid", guidText) || !IsGuidString (guidText)) {
            error = path + GS::UniString (".guid: erwartet GUID-Text");
            return false;
        }
        requested = guidText;
        return ResolvePropertyString (vs, guidText, path, defIndex, error);
    }
    GS::UniString group;
    GS::UniString name;
    if (os.GetType ("group") != GS::ObjectState::String || os.GetType ("name") != GS::ObjectState::String ||
        !os.Get ("group", group) || !os.Get ("name", name)) {
        error = path + GS::UniString (": erwartet {guid} oder {group, name}");
        return false;
    }
    requested = group + GS::UniString ("/") + name;
    const GS::UniString groupLower = group.ToLowerCase ();
    const GS::UniString nameLower = name.ToLowerCase ();
    std::vector<UInt32> found;
    for (UInt32 i = 0; i < (UInt32) vs.propDefs.size (); ++i) {
        if (vs.propDefs[i].groupLower == groupLower && vs.propDefs[i].nameLower == nameLower) {
            AddUnique (found, i);
        }
    }
    return FinishPropertyMatch (vs, found, requested, nameLower, path, defIndex, error);
}

bool ResolvePropertyField (ValueLeafState& vs, const GS::ObjectState& holder, const GS::String& key, const GS::UniString& path,
                           UInt32& defIndex, GS::UniString& requested, GS::UniString& error)
{
    if (!holder.Contains (key)) {
        error = path + GS::UniString (": Eigenschaft fehlt");
        return false;
    }
    const GS::ObjectState::Type type = holder.GetType (key);
    if (type == GS::ObjectState::String) {
        holder.Get (key, requested);
        return ResolvePropertyString (vs, requested, path, defIndex, error);
    }
    const GS::ObjectState* os = (type == GS::ObjectState::Object) ? holder.Get (key) : nullptr;
    if (os == nullptr) {
        error = path + GS::UniString (": erwartet \"Name\", \"Gruppe/Name\", GUID, {guid} oder {group, name}, erhalten: ") + TypeNameOf (holder, key);
        return false;
    }
    return ResolvePropertyObject (vs, *os, path, defIndex, requested, error);
}

UInt32 SlotFor (ValueLeafState& vs, UInt32 defIndex, bool display)
{
    const API_Guid guid = vs.propDefs[defIndex].guid;
    if (vs.slotByGuid.ContainsKey (guid)) {
        const UInt32 slot = vs.slotByGuid.Get (guid);
        if (display) {
            vs.neededDisplay[slot] = true;
        }
        return slot;
    }
    const UInt32 slot = (UInt32) vs.neededDef.size ();
    vs.neededProps.Push (guid);
    vs.neededDef.push_back (defIndex);
    vs.neededDisplay.push_back (display);
    vs.slotByGuid.Add (guid, slot);
    return slot;
}

enum class PropKind { Number, Text, Enum, Bool, Unknown };

PropKind KindOfDef (const PropDef& def)
{
    if (IsEnumCollection (def.collectionType)) {
        return PropKind::Enum;
    }
    switch (def.valueType) {
        case API_PropertyIntegerValueType:
        case API_PropertyRealValueType:
            return PropKind::Number;
        case API_PropertyStringValueType:
        case API_PropertyGuidValueType:
            return PropKind::Text;
        case API_PropertyBooleanValueType:
            return PropKind::Bool;
        default:
            return PropKind::Unknown;
    }
}

GS::UniString KindName (PropKind kind)
{
    switch (kind) {
        case PropKind::Number: return GS::UniString ("Zahl");
        case PropKind::Text:   return GS::UniString ("Text");
        case PropKind::Enum:   return GS::UniString ("Aufzaehlung");
        case PropKind::Bool:   return GS::UniString ("Wahrheitswert");
        default:               return GS::UniString ("unbekannt");
    }
}

// Beim Parsen: Operator passt zum Definitionstyp? Werte in den Typ der Eigenschaft bringen.
bool CheckPropertyKind (const PropDef& def, Comparison& cmp, const GS::UniString& path, GS::UniString& error)
{
    const PropKind kind = KindOfDef (def);
    if (kind == PropKind::Unknown) {
        return true;
    }
    const GS::UniString typeText = GS::UniString (" (Eigenschaft '") + DefLabel (def) + GS::UniString ("' ist ") + KindName (kind) + GS::UniString (")");
    if (kind == PropKind::Number && IsTextOp (cmp.op)) {
        error = path + GS::UniString (": Text-Operator '") + cmp.opName + GS::UniString ("' passt nicht") + typeText;
        return false;
    }
    if ((kind == PropKind::Text || kind == PropKind::Enum) && IsNumberOp (cmp.op)) {
        error = path + GS::UniString (": Zahl-Operator '") + cmp.opName + GS::UniString ("' passt nicht") + typeText;
        return false;
    }
    if (kind == PropKind::Bool && cmp.op != Op::Eq && cmp.op != Op::Ne && cmp.op != Op::HasValue && cmp.op != Op::IsEmpty) {
        error = path + GS::UniString (": Operator '") + cmp.opName + GS::UniString ("' passt nicht - erlaubt =, !=, hasValue, isEmpty") + typeText;
        return false;
    }
    for (CmpValue& cv : cmp.values) {
        if (kind == PropKind::Number) {
            if (cv.isText) {
                double number = 0.0;
                if (!ParseNumberText (cv.text, number)) {
                    error = path + GS::UniString (".value: '") + cv.text + GS::UniString ("' ist keine Zahl") + typeText;
                    return false;
                }
                SetNumber (cv, number);
            } else if (cv.isBool) {
                error = path + GS::UniString (".value: Wahrheitswert passt nicht") + typeText;
                return false;
            }
        } else if (kind == PropKind::Text || kind == PropKind::Enum) {
            if (cv.isNumber) {
                SetText (cv, NumberText (cv.number));
            } else if (cv.isBool) {
                error = path + GS::UniString (".value: Wahrheitswert passt nicht") + typeText;
                return false;
            }
        } else if (kind == PropKind::Bool) {
            if (cv.isText) {
                bool b = false;
                if (!BoolFromText (cv.text, b)) {
                    error = path + GS::UniString (".value: '") + cv.text + GS::UniString ("' ist kein Wahrheitswert") + typeText;
                    return false;
                }
                cv.isText = false;
                cv.isBool = true;
                cv.boolValue = b;
            } else if (cv.isNumber) {
                error = path + GS::UniString (".value: Zahl passt nicht") + typeText;
                return false;
            }
        }
    }
    return true;
}

// ---- Eigenschaften: Werte lesen ----------------------------------------------------------------

ValItem ItemFromVariant (const API_Variant& variant, API_VariantType fallbackType)
{
    ValItem item;
    const API_VariantType type = (variant.type != API_PropertyUndefinedValueType) ? variant.type : fallbackType;
    switch (type) {
        case API_PropertyIntegerValueType:
            item.hasNumber = true;
            item.number = (double) variant.intValue;
            break;
        case API_PropertyRealValueType:
            item.hasNumber = true;
            item.number = variant.doubleValue;
            break;
        case API_PropertyStringValueType:
            item.texts.push_back (variant.uniStringValue);
            break;
        case API_PropertyBooleanValueType:
            item.hasBool = true;
            item.boolValue = variant.boolValue;
            break;
        case API_PropertyGuidValueType:
            item.texts.push_back (APIGuidToString (variant.guidValue));
            break;
        default:
            break;
    }
    return item;
}

// Aufzaehlung: Schluessel (keyVariant.guidValue) -> Textformen nonLocalizedValue UND Anzeige.
ValItem ItemFromEnumKey (const PropDef& def, const API_PropertyDefinition& fetched, const API_Guid& key)
{
    ValItem item;
    for (const EnumEntry& entry : def.enums) {
        if (entry.key == key) {
            if (entry.hasNonLocalized) {
                item.texts.push_back (entry.nonLocalized);
            }
            item.texts.push_back (entry.display);
            if (entry.displayIsNumber) {
                item.hasNumber = true;
                item.number = entry.displayNumber;
            }
            return item;
        }
    }
    for (const API_SingleEnumerationVariant& variant : fetched.possibleEnumValues) {
        if (variant.keyVariant.guidValue == key) {
            if (variant.nonLocalizedValue.HasValue ()) {
                item.texts.push_back (*variant.nonLocalizedValue);
            }
            item.texts.push_back (variant.displayVariant.uniStringValue);
            return item;
        }
    }
    return item;
}

ExtractedValue ExtractProperty (const API_Property& property, const PropDef& def, bool wantDisplay)
{
    ExtractedValue v;
    if (property.status == API_Property_NotAvailable || property.status == API_Property_NotEvaluated) {
        return v;
    }
    if (wantDisplay) {
        GS::UniString display;
        if (ACAPI_Property_GetPropertyValueString (property, &display) == NoError) {
            v.display = display;
        }
    }
    if (property.value.variantStatus != API_VariantStatusNormal) {
        v.status = ValStatus::Undefined;
        return v;
    }
    v.status = ValStatus::Normal;
    switch (property.definition.collectionType) {
        case API_PropertySingleCollectionType:
            v.items.push_back (ItemFromVariant (property.value.singleVariant.variant, property.definition.valueType));
            break;
        case API_PropertyListCollectionType:
            v.isList = true;
            for (const API_Variant& variant : property.value.listVariant.variants) {
                v.items.push_back (ItemFromVariant (variant, property.definition.valueType));
            }
            break;
        case API_PropertySingleChoiceEnumerationCollectionType:
            v.isEnum = true;
            v.items.push_back (ItemFromEnumKey (def, property.definition, property.value.singleVariant.variant.guidValue));
            break;
        case API_PropertyMultipleChoiceEnumerationCollectionType:
            v.isEnum = true;
            v.isList = true;
            for (const API_Variant& variant : property.value.listVariant.variants) {
                v.items.push_back (ItemFromEnumKey (def, property.definition, variant.guidValue));
            }
            break;
        default:
            break;
    }
    return v;
}

// EIN GetPropertyValuesByGuid je noch nicht gelesenem Element mit ALLEN benoetigten Eigenschaften.
void EnsurePropertyValues (ValueLeafState& vs, const GuidList& guids)
{
    for (const API_Guid& guid : guids) {
        if (vs.propCache.ContainsKey (guid)) {
            continue;
        }
        std::vector<ExtractedValue> values (vs.neededDef.size ());
        if (!vs.neededProps.IsEmpty ()) {
            GS::Array<API_Property> properties;
            if (ACAPI_Element_GetPropertyValuesByGuid (guid, vs.neededProps, properties) == NoError) {
                for (const API_Property& property : properties) {
                    if (!vs.slotByGuid.ContainsKey (property.definition.guid)) {
                        continue;
                    }
                    const UInt32 slot = vs.slotByGuid.Get (property.definition.guid);
                    values[slot] = ExtractProperty (property, vs.propDefs[vs.neededDef[slot]], vs.neededDisplay[slot]);
                }
            }
        }
        vs.propCache.Add (guid, values);
    }
}

// ---- Bibliotheksteil -------------------------------------------------------------------------

// Kopie aus InspectionCommands.cpp (anonymer Namespace dort). -1 = nicht bibliotheksteilbasiert.
Int32 LibIndOfElement (const API_Element& e)
{
    switch (GetElemTypeId (e.header)) {
        case API_ObjectID:            return e.object.libInd;
        case API_LampID:              return e.lamp.libInd;
        case API_WindowID:            return e.window.openingBase.libInd;
        case API_DoorID:              return e.door.openingBase.libInd;
        case API_SkylightID:          return e.skylight.openingBase.libInd;
        case API_ZoneID:              return e.zone.libInd;
        case API_LabelID:             return e.label.u.symbol.libInd;
        case API_CurtainWallFrameID:  return e.cwFrame.libInd;
        case API_CurtainWallPanelID:  return e.cwPanel.libInd;
        case API_CurtainWallJunctionID:  return e.cwJunction.libInd;
        case API_CurtainWallAccessoryID: return e.cwAccessory.libInd;
        default:                      return -1;
    }
}

// Muster LibPartNameOf (LabelCommandsELM.cpp): location gehoert dem Aufrufer.
GS::UniString LibPartNameOfIndex (Int32 libInd)
{
    if (libInd <= 0) {
        return GS::UniString ();
    }
    API_LibPart lp = {};
    lp.index = libInd;
    GS::UniString name;
    if (ACAPI_LibraryPart_Get (&lp) == NoError) {
        name = GS::UniString (lp.docu_UName);
        delete lp.location;
        lp.location = nullptr;
    }
    return name;
}

// ---- GDL -----------------------------------------------------------------------------------------

ExtractedValue ExtractGdl (const API_AddParType& p)
{
    ExtractedValue v;
    if (p.typeMod == API_ParArray) {
        v.status = ValStatus::TypeMismatch;
        return v;
    }
    ValItem item;
    switch (p.typeID) {
        case APIParT_Integer:
        case APIParT_Length:
        case APIParT_Angle:
        case APIParT_RealNum:
        case APIParT_ColRGB:
        case APIParT_Intens:
        case APIParT_LineTyp:
        case APIParT_Mater:
        case APIParT_FillPat:
        case APIParT_PenCol:
        case APIParT_BuildingMaterial:
        case APIParT_Profile:
            item.hasNumber = true;
            item.number = p.value.real;
            break;
        case APIParT_LightSw:
        case APIParT_Boolean:
            item.hasNumber = true;
            item.number = p.value.real;
            item.hasBool = true;
            item.boolValue = (p.value.real != 0.0);
            break;
        case APIParT_CString:
        case APIParT_Title:
            item.texts.push_back (GS::UniString (p.value.uStr));
            break;
        default:
            return v;   // Separator, Dictionary: kein vergleichbarer Wert
    }
    v.status = ValStatus::Normal;
    v.items.push_back (item);
    return v;
}

// Memo je Element EINMAL fuer ALLE gdl-Blaetter; nur die benoetigten Parameter werden gecacht.
void EnsureGdlValues (ValueLeafState& vs, const GuidList& guids)
{
    for (const API_Guid& guid : guids) {
        if (vs.gdlCache.ContainsKey (guid)) {
            continue;
        }
        std::vector<ExtractedValue> values (vs.gdlNamesLower.size ());
        API_ElementMemo memo = {};
        const GSErrCode err = ACAPI_Element_GetMemo (guid, &memo, APIMemoMask_AddPars);
        if (err == NoError && memo.params != nullptr) {
            const GSSize nParams = BMGetHandleSize (reinterpret_cast<GSHandle> (memo.params)) / sizeof (API_AddParType);
            for (GSIndex i = 0; i < nParams; ++i) {
                const API_AddParType& p = (*memo.params)[i];
                const std::string nameLower = LowerAscii (p.name);
                for (size_t k = 0; k < vs.gdlNamesLower.size (); ++k) {
                    if (values[k].status == ValStatus::NotAvailable && vs.gdlNamesLower[k] == nameLower) {
                        values[k] = ExtractGdl (p);
                    }
                }
            }
        }
        ACAPI_DisposeElemMemoHdls (&memo);
        vs.gdlCache.Add (guid, values);
    }
}

// ---- IFC -----------------------------------------------------------------------------------------

void EnsureIfcEntry (ValueLeafState& vs, EvalContext& ctx, const API_Guid& guid)
{
    if (vs.ifcCache.ContainsKey (guid)) {
        return;
    }
    IfcEntry entry;
    API_Elem_Head head = {};
    if (!ctx.Header (guid, head)) {
        entry.err = APIERR_BADID;
    } else {
        entry.err = ELMGetIFCData (head, vs.ifcWantProperties, vs.ifcWantAttributes, entry.type, entry.properties, entry.attributes);
    }
    vs.ifcCache.Add (guid, entry);
}

// ---- Bounds --------------------------------------------------------------------------------------

bool BoundsOf (ValueLeafState& vs, EvalContext& ctx, const API_Guid& guid, API_Box3D& box)
{
    if (!vs.boundsCache.ContainsKey (guid)) {
        BoundsEntry entry;
        API_Elem_Head head = {};
        if (ctx.Header (guid, head)) {
            entry.ok = (ACAPI_Element_CalcBounds (&head, &entry.box) == NoError);
        }
        vs.boundsCache.Add (guid, entry);
    }
    const BoundsEntry& entry = vs.boundsCache.Get (guid);
    box = entry.box;
    return entry.ok;
}

// ---- Wirt / Oeffnungen -------------------------------------------------------------------------

bool IsHostType (API_ElemTypeID type)
{
    return type == API_WallID || type == API_SlabID || type == API_RoofID || type == API_ShellID ||
           type == API_BeamID || type == API_ColumnID;
}

// Rueckwaertskarte Oeffnung -> Wirt (API_OpeningID hat in AC29 kein owner-Feld mehr).
void EnsureOpeningMap (ValueLeafState& vs)
{
    if (vs.openingMapBuilt) {
        return;
    }
    vs.openingMapBuilt = true;
    const API_ElemTypeID hostTypes[] = { API_WallID, API_SlabID, API_RoofID, API_ShellID, API_BeamID, API_ColumnID };
    for (const API_ElemTypeID hostType : hostTypes) {
        GS::Array<API_Guid> hosts;
        if (ACAPI_Element_GetElemList (hostType, &hosts) != NoError) {
            continue;
        }
        for (const API_Guid& host : hosts) {
            GS::Array<API_Guid> openings;
            if (ACAPI_Grouping_GetConnectedElements (host, API_OpeningID, &openings) != NoError) {
                continue;
            }
            for (const API_Guid& opening : openings) {
                if (!vs.openingToHost.ContainsKey (opening)) {
                    vs.openingToHost.Add (opening, host);
                }
            }
        }
    }
}

HostEntry HostOf (ValueLeafState& vs, EvalContext& ctx, const API_Guid& guid)
{
    if (vs.hostCache.ContainsKey (guid)) {
        return vs.hostCache.Get (guid);
    }
    HostEntry entry;
    API_Elem_Head head = {};
    if (ctx.Header (guid, head)) {
        const API_ElemTypeID type = GetElemTypeId (head);
        if (type == API_DoorID || type == API_WindowID || type == API_SkylightID) {
            entry.applicable = true;
            API_Element element = {};
            element.header.guid = guid;
            if (ACAPI_Element_Get (&element) == NoError) {
                // Tuer UND Fenster ueber window.owner (wie Tapirs BuildOpeningToHostMap).
                entry.host = (type == API_SkylightID) ? element.skylight.owner : element.window.owner;
            }
        } else if (type == API_OpeningID) {
            entry.applicable = true;
            EnsureOpeningMap (vs);
            if (vs.openingToHost.ContainsKey (guid)) {
                entry.host = vs.openingToHost.Get (guid);
            }
        }
    }
    vs.hostCache.Add (guid, entry);
    return entry;
}

OpeningsEntry OpeningsOf (ValueLeafState& vs, EvalContext& ctx, const API_Guid& guid)
{
    if (vs.openingsCache.ContainsKey (guid)) {
        return vs.openingsCache.Get (guid);
    }
    OpeningsEntry entry;
    API_Elem_Head head = {};
    if (ctx.Header (guid, head) && IsHostType (GetElemTypeId (head))) {
        entry.applicable = true;
        const API_ElemTypeID openingTypes[] = { API_DoorID, API_WindowID, API_SkylightID, API_OpeningID };
        for (const API_ElemTypeID openingType : openingTypes) {
            GS::Array<API_Guid> connected;
            if (ACAPI_Grouping_GetConnectedElements (guid, openingType, &connected) == NoError) {
                for (const API_Guid& c : connected) {
                    entry.openings.push_back (c);
                }
            }
        }
    }
    vs.openingsCache.Add (guid, entry);
    return entry;
}

// ---- Klassifizierung ---------------------------------------------------------------------------

struct ClassItem {
    API_Guid      guid = APINULLGuid;
    GS::UniString id;
    GS::UniString name;
    Int32         parent = -1;
};

void CollectClassificationTree (const API_Guid& systemGuid, std::vector<ClassItem>& items)
{
    GS::Array<API_ClassificationItem> roots;
    if (ACAPI_Classification_GetClassificationSystemRootItems (systemGuid, roots) != NoError) {
        return;
    }
    for (const API_ClassificationItem& root : roots) {
        ClassItem item;
        item.guid = root.guid;
        item.id = root.id;
        item.name = root.name;
        item.parent = -1;
        items.push_back (item);
    }
    // Breitensuche: Eltern stehen immer vor ihren Kindern.
    for (size_t i = 0; i < items.size (); ++i) {
        const API_Guid parentGuid = items[i].guid;
        GS::Array<API_ClassificationItem> children;
        if (ACAPI_Classification_GetClassificationItemChildren (parentGuid, children) != NoError) {
            continue;
        }
        for (const API_ClassificationItem& child : children) {
            ClassItem item;
            item.guid = child.guid;
            item.id = child.id;
            item.name = child.name;
            item.parent = (Int32) i;
            items.push_back (item);
        }
    }
}

GS::UniString SystemLabel (const API_ClassificationSystem& classSystem)
{
    return classSystem.name + GS::UniString (" (") + classSystem.editionVersion + GS::UniString (")");
}

bool ReadBoolField (const GS::ObjectState& os, const GS::String& key, const GS::UniString& path, bool& value, GS::UniString& error)
{
    if (!os.Contains (key)) {
        return true;
    }
    if (os.GetType (key) != GS::ObjectState::Bool) {
        error = path + GS::UniString (".") + ToUni (key) + GS::UniString (": erwartet true/false, erhalten: ") + TypeNameOf (os, key);
        return false;
    }
    bool b = false;
    os.Get (key, b);
    value = value || b;
    return true;
}

bool ReadTextField (const GS::ObjectState& os, const GS::String& key, const GS::UniString& path, GS::UniString& value, bool& present, GS::UniString& error)
{
    present = os.Contains (key);
    if (!present) {
        return true;
    }
    if (os.GetType (key) != GS::ObjectState::String || !os.Get (key, value)) {
        error = path + GS::UniString (".") + ToUni (key) + GS::UniString (": erwartet Text, erhalten: ") + TypeNameOf (os, key);
        return false;
    }
    return true;
}

bool CheckKeys (const GS::ObjectState& os, const char* const* allowed, size_t allowedCount, const GS::UniString& path, GS::UniString& error)
{
    const GS::HashSet<GS::String> fields = os.GetFieldNames ();
    for (const GS::String& field : fields) {
        bool known = false;
        for (size_t i = 0; i < allowedCount; ++i) {
            if (ToUni (field) == GS::UniString (allowed[i])) {
                known = true;
                break;
            }
        }
        if (!known) {
            std::vector<GS::UniString> names;
            for (size_t i = 0; i < allowedCount; ++i) {
                names.push_back (GS::UniString (allowed[i]));
            }
            error = path + GS::UniString (".") + ToUni (field) + GS::UniString (": unbekannter Schluessel - erlaubt: ") + JoinNames (names, 20);
            return false;
        }
    }
    return true;
}

bool ParseClassification (const GS::ObjectState& nodeOs, const GS::UniString& path, Node& node, GS::UniString& error)
{
    const GS::ObjectState* co = (nodeOs.GetType ("classification") == GS::ObjectState::Object) ? nodeOs.Get ("classification") : nullptr;
    if (co == nullptr) {
        error = path + GS::UniString (": erwartet Objekt {system, code | name | item, includeChildren, unclassified, version}, erhalten: ") + TypeNameOf (nodeOs, "classification");
        return false;
    }
    const char* const allowed[] = { "system", "code", "name", "item", "includeChildren", "unclassified", "version" };
    if (!CheckKeys (*co, allowed, sizeof (allowed) / sizeof (allowed[0]), path, error)) {
        return false;
    }
    GS::UniString systemName;
    GS::UniString version;
    GS::UniString code;
    GS::UniString itemName;
    GS::UniString itemGuid;
    bool hasSystem = false;
    bool hasVersion = false;
    bool hasCode = false;
    bool hasName = false;
    bool hasItem = false;
    if (!ReadTextField (*co, "system", path, systemName, hasSystem, error) ||
        !ReadTextField (*co, "version", path, version, hasVersion, error) ||
        !ReadTextField (*co, "code", path, code, hasCode, error) ||
        !ReadTextField (*co, "name", path, itemName, hasName, error) ||
        !ReadTextField (*co, "item", path, itemGuid, hasItem, error)) {
        return false;
    }
    bool includeChildren = false;
    bool unclassified = false;
    if (!ReadBoolField (*co, "includeChildren", path, includeChildren, error) ||
        !ReadBoolField (nodeOs, "includeChildren", path, includeChildren, error) ||
        !ReadBoolField (*co, "unclassified", path, unclassified, error) ||
        !ReadBoolField (nodeOs, "unclassified", path, unclassified, error)) {
        return false;
    }
    if (!hasSystem) {
        error = path + GS::UniString (".system: fehlt (Name des Klassifizierungssystems)");
        return false;
    }
    const Int32 selectorCount = (hasCode ? 1 : 0) + (hasName ? 1 : 0) + (hasItem ? 1 : 0);
    if (unclassified && selectorCount > 0) {
        error = path + GS::UniString (": unclassified:true schliesst code/name/item aus");
        return false;
    }
    if (!unclassified && selectorCount != 1) {
        error = path + GS::UniString (": genau eines von code, name, item angeben - oder unclassified:true");
        return false;
    }

    GS::Array<API_ClassificationSystem> systems;
    ACAPI_Classification_GetClassificationSystems (systems);
    std::vector<GS::UniString> allSystems;
    std::vector<UInt32> matches;
    const GS::UniString systemLower = systemName.ToLowerCase ();
    for (UInt32 i = 0; i < systems.GetSize (); ++i) {
        allSystems.push_back (SystemLabel (systems[i]));
        if (!(systems[i].name.ToLowerCase () == systemLower)) {
            continue;
        }
        if (hasVersion && !(systems[i].editionVersion.ToLowerCase () == version.ToLowerCase ())) {
            continue;
        }
        matches.push_back (i);
    }
    if (matches.empty ()) {
        GS::UniString wanted = systemName;
        if (hasVersion) {
            wanted += GS::UniString (" (") + version + GS::UniString (")");
        }
        error = path + GS::UniString (".system: unbekanntes Klassifizierungssystem '") + wanted +
                GS::UniString ("' - vorhanden: ") + JoinNames (allSystems, 40);
        return false;
    }
    if (matches.size () > 1) {
        std::vector<GS::UniString> candidates;
        for (const UInt32 m : matches) {
            candidates.push_back (SystemLabel (systems[m]));
        }
        error = path + GS::UniString (".system: '") + systemName + GS::UniString ("' mehrdeutig: ") + JoinNames (candidates, 20) +
                GS::UniString (" - version angeben");
        return false;
    }
    const API_ClassificationSystem& classSystem = systems[matches[0]];

    std::unique_ptr<ClassificationLeaf> leaf (new ClassificationLeaf ());
    leaf->systemGuid = classSystem.guid;
    leaf->unclassified = unclassified;
    if (!unclassified) {
        std::vector<ClassItem> tree;
        CollectClassificationTree (classSystem.guid, tree);
        std::vector<bool> selected (tree.size (), false);
        bool any = false;
        if (hasItem && !IsGuidString (itemGuid)) {
            error = path + GS::UniString (".item: ungueltige GUID '") + itemGuid + GS::UniString ("'");
            return false;
        }
        const API_Guid wantedGuid = hasItem ? APIGuidFromString (itemGuid.ToCStr ().Get ()) : APINULLGuid;
        const GS::UniString wantedLower = hasCode ? code.ToLowerCase () : itemName.ToLowerCase ();
        for (size_t i = 0; i < tree.size (); ++i) {
            bool hit = false;
            if (hasItem) {
                hit = (tree[i].guid == wantedGuid);
            } else if (hasCode) {
                hit = (tree[i].id.ToLowerCase () == wantedLower);
            } else {
                hit = (tree[i].name.ToLowerCase () == wantedLower);
            }
            if (hit) {
                selected[i] = true;
                any = true;
            }
        }
        if (!any) {
            const GS::UniString wanted = hasItem ? itemGuid : (hasCode ? code : itemName);
            std::vector<GS::UniString> similar;
            for (const ClassItem& ci : tree) {
                if (similar.size () >= 10) {
                    break;
                }
                if (!wantedLower.IsEmpty () && (ci.id.ToLowerCase ().Contains (wantedLower) || ci.name.ToLowerCase ().Contains (wantedLower))) {
                    similar.push_back (ci.id + GS::UniString (" ") + ci.name);
                }
            }
            error = path + GS::UniString (": unbekannter Eintrag '") + wanted + GS::UniString ("' im System ") + SystemLabel (classSystem);
            if (!similar.empty ()) {
                error += GS::UniString (" - aehnlich: ") + JoinNames (similar, 10);
            }
            return false;
        }
        if (includeChildren) {
            for (size_t i = 0; i < tree.size (); ++i) {
                if (!selected[i] && tree[i].parent >= 0 && selected[(size_t) tree[i].parent]) {
                    selected[i] = true;
                }
            }
        }
        for (size_t i = 0; i < tree.size (); ++i) {
            if (selected[i]) {
                leaf->items.Add (tree[i].guid);
            }
        }
    }
    node.leaf = std::move (leaf);
    return true;
}

// ---- weitere Parser ------------------------------------------------------------------------------

bool ParseLibPart (const GS::ObjectState& nodeOs, const GS::UniString& path, Node& node, GS::UniString& error)
{
    std::unique_ptr<LibPartLeaf> leaf (new LibPartLeaf ());
    const GS::ObjectState::Type type = nodeOs.GetType ("libPart");
    if (type == GS::ObjectState::String) {
        GS::UniString name;
        nodeOs.Get ("libPart", name);
        leaf->nameLower = name.ToLowerCase ();
    } else if (type == GS::ObjectState::Object) {
        const GS::ObjectState* lo = nodeOs.Get ("libPart");
        const char* const allowed[] = { "name", "pattern" };
        if (lo == nullptr || !CheckKeys (*lo, allowed, 2, path, error)) {
            if (error.IsEmpty ()) {
                error = path + GS::UniString (": Objekt nicht lesbar");
            }
            return false;
        }
        GS::UniString name;
        GS::UniString pattern;
        bool hasName = false;
        bool hasPattern = false;
        if (!ReadTextField (*lo, "name", path, name, hasName, error) || !ReadTextField (*lo, "pattern", path, pattern, hasPattern, error)) {
            return false;
        }
        if (hasName == hasPattern) {
            error = path + GS::UniString (": genau eines von name oder pattern angeben");
            return false;
        }
        leaf->usePattern = hasPattern;
        leaf->pattern = pattern;
        leaf->nameLower = name.ToLowerCase ();
    } else {
        error = path + GS::UniString (": erwartet Name oder {name} / {pattern}, erhalten: ") + TypeNameOf (nodeOs, "libPart");
        return false;
    }
    node.leaf = std::move (leaf);
    return true;
}

bool ParseGdl (const GS::ObjectState& nodeOs, const GS::UniString& path, ValueLeafState& vs, Node& node, GS::UniString& error)
{
    GS::UniString name;
    if (nodeOs.GetType ("gdl") != GS::ObjectState::String || !nodeOs.Get ("gdl", name) || name.IsEmpty ()) {
        error = path + GS::UniString (": erwartet GDL-Parametername (Text), erhalten: ") + TypeNameOf (nodeOs, "gdl");
        return false;
    }
    std::unique_ptr<GdlLeaf> leaf (new GdlLeaf ());
    if (!ParseComparison (nodeOs, path, leaf->cmp, error)) {
        return false;
    }
    const std::string nameLower = LowerAscii (ToUtf8 (name).c_str ());
    UInt32 slot = (UInt32) vs.gdlNamesLower.size ();
    for (size_t i = 0; i < vs.gdlNamesLower.size (); ++i) {
        if (vs.gdlNamesLower[i] == nameLower) {
            slot = (UInt32) i;
            break;
        }
    }
    if (slot == (UInt32) vs.gdlNamesLower.size ()) {
        vs.gdlNamesLower.push_back (nameLower);
    }
    leaf->slot = slot;
    node.leaf = std::move (leaf);
    return true;
}

bool ParseIfc (const GS::ObjectState& nodeOs, const GS::UniString& path, ValueLeafState& vs, Node& node, GS::UniString& error)
{
    const GS::ObjectState* io = (nodeOs.GetType ("ifc") == GS::ObjectState::Object) ? nodeOs.Get ("ifc") : nullptr;
    if (io == nullptr) {
        error = path + GS::UniString (": erwartet Objekt {type} | {pset?, property} | {attribute}, erhalten: ") + TypeNameOf (nodeOs, "ifc");
        return false;
    }
    const char* const allowed[] = { "type", "pset", "property", "attribute" };
    if (!CheckKeys (*io, allowed, 4, path, error)) {
        return false;
    }
    std::unique_ptr<IfcLeaf> leaf (new IfcLeaf ());
    const bool hasType = io->Contains ("type");
    const bool hasProperty = io->Contains ("property");
    const bool hasAttribute = io->Contains ("attribute");
    const bool hasPset = io->Contains ("pset");
    if ((hasType ? 1 : 0) + (hasProperty ? 1 : 0) + (hasAttribute ? 1 : 0) != 1) {
        error = path + GS::UniString (": genau eines von type, property, attribute angeben");
        return false;
    }
    if (hasPset && !hasProperty) {
        error = path + GS::UniString (".pset: nur zusammen mit property");
        return false;
    }
    if (hasType) {
        if (nodeOs.Contains ("op") || nodeOs.Contains ("value") || nodeOs.Contains ("caseSensitive")) {
            error = path + GS::UniString (": op/value/caseSensitive sind bei ifc.type nicht erlaubt");
            return false;
        }
        std::vector<GS::UniString> types;
        if (!ReadStringOrList (*io, "type", types) || types.empty ()) {
            error = path + GS::UniString (".type: erwartet IFC-Typ oder Liste (z. B. \"IfcDoor\"), erhalten: ") + TypeNameOf (*io, "type");
            return false;
        }
        leaf->mode = IfcMode::Type;
        for (const GS::UniString& t : types) {
            leaf->typesLower.push_back (t.ToLowerCase ());
        }
    } else {
        GS::UniString name;
        bool present = false;
        const GS::String key (hasProperty ? "property" : "attribute");
        if (!ReadTextField (*io, key, path, name, present, error)) {
            return false;
        }
        leaf->mode = hasProperty ? IfcMode::Property : IfcMode::Attribute;
        leaf->nameLower = name.ToLowerCase ();
        if (hasPset) {
            GS::UniString pset;
            if (!ReadTextField (*io, "pset", path, pset, present, error)) {
                return false;
            }
            leaf->psetLower = pset.ToLowerCase ();
        }
        if (!ParseComparison (nodeOs, path, leaf->cmp, error)) {
            return false;
        }
        if (hasProperty) {
            vs.ifcWantProperties = true;
        } else {
            vs.ifcWantAttributes = true;
        }
    }
    node.leaf = std::move (leaf);
    return true;
}

bool ReadPoint (const GS::ObjectState* os, const GS::UniString& path, double& x, double& y, double& z, bool& hasZ, GS::UniString& error)
{
    if (os == nullptr) {
        error = path + GS::UniString (": erwartet Objekt {x, y[, z]} (Meter)");
        return false;
    }
    if (!ReadNumber (*os, "x", x) || !ReadNumber (*os, "y", y)) {
        error = path + GS::UniString (": x und y (Zahlen, Meter) erforderlich");
        return false;
    }
    hasZ = os->Contains ("z");
    if (hasZ && !ReadNumber (*os, "z", z)) {
        error = path + GS::UniString (".z: erwartet Zahl");
        return false;
    }
    return true;
}

const GS::ObjectState* ObjectField (const GS::ObjectState& os, const GS::String& key)
{
    if (!os.Contains (key) || os.GetType (key) != GS::ObjectState::Object) {
        return nullptr;
    }
    return os.Get (key);
}

bool ParseBBox (const GS::ObjectState& nodeOs, const GS::UniString& path, Node& node, GS::UniString& error)
{
    const GS::ObjectState* bo = ObjectField (nodeOs, "bbox");
    if (bo == nullptr) {
        error = path + GS::UniString (": erwartet {min:{x,y[,z]}, max:{x,y[,z]}, mode}, erhalten: ") + TypeNameOf (nodeOs, "bbox");
        return false;
    }
    const char* const allowed[] = { "min", "max", "mode" };
    if (!CheckKeys (*bo, allowed, 3, path, error)) {
        return false;
    }
    std::unique_ptr<BoxLeaf> leaf (new BoxLeaf ());
    if (!ReadPoint (ObjectField (*bo, "min"), path + GS::UniString (".min"), leaf->minX, leaf->minY, leaf->minZ, leaf->hasMinZ, error) ||
        !ReadPoint (ObjectField (*bo, "max"), path + GS::UniString (".max"), leaf->maxX, leaf->maxY, leaf->maxZ, leaf->hasMaxZ, error)) {
        return false;
    }
    if (leaf->minX > leaf->maxX || leaf->minY > leaf->maxY || (leaf->hasMinZ && leaf->hasMaxZ && leaf->minZ > leaf->maxZ)) {
        error = path + GS::UniString (": min > max");
        return false;
    }
    if (bo->Contains ("mode")) {
        GS::UniString mode;
        if (bo->GetType ("mode") != GS::ObjectState::String || !bo->Get ("mode", mode)) {
            error = path + GS::UniString (".mode: erwartet \"intersects\" oder \"inside\"");
            return false;
        }
        const GS::UniString modeLower = mode.ToLowerCase ();
        if (modeLower == GS::UniString ("inside")) {
            leaf->inside = true;
        } else if (!(modeLower == GS::UniString ("intersects"))) {
            error = path + GS::UniString (".mode: unbekannt '") + mode + GS::UniString ("' - erlaubt: intersects, inside");
            return false;
        }
    }
    node.leaf = std::move (leaf);
    return true;
}

bool ParseNear (const GS::ObjectState& nodeOs, const GS::UniString& path, Node& node, GS::UniString& error)
{
    const GS::ObjectState* no = ObjectField (nodeOs, "near");
    if (no == nullptr) {
        error = path + GS::UniString (": erwartet {point:{x,y[,z]}, radius}, erhalten: ") + TypeNameOf (nodeOs, "near");
        return false;
    }
    const char* const allowed[] = { "point", "radius" };
    if (!CheckKeys (*no, allowed, 2, path, error)) {
        return false;
    }
    std::unique_ptr<NearLeaf> leaf (new NearLeaf ());
    if (!ReadPoint (ObjectField (*no, "point"), path + GS::UniString (".point"), leaf->x, leaf->y, leaf->z, leaf->hasZ, error)) {
        return false;
    }
    if (!ReadNumber (*no, "radius", leaf->radius) || leaf->radius < 0.0) {
        error = path + GS::UniString (".radius: erwartet Zahl >= 0 (Meter)");
        return false;
    }
    node.leaf = std::move (leaf);
    return true;
}

#ifdef ServerMainVers_2900
GS::UniString DesignOptionLabel (const ELMDesignOptions::DesignOption& option)
{
    return option.GetName () + GS::UniString (" [") + option.GetOwnerSetName () + GS::UniString ("]");
}

bool ParseDesignOption (const GS::ObjectState& nodeOs, const GS::UniString& path, Node& node, GS::UniString& error)
{
    std::unique_ptr<DesignOptionLeaf> leaf (new DesignOptionLeaf ());
    GS::UniString name;
    GS::UniString id;
    GS::UniString guidText;
    GS::UniString setName;
    bool hasName = false;
    bool hasId = false;
    bool hasGuid = false;
    bool hasSet = false;
    const GS::ObjectState::Type type = nodeOs.GetType ("designOption");
    if (type == GS::ObjectState::String) {
        nodeOs.Get ("designOption", name);
        hasName = true;
    } else if (type == GS::ObjectState::Object) {
        const GS::ObjectState* dobj = nodeOs.Get ("designOption");
        const char* const allowed[] = { "name", "id", "guid", "set", "mainModel" };
        if (dobj == nullptr || !CheckKeys (*dobj, allowed, 5, path, error)) {
            if (error.IsEmpty ()) {
                error = path + GS::UniString (": Objekt nicht lesbar");
            }
            return false;
        }
        bool mainModel = false;
        if (!ReadBoolField (*dobj, "mainModel", path, mainModel, error) ||
            !ReadTextField (*dobj, "name", path, name, hasName, error) ||
            !ReadTextField (*dobj, "id", path, id, hasId, error) ||
            !ReadTextField (*dobj, "guid", path, guidText, hasGuid, error) ||
            !ReadTextField (*dobj, "set", path, setName, hasSet, error)) {
            return false;
        }
        if (mainModel) {
            if (hasName || hasId || hasGuid || hasSet) {
                error = path + GS::UniString (": mainModel:true schliesst name/id/guid/set aus");
                return false;
            }
            leaf->mainModel = true;
            node.leaf = std::move (leaf);
            return true;
        }
    } else {
        error = path + GS::UniString (": erwartet Name oder {name | id | guid, set} / {mainModel:true}, erhalten: ") + TypeNameOf (nodeOs, "designOption");
        return false;
    }
    if ((hasName ? 1 : 0) + (hasId ? 1 : 0) + (hasGuid ? 1 : 0) != 1) {
        error = path + GS::UniString (": genau eines von name, id, guid angeben (oder mainModel:true)");
        return false;
    }
    if (hasGuid && !IsGuidString (guidText)) {
        error = path + GS::UniString (".guid: ungueltige GUID '") + guidText + GS::UniString ("'");
        return false;
    }

    auto manager = ELMDesignOptions::CreateDesignOptionManager ();
    if (manager.IsErr ()) {
        error = path + GS::UniString (": Design-Optionen nicht lesbar (Fehler ") + IntToUni (manager.UnwrapErr ().kind) + GS::UniString (")");
        return false;
    }
    auto allOptions = manager->GetAllDesignOptions ();
    if (allOptions.IsErr ()) {
        error = path + GS::UniString (": Design-Optionen nicht lesbar (Fehler ") + IntToUni (allOptions.UnwrapErr ().kind) + GS::UniString (")");
        return false;
    }
    const GS::Guid wantedGuid = hasGuid ? APIGuid2GSGuid (APIGuidFromString (guidText.ToCStr ().Get ())) : GS::Guid ();
    const GS::UniString nameLower = name.ToLowerCase ();
    const GS::UniString idLower = id.ToLowerCase ();
    const GS::UniString setLower = setName.ToLowerCase ();
    std::vector<GS::UniString> available;
    std::vector<GS::UniString> candidates;
    for (const auto& option : *allOptions) {
        available.push_back (DesignOptionLabel (option));
        bool hit = false;
        if (hasGuid) {
            hit = (option.GetGuid () == wantedGuid);
        } else if (hasId) {
            hit = (option.GetId ().ToLowerCase () == idLower);
        } else {
            hit = (option.GetName ().ToLowerCase () == nameLower);
        }
        if (hit && hasSet && !(option.GetOwnerSetName ().ToLowerCase () == setLower)) {
            hit = false;
        }
        if (hit) {
            leaf->options.push_back (option.GetGuid ());
            candidates.push_back (DesignOptionLabel (option));
        }
    }
    if (leaf->options.empty ()) {
        error = path + GS::UniString (": unbekannte Design-Option - vorhanden: ") + JoinNames (available, 40);
        return false;
    }
    if (leaf->options.size () > 1) {
        error = path + GS::UniString (": Design-Option mehrdeutig: ") + JoinNames (candidates, 20) + GS::UniString (" - set angeben");
        return false;
    }
    node.leaf = std::move (leaf);
    return true;
}

void EvaluateDesignOption (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable)
{
    const DesignOptionLeaf& leaf = static_cast<const DesignOptionLeaf&> (*node.leaf);
    auto manager = ELMDesignOptions::CreateDesignOptionManager ();
    if (manager.IsErr ()) {
        ctx.fatalError = node.path + GS::UniString (": Design-Optionen nicht lesbar (Fehler ") + IntToUni (manager.UnwrapErr ().kind) + GS::UniString (")");
        ctx.fatalCode = manager.UnwrapErr ().kind;
        return;
    }
    for (const API_Guid& guid : in) {
        auto status = manager->GetDesignOptionStatusOf (guid);
        if (status.IsErr ()) {
            ++notAvailable;
            continue;
        }
        const ELMDesignOptions::ElemDesignOptionStatus& st = *status;
        bool hit = false;
        if (std::holds_alternative<ELMDesignOptions::MainModelDesignOption> (st)) {
            hit = leaf.mainModel;
        } else if (std::holds_alternative<ELMDesignOptions::DesignOption> (st)) {
            if (!leaf.mainModel) {
                const GS::Guid optionGuid = std::get<ELMDesignOptions::DesignOption> (st).GetGuid ();
                for (const GS::Guid& g : leaf.options) {
                    if (g == optionGuid) {
                        hit = true;
                        break;
                    }
                }
            }
        } else {
            ++notAvailable;
        }
        if (hit) {
            out.push_back (guid);
        }
    }
}
#endif

// ---- Auswertung der Wert-Blaetter --------------------------------------------------------------

void EvaluatePropertyLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable, UInt64& typeMismatch)
{
    ValueLeafState& vs = *ctx.valueState;
    const PropertyLeaf& leaf = static_cast<const PropertyLeaf&> (*node.leaf);
    EnsurePropertyValues (vs, in);
    for (const API_Guid& guid : in) {
        const std::vector<ExtractedValue>& values = vs.propCache.Get (guid);
        const ExtractedValue& v = values[leaf.slot];
        if (v.status == ValStatus::NotAvailable) {
            ++notAvailable;
            continue;
        }
        const MatchResult r = MatchValue (ctx, v, leaf.cmp, false);
        if (r == MatchResult::Yes) {
            out.push_back (guid);
        } else if (r == MatchResult::Mismatch) {
            ++typeMismatch;
        }
        if (!ctx.fatalError.IsEmpty ()) {
            return;
        }
    }
}

void EvaluateGdlLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable, UInt64& typeMismatch)
{
    ValueLeafState& vs = *ctx.valueState;
    const GdlLeaf& leaf = static_cast<const GdlLeaf&> (*node.leaf);
    EnsureGdlValues (vs, in);
    for (const API_Guid& guid : in) {
        const std::vector<ExtractedValue>& values = vs.gdlCache.Get (guid);
        const ExtractedValue& v = values[leaf.slot];
        if (v.status == ValStatus::NotAvailable) {
            ++notAvailable;
            continue;
        }
        if (v.status == ValStatus::TypeMismatch) {
            ++typeMismatch;
            continue;
        }
        const MatchResult r = MatchValue (ctx, v, leaf.cmp, false);
        if (r == MatchResult::Yes) {
            out.push_back (guid);
        } else if (r == MatchResult::Mismatch) {
            ++typeMismatch;
        }
        if (!ctx.fatalError.IsEmpty ()) {
            return;
        }
    }
}

void EvaluateIfcLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable, UInt64& typeMismatch)
{
    ValueLeafState& vs = *ctx.valueState;
    const IfcLeaf& leaf = static_cast<const IfcLeaf&> (*node.leaf);
    UInt64    failed = 0;
    GSErrCode firstErr = NoError;
    for (const API_Guid& guid : in) {
        EnsureIfcEntry (vs, ctx, guid);
        const IfcEntry& entry = vs.ifcCache.Get (guid);
        if (entry.err == APIERR_MODULNOTINSTALLED) {
            ctx.fatalError = node.path + GS::UniString (": IFC-Modul nicht verfuegbar (Code ") + IntToUni (entry.err) + GS::UniString (")");
            ctx.fatalCode = entry.err;
            return;
        }
        if (entry.err != NoError) {
            if (firstErr == NoError) {
                firstErr = entry.err;
            }
            ++failed;
            ++notAvailable;
            continue;
        }
        bool hit = false;
        if (leaf.mode == IfcMode::Type) {
            if (entry.type.IsEmpty ()) {
                ++notAvailable;
                continue;
            }
            const GS::UniString typeLower = entry.type.ToLowerCase ();
            for (const GS::UniString& t : leaf.typesLower) {
                if (t == typeLower) {
                    hit = true;
                    break;
                }
            }
        } else {
            const GS::Array<ELMIFCText>& texts = (leaf.mode == IfcMode::Property) ? entry.properties : entry.attributes;
            bool found = false;
            bool anyComparable = false;
            for (const ELMIFCText& text : texts) {
                if (!(text.name.ToLowerCase () == leaf.nameLower)) {
                    continue;
                }
                if (!leaf.psetLower.IsEmpty () && !(text.propertySetName.ToLowerCase () == leaf.psetLower)) {
                    continue;
                }
                found = true;
                ExtractedValue v;
                if (text.hasValue) {
                    v.status = ValStatus::Normal;
                    ValItem item;
                    item.texts.push_back (text.value);
                    v.items.push_back (item);
                } else {
                    v.status = ValStatus::Undefined;
                }
                const MatchResult r = MatchValue (ctx, v, leaf.cmp, true);
                if (r != MatchResult::Mismatch) {
                    anyComparable = true;
                }
                if (r == MatchResult::Yes) {
                    hit = true;
                    break;
                }
            }
            if (!found) {
                ++notAvailable;
                continue;
            }
            if (!hit && !anyComparable) {
                ++typeMismatch;
            }
        }
        if (hit) {
            out.push_back (guid);
        }
        if (!ctx.fatalError.IsEmpty ()) {
            return;
        }
    }
    if (failed > 0) {
        ctx.warnings.push_back (node.path + GS::UniString (": ") + IntToUni ((Int64) failed) +
            GS::UniString (" Elemente ohne IFC-Daten (erster Fehler Code ") + IntToUni (firstErr) + GS::UniString (")"));
    }
}

void EvaluateClassificationLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out)
{
    ValueLeafState& vs = *ctx.valueState;
    const ClassificationLeaf& leaf = static_cast<const ClassificationLeaf&> (*node.leaf);
    if (!vs.classCache.ContainsKey (leaf.systemGuid)) {
        vs.classCache.Add (leaf.systemGuid, GS::HashTable<API_Guid, API_Guid> ());
    }
    GS::HashTable<API_Guid, API_Guid>& cache = vs.classCache.Get (leaf.systemGuid);
    for (const API_Guid& guid : in) {
        API_Guid itemGuid = APINULLGuid;
        if (cache.ContainsKey (guid)) {
            itemGuid = cache.Get (guid);
        } else {
            API_ClassificationItem item;
            if (ACAPI_Element_GetClassificationInSystem (guid, leaf.systemGuid, item) == NoError) {
                itemGuid = item.guid;
            }
            cache.Add (guid, itemGuid);
        }
        const bool hit = leaf.unclassified ? (itemGuid == APINULLGuid) : leaf.items.Contains (itemGuid);
        if (hit) {
            out.push_back (guid);
        }
    }
}

void EvaluateLibPartLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable)
{
    ValueLeafState& vs = *ctx.valueState;
    const LibPartLeaf& leaf = static_cast<const LibPartLeaf&> (*node.leaf);
    for (const API_Guid& guid : in) {
        Int32 libInd = -1;
        if (vs.libIndCache.ContainsKey (guid)) {
            libInd = vs.libIndCache.Get (guid);
        } else {
            API_Element element = {};
            element.header.guid = guid;
            if (ACAPI_Element_Get (&element) == NoError) {
                libInd = LibIndOfElement (element);
            }
            vs.libIndCache.Add (guid, libInd);
        }
        if (libInd <= 0) {
            ++notAvailable;
            continue;
        }
        if (vs.libNameCache.find (libInd) == vs.libNameCache.end ()) {
            vs.libNameCache[libInd] = LibPartNameOfIndex (libInd);
        }
        const GS::UniString& name = vs.libNameCache[libInd];
        if (name.IsEmpty ()) {
            ++notAvailable;
            continue;
        }
        const bool hit = leaf.usePattern ? WildcardMatch (name, leaf.pattern) : (name.ToLowerCase () == leaf.nameLower);
        if (hit) {
            out.push_back (guid);
        }
    }
}

void EvaluateBoxLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable)
{
    ValueLeafState& vs = *ctx.valueState;
    for (const API_Guid& guid : in) {
        API_Box3D box = {};
        if (!BoundsOf (vs, ctx, guid, box)) {
            ++notAvailable;
            continue;
        }
        bool hit = false;
        if (node.kind == NodeKind::BBox) {
            const BoxLeaf& leaf = static_cast<const BoxLeaf&> (*node.leaf);
            if (leaf.inside) {
                hit = box.xMin >= leaf.minX && box.xMax <= leaf.maxX && box.yMin >= leaf.minY && box.yMax <= leaf.maxY &&
                      (!leaf.hasMinZ || box.zMin >= leaf.minZ) && (!leaf.hasMaxZ || box.zMax <= leaf.maxZ);
            } else {
                hit = box.xMax >= leaf.minX && box.xMin <= leaf.maxX && box.yMax >= leaf.minY && box.yMin <= leaf.maxY &&
                      (!leaf.hasMinZ || box.zMax >= leaf.minZ) && (!leaf.hasMaxZ || box.zMin <= leaf.maxZ);
            }
        } else {
            const NearLeaf& leaf = static_cast<const NearLeaf&> (*node.leaf);
            // Abstand Punkt <-> Bounding-Box (0 innerhalb).
            const double dx = (leaf.x < box.xMin) ? (box.xMin - leaf.x) : ((leaf.x > box.xMax) ? (leaf.x - box.xMax) : 0.0);
            const double dy = (leaf.y < box.yMin) ? (box.yMin - leaf.y) : ((leaf.y > box.yMax) ? (leaf.y - box.yMax) : 0.0);
            double dz = 0.0;
            if (leaf.hasZ) {
                dz = (leaf.z < box.zMin) ? (box.zMin - leaf.z) : ((leaf.z > box.zMax) ? (leaf.z - box.zMax) : 0.0);
            }
            hit = std::sqrt (dx * dx + dy * dy + dz * dz) <= leaf.radius + kTolerance;
        }
        if (hit) {
            out.push_back (guid);
        }
    }
}

void EvaluateHostedByLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable)
{
    ValueLeafState& vs = *ctx.valueState;
    std::vector<API_Guid> hostOfElement (in.size (), APINULLGuid);
    GuidList hosts;
    GS::HashSet<API_Guid> hostSeen;
    for (size_t i = 0; i < in.size (); ++i) {
        const HostEntry entry = HostOf (vs, ctx, in[i]);
        if (!entry.applicable || entry.host == APINULLGuid) {
            ++notAvailable;
            continue;
        }
        hostOfElement[i] = entry.host;
        if (!hostSeen.Contains (entry.host)) {
            hostSeen.Add (entry.host);
            hosts.push_back (entry.host);
        }
    }
    GuidList matchedHosts;
    EvaluateNode (ctx, *node.children[0], hosts, matchedHosts);
    GS::HashSet<API_Guid> matched;
    for (const API_Guid& h : matchedHosts) {
        matched.Add (h);
    }
    for (size_t i = 0; i < in.size (); ++i) {
        if (hostOfElement[i] != APINULLGuid && matched.Contains (hostOfElement[i])) {
            out.push_back (in[i]);
        }
    }
}

void EvaluateHostsLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out, UInt64& notAvailable)
{
    ValueLeafState& vs = *ctx.valueState;
    std::vector<std::vector<API_Guid>> openingsOfElement (in.size ());
    GuidList openings;
    GS::HashSet<API_Guid> seen;
    for (size_t i = 0; i < in.size (); ++i) {
        const OpeningsEntry entry = OpeningsOf (vs, ctx, in[i]);
        if (!entry.applicable) {
            ++notAvailable;
            continue;
        }
        openingsOfElement[i] = entry.openings;
        for (const API_Guid& o : entry.openings) {
            if (!seen.Contains (o)) {
                seen.Add (o);
                openings.push_back (o);
            }
        }
    }
    GuidList matchedOpenings;
    EvaluateNode (ctx, *node.children[0], openings, matchedOpenings);
    GS::HashSet<API_Guid> matched;
    for (const API_Guid& o : matchedOpenings) {
        matched.Add (o);
    }
    for (size_t i = 0; i < in.size (); ++i) {
        for (const API_Guid& o : openingsOfElement[i]) {
            if (matched.Contains (o)) {
                out.push_back (in[i]);
                break;
            }
        }
    }
}

void EvaluateInZoneLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out)
{
    ValueLeafState& vs = *ctx.valueState;
    if (vs.zoneMembers.find (node.statsIndex) == vs.zoneMembers.end ()) {
        // Alle Zonen (unabhaengig vom scope) -> Unterbaum -> Elemente der passenden Zonen.
        GS::Array<API_Guid> zoneArray;
        ACAPI_Element_GetElemList (API_ZoneID, &zoneArray);
        GuidList zones;
        for (const API_Guid& z : zoneArray) {
            zones.push_back (z);
        }
        GuidList matchedZones;
        EvaluateNode (ctx, *node.children[0], zones, matchedZones);
        GS::HashSet<API_Guid> members;
        for (const API_Guid& zone : matchedZones) {
            API_RoomRelation relation;
            if (ACAPI_Element_GetRelations (zone, API_ZombieElemID, &relation) == NoError) {
                for (const auto& kv : relation.elementsGroupedByType) {
#ifdef ServerMainVers_2800
                    const GS::Array<API_Guid>& grouped = kv.value;
#else
                    const GS::Array<API_Guid>& grouped = *kv.value;
#endif
                    for (const API_Guid& g : grouped) {
                        members.Add (g);
                    }
                }
                for (const auto& part : relation.wallPart) {
                    members.Add (part.guid);
                }
                for (const auto& part : relation.beamPart) {
                    members.Add (part.guid);
                }
                for (const auto& part : relation.cwSegmentPart) {
                    members.Add (part.guid);
                }
            }
            ACAPI_DisposeRoomRelationHdls (&relation);
        }
        vs.zoneMembers[node.statsIndex] = members;
    }
    const GS::HashSet<API_Guid>& members = vs.zoneMembers[node.statsIndex];
    for (const API_Guid& guid : in) {
        if (members.Contains (guid)) {
            out.push_back (guid);
        }
    }
}

}   // namespace

// =====================================================================================
// Vertragsfunktionen (Task 2)
// =====================================================================================

std::shared_ptr<ValueLeafState> CreateValueLeafState ()
{
    return std::make_shared<ValueLeafState> ();
}

bool ParseValueLeaf (NodeKind kind, const GS::ObjectState& nodeOs, const GS::UniString& path,
                     ValueLeafState& vs, Node& node, GS::UniString& error)
{
    switch (kind) {
        case NodeKind::Property: {
            UInt32 defIndex = 0;
            GS::UniString requested;
            if (!ResolvePropertyField (vs, nodeOs, "property", path, defIndex, requested, error)) {
                return false;
            }
            std::unique_ptr<PropertyLeaf> leaf (new PropertyLeaf ());
            if (!ParseComparison (nodeOs, path, leaf->cmp, error)) {
                return false;
            }
            if (!CheckPropertyKind (vs.propDefs[defIndex], leaf->cmp, path, error)) {
                return false;
            }
            leaf->slot = SlotFor (vs, defIndex, false);
            node.leaf = std::move (leaf);
            return true;
        }
        case NodeKind::Classification:
            return ParseClassification (nodeOs, path, node, error);
        case NodeKind::LibPart:
            return ParseLibPart (nodeOs, path, node, error);
        case NodeKind::Gdl:
            return ParseGdl (nodeOs, path, vs, node, error);
        case NodeKind::Ifc:
            return ParseIfc (nodeOs, path, vs, node, error);
        case NodeKind::BBox:
            return ParseBBox (nodeOs, path, node, error);
        case NodeKind::Near:
            return ParseNear (nodeOs, path, node, error);
        case NodeKind::DesignOption:
#ifdef ServerMainVers_2900
            return ParseDesignOption (nodeOs, path, node, error);
#elif defined (ServerMainVers_2800)
            error = path + GS::UniString (": Kriterium designOption in Archicad 28 nicht verfuegbar");
            return false;
#else
            error = path + GS::UniString (": Kriterium designOption in Archicad 27 nicht verfuegbar");
            return false;
#endif
        default:
            error = path + GS::UniString (": kein Wert-Kriterium");
            return false;
    }
}

void EvaluateValueLeaf (EvalContext& ctx, const Node& node, const GuidList& in, GuidList& out)
{
    UInt64 notAvailable = 0;
    UInt64 typeMismatch = 0;
    switch (node.kind) {
        case NodeKind::Property:
            EvaluatePropertyLeaf (ctx, node, in, out, notAvailable, typeMismatch);
            break;
        case NodeKind::Gdl:
            EvaluateGdlLeaf (ctx, node, in, out, notAvailable, typeMismatch);
            break;
        case NodeKind::Ifc:
            EvaluateIfcLeaf (ctx, node, in, out, notAvailable, typeMismatch);
            break;
        case NodeKind::Classification:
            EvaluateClassificationLeaf (ctx, node, in, out);
            break;
        case NodeKind::LibPart:
            EvaluateLibPartLeaf (ctx, node, in, out, notAvailable);
            break;
        case NodeKind::BBox:
        case NodeKind::Near:
            EvaluateBoxLeaf (ctx, node, in, out, notAvailable);
            break;
        case NodeKind::HostedBy:
            EvaluateHostedByLeaf (ctx, node, in, out, notAvailable);
            break;
        case NodeKind::Hosts:
            EvaluateHostsLeaf (ctx, node, in, out, notAvailable);
            break;
        case NodeKind::InZone:
            EvaluateInZoneLeaf (ctx, node, in, out);
            break;
        case NodeKind::DesignOption:
#ifdef ServerMainVers_2900
            EvaluateDesignOption (ctx, node, in, out, notAvailable);
#endif
            break;
        default:
            break;
    }
    ctx.stats[node.statsIndex].notAvailable += notAvailable;
    ctx.stats[node.statsIndex].typeMismatch += typeMismatch;
}

bool ParseReturnList (const GS::ObjectState& parameters, ValueLeafState& vs, GS::UniString& error)
{
    if (!parameters.Contains ("return")) {
        return true;
    }
    if (parameters.GetType ("return") != GS::ObjectState::List) {
        error = GS::UniString ("return: erwartet Liste von Eigenschaften (\"Name\", \"Gruppe/Name\", GUID, {guid} oder {group, name}), erhalten: ") + TypeNameOf (parameters, "return");
        return false;
    }
    GS::Array<GS::UniString> strings;
    GS::Array<GS::ObjectState> objects;
    if (parameters.Get ("return", strings)) {
        for (UInt32 i = 0; i < strings.GetSize (); ++i) {
            const GS::UniString path = GS::UniString ("return[") + IntToUni (i) + GS::UniString ("]");
            UInt32 defIndex = 0;
            if (!ResolvePropertyString (vs, strings[i], path, defIndex, error)) {
                return false;
            }
            ReturnColumn column;
            column.requested = strings[i];
            column.slot = SlotFor (vs, defIndex, true);
            vs.returnColumns.push_back (column);
        }
        return true;
    }
    if (parameters.Get ("return", objects)) {
        for (UInt32 i = 0; i < objects.GetSize (); ++i) {
            const GS::UniString path = GS::UniString ("return[") + IntToUni (i) + GS::UniString ("]");
            UInt32 defIndex = 0;
            GS::UniString requested;
            if (!ResolvePropertyObject (vs, objects[i], path, defIndex, requested, error)) {
                return false;
            }
            ReturnColumn column;
            column.requested = requested;
            column.slot = SlotFor (vs, defIndex, true);
            vs.returnColumns.push_back (column);
        }
        return true;
    }
    error = GS::UniString ("return: einheitlich Strings oder Objekte angeben (nicht gemischt)");
    return false;
}

bool HasReturnColumns (const ValueLeafState& vs)
{
    return !vs.returnColumns.empty ();
}

void AddReturnColumns (const ValueLeafState& vs, GS::ObjectState& response)
{
    const auto& columns = response.AddList<GS::ObjectState> ("returnColumns");
    for (const ReturnColumn& column : vs.returnColumns) {
        const PropDef& def = vs.propDefs[vs.neededDef[column.slot]];
        GS::ObjectState o;
        o.Add ("requested", column.requested);
        o.Add ("propertyId", CreateGuidObjectState (def.guid));
        o.Add ("group", def.group);
        o.Add ("name", def.name);
        columns (o);
    }
}

void AddReturnValues (EvalContext& ctx, const API_Guid& guid, GS::ObjectState& hit)
{
    ValueLeafState& vs = *ctx.valueState;
    const GuidList single (1, guid);
    EnsurePropertyValues (vs, single);
    const std::vector<ExtractedValue>& values = vs.propCache.Get (guid);
    const auto& out = hit.AddList<GS::ObjectState> ("values");
    for (const ReturnColumn& column : vs.returnColumns) {
        GS::ObjectState o;
        const ExtractedValue& v = values[column.slot];
        if (v.status == ValStatus::NotAvailable) {
            o.Add ("status", GS::UniString ("notAvailable"));
        } else if (v.status == ValStatus::Undefined) {
            o.Add ("status", GS::UniString ("undefined"));
            o.Add ("display", v.display);
        } else {
            if (!v.isList && v.items.size () == 1) {
                const ValItem& item = v.items[0];
                if (v.isEnum && !item.texts.empty ()) {
                    o.Add ("value", item.texts[0]);          // Aufzaehlung: nonLocalizedValue (sonst Anzeige)
                } else if (item.hasNumber) {
                    o.Add ("value", item.number);
                } else if (item.hasBool) {
                    o.Add ("value", item.boolValue);
                } else if (!item.texts.empty ()) {
                    o.Add ("value", item.texts[0]);
                } else {
                    o.Add ("value", v.display);
                }
            } else {
                o.Add ("value", v.display);
            }
            o.Add ("display", v.display);
        }
        out (o);
    }
}

}   // namespace FindElementsELM
