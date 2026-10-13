#include "Create2DElementsCommand.hpp"
#include "MigrationHelper.hpp"
#include <vector>
#include <memory>
#include <cmath>
#include <cstdio>

// ELM_SAB 0.9.26 — Create2DElements. Aufbau nach Spec 261013-n2y Abschnitte 2-5 und 7:
// (1) Pruefen je Element ohne anzulegen, (2) Anlegen in EINEM Undo-Lambda, Status ausserhalb
// (Lehre 0.9.16), (3) Memos freigeben, (4) Ruecklese + Antwort NACH dem Lambda.
// Alle Helfer tragen das Praefix C2D (anonyme Helfer anderer TUs heissen teils gleich).

namespace {

constexpr double C2D_PI  = 3.14159265358979323846;
constexpr double C2D_EPS = 1e-4;     // 0,1 mm Toleranz fuer Punktvergleiche

enum class C2DKind { Unknown, Line, Arc, Circle, PolyLine, Spline, Hatch };

struct C2DArc {
    Int32  begIndex = 0;
    Int32  endIndex = 0;
    double arcAngle = 0.0;
};

struct C2DPrep {
    GSErrCode              err = NoError;
    GS::UniString          message;
    C2DKind                kind = C2DKind::Unknown;
    GS::UniString          typeName;
    API_Element            element = {};
    API_ElementMemo        memo = {};
    Int32                  wantFloor = 0;
    Int32                  wantLayer = 0;
    // Sollgeometrie
    API_Coord              wantBeg = {}, wantEnd = {}, wantOrigin = {};
    double                 wantRadius = 0.0;
    std::vector<API_Coord> wantCoords;   // PolyLine/Hatch inkl. Schlusspunkt (falls Ring), Spline 0-basiert
    bool                   wantRing = false;
    std::vector<Int32>     wantEnds;     // 0.9.28: Hatch-Teilpolygon-Enden (1-basiert, flache Liste), leer = ein Polygon
    API_Guid               created = APINULLGuid;
};

void C2DFail (C2DPrep& p, GSErrCode err, const GS::UniString& msg)
{
    p.err = err;
    p.message = msg;
}

GS::UniString C2DInt (Int32 v)
{
    char buffer[32] = {};
    std::snprintf (buffer, sizeof (buffer), "%d", (int) v);
    return GS::UniString (buffer);
}

GS::UniString C2DNum (double v)
{
    char buffer[64] = {};
    std::snprintf (buffer, sizeof (buffer), "%.6f", v);
    return GS::UniString (buffer);
}

bool C2DNear (const API_Coord& a, const API_Coord& b)
{
    return std::fabs (a.x - b.x) <= C2D_EPS && std::fabs (a.y - b.y) <= C2D_EPS;
}

double C2DDist (const API_Coord& a, const API_Coord& b)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return std::sqrt (dx * dx + dy * dy);
}

bool C2DReadNumber (const GS::ObjectState& os, const char* key, double& out)
{
    double v = 0.0;
    if (!os.Get (key, v))
        return false;
    if (!std::isfinite (v))
        return false;
    out = v;
    return true;
}

bool C2DReadCoord (const GS::ObjectState& os, API_Coord& c)
{
    double x = 0.0, y = 0.0;
    if (!C2DReadNumber (os, "x", x) || !C2DReadNumber (os, "y", y))
        return false;
    c.x = x;
    c.y = y;
    return true;
}

bool C2DReadCoordField (const GS::ObjectState& item, const char* key, API_Coord& c)
{
    const GS::ObjectState* os = item.Get (key);
    return os != nullptr && C2DReadCoord (*os, c);
}

C2DKind C2DKindOf (const GS::UniString& name)
{
    if (name == "Line")     return C2DKind::Line;
    if (name == "Arc")      return C2DKind::Arc;
    if (name == "Circle")   return C2DKind::Circle;
    if (name == "PolyLine") return C2DKind::PolyLine;
    if (name == "Spline")   return C2DKind::Spline;
    if (name == "Hatch")    return C2DKind::Hatch;
    return C2DKind::Unknown;
}

API_ElemTypeID C2DTypeIdOf (C2DKind kind)
{
    switch (kind) {
        case C2DKind::Line:     return API_LineID;
        case C2DKind::Arc:      return API_ArcID;
        case C2DKind::Circle:   return API_CircleID;
        case C2DKind::PolyLine: return API_PolyLineID;
        case C2DKind::Spline:   return API_SplineID;
        case C2DKind::Hatch:    return API_HatchID;
        default:                return API_ZombieElemID;
    }
}

GS::UniString C2DLayerNameOf (Int32 idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = API_LayerID;
    attr.header.index = ACAPI_CreateAttributeIndex (idx);
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

GS::UniString C2DFillNameOf (Int32 idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = API_FilltypeID;
    attr.header.index = ACAPI_CreateAttributeIndex (idx);
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

// Attribut (Ebene, Linientyp, Fuellung) per Name (Vorrang) oder Index aufloesen, mit Cache je Aufruf.
// given = false, wenn weder Name noch Index angegeben ist (dann gilt der Werkzeug-Standard).
GSErrCode C2DResolveAttr (API_AttrTypeID typeID, const char* what,
                          const GS::ObjectState& item, const char* nameKey, const char* indexKey, const char* indexAlias,
                          GS::HashTable<GS::UniString, Int32>& cache,
                          bool& given, Int32& out, GS::UniString& msg)
{
    given = false;
    GS::UniString name;
    if (item.Get (nameKey, name)) {
        given = true;
        const GS::UniString key = GS::UniString (what) + GS::UniString ("|") + name;
        if (cache.ContainsKey (key)) {
            out = cache.Get (key);
            return NoError;
        }
        API_Attr_Head head = {};
        head.typeID = typeID;
        head.uniStringNamePtr = &name;
        if (ACAPI_Attribute_Search (&head) != NoError) {
            msg = GS::UniString (what) + GS::UniString (" '") + name + GS::UniString ("' nicht gefunden");
            return APIERR_BADNAME;
        }
        out = (Int32) head.index.ToInt32_Deprecated ();
        cache.Add (key, out);
        return NoError;
    }

    Int32 idx = 0;
    bool hasIndex = item.Get (indexKey, idx);
    if (!hasIndex && indexAlias != nullptr)
        hasIndex = item.Get (indexAlias, idx);
    if (!hasIndex)
        return NoError;

    given = true;
    const GS::UniString key = GS::UniString (what) + GS::UniString ("#") + C2DInt (idx);
    if (cache.ContainsKey (key)) {
        out = cache.Get (key);
        return NoError;
    }
    API_Attribute attr = {};
    GS::UniString foundName;
    attr.header.typeID = typeID;
    attr.header.index = ACAPI_CreateAttributeIndex (idx);
    attr.header.uniStringNamePtr = &foundName;
    if (idx <= 0 || ACAPI_Attribute_Get (&attr) != NoError) {
        msg = GS::UniString (what) + GS::UniString ("-Index ") + C2DInt (idx) + GS::UniString (" nicht vorhanden");
        return APIERR_BADINDEX;
    }
    out = idx;
    cache.Add (key, out);
    return NoError;
}

bool C2DReadRGB (const GS::ObjectState& item, const char* key, bool& given, API_RGBColor& rgb)
{
    given = false;
    const GS::ObjectState* os = item.Get (key);
    if (os == nullptr)
        return true;
    given = true;
    double r = 0.0, g = 0.0, b = 0.0;
    if (!C2DReadNumber (*os, "red", r) || !C2DReadNumber (*os, "green", g) || !C2DReadNumber (*os, "blue", b))
        return false;
    if (r < 0.0 || r > 1.0 || g < 0.0 || g > 1.0 || b < 0.0 || b > 1.0)
        return false;
    rgb.f_red = r;
    rgb.f_green = g;
    rgb.f_blue = b;
    return true;
}

// Ganzzahliger Stift im Bereich [minPen, 255].
bool C2DReadPen (const GS::ObjectState& item, const char* key, const char* alias, Int32 minPen, bool& given, short& pen, GS::UniString& msg)
{
    given = false;
    Int32 v = 0;
    bool has = item.Get (key, v);
    if (!has && alias != nullptr)
        has = item.Get (alias, v);
    if (!has)
        return true;
    given = true;
    if (v < minPen || v > 255) {
        msg = GS::UniString (key) + GS::UniString (" ausserhalb ") + C2DInt (minPen) + GS::UniString ("..255: ") + C2DInt (v);
        return false;
    }
    pen = (short) v;
    return true;
}

// ---- Laengen ------------------------------------------------------------------------

double C2DNormSweep (double begAng, double endAng)
{
    double d = std::fmod (endAng - begAng, 2.0 * C2D_PI);
    if (d < 0.0)
        d += 2.0 * C2D_PI;
    return d;
}

// Ellipsenbogen-Laenge: Halbachsen a = r, b = r * ratio, Parameterwinkel von t0 ueber sweep.
// NAEHERUNG per Simpson-Integration mit 720 Schritten (keine geschlossene Form).
double C2DEllipseArcLength (double r, double ratio, double t0, double sweep)
{
    const double a = r;
    const double b = r * ratio;
    const int steps = 720;
    const double h = sweep / steps;
    double sum = 0.0;
    for (int i = 0; i <= steps; ++i) {
        const double t = t0 + i * h;
        const double f = std::sqrt (a * a * std::sin (t) * std::sin (t) + b * b * std::cos (t) * std::cos (t));
        const double w = (i == 0 || i == steps) ? 1.0 : ((i % 2 == 1) ? 4.0 : 2.0);
        sum += w * f;
    }
    return sum * h / 3.0;
}

double C2DArcLength (const API_ArcType& arc, bool whole)
{
    const bool circular = std::fabs (arc.ratio - 1.0) < 1e-9;
    if (whole) {
        if (circular)
            return 2.0 * C2D_PI * arc.r;
        // NAEHERUNG nach Ramanujan fuer den Ellipsenumfang.
        const double a = arc.r;
        const double b = arc.r * arc.ratio;
        return C2D_PI * (3.0 * (a + b) - std::sqrt ((3.0 * a + b) * (a + 3.0 * b)));
    }
    const double sweep = C2DNormSweep (arc.begAng, arc.endAng);
    if (circular)
        return arc.r * sweep;
    return C2DEllipseArcLength (arc.r, arc.ratio, arc.begAng, sweep);   // NAEHERUNG
}

// Polygonzug-Laenge (Punkte 0-basiert im Vektor, Bogen-Indizes 1-basiert wie im Memo).
double C2DPolyLength (const std::vector<API_Coord>& pts, const std::vector<C2DArc>& arcs)
{
    double len = 0.0;
    for (size_t i = 0; i + 1 < pts.size (); ++i) {
        const double c = C2DDist (pts[i], pts[i + 1]);
        double a = 0.0;
        for (const C2DArc& arc : arcs) {
            if (arc.begIndex == (Int32) (i + 1) && arc.endIndex == (Int32) (i + 2)) { a = std::fabs (arc.arcAngle); break; }
        }
        if (a > 1e-9 && std::sin (a / 2.0) > 1e-12)
            len += a * c / (2.0 * std::sin (a / 2.0));
        else
            len += c;
    }
    return len;
}

// ---- Koordinatenvergleich ---------------------------------------------------------------

// Offener Linienzug: gleiche Reihenfolge. Ring: auch zyklisch verschoben oder umgekehrt.
bool C2DCompareCoords (const std::vector<API_Coord>& want, const std::vector<API_Coord>& got, bool ring, GS::UniString& order)
{
    order = GS::UniString ("same");
    if (!ring) {
        if (want.size () != got.size ())
            return false;
        for (size_t i = 0; i < want.size (); ++i)
            if (!C2DNear (want[i], got[i]))
                return false;
        return true;
    }

    // Ringe: Schlusspunkt abschneiden, dann zyklisch vergleichen.
    std::vector<API_Coord> w (want), g (got);
    if (w.size () >= 2 && C2DNear (w.front (), w.back ())) w.pop_back ();
    if (g.size () >= 2 && C2DNear (g.front (), g.back ())) g.pop_back ();
    const size_t k = w.size ();
    if (k == 0 || k != g.size ())
        return false;

    for (size_t s = 0; s < k; ++s) {
        bool ok = true;
        for (size_t i = 0; i < k && ok; ++i)
            ok = C2DNear (w[i], g[(i + s) % k]);
        if (ok) {
            order = GS::UniString (s == 0 ? "same" : "shifted");
            return true;
        }
    }
    for (size_t s = 0; s < k; ++s) {
        bool ok = true;
        for (size_t i = 0; i < k && ok; ++i)
            ok = C2DNear (w[i], g[(s + k - i) % k]);
        if (ok) {
            order = GS::UniString ("reversed");
            return true;
        }
    }
    return false;
}

// Polygon-Memo (PolyLine/Hatch) 1-basiert auslesen.
// 0.9.28: Flache Liste in Teilpolygone schneiden (ends 1-basiert, letzter = Gesamtzahl).
std::vector<std::vector<API_Coord>> C2DSplitByEnds (const std::vector<API_Coord>& pts, const std::vector<Int32>& ends)
{
    std::vector<std::vector<API_Coord>> out;
    Int32 start = 0;
    for (Int32 e : ends) {
        if (e <= start || (size_t) e > pts.size ()) return {};
        out.push_back (std::vector<API_Coord> (pts.begin () + start, pts.begin () + e));
        start = e;
    }
    return out;
}

// 0.9.28: Teilpolygone vergleichen. Aussenkontur muss Aussenkontur bleiben, Loecher duerfen in
// beliebiger Reihenfolge zurueckkommen; jeder Ring darf verschoben/umgekehrt sein.
bool C2DCompareSubPolys (const std::vector<API_Coord>& want, const std::vector<Int32>& wantEnds,
                         const std::vector<API_Coord>& got, const std::vector<Int32>& gotEnds, GS::UniString& order);

bool C2DReadPolyMemo (const API_ElementMemo& memo, Int32 nCoords, Int32 nArcs, std::vector<API_Coord>& pts, std::vector<C2DArc>& arcs)
{
    if (memo.coords == nullptr || nCoords <= 0)
        return false;
    const GSSize have = BMhGetSize (reinterpret_cast<GSHandle> (memo.coords)) / (GSSize) sizeof (API_Coord);
    if (have < (GSSize) nCoords + 1)
        return false;
    for (Int32 i = 1; i <= nCoords; ++i)
        pts.push_back ((*memo.coords)[i]);
    if (memo.parcs != nullptr && nArcs > 0) {
        const GSSize haveArcs = BMhGetSize (reinterpret_cast<GSHandle> (memo.parcs)) / (GSSize) sizeof (API_PolyArc);
        for (Int32 a = 0; a < nArcs && (GSSize) a < haveArcs; ++a) {
            C2DArc arc;
            arc.begIndex = (*memo.parcs)[a].begIndex;
            arc.endIndex = (*memo.parcs)[a].endIndex;
            arc.arcAngle = (*memo.parcs)[a].arcAngle;
            arcs.push_back (arc);
        }
    }
    return true;
}

bool C2DCompareSubPolys (const std::vector<API_Coord>& want, const std::vector<Int32>& wantEnds,
                         const std::vector<API_Coord>& got, const std::vector<Int32>& gotEnds, GS::UniString& order)
{
    order = GS::UniString ("same");
    const auto w = C2DSplitByEnds (want, wantEnds);
    const auto g = C2DSplitByEnds (got, gotEnds);
    if (w.empty () || w.size () != g.size ())
        return false;
    GS::UniString o;
    if (!C2DCompareCoords (w[0], g[0], true, o))
        return false;
    bool anyChange = (o != GS::UniString ("same"));
    std::vector<bool> used (g.size (), false);
    used[0] = true;
    for (size_t i = 1; i < w.size (); ++i) {
        bool found = false;
        for (size_t j = 1; j < g.size () && !found; ++j) {
            if (used[j]) continue;
            if (C2DCompareCoords (w[i], g[j], true, o)) {
                used[j] = true;
                found = true;
                if (j != i || o != GS::UniString ("same")) anyChange = true;
            }
        }
        if (!found) return false;
    }
    if (anyChange) order = GS::UniString ("normalized");
    return true;
}

// ---- Pruefen + Vorbereiten ------------------------------------------------------------

struct C2DCaches {
    GS::HashTable<GS::UniString, Int32> attr;
};

// Liest coordinates[]; prueft Zahlen. false + msg bei Fehler.
bool C2DReadCoordList (const GS::ObjectState& item, std::vector<API_Coord>& pts, GS::UniString& msg)
{
    GS::Array<GS::ObjectState> list;
    if (!item.Get ("coordinates", list)) {
        msg = GS::UniString ("coordinates fehlt");
        return false;
    }
    for (UIndex i = 0; i < list.GetSize (); ++i) {
        API_Coord c = {};
        if (!C2DReadCoord (list[i], c)) {
            msg = GS::UniString ("coordinates[") + C2DInt ((Int32) i) + GS::UniString ("] braucht x und y (Zahlen)");
            return false;
        }
        pts.push_back (c);
    }
    return true;
}

bool C2DReadArcList (const GS::ObjectState& item, std::vector<C2DArc>& arcs, GS::UniString& msg)
{
    GS::Array<GS::ObjectState> list;
    if (!item.Get ("arcs", list))
        return true;
    for (UIndex i = 0; i < list.GetSize (); ++i) {
        C2DArc a;
        double ang = 0.0;
        if (!list[i].Get ("begIndex", a.begIndex) || !list[i].Get ("endIndex", a.endIndex) || !C2DReadNumber (list[i], "arcAngle", ang)) {
            msg = GS::UniString ("arcs[") + C2DInt ((Int32) i) + GS::UniString ("] braucht begIndex, endIndex, arcAngle");
            return false;
        }
        a.arcAngle = ang;
        arcs.push_back (a);
    }
    return true;
}

void C2DPrepareOne (const GS::ObjectState& item, C2DPrep& p, C2DCaches& caches, const Stories& stories)
{
    // -- Typ
    GS::UniString typeName;
    if (!item.Get ("elementType", typeName)) { C2DFail (p, APIERR_BADPARS, "elementType fehlt"); return; }
    p.typeName = typeName;
    p.kind = C2DKindOf (typeName);
    if (p.kind == C2DKind::Unknown) {
        C2DFail (p, APIERR_BADPARS, GS::UniString ("unbekannter elementType '") + typeName + GS::UniString ("' (Line, Arc, Circle, PolyLine, Spline, Hatch)"));
        return;
    }
    const bool isHatch = (p.kind == C2DKind::Hatch);
    GS::UniString msg;

    // -- Gemeinsame Felder. floorIndex/penIndex/ltypeIndex sind die Feldnamen des Lesebefehls
    //    (Rundweg Spec 1) und gelten als Rueckfall-Aliase.
    Int32 floorIn = 0;
    bool hasFloor = item.Get ("floorInd", floorIn);
    if (!hasFloor)
        hasFloor = item.Get ("floorIndex", floorIn);
    if (hasFloor && stories.count ((short) floorIn) == 0) {
        C2DFail (p, APIERR_BADINDEX, GS::UniString ("Geschoss ") + C2DInt (floorIn) + GS::UniString (" existiert nicht"));
        return;
    }

    Int32 drawIndex = 0;
    const bool hasDrawIndex = item.Get ("drawIndex", drawIndex);
    if (hasDrawIndex && (drawIndex < 1 || drawIndex > 7)) {
        C2DFail (p, APIERR_BADPARS, GS::UniString ("drawIndex ausserhalb 1..7: ") + C2DInt (drawIndex));
        return;
    }

    bool hasLayer = false;
    Int32 layerIdx = 0;
    GSErrCode aerr = C2DResolveAttr (API_LayerID, "Ebene", item, "layerName", "layerIndex", nullptr, caches.attr, hasLayer, layerIdx, msg);
    if (aerr != NoError) { C2DFail (p, aerr, msg); return; }

    // Linientyp: bei Hatch der Konturlinientyp (contourLineType*), sonst lineType*.
    bool hasLtype = false;
    Int32 ltypeIdx = 0;
    if (isHatch)
        aerr = C2DResolveAttr (API_LinetypeID, "Konturlinientyp", item, "contourLineTypeName", "contourLineTypeIndex", nullptr, caches.attr, hasLtype, ltypeIdx, msg);
    else
        aerr = C2DResolveAttr (API_LinetypeID, "Linientyp", item, "lineTypeName", "lineTypeIndex", "ltypeIndex", caches.attr, hasLtype, ltypeIdx, msg);
    if (aerr != NoError) { C2DFail (p, aerr, msg); return; }

    bool hasPen = false;
    short pen = 0;
    if (!isHatch && !C2DReadPen (item, "pen", "penIndex", 1, hasPen, pen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }

    // -- Schraffur-Felder
    bool hasFill = false, hasFgPen = false, hasBgPen = false, hasContPen = false, hasFgRGB = false, hasBgRGB = false;
    Int32 fillIdx = 0;
    short fgPen = 0, bgPen = 0, contPen = 0;
    API_RGBColor fgRGB = {}, bgRGB = {};
    bool showArea = false;
    if (isHatch) {
        aerr = C2DResolveAttr (API_FilltypeID, "Fuellung", item, "fillName", "fillIndex", nullptr, caches.attr, hasFill, fillIdx, msg);
        if (aerr != NoError) { C2DFail (p, aerr, msg); return; }
        if (!C2DReadPen (item, "foregroundPen", nullptr, 1, hasFgPen, fgPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadPen (item, "backgroundPen", nullptr, 0, hasBgPen, bgPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadPen (item, "contourPen", nullptr, 0, hasContPen, contPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadRGB (item, "foregroundRGB", hasFgRGB, fgRGB)) { C2DFail (p, APIERR_BADPARS, "foregroundRGB braucht red, green, blue je 0..1"); return; }
        if (!C2DReadRGB (item, "backgroundRGB", hasBgRGB, bgRGB)) { C2DFail (p, APIERR_BADPARS, "backgroundRGB braucht red, green, blue je 0..1"); return; }
        item.Get ("showArea", showArea);
    }

    // -- Geometrie pruefen
    double radius = 0.0, begAng = 0.0, endAng = 0.0, ratio = 1.0, angle = 0.0;
    bool reflected = false, closed = false;
    std::vector<API_Coord> input;
    std::vector<C2DArc> arcs;
    std::vector<API_SplineDir> dirs;
    bool hasDirs = false;

    switch (p.kind) {
        case C2DKind::Line:
            if (!C2DReadCoordField (item, "begCoordinate", p.wantBeg) || !C2DReadCoordField (item, "endCoordinate", p.wantEnd)) {
                C2DFail (p, APIERR_BADPARS, "Line braucht begCoordinate und endCoordinate {x,y}");
                return;
            }
            if (C2DNear (p.wantBeg, p.wantEnd)) { C2DFail (p, APIERR_BADPARS, "Line: Anfangs- und Endpunkt fallen zusammen"); return; }
            break;

        case C2DKind::Arc:
        case C2DKind::Circle: {
            const bool isArc = (p.kind == C2DKind::Arc);
            if (!C2DReadCoordField (item, "origin", p.wantOrigin) || !C2DReadNumber (item, "radius", radius)) {
                C2DFail (p, APIERR_BADPARS, isArc ? "Arc braucht origin, radius, begAngle, endAngle" : "Circle braucht origin und radius");
                return;
            }
            if (isArc && (!C2DReadNumber (item, "begAngle", begAng) || !C2DReadNumber (item, "endAngle", endAng))) {
                C2DFail (p, APIERR_BADPARS, "Arc braucht origin, radius, begAngle, endAngle");
                return;
            }
            if (radius <= 0.0) { C2DFail (p, APIERR_BADPARS, GS::UniString ("radius muss > 0 sein: ") + C2DNum (radius)); return; }
            if (item.Contains ("ratio") && !C2DReadNumber (item, "ratio", ratio)) { C2DFail (p, APIERR_BADPARS, "ratio ist keine Zahl"); return; }
            if (ratio <= 0.0) { C2DFail (p, APIERR_BADPARS, GS::UniString ("ratio muss > 0 sein: ") + C2DNum (ratio)); return; }
            if (item.Contains ("angle") && !C2DReadNumber (item, "angle", angle)) { C2DFail (p, APIERR_BADPARS, "angle ist keine Zahl"); return; }
            if (isArc) item.Get ("reflected", reflected);
            if (isArc && C2DNormSweep (begAng, endAng) <= 1e-9) { C2DFail (p, APIERR_BADPARS, "Arc: begAngle und endAngle fallen zusammen (Vollkreis = Circle)"); return; }
            p.wantRadius = radius;
            break;
        }

        case C2DKind::PolyLine:
        case C2DKind::Hatch: {
            if (!C2DReadCoordList (item, input, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
            if (!C2DReadArcList (item, arcs, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
            if (p.kind == C2DKind::PolyLine) item.Get ("closed", closed);

            const bool inputClosed = input.size () >= 2 && C2DNear (input.front (), input.back ());
            const size_t distinct = inputClosed ? input.size () - 1 : input.size ();
            const size_t minPts = isHatch ? 3 : 2;
            if (distinct < minPts) {
                C2DFail (p, APIERR_BADPARS, GS::UniString (isHatch ? "Hatch braucht mindestens 3 Punkte, hat " : "PolyLine braucht mindestens 2 Punkte, hat ") + C2DInt ((Int32) distinct));
                return;
            }
            // 0.9.28: Teilpolygone (Loecher) per subPolyEnds - so liefert sie Get2DGeometryOfElements.
            GS::Array<Int32> endsIn;
            const bool hasEnds = item.Get ("subPolyEnds", endsIn) && endsIn.GetSize () > 0;
            if (hasEnds && !isHatch && endsIn.GetSize () > 1) {
                C2DFail (p, APIERR_BADPARS, "subPolyEnds mit mehreren Teilpolygonen geht nur bei Hatch");
                return;
            }
            if (isHatch && hasEnds && endsIn.GetSize () > 1) {
                Int32 start = 0;
                for (UIndex e = 0; e < endsIn.GetSize (); ++e) {
                    const Int32 end = endsIn[e];
                    if (end <= start || (size_t) end > input.size ()) {
                        C2DFail (p, APIERR_BADPARS, GS::UniString ("subPolyEnds[") + C2DInt ((Int32) e) + GS::UniString ("] = ") + C2DInt (end)
                            + GS::UniString (": muss steigen und <= Punktzahl ") + C2DInt ((Int32) input.size ()) + GS::UniString (" sein"));
                        return;
                    }
                    if (end - start < 4 || !C2DNear (input[(size_t) start], input[(size_t) end - 1])) {
                        C2DFail (p, APIERR_BADPARS, GS::UniString ("Teilpolygon ") + C2DInt ((Int32) e + 1) + GS::UniString (" (Punkte ") + C2DInt (start + 1)
                            + GS::UniString ("..") + C2DInt (end) + GS::UniString (") muss geschlossen sein und mindestens 3 verschiedene Punkte haben"));
                        return;
                    }
                    p.wantEnds.push_back (end);
                    start = end;
                }
                if ((size_t) start != input.size ()) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("letzter subPolyEnds-Wert (") + C2DInt (start) + GS::UniString (") muss gleich der Punktzahl (")
                        + C2DInt ((Int32) input.size ()) + GS::UniString (") sein"));
                    return;
                }
                for (size_t a = 0; a < arcs.size (); ++a) {
                    bool inside = false;
                    Int32 st = 0;
                    for (Int32 end : p.wantEnds) {
                        if (arcs[a].begIndex > st && arcs[a].endIndex <= end) { inside = true; break; }
                        st = end;
                    }
                    if (!inside) {
                        C2DFail (p, APIERR_BADPARS, GS::UniString ("arcs[") + C2DInt ((Int32) a) + GS::UniString ("] liegt nicht innerhalb eines Teilpolygons"));
                        return;
                    }
                }
            } else if (isHatch) {
                // Ohne subPolyEnds: Loch erkennen (innerer Punkt wiederholt den Startpunkt) und klar ablehnen.
                const size_t lastInner = inputClosed ? input.size () - 1 : input.size ();
                for (size_t i = 1; i < lastInner; ++i) {
                    if (C2DNear (input[i], input.front ())) {
                        C2DFail (p, APIERR_BADPARS, GS::UniString ("Hatch mit Teilpolygonen/Loechern: Punkt ") + C2DInt ((Int32) i + 1)
                            + GS::UniString (" wiederholt den Startpunkt - subPolyEnds mitgeben (wie Get2DGeometryOfElements) oder nur die Aussenkontur uebergeben."));
                        return;
                    }
                }
            }
            // Soll-Liste: bei Hatch immer, bei PolyLine closed:true Schlusspunkt anhaengen - ausser er ist schon da.
            p.wantCoords = input;
            if ((isHatch || closed) && !inputClosed && p.wantEnds.empty ())
                p.wantCoords.push_back (input.front ());
            p.wantRing = isHatch || closed || inputClosed;

            for (size_t i = 0; i + 1 < p.wantCoords.size (); ++i) {
                bool boundary = false;
                for (Int32 end : p.wantEnds) if ((size_t) end == i + 1) { boundary = true; break; }
                if (boundary) continue;
                if (C2DNear (p.wantCoords[i], p.wantCoords[i + 1])) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("doppelter Punkt an coordinates[") + C2DInt ((Int32) i) + GS::UniString ("]/[") + C2DInt ((Int32) i + 1) + GS::UniString ("] (Abstand <= 0,1 mm)"));
                    return;
                }
            }
            const Int32 n = (Int32) p.wantCoords.size ();
            for (size_t a = 0; a < arcs.size (); ++a) {
                if (arcs[a].begIndex < 1 || arcs[a].begIndex >= arcs[a].endIndex || arcs[a].endIndex > n) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("arcs[") + C2DInt ((Int32) a) + GS::UniString ("]: Indizes 1-basiert, 1 <= begIndex < endIndex <= ") + C2DInt (n));
                    return;
                }
                if (std::fabs (arcs[a].arcAngle) <= 1e-9) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("arcs[") + C2DInt ((Int32) a) + GS::UniString ("]: arcAngle darf nicht 0 sein"));
                    return;
                }
            }
            break;
        }

        case C2DKind::Spline: {
            if (!C2DReadCoordList (item, input, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
            item.Get ("closed", closed);
            if (input.size () < 2) { C2DFail (p, APIERR_BADPARS, GS::UniString ("Spline braucht mindestens 2 Punkte, hat ") + C2DInt ((Int32) input.size ())); return; }
            if (C2DNear (input.front (), input.back ())) {
                C2DFail (p, APIERR_BADPARS, "Spline: erster und letzter Punkt muessen verschieden sein (geschlossen: closed:true)");
                return;
            }
            for (size_t i = 0; i + 1 < input.size (); ++i) {
                if (C2DNear (input[i], input[i + 1])) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("doppelter Punkt an coordinates[") + C2DInt ((Int32) i) + GS::UniString ("]/[") + C2DInt ((Int32) i + 1) + GS::UniString ("] (Abstand <= 0,1 mm)"));
                    return;
                }
            }
            GS::Array<GS::ObjectState> dirList;
            if (item.Get ("bezierDirs", dirList)) {
                hasDirs = true;
                if (dirList.GetSize () != (UIndex) input.size ()) {
                    C2DFail (p, APIERR_BADPARS, GS::UniString ("bezierDirs braucht genau einen Eintrag je Punkt: ") + C2DInt ((Int32) dirList.GetSize ()) + GS::UniString (" statt ") + C2DInt ((Int32) input.size ()));
                    return;
                }
                for (UIndex i = 0; i < dirList.GetSize (); ++i) {
                    API_SplineDir d = {};
                    if (!C2DReadNumber (dirList[i], "lenPrev", d.lenPrev) || !C2DReadNumber (dirList[i], "lenNext", d.lenNext) || !C2DReadNumber (dirList[i], "dirAng", d.dirAng)) {
                        C2DFail (p, APIERR_BADPARS, GS::UniString ("bezierDirs[") + C2DInt ((Int32) i) + GS::UniString ("] braucht lenPrev, lenNext, dirAng"));
                        return;
                    }
                    dirs.push_back (d);
                }
            }
            p.wantCoords = input;
            p.wantRing = false;     // Spline: nur Punktabgleich in Eingabereihenfolge (Spec 4.4)
            break;
        }

        default:
            break;
    }

    // -- Werkzeug-Default holen (einmal Retry wie CreateLabels), Default-Memo verwerfen.
    p.element = {};
    p.element.header.type = C2DTypeIdOf (p.kind);
    GSErrCode defErr = ACAPI_Element_GetDefaults (&p.element, &p.memo);
    if (defErr != NoError) {
        ACAPI_DisposeElemMemoHdls (&p.memo);
        p.memo = {};
        p.element = {};
        p.element.header.type = C2DTypeIdOf (p.kind);
        defErr = ACAPI_Element_GetDefaults (&p.element, &p.memo);
    }
    ACAPI_DisposeElemMemoHdls (&p.memo);
    p.memo = {};
    if (defErr != NoError) { C2DFail (p, defErr, GS::UniString ("Werkzeug-Default fuer ") + typeName + GS::UniString (" nicht lesbar")); return; }

    // -- Gemeinsame Felder setzen
    if (hasFloor)     p.element.header.floorInd = (short) floorIn;
    if (hasLayer)     p.element.header.layer = ACAPI_CreateAttributeIndex (layerIdx);
    if (hasDrawIndex) p.element.header.drwIndex = (char) drawIndex;
    p.wantFloor = (Int32) p.element.header.floorInd;
    p.wantLayer = (Int32) p.element.header.layer.ToInt32_Deprecated ();

    switch (p.kind) {
        case C2DKind::Line:
            if (hasPen)   { p.element.line.linePen.penIndex = pen; p.element.line.linePen.colorOverridePenIndex = 0; }
            if (hasLtype) p.element.line.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
            p.element.line.begC = p.wantBeg;
            p.element.line.endC = p.wantEnd;
            break;

        case C2DKind::Arc:
            if (hasPen)   { p.element.arc.linePen.penIndex = pen; p.element.arc.linePen.colorOverridePenIndex = 0; }
            if (hasLtype) p.element.arc.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
            p.element.arc.origC = p.wantOrigin;
            p.element.arc.r = radius;
            p.element.arc.begAng = begAng;
            p.element.arc.endAng = endAng;
            p.element.arc.ratio = ratio;
            p.element.arc.angle = angle;
            p.element.arc.reflected = reflected;
            p.element.arc.whole = false;
            break;

        case C2DKind::Circle:
            // Kreis = API_CircleID mit API_ArcType-Feldern (using API_CircleType = API_ArcType).
            if (hasPen)   { p.element.circle.linePen.penIndex = pen; p.element.circle.linePen.colorOverridePenIndex = 0; }
            if (hasLtype) p.element.circle.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
            p.element.circle.origC = p.wantOrigin;
            p.element.circle.r = radius;
            p.element.circle.ratio = ratio;
            p.element.circle.angle = angle;
            p.element.circle.reflected = false;
            p.element.circle.whole = true;
            p.element.circle.begAng = 0.0;
            p.element.circle.endAng = 2.0 * C2D_PI;
            break;

        case C2DKind::PolyLine:
        case C2DKind::Hatch: {
            API_Polygon& poly = isHatch ? p.element.hatch.poly : p.element.polyLine.poly;
            const Int32 n = (Int32) p.wantCoords.size ();
            const Int32 nSub = p.wantEnds.empty () ? 1 : (Int32) p.wantEnds.size ();
            poly.nCoords   = n;
            poly.nSubPolys = nSub;
            poly.nArcs     = (Int32) arcs.size ();

            // HARTE FALLE: Polygon-Memo 1-basiert, Groesse n+1 (Index 0 bleibt leer), pends[1] = n.
            p.memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((n + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
            p.memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((nSub + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
            if (p.memo.coords == nullptr || p.memo.pends == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return; }
            for (Int32 i = 0; i < n; ++i)
                (*p.memo.coords)[i + 1] = p.wantCoords[(size_t) i];
            (*p.memo.pends)[0] = 0;
            if (p.wantEnds.empty ())
                (*p.memo.pends)[1] = n;
            else
                for (Int32 sIdx = 1; sIdx <= nSub; ++sIdx)
                    (*p.memo.pends)[sIdx] = p.wantEnds[(size_t) sIdx - 1];
            if (!arcs.empty ()) {
                // parcs 0-basiert; begIndex/endIndex sind schon 1-basiert (wie der Lesebefehl sie
                // ausgibt) und werden UNVERAENDERT uebernommen - NICHT Tapirs GetPolyArcs (addiert iStart).
                p.memo.parcs = reinterpret_cast<API_PolyArc**> (BMAllocateHandle ((GSSize) arcs.size () * sizeof (API_PolyArc), ALLOCATE_CLEAR, 0));
                if (p.memo.parcs == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return; }
                for (size_t a = 0; a < arcs.size (); ++a) {
                    (*p.memo.parcs)[a].begIndex = arcs[a].begIndex;
                    (*p.memo.parcs)[a].endIndex = arcs[a].endIndex;
                    (*p.memo.parcs)[a].arcAngle = arcs[a].arcAngle;
                }
            }

            if (!isHatch) {
                if (hasPen)   { p.element.polyLine.linePen.penIndex = pen; p.element.polyLine.linePen.colorOverridePenIndex = 0; }
                if (hasLtype) p.element.polyLine.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
                break;
            }

            // Schraffur (Spec 3.3)
            p.element.hatch.hatchType = API_FillHatch;
            if (hasFill)    p.element.hatch.fillInd = ACAPI_CreateAttributeIndex (fillIdx);
            if (hasFgPen)   { p.element.hatch.fillPen.penIndex = fgPen; p.element.hatch.fillPen.colorOverridePenIndex = 0; }
            if (hasBgPen)   p.element.hatch.fillBGPen = bgPen;
            if (hasContPen) { p.element.hatch.contPen.penIndex = contPen; p.element.hatch.contPen.colorOverridePenIndex = 0; }
            if (hasLtype)   p.element.hatch.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
            p.element.hatch.showArea = showArea;
            // HARTE FALLE: RGB-Flags ZUERST unbedingt loeschen - kein Werkzeug-Default-Override darf durchrutschen.
            p.element.hatch.hatchFlags &= ~((UInt32) (APIHatch_HasFgRGBColor | APIHatch_HasBkgRGBColor));
            if (hasFgRGB) {
                p.element.hatch.hatchFlags |= (UInt32) APIHatch_HasFgRGBColor;
                p.element.hatch.foregroundRGB.f_red   = fgRGB.f_red;
                p.element.hatch.foregroundRGB.f_green = fgRGB.f_green;
                p.element.hatch.foregroundRGB.f_blue  = fgRGB.f_blue;
            }
            if (hasBgRGB) {
                p.element.hatch.hatchFlags |= (UInt32) APIHatch_HasBkgRGBColor;
                p.element.hatch.backgroundRGB.f_red   = bgRGB.f_red;
                p.element.hatch.backgroundRGB.f_green = bgRGB.f_green;
                p.element.hatch.backgroundRGB.f_blue  = bgRGB.f_blue;
            }
            break;
        }

        case C2DKind::Spline: {
            if (hasPen)   { p.element.spline.linePen.penIndex = pen; p.element.spline.linePen.colorOverridePenIndex = 0; }
            if (hasLtype) p.element.spline.ltypeInd = ACAPI_CreateAttributeIndex (ltypeIdx);
            p.element.spline.closed = closed;
            const GSSize n = (GSSize) p.wantCoords.size ();
            // HARTE FALLE: Spline-coords 0-BASIERT, Groesse exakt n (kein Leerplatz, kein Schlusspunkt).
            p.memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle (n * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
            if (p.memo.coords == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return; }
            for (GSSize i = 0; i < n; ++i)
                (*p.memo.coords)[i] = p.wantCoords[(size_t) i];
            if (hasDirs) {
                p.memo.bezierDirs = reinterpret_cast<API_SplineDir**> (BMAllocateHandle (n * sizeof (API_SplineDir), ALLOCATE_CLEAR, 0));
                if (p.memo.bezierDirs == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return; }
                for (GSSize i = 0; i < n; ++i)
                    (*p.memo.bezierDirs)[i] = dirs[(size_t) i];
                p.element.spline.autoSmooth = false;
            } else {
                // Ohne bezierDirs: autoSmooth, bezierDirs = nullptr (ACAPinc Create-Tabelle). Rueckfall,
                // falls Archicad das ablehnt: Richtungen selbst per Geometry::CalcSpline berechnen
                // (DevKit Element_Basics.cpp, Do_CreateSpline) - hier bewusst nicht eingebaut.
                p.element.spline.autoSmooth = true;
                p.memo.bezierDirs = nullptr;
            }
            break;
        }

        default:
            break;
    }
}

// ---- Ruecklese ----------------------------------------------------------------------

GS::ObjectState C2DReadBack (const C2DPrep& p)
{
    GS::ObjectState r;
    API_Element check = {};
    check.header.guid = p.created;
    if (ACAPI_Element_Get (&check) != NoError) {
        GS::ObjectState err;
        err.Add ("code", (GSErrCode) APIERR_GENERAL);
        err.Add ("message", GS::UniString ("Ruecklese fehlgeschlagen (Element angelegt, aber nicht lesbar)"));
        r.Add ("success", false);
        r.Add ("elementId", CreateGuidObjectStateELM (p.created));
        r.Add ("elementType", p.typeName);
        r.Add ("error", err);
        return r;
    }

    const Int32 istFloor = (Int32) check.header.floorInd;
    const Int32 istLayer = (Int32) check.header.layer.ToInt32_Deprecated ();
    // 0.9.27: Archicad speichert einen Kreis als API_ArcID mit whole=true (live WiSchu 13.10.).
    const bool typeMatches  = (p.kind == C2DKind::Circle)
        ? (check.header.type.typeID == API_CircleID || (check.header.type.typeID == API_ArcID && check.arc.whole))
        : check.header.type.typeID == C2DTypeIdOf (p.kind);
    const bool floorMatches = istFloor == p.wantFloor;
    const bool layerMatches = istLayer == p.wantLayer;
    bool coordinatesMatch = false;
    GS::UniString order;
    bool reportOrder = false;
    GS::UniString detail;

    r.Add ("elementId", CreateGuidObjectStateELM (p.created));
    r.Add ("elementType", p.typeName);
    r.Add ("floorIndex", istFloor);
    r.Add ("layerIndex", istLayer);
    r.Add ("layerName", C2DLayerNameOf (istLayer));

    if (typeMatches) {
        switch (p.kind) {
            case C2DKind::Line:
                coordinatesMatch = C2DNear (check.line.begC, p.wantBeg) && C2DNear (check.line.endC, p.wantEnd);
                r.Add ("length", C2DDist (check.line.begC, check.line.endC));
                break;

            case C2DKind::Arc:
            case C2DKind::Circle:
                coordinatesMatch = C2DNear (check.arc.origC, p.wantOrigin) && std::fabs (check.arc.r - p.wantRadius) <= C2D_EPS;
                r.Add ("length", C2DArcLength (check.arc, p.kind == C2DKind::Circle || check.arc.whole));
                if (std::fabs (check.arc.ratio - 1.0) >= 1e-9)
                    r.Add ("lengthApproximate", true);
                break;

            case C2DKind::PolyLine:
            case C2DKind::Hatch: {
                const bool isHatch = (p.kind == C2DKind::Hatch);
                const API_Polygon& poly = isHatch ? check.hatch.poly : check.polyLine.poly;
                API_ElementMemo memo = {};
                std::vector<API_Coord> pts;
                std::vector<C2DArc> arcs;
                const GSErrCode merr = ACAPI_Element_GetMemo (p.created, &memo, APIMemoMask_Polygon);
                const bool readOk = merr == NoError && C2DReadPolyMemo (memo, poly.nCoords, poly.nArcs, pts, arcs);
                std::vector<Int32> gotEnds;
                if (readOk && memo.pends != nullptr)
                    for (Int32 sIdx = 1; sIdx <= poly.nSubPolys; ++sIdx)
                        gotEnds.push_back ((*memo.pends)[sIdx]);
                ACAPI_DisposeElemMemoHdls (&memo);
                if (!readOk) {
                    detail = GS::UniString ("Polygon-Memo nicht lesbar");
                    break;
                }
                if (!p.wantEnds.empty () || gotEnds.size () > 1) {
                    std::vector<Int32> wantE = p.wantEnds.empty () ? std::vector<Int32> (1, (Int32) p.wantCoords.size ()) : p.wantEnds;
                    coordinatesMatch = C2DCompareSubPolys (p.wantCoords, wantE, pts, gotEnds, order);
                    const auto& endsOut = r.AddList<Int32> ("subPolyEnds");
                    for (Int32 e : gotEnds) endsOut (e);
                } else {
                    coordinatesMatch = C2DCompareCoords (p.wantCoords, pts, p.wantRing, order);
                }
                reportOrder = p.wantRing;
                if (!isHatch) {
                    r.Add ("length", C2DPolyLength (pts, arcs));
                    break;
                }
                // Flaeche/Umfang aus Archicads Mengenberechnung.
                API_ElementQuantity q = {};
                API_Quantities quantities;
                quantities.elements = &q;
                API_QuantityPar qp = {};
                qp.minOpeningSize = C2D_EPS;
                API_QuantitiesMask mask;
                ACAPI_ELEMENT_QUANTITY_MASK_CLEAR (mask);
                ACAPI_ELEMENT_QUANTITY_MASK_SET (mask, hatch, surface);
                ACAPI_ELEMENT_QUANTITY_MASK_SET (mask, hatch, perimeter);
                ACAPI_ELEMENT_QUANTITY_MASK_SET (mask, hatch, holesSurf);
                const GSErrCode qerr = ACAPI_Element_GetQuantities (p.created, &qp, &quantities, &mask);
                if (qerr == NoError) {
                    r.Add ("area", q.hatch.surface);
                    r.Add ("perimeter", q.hatch.perimeter);
                    if (!p.wantEnds.empty ()) r.Add ("holesArea", q.hatch.holesSurf);
                } else {
                    r.Add ("quantityError", (Int32) qerr);     // success haengt NICHT davon ab
                }
                const Int32 fillInd = (Int32) check.hatch.fillInd.ToInt32_Deprecated ();
                r.Add ("fillIndex", fillInd);
                r.Add ("fillName", C2DFillNameOf (fillInd));
                r.Add ("hasForegroundRGB", (check.hatch.hatchFlags & APIHatch_HasFgRGBColor) != 0);
                r.Add ("hasBackgroundRGB", (check.hatch.hatchFlags & APIHatch_HasBkgRGBColor) != 0);
                break;
            }

            case C2DKind::Spline: {
                API_ElementMemo memo = {};
                std::vector<API_Coord> pts;
                const GSErrCode merr = ACAPI_Element_GetMemo (p.created, &memo, APIMemoMask_Polygon);
                if (merr == NoError && memo.coords != nullptr) {
                    const GSSize n = BMhGetSize (reinterpret_cast<GSHandle> (memo.coords)) / (GSSize) sizeof (API_Coord);
                    for (GSSize i = 0; i < n; ++i)          // Spline 0-basiert
                        pts.push_back ((*memo.coords)[i]);
                }
                ACAPI_DisposeElemMemoHdls (&memo);
                if (merr != NoError) {
                    detail = GS::UniString ("Spline-Memo nicht lesbar");
                    break;
                }
                coordinatesMatch = C2DCompareCoords (p.wantCoords, pts, false, order);
                r.Add ("closed", check.spline.closed);
                r.Add ("autoSmooth", check.spline.autoSmooth);
                break;
            }

            default:
                break;
        }
    }

    r.Add ("floorMatches", floorMatches);
    r.Add ("layerMatches", layerMatches);
    r.Add ("coordinatesMatch", coordinatesMatch);
    if (reportOrder && coordinatesMatch)
        r.Add ("coordinateOrder", order);

    const bool ok = typeMatches && floorMatches && layerMatches && coordinatesMatch;
    r.Add ("success", ok);
    if (!ok) {
        // Element existiert - Aufrufer braucht elementId + Ist-Werte, um es zu loeschen.
        GS::UniString what;
        if (!typeMatches)      what += GS::UniString (" Typ");
        if (!floorMatches)     what += GS::UniString (" Geschoss (Soll ") + C2DInt (p.wantFloor) + GS::UniString (")");
        if (!layerMatches)     what += GS::UniString (" Ebene (Soll ") + C2DInt (p.wantLayer) + GS::UniString (")");
        if (!coordinatesMatch) what += GS::UniString (" Koordinaten");
        if (!detail.IsEmpty ()) what += GS::UniString (" - ") + detail;
        GS::ObjectState err;
        err.Add ("code", (GSErrCode) APIERR_GENERAL);
        err.Add ("message", GS::UniString (GS::UniString ("Ruecklese weicht ab:") + what));
        r.Add ("error", err);
    }
    return r;
}

} // namespace

GS::Optional<GS::UniString> Create2DElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "description": "2D-Elemente fuer die aktive Grundriss-Datenbank. Ausgabe von Get2DGeometryOfElements ist direkt verwendbar.",
                "items": { "type": "object",
                    "properties": {
                        "elementType": { "type": "string", "enum": ["Line", "Arc", "Circle", "PolyLine", "Spline", "Hatch"] },
                        "floorInd": { "type": "integer", "description": "Geschoss. Fehlt er, gilt das aktuelle Geschoss (Werkzeug-Default). Alias floorIndex." },
                        "floorIndex": { "type": "integer", "description": "Alias fuer floorInd (Feldname des Lesebefehls)." },
                        "layerName": { "type": "string", "description": "Ebene per Name (Vorrang vor layerIndex)." },
                        "layerIndex": { "type": "integer", "description": "Ebene per Attributindex." },
                        "pen": { "type": "integer", "description": "Linienstift (nicht Hatch). Alias penIndex." },
                        "penIndex": { "type": "integer", "description": "Alias fuer pen (Feldname des Lesebefehls)." },
                        "lineTypeName": { "type": "string", "description": "Linientyp per Name (nicht Hatch)." },
                        "lineTypeIndex": { "type": "integer", "description": "Linientyp per Index (nicht Hatch). Alias ltypeIndex." },
                        "ltypeIndex": { "type": "integer", "description": "Alias fuer lineTypeIndex (Feldname des Lesebefehls)." },
                        "drawIndex": { "type": "integer", "description": "Anzeigereihenfolge 1..7 (Pruefung je Element)." },
                        "begCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                        "endCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                        "origin": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                        "radius": { "type": "number", "description": "Radius in m (> 0)." },
                        "begAngle": { "type": "number", "description": "Anfangswinkel in Radiant (Arc)." },
                        "endAngle": { "type": "number", "description": "Endwinkel in Radiant (Arc)." },
                        "ratio": { "type": "number", "description": "Achsverhaeltnis der Ellipse, Standard 1." },
                        "angle": { "type": "number", "description": "Drehung der Hauptachse in Radiant, Standard 0." },
                        "reflected": { "type": "boolean" },
                        "closed": { "type": "boolean", "description": "PolyLine: Schlusspunkt anhaengen. Spline: geschlossen." },
                        "showArea": { "type": "boolean", "description": "Hatch: Flaeche anzeigen, Standard false." },
                        "coordinates": { "type": "array", "items": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                            "description": "PolyLine/Hatch/Spline-Punkte in m, mit oder ohne Schlusspunkt." },
                        "arcs": { "type": "array", "items": { "type": "object", "properties": {
                                "begIndex": { "type": "integer" }, "endIndex": { "type": "integer" }, "arcAngle": { "type": "number" } } },
                            "description": "Bogensegmente, Indizes 1-basiert wie im Lesebefehl." },
                        "subPolyEnds": { "type": "array", "items": { "type": "integer" },
                            "description": "Hatch: Teilpolygon-Enden 1-basiert (Aussenkontur zuerst, dann Loecher, jedes geschlossen) - wie Get2DGeometryOfElements." },
                        "bezierDirs": { "type": "array", "items": { "type": "object", "properties": {
                                "lenPrev": { "type": "number" }, "lenNext": { "type": "number" }, "dirAng": { "type": "number" } } },
                            "description": "Spline: genau ein Eintrag je Punkt; fehlt das Feld, gilt autoSmooth." },
                        "fillName": { "type": "string", "description": "Hatch: Fuellung per Name (Vorrang vor fillIndex)." },
                        "fillIndex": { "type": "integer", "description": "Hatch: Fuellung per Index." },
                        "foregroundPen": { "type": "integer", "description": "Hatch: Vordergrundstift." },
                        "backgroundPen": { "type": "integer", "description": "Hatch: Hintergrundstift (0 = transparent)." },
                        "contourPen": { "type": "integer", "description": "Hatch: Konturstift (0 = keine Kontur)." },
                        "foregroundRGB": { "type": "object", "properties": { "red": { "type": "number" }, "green": { "type": "number" }, "blue": { "type": "number" } },
                            "description": "Hatch: Vordergrund-RGB je 0..1. Ohne Angabe wird das RGB-Flag geloescht." },
                        "backgroundRGB": { "type": "object", "properties": { "red": { "type": "number" }, "green": { "type": "number" }, "blue": { "type": "number" } },
                            "description": "Hatch: Hintergrund-RGB je 0..1. Ohne Angabe wird das RGB-Flag geloescht." },
                        "contourLineTypeName": { "type": "string", "description": "Hatch: Konturlinientyp per Name." },
                        "contourLineTypeIndex": { "type": "integer", "description": "Hatch: Konturlinientyp per Index." }
                    },
                    "required": ["elementType"] } }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> Create2DElementsCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (GS::UniString (R"({"type":"object","properties":{"executionResults":{"type":"array","items":{"type":"object"}},"undoScope":{"type":"object"}},"required":["executionResults"]})"));
}

GS::ObjectState Create2DElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    if (!parameters.Get ("elements", items) || items.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements fehlt oder ist leer - erwartet wird eine Liste von 2D-Elementen (elementType + Geometrie)."));

    // 1) Pruefen + Vorbereiten VOR dem Undo-Lambda.
    const Stories stories = GetStories ();
    C2DCaches caches;
    std::vector<std::unique_ptr<C2DPrep>> preps;
    for (const GS::ObjectState& item : items) {
        preps.push_back (std::unique_ptr<C2DPrep> (new C2DPrep ()));
        C2DPrepareOne (item, *preps.back (), caches, stories);
    }

    // 2) Anlegen in EINEM Undo-Schritt; Status liegt ausserhalb (Lehre 0.9.16).
    //    Keine Teamwork-Reservierung: neue Elemente gehoeren dem Anleger.
    std::vector<GSErrCode> createErr (preps.size (), NoError);
    bool lambdaStarted = false;
    ACAPI_CallUndoableCommand ("ELM_SAB Create2DElements", [&] () -> GSErrCode {
        lambdaStarted = true;
        for (size_t i = 0; i < preps.size (); ++i) {
            C2DPrep& p = *preps[i];
            if (p.err != NoError) continue;
            createErr[i] = ACAPI_Element_Create (&p.element, &p.memo);
            if (createErr[i] == NoError)
                p.created = p.element.header.guid;
        }
        return NoError;
    });

    // 3) Memos ALLER Preps freigeben (auch fehlerhafte/uebersprungene).
    for (auto& p : preps) {
        ACAPI_DisposeElemMemoHdls (&p->memo);
        p->memo = {};
    }

    // 4) Ruecklese + Antwort NACH dem Lambda, in Eingabereihenfolge.
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < preps.size (); ++i) {
        const C2DPrep& p = *preps[i];
        if (p.err != NoError) {
            results (CreateFailedExecutionResult (p.err, p.message));
            continue;
        }
        if (!lambdaStarted) {
            results (CreateFailedExecutionResult (APIERR_GENERAL, "Undo-Lambda wurde nicht gestartet - nichts angelegt"));
            continue;
        }
        if (createErr[i] != NoError) {
            results (CreateFailedExecutionResult (createErr[i], p.typeName + GS::UniString (" konnte nicht angelegt werden (ACAPI_Element_Create)")));
            continue;
        }
        results (C2DReadBack (p));
    }

    GS::ObjectState undoScope;
    undoScope.Add ("lambdaStarted", lambdaStarted);
    response.Add ("undoScope", undoScope);
    return response;
}
