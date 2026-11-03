#include "Create2DElementsCommand.hpp"
#include "MigrationHelper.hpp"
#include "ElmPreflight.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <cstring>
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

// ---- 0.9.34: gemeinsame Pfad-Geometrie (Create2DElements + Modify2DElements) ------------

struct C2DGeom {
    std::vector<C2DArc>        arcs;
    std::vector<API_SplineDir> dirs;
    bool                       hasDirs = false;
    bool                       closed = false;
};

// PolyLine/Hatch/Spline: coordinates (+ arcs, subPolyEnds, closed, bezierDirs) pruefen und die
// Sollgeometrie in p.wantCoords/wantEnds/wantRing ablegen. Fehler per C2DFail (p.err).
void C2DParsePathGeometry (const GS::ObjectState& item, C2DPrep& p, C2DGeom& g)
{
    const bool isHatch = (p.kind == C2DKind::Hatch);
    GS::UniString msg;
    std::vector<API_Coord> input;
    std::vector<C2DArc>& arcs = g.arcs;
    std::vector<API_SplineDir>& dirs = g.dirs;
    bool& hasDirs = g.hasDirs;
    bool& closed = g.closed;

    switch (p.kind) {
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
}

// Polygon-Memo (PolyLine/Hatch) aus p.wantCoords/wantEnds + arcs bauen; setzt die Zaehler in poly.
bool C2DBuildPolyMemo (C2DPrep& p, API_Polygon& poly, const std::vector<C2DArc>& arcs)
{
    const Int32 n = (Int32) p.wantCoords.size ();
    const Int32 nSub = p.wantEnds.empty () ? 1 : (Int32) p.wantEnds.size ();
    poly.nCoords   = n;
    poly.nSubPolys = nSub;
    poly.nArcs     = (Int32) arcs.size ();

    // HARTE FALLE: Polygon-Memo 1-basiert, Groesse n+1 (Index 0 bleibt leer), pends[1] = n.
    p.memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle ((n + 1) * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
    p.memo.pends  = reinterpret_cast<Int32**> (BMAllocateHandle ((nSub + 1) * sizeof (Int32), ALLOCATE_CLEAR, 0));
    if (p.memo.coords == nullptr || p.memo.pends == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return false; }
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
        if (p.memo.parcs == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return false; }
        for (size_t a = 0; a < arcs.size (); ++a) {
            (*p.memo.parcs)[a].begIndex = arcs[a].begIndex;
            (*p.memo.parcs)[a].endIndex = arcs[a].endIndex;
            (*p.memo.parcs)[a].arcAngle = arcs[a].arcAngle;
        }
    }
    return true;
}

// Spline-Memo (0-basiert) aus p.wantCoords + bezierDirs bauen; setzt autoSmooth.
bool C2DBuildSplineMemo (C2DPrep& p, const C2DGeom& g)
{
    const std::vector<API_SplineDir>& dirs = g.dirs;
    const bool hasDirs = g.hasDirs;
    const GSSize n = (GSSize) p.wantCoords.size ();
    // HARTE FALLE: Spline-coords 0-BASIERT, Groesse exakt n (kein Leerplatz, kein Schlusspunkt).
    p.memo.coords = reinterpret_cast<API_Coord**> (BMAllocateHandle (n * sizeof (API_Coord), ALLOCATE_CLEAR, 0));
    if (p.memo.coords == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return false; }
    for (GSSize i = 0; i < n; ++i)
        (*p.memo.coords)[i] = p.wantCoords[(size_t) i];
    if (hasDirs) {
        p.memo.bezierDirs = reinterpret_cast<API_SplineDir**> (BMAllocateHandle (n * sizeof (API_SplineDir), ALLOCATE_CLEAR, 0));
        if (p.memo.bezierDirs == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return false; }
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
    bool reflected = false;
    C2DGeom g;

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
        case C2DKind::Hatch:
        case C2DKind::Spline:
            C2DParsePathGeometry (item, p, g);
            if (p.err != NoError) return;
            break;

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
            if (!C2DBuildPolyMemo (p, poly, g.arcs)) return;

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
            p.element.spline.closed = g.closed;
            if (!C2DBuildSplineMemo (p, g)) return;
            break;
        }

        default:
            break;
    }
}

// ---- Ruecklese ----------------------------------------------------------------------

// Ist-Werte + Vergleiche in r eintragen; true = Typ, Geschoss, Ebene und Geometrie stimmen.
// 'what' sammelt die Abweichungen. 0.9.34 aus C2DReadBack herausgeloest (Modify nutzt es mit).
bool C2DReadBackFill (const C2DPrep& p, const API_Element& check, GS::ObjectState& r, GS::UniString& what)
{
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
    if (!typeMatches)      what += GS::UniString (" Typ");
    if (!floorMatches)     what += GS::UniString (" Geschoss (Soll ") + C2DInt (p.wantFloor) + GS::UniString (")");
    if (!layerMatches)     what += GS::UniString (" Ebene (Soll ") + C2DInt (p.wantLayer) + GS::UniString (")");
    if (!coordinatesMatch) what += GS::UniString (" Koordinaten");
    if (!detail.IsEmpty ()) what += GS::UniString (" - ") + detail;
    return ok;
}

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

    GS::UniString what;
    const bool ok = C2DReadBackFill (p, check, r, what);
    r.Add ("success", ok);
    if (!ok) {
        // Element existiert - Aufrufer braucht elementId + Ist-Werte, um es zu loeschen.
        GS::ObjectState err;
        err.Add ("code", (GSErrCode) APIERR_GENERAL);
        err.Add ("message", GS::UniString (GS::UniString ("Ruecklese weicht ab:") + what));
        r.Add ("error", err);
    }
    return r;
}

// ---- 0.9.34 Modify2DElements (Wunschliste Michi B4/B5) ------------------------------------------
// Bestehende 2D-Elemente aendern, die GUID bleibt. Ablauf wie Create: Pruefen je Element (ohne zu
// aendern) -> EIN Undo-Schritt mit ACAPI_Element_Change -> Ruecklese. Nicht angegebene Felder bleiben
// unveraendert; die Geometrie einer PolyLine/Hatch/Spline wird nur als Ganzes ersetzt (coordinates).
// Die Ruecklese prueft Geometrie, Geschoss und Ebene ueber C2DReadBackFill (Sollgeometrie = neue oder
// die unveraenderte Ist-Geometrie) und zusaetzlich jedes geaenderte Attribut.

struct C2DMod {
    C2DPrep                    p;          // element = Ist-Stand + Aenderungen, memo = neue Geometrie, want* = Soll
    API_Element                mask = {};
    UInt64                     memoMask = 0;
    std::vector<GS::UniString> changed;
    bool         hasPen = false, hasLtype = false, arrowsOff = false;
    Int32        ltype = 0;
    short        pen = 0;
    bool         hasFill = false, hasFgPen = false, hasBgPen = false, hasContPen = false, hasShowArea = false, showArea = false;
    Int32        fill = 0;
    short        fgPen = 0, bgPen = 0, contPen = 0;
    bool         hasFgRGB = false, hasBgRGB = false, clearFgRGB = false, clearBgRGB = false;
    API_RGBColor fgRGB = {}, bgRGB = {};
    bool         hasFillAngle = false;
    double       fillAngle = 0.0;          // Radiant
    // 0.9.35 Polygon-Ersatz (PolyLine/Hatch): bisherige Hoechst-IDs + benutzter Weg fuer die Antwort
    bool          polyChange = false;
    UInt32        maxVertexId = 0, maxEdgeId = 0, maxContourId = 0;
    GS::UniString polyVariant;
};

C2DKind C2DKindOfElement (const API_Element& e)
{
    switch (e.header.type.typeID) {
        case API_LineID:     return C2DKind::Line;
        case API_ArcID:      return e.arc.whole ? C2DKind::Circle : C2DKind::Arc;
        case API_CircleID:   return C2DKind::Circle;
        case API_PolyLineID: return C2DKind::PolyLine;
        case API_SplineID:   return C2DKind::Spline;
        case API_HatchID:    return C2DKind::Hatch;
        default:             return C2DKind::Unknown;
    }
}

GS::UniString C2DKindName (C2DKind kind)
{
    switch (kind) {
        case C2DKind::Line:     return "Line";
        case C2DKind::Arc:      return "Arc";
        case C2DKind::Circle:   return "Circle";
        case C2DKind::PolyLine: return "PolyLine";
        case C2DKind::Spline:   return "Spline";
        case C2DKind::Hatch:    return "Hatch";
        default:                return "Unknown";
    }
}

GS::UniString C2DAttrNameOf (API_AttrTypeID typeID, Int32 idx)
{
    API_Attribute attr = {};
    GS::UniString name;
    attr.header.typeID = typeID;
    attr.header.index = ACAPI_CreateAttributeIndex (idx);
    attr.header.uniStringNamePtr = &name;
    if (ACAPI_Attribute_Get (&attr) != NoError)
        return GS::UniString ();
    return name;
}

// Linienstift / Linientyp der linienartigen Typen (Kreis = API_ArcType, auch bei API_CircleID).
API_ExtendedPenType* C2DLinePenOf (API_Element& e, C2DKind kind)
{
    switch (kind) {
        case C2DKind::Line:     return &e.line.linePen;
        case C2DKind::Arc:
        case C2DKind::Circle:   return &e.arc.linePen;
        case C2DKind::PolyLine: return &e.polyLine.linePen;
        case C2DKind::Spline:   return &e.spline.linePen;
        default:                return nullptr;
    }
}

API_AttributeIndex* C2DLineTypeOf (API_Element& e, C2DKind kind)
{
    switch (kind) {
        case C2DKind::Line:     return &e.line.ltypeInd;
        case C2DKind::Arc:
        case C2DKind::Circle:   return &e.arc.ltypeInd;
        case C2DKind::PolyLine: return &e.polyLine.ltypeInd;
        case C2DKind::Spline:   return &e.spline.ltypeInd;
        case C2DKind::Hatch:    return &e.hatch.ltypeInd;
        default:                return nullptr;
    }
}

// Pfeilspitzen: AC29 hat getrennte startArrowData/endArrowData mit arrowVisibility,
// AC27/28 ein arrowData mit begArrow/endArrow (DevKit 27.3001/28.3001 vs. 29.3100).
template <typename T>
void C2DArrowsOffT (T& t)
{
#ifdef ServerMainVers_2900
    t.startArrowData.arrowVisibility = false;
    t.endArrowData.arrowVisibility = false;
#else
    t.arrowData.begArrow = false;
    t.arrowData.endArrow = false;
#endif
}

template <typename T>
bool C2DArrowsVisibleT (const T& t)
{
#ifdef ServerMainVers_2900
    return t.startArrowData.arrowVisibility || t.endArrowData.arrowVisibility;
#else
    return t.arrowData.begArrow || t.arrowData.endArrow;
#endif
}

#ifdef ServerMainVers_2900
#define C2D_MASK_ARROWS(maskRec, T) \
    do { ACAPI_ELEMENT_MASK_SET (maskRec, T, startArrowData); ACAPI_ELEMENT_MASK_SET (maskRec, T, startArrowData.arrowVisibility); \
         ACAPI_ELEMENT_MASK_SET (maskRec, T, endArrowData);   ACAPI_ELEMENT_MASK_SET (maskRec, T, endArrowData.arrowVisibility); } while (false)
#else
#define C2D_MASK_ARROWS(maskRec, T) \
    do { ACAPI_ELEMENT_MASK_SET (maskRec, T, arrowData); ACAPI_ELEMENT_MASK_SET (maskRec, T, arrowData.begArrow); \
         ACAPI_ELEMENT_MASK_SET (maskRec, T, arrowData.endArrow); } while (false)
#endif

void C2DArrowsOff (C2DMod& m)
{
    API_Element& e = m.p.element;
    switch (m.p.kind) {
        case C2DKind::Line:     C2DArrowsOffT (e.line);     C2D_MASK_ARROWS (m.mask, API_LineType);     break;
        case C2DKind::Arc:
        case C2DKind::Circle:   C2DArrowsOffT (e.arc);      C2D_MASK_ARROWS (m.mask, API_ArcType);      break;
        case C2DKind::PolyLine: C2DArrowsOffT (e.polyLine); C2D_MASK_ARROWS (m.mask, API_PolyLineType); break;
        case C2DKind::Spline:   C2DArrowsOffT (e.spline);   C2D_MASK_ARROWS (m.mask, API_SplineType);   break;
        default: break;
    }
}

bool C2DArrowsVisible (const API_Element& e, C2DKind kind)
{
    switch (kind) {
        case C2DKind::Line:     return C2DArrowsVisibleT (e.line);
        case C2DKind::Arc:
        case C2DKind::Circle:   return C2DArrowsVisibleT (e.arc);
        case C2DKind::PolyLine: return C2DArrowsVisibleT (e.polyLine);
        case C2DKind::Spline:   return C2DArrowsVisibleT (e.spline);
        default:                return false;
    }
}

void C2DMaskLinePenAndType (C2DMod& m, bool pen, bool ltype)
{
    switch (m.p.kind) {
        case C2DKind::Line:
            if (pen)   { ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, linePen); ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, linePen.penIndex); ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, linePen.colorOverridePenIndex); }
            if (ltype) ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, ltypeInd);
            break;
        case C2DKind::Arc:
        case C2DKind::Circle:
            if (pen)   { ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, linePen); ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, linePen.penIndex); ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, linePen.colorOverridePenIndex); }
            if (ltype) ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, ltypeInd);
            break;
        case C2DKind::PolyLine:
            if (pen)   { ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, linePen); ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, linePen.penIndex); ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, linePen.colorOverridePenIndex); }
            if (ltype) ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, ltypeInd);
            break;
        case C2DKind::Spline:
            if (pen)   { ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, linePen); ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, linePen.penIndex); ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, linePen.colorOverridePenIndex); }
            if (ltype) ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, ltypeInd);
            break;
        default:
            break;
    }
}

// ---- 0.9.35: Polygon-IDs. DevKit API_Polygon: jede Koordinate hat eine vertexID, der Schlusspunkt
// eines Rings traegt die ID seines Startpunkts, Zelle 0 = groesste ID, und beim Aendern darf die
// groesste ID nicht sinken. Ein frisches Memo ohne IDs lehnte ACAPI_Element_Change mit APIERR_BADPOLY ab.

UInt32 C2DMaxId (UInt32** h)
{
    if (h == nullptr)
        return 0;
    const GSSize n = BMhGetSize (reinterpret_cast<GSHandle> (h)) / (GSSize) sizeof (UInt32);
    UInt32 mx = 0;
    for (GSSize i = 0; i < n; ++i)
        if ((*h)[i] > mx) mx = (*h)[i];
    return mx;
}

// IDs je Koordinate (Vertex- bzw. Kanten-IDs) fortlaufend ab base+1 vergeben.
UInt32** C2DMakeCoordIds (const C2DPrep& p, UInt32 base)
{
    const Int32 n = (Int32) p.wantCoords.size ();
    UInt32** h = reinterpret_cast<UInt32**> (BMAllocateHandle ((n + 1) * (GSSize) sizeof (UInt32), ALLOCATE_CLEAR, 0));
    if (h == nullptr)
        return nullptr;
    const std::vector<Int32> ends = p.wantEnds.empty () ? std::vector<Int32> (1, n) : p.wantEnds;
    UInt32 next = base;
    Int32 start = 1;
    for (Int32 end : ends) {
        for (Int32 i = start; i <= end; ++i) {
            const bool closesRing = (i == end && i > start && C2DNear (p.wantCoords[(size_t) (end - 1)], p.wantCoords[(size_t) (start - 1)]));
            (*h)[i] = closesRing ? (*h)[start] : ++next;
        }
        start = end + 1;
    }
    (*h)[0] = next;
    return h;
}

// Kontur-IDs je Teilpolygon ab base+1.
UInt32** C2DMakeContourIds (const C2DPrep& p, UInt32 base)
{
    const Int32 nSub = p.wantEnds.empty () ? 1 : (Int32) p.wantEnds.size ();
    UInt32** h = reinterpret_cast<UInt32**> (BMAllocateHandle ((nSub + 1) * (GSSize) sizeof (UInt32), ALLOCATE_CLEAR, 0));
    if (h == nullptr)
        return nullptr;
    for (Int32 sIdx = 1; sIdx <= nSub; ++sIdx)
        (*h)[sIdx] = base + (UInt32) sIdx;
    (*h)[0] = base + (UInt32) nSub;
    return h;
}

template <typename T>
T** C2DCloneHandle (T** src)
{
    if (src == nullptr)
        return nullptr;
    const GSSize size = BMhGetSize (reinterpret_cast<GSHandle> (src));
    T** h = reinterpret_cast<T**> (BMAllocateHandle (size, ALLOCATE_CLEAR, 0));
    if (h != nullptr && size > 0)
        std::memcpy (*h, *src, (size_t) size);
    return h;
}

void C2DKillIdHandle (UInt32**& h)
{
    if (h != nullptr) {
        BMKillHandle (reinterpret_cast<GSHandle*> (&h));
        h = nullptr;
    }
}

// Ohne neue coordinates gilt die Ist-Geometrie als Soll - die Ruecklese bestaetigt dann "unveraendert".
bool C2DLoadCurrentPath (C2DPrep& p)
{
    API_ElementMemo memo = {};
    if (ACAPI_Element_GetMemo (p.created, &memo, APIMemoMask_Polygon) != NoError) {
        ACAPI_DisposeElemMemoHdls (&memo);
        return false;
    }
    bool ok = true;
    if (p.kind == C2DKind::Spline) {
        ok = memo.coords != nullptr;
        if (ok) {
            const GSSize n = BMhGetSize (reinterpret_cast<GSHandle> (memo.coords)) / (GSSize) sizeof (API_Coord);
            for (GSSize i = 0; i < n; ++i)          // Spline 0-basiert
                p.wantCoords.push_back ((*memo.coords)[i]);
        }
        p.wantRing = false;
    } else {
        const API_Polygon& poly = (p.kind == C2DKind::Hatch) ? p.element.hatch.poly : p.element.polyLine.poly;
        std::vector<C2DArc> arcs;
        ok = C2DReadPolyMemo (memo, poly.nCoords, poly.nArcs, p.wantCoords, arcs);
        if (ok && memo.pends != nullptr && poly.nSubPolys > 1)
            for (Int32 sIdx = 1; sIdx <= poly.nSubPolys; ++sIdx)
                p.wantEnds.push_back ((*memo.pends)[sIdx]);
        p.wantRing = (p.kind == C2DKind::Hatch) || (p.wantCoords.size () >= 2 && C2DNear (p.wantCoords.front (), p.wantCoords.back ()));
    }
    ACAPI_DisposeElemMemoHdls (&memo);
    return ok;
}

void C2DPrepareModify (const GS::ObjectState& item, C2DMod& m, C2DCaches& caches, short myUserId, std::map<Int32, GS::UniString>& layerCache)
{
    C2DPrep& p = m.p;
    GS::UniString msg;

    // -- Element
    const GS::ObjectState* idOs = item.Get ("elementId");
    GS::UniString guidStr;
    if (idOs == nullptr || !idOs->Get ("guid", guidStr)) { C2DFail (p, APIERR_BADPARS, "elementId {guid} fehlt"); return; }
    p.created = APIGuidFromString (guidStr.ToCStr ().Get ());
    const GS::UniString reason = PreflightElementELM (p.created, myUserId, layerCache);
    if (!reason.IsEmpty ()) { C2DFail (p, ReasonToErrCodeELM (reason), reason); return; }

    p.element = {};
    p.element.header.guid = p.created;
    if (ACAPI_Element_Get (&p.element) != NoError) { C2DFail (p, APIERR_BADID, "[NOT_FOUND] Element nicht lesbar"); return; }
    p.kind = C2DKindOfElement (p.element);
    if (p.kind == C2DKind::Unknown) {
        C2DFail (p, APIERR_BADELEMENTTYPE, GS::UniString ("Elementtyp ") + GetElementTypeNonLocalizedName (p.element.header.type.typeID)
            + GS::UniString (" wird nicht unterstuetzt (Line, Arc, Circle, PolyLine, Spline, Hatch)"));
        return;
    }
    p.typeName = C2DKindName (p.kind);
    GS::UniString givenType;
    if (item.Get ("elementType", givenType) && givenType != p.typeName) {
        C2DFail (p, APIERR_BADPARS, GS::UniString ("elementType '") + givenType + GS::UniString ("' passt nicht - das Element ist ") + p.typeName);
        return;
    }
    const bool isHatch = (p.kind == C2DKind::Hatch);
    ACAPI_ELEMENT_MASK_CLEAR (m.mask);
    p.wantFloor = (Int32) p.element.header.floorInd;

    // -- Felder, die nicht zum Typ passen, klar ablehnen statt still zu ignorieren.
    static const char* const hatchOnly[] = { "fillName", "fillIndex", "foregroundPen", "backgroundPen", "contourPen", "foregroundRGB", "backgroundRGB",
                                             "clearForegroundRGB", "clearBackgroundRGB", "contourLineTypeName", "contourLineTypeIndex", "fillAngleDeg", "showArea" };
    static const char* const lineOnly[]  = { "pen", "penIndex", "lineTypeName", "lineTypeIndex", "ltypeIndex", "removeArrows" };
    for (const char* key : hatchOnly)
        if (!isHatch && item.Contains (key)) { C2DFail (p, APIERR_BADPARS, GS::UniString (key) + GS::UniString (" gilt nur fuer Hatch - das Element ist ") + p.typeName); return; }
    for (const char* key : lineOnly)
        if (isHatch && item.Contains (key)) { C2DFail (p, APIERR_BADPARS, GS::UniString (key) + GS::UniString (" gilt nicht fuer Hatch (contourPen / contourLineTypeName)")); return; }
    if (item.Contains ("floorInd") || item.Contains ("floorIndex")) {
        C2DFail (p, APIERR_BADPARS, "Geschoss kann Modify2DElements nicht aendern (neu anlegen per Create2DElements)");
        return;
    }

    // -- Ebene, Anzeigereihenfolge
    bool hasLayer = false;
    Int32 layerIdx = 0;
    GSErrCode aerr = C2DResolveAttr (API_LayerID, "Ebene", item, "layerName", "layerIndex", nullptr, caches.attr, hasLayer, layerIdx, msg);
    if (aerr != NoError) { C2DFail (p, aerr, msg); return; }
    if (hasLayer) {
        const GS::UniString block = LayerBlockReasonELM (ACAPI_CreateAttributeIndex (layerIdx));
        if (!block.IsEmpty ()) { C2DFail (p, ReasonToErrCodeELM (block), GS::UniString ("Zielebene: ") + block); return; }
        p.element.header.layer = ACAPI_CreateAttributeIndex (layerIdx);
        ACAPI_ELEMENT_MASK_SET (m.mask, API_Elem_Head, layer);
        m.changed.push_back ("layer");
    }
    p.wantLayer = (Int32) p.element.header.layer.ToInt32_Deprecated ();

    // 0.9.35: Archicad ignoriert drwIndex bei ACAPI_Element_Change (live 20.10.: 3 gesetzt, 9 blieb) -
    // beim Anlegen wirkt es. Klar ablehnen statt still nichts zu tun.
    if (item.Contains ("drawIndex")) {
        C2DFail (p, APIERR_BADPARS, "drawIndex laesst sich beim Aendern nicht setzen (Archicad ignoriert es) - per Create2DElements neu anlegen");
        return;
    }

    // -- Linienattribute (alles ausser Hatch)
    if (!isHatch) {
        if (!C2DReadPen (item, "pen", "penIndex", 1, m.hasPen, m.pen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        aerr = C2DResolveAttr (API_LinetypeID, "Linientyp", item, "lineTypeName", "lineTypeIndex", "ltypeIndex", caches.attr, m.hasLtype, m.ltype, msg);
        if (aerr != NoError) { C2DFail (p, aerr, msg); return; }
        item.Get ("removeArrows", m.arrowsOff);
        if (m.hasPen) {
            API_ExtendedPenType* lp = C2DLinePenOf (p.element, p.kind);
            lp->penIndex = m.pen;
            lp->colorOverridePenIndex = 0;
            m.changed.push_back ("pen");
        }
        if (m.hasLtype) {
            *C2DLineTypeOf (p.element, p.kind) = ACAPI_CreateAttributeIndex (m.ltype);
            m.changed.push_back ("lineType");
        }
        C2DMaskLinePenAndType (m, m.hasPen, m.hasLtype);
        if (m.arrowsOff) {
            C2DArrowsOff (m);
            m.changed.push_back ("arrows");
        }
    }

    // -- Schraffur-Attribute
    if (isHatch) {
        API_HatchType& h = p.element.hatch;
        aerr = C2DResolveAttr (API_FilltypeID, "Fuellung", item, "fillName", "fillIndex", nullptr, caches.attr, m.hasFill, m.fill, msg);
        if (aerr != NoError) { C2DFail (p, aerr, msg); return; }
        aerr = C2DResolveAttr (API_LinetypeID, "Konturlinientyp", item, "contourLineTypeName", "contourLineTypeIndex", nullptr, caches.attr, m.hasLtype, m.ltype, msg);
        if (aerr != NoError) { C2DFail (p, aerr, msg); return; }
        if (!C2DReadPen (item, "foregroundPen", nullptr, 1, m.hasFgPen, m.fgPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadPen (item, "backgroundPen", nullptr, 0, m.hasBgPen, m.bgPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadPen (item, "contourPen", nullptr, 0, m.hasContPen, m.contPen, msg)) { C2DFail (p, APIERR_BADPARS, msg); return; }
        if (!C2DReadRGB (item, "foregroundRGB", m.hasFgRGB, m.fgRGB)) { C2DFail (p, APIERR_BADPARS, "foregroundRGB braucht red, green, blue je 0..1"); return; }
        if (!C2DReadRGB (item, "backgroundRGB", m.hasBgRGB, m.bgRGB)) { C2DFail (p, APIERR_BADPARS, "backgroundRGB braucht red, green, blue je 0..1"); return; }
        item.Get ("clearForegroundRGB", m.clearFgRGB);
        item.Get ("clearBackgroundRGB", m.clearBgRGB);
        if ((m.hasFgRGB && m.clearFgRGB) || (m.hasBgRGB && m.clearBgRGB)) { C2DFail (p, APIERR_BADPARS, "RGB setzen und loeschen im selben Eintrag widerspricht sich"); return; }
        // 0.9.35: Mit RGB-Vordergrund setzt Archicad den Vordergrundstift selbst auf 1 (live 20.10.,
        // auch beim Anlegen) - Stift UND RGB im selben Feld waere nie erreichbar.
        if (m.hasFgPen && m.hasFgRGB) { C2DFail (p, APIERR_BADPARS, "foregroundPen und foregroundRGB schliessen sich aus - mit RGB setzt Archicad den Vordergrundstift auf 1"); return; }
        if (m.hasBgPen && m.hasBgRGB) { C2DFail (p, APIERR_BADPARS, "backgroundPen und backgroundRGB schliessen sich aus"); return; }
        m.hasShowArea = item.Get ("showArea", m.showArea);
        if (item.Contains ("fillAngleDeg")) {
            double deg = 0.0;
            if (!C2DReadNumber (item, "fillAngleDeg", deg)) { C2DFail (p, APIERR_BADPARS, "fillAngleDeg ist keine Zahl"); return; }
            m.hasFillAngle = true;
            m.fillAngle = deg * C2D_PI / 180.0;
        }

        if (m.hasFill) {
            h.hatchType = API_FillHatch;          // Baustoff-Schraffur wuerde fillInd ignorieren
            h.fillInd = ACAPI_CreateAttributeIndex (m.fill);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchType);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, fillInd);
            m.changed.push_back ("fill");
        }
        if (m.hasFgPen) {
            h.fillPen.penIndex = m.fgPen;
            h.fillPen.colorOverridePenIndex = 0;
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, fillPen);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, fillPen.penIndex);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, fillPen.colorOverridePenIndex);
            m.changed.push_back ("foregroundPen");
        }
        if (m.hasBgPen) {
            h.fillBGPen = m.bgPen;
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, fillBGPen);
            m.changed.push_back ("backgroundPen");
        }
        if (m.hasContPen) {
            h.contPen.penIndex = m.contPen;
            h.contPen.colorOverridePenIndex = 0;
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, contPen);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, contPen.penIndex);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, contPen.colorOverridePenIndex);
            m.changed.push_back ("contourPen");
        }
        if (m.hasLtype) {
            h.ltypeInd = ACAPI_CreateAttributeIndex (m.ltype);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, ltypeInd);
            m.changed.push_back ("contourLineType");
        }
        if (m.hasShowArea) {
            h.showArea = m.showArea;
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, showArea);
            m.changed.push_back ("showArea");
        }
        if (m.hasFgRGB || m.clearFgRGB) {
            if (m.clearFgRGB) {
                h.hatchFlags &= ~((UInt32) APIHatch_HasFgRGBColor);
            } else {
                h.hatchFlags |= (UInt32) APIHatch_HasFgRGBColor;
                h.foregroundRGB.f_red   = m.fgRGB.f_red;
                h.foregroundRGB.f_green = m.fgRGB.f_green;
                h.foregroundRGB.f_blue  = m.fgRGB.f_blue;
            }
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchFlags);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, foregroundRGB);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, foregroundRGB.f_red);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, foregroundRGB.f_green);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, foregroundRGB.f_blue);
            m.changed.push_back ("foregroundRGB");
        }
        if (m.hasBgRGB || m.clearBgRGB) {
            if (m.clearBgRGB) {
                h.hatchFlags &= ~((UInt32) APIHatch_HasBkgRGBColor);
            } else {
                h.hatchFlags |= (UInt32) APIHatch_HasBkgRGBColor;
                h.backgroundRGB.f_red   = m.bgRGB.f_red;
                h.backgroundRGB.f_green = m.bgRGB.f_green;
                h.backgroundRGB.f_blue  = m.bgRGB.f_blue;
            }
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchFlags);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, backgroundRGB);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, backgroundRGB.f_red);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, backgroundRGB.f_green);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, backgroundRGB.f_blue);
            m.changed.push_back ("backgroundRGB");
        }
        if (m.hasFillAngle) {
            // Wie DevKit Element_Modify_2DElements.cpp (Do_Hatch_Edit): gedrehte Ausrichtung + Drehmatrix.
            const double c = std::cos (m.fillAngle);
            const double s = std::sin (m.fillAngle);
            h.hatchOrientation.type = API_HatchRotated;
            h.hatchOrientation.matrix00 = c;
            h.hatchOrientation.matrix10 = s;
            h.hatchOrientation.matrix01 = -s;
            h.hatchOrientation.matrix11 = c;
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchOrientation.type);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchOrientation.matrix00);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchOrientation.matrix10);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchOrientation.matrix01);
            ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, hatchOrientation.matrix11);
            m.changed.push_back ("fillAngle");
        }
    }

    // -- Geometrie
    switch (p.kind) {
        case C2DKind::Line: {
            API_LineType& ln = p.element.line;
            p.wantBeg = ln.begC;
            p.wantEnd = ln.endC;
            const bool hasBeg = item.Contains ("begCoordinate");
            const bool hasEnd = item.Contains ("endCoordinate");
            if (hasBeg && !C2DReadCoordField (item, "begCoordinate", p.wantBeg)) { C2DFail (p, APIERR_BADPARS, "begCoordinate braucht x und y (Zahlen)"); return; }
            if (hasEnd && !C2DReadCoordField (item, "endCoordinate", p.wantEnd)) { C2DFail (p, APIERR_BADPARS, "endCoordinate braucht x und y (Zahlen)"); return; }
            if (hasBeg || hasEnd) {
                if (C2DNear (p.wantBeg, p.wantEnd)) { C2DFail (p, APIERR_BADPARS, "Line: Anfangs- und Endpunkt fallen zusammen"); return; }
                ln.begC = p.wantBeg;
                ln.endC = p.wantEnd;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, begC);
                ACAPI_ELEMENT_MASK_SET (m.mask, API_LineType, endC);
                m.changed.push_back ("geometry");
            }
            break;
        }

        case C2DKind::Arc:
        case C2DKind::Circle: {
            const bool isArc = (p.kind == C2DKind::Arc);
            API_ArcType& a = p.element.arc;      // Kreis = API_ArcType (using API_CircleType = API_ArcType)
            p.wantOrigin = a.origC;
            p.wantRadius = a.r;
            bool any = false;
            double v = 0.0;
            if (item.Contains ("origin")) {
                if (!C2DReadCoordField (item, "origin", p.wantOrigin)) { C2DFail (p, APIERR_BADPARS, "origin braucht x und y (Zahlen)"); return; }
                a.origC = p.wantOrigin;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, origC);
                any = true;
            }
            if (item.Contains ("radius")) {
                if (!C2DReadNumber (item, "radius", v) || v <= 0.0) { C2DFail (p, APIERR_BADPARS, "radius muss eine Zahl > 0 sein"); return; }
                a.r = v;
                p.wantRadius = v;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, r);
                any = true;
            }
            if (!isArc && (item.Contains ("begAngle") || item.Contains ("endAngle") || item.Contains ("reflected"))) {
                C2DFail (p, APIERR_BADPARS, "Circle hat kein begAngle/endAngle/reflected");
                return;
            }
            if (item.Contains ("begAngle")) {
                if (!C2DReadNumber (item, "begAngle", v)) { C2DFail (p, APIERR_BADPARS, "begAngle ist keine Zahl"); return; }
                a.begAng = v;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, begAng);
                any = true;
            }
            if (item.Contains ("endAngle")) {
                if (!C2DReadNumber (item, "endAngle", v)) { C2DFail (p, APIERR_BADPARS, "endAngle ist keine Zahl"); return; }
                a.endAng = v;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, endAng);
                any = true;
            }
            if (item.Contains ("ratio")) {
                if (!C2DReadNumber (item, "ratio", v) || v <= 0.0) { C2DFail (p, APIERR_BADPARS, "ratio muss eine Zahl > 0 sein"); return; }
                a.ratio = v;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, ratio);
                any = true;
            }
            if (item.Contains ("angle")) {
                if (!C2DReadNumber (item, "angle", v)) { C2DFail (p, APIERR_BADPARS, "angle ist keine Zahl"); return; }
                a.angle = v;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, angle);
                any = true;
            }
            if (item.Contains ("reflected")) {
                bool refl = false;
                item.Get ("reflected", refl);
                a.reflected = refl;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_ArcType, reflected);
                any = true;
            }
            if (isArc && C2DNormSweep (a.begAng, a.endAng) <= 1e-9) { C2DFail (p, APIERR_BADPARS, "Arc: begAngle und endAngle fallen zusammen"); return; }
            if (any)
                m.changed.push_back ("geometry");
            break;
        }

        case C2DKind::PolyLine:
        case C2DKind::Hatch:
        case C2DKind::Spline: {
            if (!item.Contains ("coordinates")) {
                static const char* const geomKeys[] = { "arcs", "subPolyEnds", "closed", "bezierDirs" };
                for (const char* key : geomKeys)
                    if (item.Contains (key)) { C2DFail (p, APIERR_BADPARS, GS::UniString (key) + GS::UniString (" nur zusammen mit coordinates - die Geometrie wird als Ganzes ersetzt")); return; }
                if (!C2DLoadCurrentPath (p)) { C2DFail (p, APIERR_GENERAL, "Ist-Geometrie nicht lesbar (Polygon-Memo)"); return; }
                break;
            }
            C2DGeom g;
            C2DParsePathGeometry (item, p, g);
            if (p.err != NoError) return;
            if (p.kind == C2DKind::Spline) {
                p.element.spline.closed = g.closed;
                if (!C2DBuildSplineMemo (p, g)) return;
                ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, closed);
                ACAPI_ELEMENT_MASK_SET (m.mask, API_SplineType, autoSmooth);
            } else {
                API_Polygon& poly = isHatch ? p.element.hatch.poly : p.element.polyLine.poly;
                // 0.9.35: bisherige Hoechst-IDs lesen; die neuen IDs liegen darueber (DevKit-Regel).
                API_ElementMemo cur = {};
                if (ACAPI_Element_GetMemo (p.created, &cur, APIMemoMask_Polygon) == NoError) {
                    m.maxVertexId  = C2DMaxId (cur.vertexIDs);
                    m.maxEdgeId    = C2DMaxId (cur.edgeIDs);
                    m.maxContourId = C2DMaxId (cur.contourIDs);
                }
                ACAPI_DisposeElemMemoHdls (&cur);
                if (!C2DBuildPolyMemo (p, poly, g.arcs)) return;
                p.memo.vertexIDs = C2DMakeCoordIds (p, m.maxVertexId);
                if (p.memo.vertexIDs == nullptr) { C2DFail (p, APIERR_MEMFULL, "Speicher-Allokation fehlgeschlagen"); return; }
                m.polyChange = true;
                if (isHatch) {
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, poly.nCoords);
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, poly.nSubPolys);
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_HatchType, poly.nArcs);
                } else {
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, poly.nCoords);
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, poly.nSubPolys);
                    ACAPI_ELEMENT_MASK_SET (m.mask, API_PolyLineType, poly.nArcs);
                }
            }
            m.memoMask = APIMemoMask_Polygon;
            m.changed.push_back ("geometry");
            break;
        }

        default:
            break;
    }

    if (m.changed.empty ())
        C2DFail (p, APIERR_BADPARS, "[NO_CHANGE] keine aenderbaren Felder angegeben");
}

// 0.9.35: Aendern; beim Polygon-Ersatz bis zu vier Wege, weil Archicad nur APIERR_BADPOLY meldet.
// A neues Memo mit vertexIDs; B wie Tapir ModifySlabs (vorhandenes Memo, coords/pends/parcs/vertexIDs
// ersetzt, Kanten-/Kontur-IDs bleiben); C neues Memo plus Kanten- und Kontur-IDs; D wie C ohne Maske
// auf den Polygon-Zaehlern (DevKit Do_Hatch_Edit aendert nCoords ohne Maske). Die Antwort nennt den Weg.
GSErrCode C2DChangeElement (C2DMod& m)
{
    C2DPrep& p = m.p;
    GSErrCode err = ACAPI_Element_Change (&p.element, &m.mask, m.memoMask != 0 ? &p.memo : nullptr, m.memoMask, true);
    if (!m.polyChange)
        return err;
    if (err == NoError) { m.polyVariant = "A"; return err; }
    if (err != APIERR_BADPOLY) return err;

    API_ElementMemo cur = {};
    if (ACAPI_Element_GetMemo (p.created, &cur, APIMemoMask_Polygon) == NoError) {
        if (cur.coords != nullptr)    BMKillHandle (reinterpret_cast<GSHandle*> (&cur.coords));
        if (cur.pends != nullptr)     BMKillHandle (reinterpret_cast<GSHandle*> (&cur.pends));
        if (cur.parcs != nullptr)     BMKillHandle (reinterpret_cast<GSHandle*> (&cur.parcs));
        C2DKillIdHandle (cur.vertexIDs);
        cur.coords    = C2DCloneHandle (p.memo.coords);
        cur.pends     = C2DCloneHandle (p.memo.pends);
        cur.parcs     = C2DCloneHandle (p.memo.parcs);
        cur.vertexIDs = C2DCloneHandle (p.memo.vertexIDs);
        err = ACAPI_Element_Change (&p.element, &m.mask, &cur, APIMemoMask_Polygon, true);
    }
    ACAPI_DisposeElemMemoHdls (&cur);
    if (err == NoError) { m.polyVariant = "B"; return err; }
    if (err != APIERR_BADPOLY) return err;

    C2DKillIdHandle (p.memo.edgeIDs);
    C2DKillIdHandle (p.memo.contourIDs);
    p.memo.edgeIDs    = C2DMakeCoordIds (p, m.maxEdgeId);
    p.memo.contourIDs = C2DMakeContourIds (p, m.maxContourId);
    err = ACAPI_Element_Change (&p.element, &m.mask, &p.memo, APIMemoMask_Polygon, true);
    if (err == NoError) { m.polyVariant = "C"; return err; }
    if (err != APIERR_BADPOLY) return err;

    API_Element maskNoCounts = m.mask;
    API_Polygon& pm = (p.kind == C2DKind::Hatch) ? maskNoCounts.hatch.poly : maskNoCounts.polyLine.poly;
    std::memset (&pm, 0, sizeof (pm));
    err = ACAPI_Element_Change (&p.element, &maskNoCounts, &p.memo, APIMemoMask_Polygon, true);
    if (err == NoError) m.polyVariant = "D";
    return err;
}

GS::ObjectState C2DModReadBack (C2DMod& m)
{
    const C2DPrep& p = m.p;
    GS::ObjectState r;
    API_Element check = {};
    check.header.guid = p.created;
    if (ACAPI_Element_Get (&check) != NoError) {
        GS::ObjectState err;
        err.Add ("code", (GSErrCode) APIERR_GENERAL);
        err.Add ("message", GS::UniString ("Ruecklese fehlgeschlagen (Element geaendert, aber nicht lesbar)"));
        r.Add ("success", false);
        r.Add ("elementId", CreateGuidObjectStateELM (p.created));
        r.Add ("elementType", p.typeName);
        r.Add ("error", err);
        return r;
    }

    GS::UniString what;
    bool ok = C2DReadBackFill (p, check, r, what);

    if (p.kind != C2DKind::Hatch) {
        const API_ExtendedPenType* lp = C2DLinePenOf (check, p.kind);
        const Int32 ltIst = (Int32) C2DLineTypeOf (check, p.kind)->ToInt32_Deprecated ();
        r.Add ("pen", (Int32) lp->penIndex);
        r.Add ("lineTypeIndex", ltIst);
        r.Add ("lineTypeName", C2DAttrNameOf (API_LinetypeID, ltIst));
        const bool arrows = C2DArrowsVisible (check, p.kind);
        r.Add ("arrowsVisible", arrows);
        if (m.hasPen && lp->penIndex != m.pen)  { ok = false; what += GS::UniString (" Stift"); }
        if (m.hasLtype && ltIst != m.ltype)     { ok = false; what += GS::UniString (" Linientyp"); }
        if (m.arrowsOff && arrows)              { ok = false; what += GS::UniString (" Pfeilspitzen"); }
    } else {
        const API_HatchType& h = check.hatch;
        const Int32 ltIst = (Int32) h.ltypeInd.ToInt32_Deprecated ();
        r.Add ("foregroundPen", (Int32) h.fillPen.penIndex);
        r.Add ("backgroundPen", (Int32) h.fillBGPen);
        r.Add ("contourPen", (Int32) h.contPen.penIndex);
        r.Add ("contourLineTypeIndex", ltIst);
        r.Add ("contourLineTypeName", C2DAttrNameOf (API_LinetypeID, ltIst));
        r.Add ("showArea", h.showArea);
        const bool rotated = h.hatchOrientation.type != API_HatchGlobal;
        const double angIst = rotated ? std::atan2 (h.hatchOrientation.matrix10, h.hatchOrientation.matrix00) : 0.0;
        r.Add ("fillAngleDeg", angIst * 180.0 / C2D_PI);
        r.Add ("fillOrientation", GS::UniString (h.hatchOrientation.type == API_HatchGlobal ? "global" : h.hatchOrientation.type == API_HatchRotated ? "rotated" : "distorted"));
        const bool hasFg = (h.hatchFlags & APIHatch_HasFgRGBColor) != 0;
        const bool hasBg = (h.hatchFlags & APIHatch_HasBkgRGBColor) != 0;
        if (m.hasFill && (Int32) h.fillInd.ToInt32_Deprecated () != m.fill) { ok = false; what += GS::UniString (" Fuellung"); }
        if (m.hasFgPen && h.fillPen.penIndex != m.fgPen)                     { ok = false; what += GS::UniString (" Vordergrundstift"); }
        if (m.hasBgPen && h.fillBGPen != m.bgPen)                             { ok = false; what += GS::UniString (" Hintergrundstift"); }
        if (m.hasContPen && h.contPen.penIndex != m.contPen)                  { ok = false; what += GS::UniString (" Konturstift"); }
        if (m.hasLtype && ltIst != m.ltype)                                   { ok = false; what += GS::UniString (" Konturlinientyp"); }
        if (m.hasShowArea && h.showArea != m.showArea)                        { ok = false; what += GS::UniString (" showArea"); }
        if (m.clearFgRGB && hasFg)                                            { ok = false; what += GS::UniString (" Vordergrund-RGB (nicht geloescht)"); }
        if (m.clearBgRGB && hasBg)                                            { ok = false; what += GS::UniString (" Hintergrund-RGB (nicht geloescht)"); }
        // Archicad speichert RGB in 8 Bit (0,3 -> 76/255 = 0,298) - Toleranz eine Stufe.
        const double rgbTol = 1.0 / 255.0 + 1e-6;
        if (m.hasFgRGB && (!hasFg || std::fabs (h.foregroundRGB.f_red - m.fgRGB.f_red) > rgbTol || std::fabs (h.foregroundRGB.f_green - m.fgRGB.f_green) > rgbTol || std::fabs (h.foregroundRGB.f_blue - m.fgRGB.f_blue) > rgbTol))
            { ok = false; what += GS::UniString (" Vordergrund-RGB"); }
        if (m.hasBgRGB && (!hasBg || std::fabs (h.backgroundRGB.f_red - m.bgRGB.f_red) > rgbTol || std::fabs (h.backgroundRGB.f_green - m.bgRGB.f_green) > rgbTol || std::fabs (h.backgroundRGB.f_blue - m.bgRGB.f_blue) > rgbTol))
            { ok = false; what += GS::UniString (" Hintergrund-RGB"); }
        if (m.hasFillAngle) {
            const double d = std::remainder (angIst - m.fillAngle, 2.0 * C2D_PI);
            if (!rotated || std::fabs (d) > 1e-6) { ok = false; what += GS::UniString (" Schraffurwinkel"); }
        }
    }

    if (m.polyChange)
        r.Add ("polygonVariant", m.polyVariant);
    const auto& changedOut = r.AddList<GS::UniString> ("changed");
    for (const GS::UniString& c : m.changed)
        changedOut (c);
    r.Add ("success", ok);
    if (!ok) {
        GS::ObjectState err;
        err.Add ("code", (GSErrCode) APIERR_GENERAL);
        err.Add ("message", GS::UniString (GS::UniString ("[NO_EFFECT] Ruecklese weicht ab:") + what));
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

// ============================================================================================
// 0.9.34 Modify2DElements
// ============================================================================================

GS::Optional<GS::UniString> Modify2DElementsCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "elements": { "type": "array", "minItems": 1,
                "description": "Bestehende 2D-Elemente (Line, Arc, Circle, PolyLine, Spline, Hatch) aendern; GUID bleibt. Nur angegebene Felder werden geaendert. Feldnamen wie Create2DElements.",
                "items": { "type": "object",
                    "properties": {
                        "elementId": { "type": "object", "properties": { "guid": { "type": "string" } }, "required": ["guid"] },
                        "elementType": { "type": "string", "enum": ["Line", "Arc", "Circle", "PolyLine", "Spline", "Hatch"], "description": "Optional als Kontrolle - weicht der Ist-Typ ab, wird abgelehnt." },
                        "layerName": { "type": "string", "description": "Neue Ebene per Name (Vorrang vor layerIndex)." },
                        "layerIndex": { "type": "integer", "description": "Neue Ebene per Attributindex." },
                        "pen": { "type": "integer", "description": "Linienstift (nicht Hatch). Alias penIndex." },
                        "penIndex": { "type": "integer", "description": "Alias fuer pen." },
                        "lineTypeName": { "type": "string", "description": "Linientyp per Name (nicht Hatch)." },
                        "lineTypeIndex": { "type": "integer", "description": "Linientyp per Index (nicht Hatch). Alias ltypeIndex." },
                        "ltypeIndex": { "type": "integer", "description": "Alias fuer lineTypeIndex." },
                        "removeArrows": { "type": "boolean", "description": "true = Pfeilspitzen an beiden Enden aus (Line, Arc, PolyLine, Spline)." },
                        "begCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "description": "Line: neuer Anfangspunkt (fehlt er, bleibt er)." },
                        "endCoordinate": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "description": "Line: neuer Endpunkt (fehlt er, bleibt er)." },
                        "origin": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } }, "description": "Arc/Circle: neuer Mittelpunkt." },
                        "radius": { "type": "number", "description": "Arc/Circle: Radius in m (> 0)." },
                        "begAngle": { "type": "number", "description": "Arc: Anfangswinkel in Radiant." },
                        "endAngle": { "type": "number", "description": "Arc: Endwinkel in Radiant." },
                        "ratio": { "type": "number", "description": "Arc/Circle: Achsverhaeltnis (> 0)." },
                        "angle": { "type": "number", "description": "Arc/Circle: Drehung der Hauptachse in Radiant." },
                        "reflected": { "type": "boolean", "description": "Arc: gespiegelt." },
                        "coordinates": { "type": "array", "items": { "type": "object", "properties": { "x": { "type": "number" }, "y": { "type": "number" } } },
                            "description": "PolyLine/Hatch/Spline: neue Geometrie als Ganzes (wie Create2DElements). Fehlt das Feld, bleibt die Geometrie." },
                        "arcs": { "type": "array", "items": { "type": "object", "properties": {
                                "begIndex": { "type": "integer" }, "endIndex": { "type": "integer" }, "arcAngle": { "type": "number" } } },
                            "description": "Nur mit coordinates: Bogensegmente, Indizes 1-basiert." },
                        "subPolyEnds": { "type": "array", "items": { "type": "integer" }, "description": "Nur mit coordinates (Hatch): Teilpolygon-Enden 1-basiert, Aussenkontur zuerst." },
                        "closed": { "type": "boolean", "description": "Nur mit coordinates: PolyLine schliessen bzw. Spline geschlossen." },
                        "bezierDirs": { "type": "array", "items": { "type": "object", "properties": {
                                "lenPrev": { "type": "number" }, "lenNext": { "type": "number" }, "dirAng": { "type": "number" } } },
                            "description": "Nur mit coordinates (Spline): ein Eintrag je Punkt; fehlt das Feld, gilt autoSmooth." },
                        "fillName": { "type": "string", "description": "Hatch: Fuellung per Name (Vorrang vor fillIndex); stellt auf Fuell-Schraffur um." },
                        "fillIndex": { "type": "integer", "description": "Hatch: Fuellung per Index." },
                        "foregroundPen": { "type": "integer", "description": "Hatch: Vordergrundstift (1..255). Nicht zusammen mit foregroundRGB (mit RGB setzt Archicad den Stift auf 1)." },
                        "backgroundPen": { "type": "integer", "description": "Hatch: Hintergrundstift (0 = transparent)." },
                        "contourPen": { "type": "integer", "description": "Hatch: Konturstift (0 = keine Kontur)." },
                        "contourLineTypeName": { "type": "string", "description": "Hatch: Konturlinientyp per Name." },
                        "contourLineTypeIndex": { "type": "integer", "description": "Hatch: Konturlinientyp per Index." },
                        "foregroundRGB": { "type": "object", "properties": { "red": { "type": "number" }, "green": { "type": "number" }, "blue": { "type": "number" } },
                            "description": "Hatch: Vordergrund-RGB je 0..1 (setzt das RGB-Flag)." },
                        "backgroundRGB": { "type": "object", "properties": { "red": { "type": "number" }, "green": { "type": "number" }, "blue": { "type": "number" } },
                            "description": "Hatch: Hintergrund-RGB je 0..1 (setzt das RGB-Flag)." },
                        "clearForegroundRGB": { "type": "boolean", "description": "Hatch: Vordergrund-RGB entfernen (Stiftfarbe gilt wieder)." },
                        "clearBackgroundRGB": { "type": "boolean", "description": "Hatch: Hintergrund-RGB entfernen." },
                        "fillAngleDeg": { "type": "number", "description": "Hatch: Schraffurwinkel in Grad (Ausrichtung 'gedreht')." },
                        "showArea": { "type": "boolean", "description": "Hatch: Flaeche anzeigen." }
                    },
                    "required": ["elementId"] } },
            "reserve": { "type": "boolean", "description": "Teamwork: vorab reservieren und danach freigeben (Standard true)." }
        },
        "required": ["elements"]
    })");
}

GS::Optional<GS::UniString> Modify2DElementsCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (GS::UniString (R"({"type":"object","properties":{"executionResults":{"type":"array","items":{"type":"object"}},"undoScope":{"type":"object"}},"required":["executionResults"]})"));
}

GS::ObjectState Modify2DElementsCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl&) const
{
    GS::Array<GS::ObjectState> items;
    if (!parameters.Get ("elements", items) || items.IsEmpty ())
        return CreateErrorResponseELM (APIERR_BADPARS, GS::UniString ("elements fehlt oder ist leer - erwartet wird eine Liste [{elementId:{guid}, ...Aenderungen}]."));
    bool reserve = true;
    parameters.Get ("reserve", reserve);

    // 1) Pruefen + Vorbereiten VOR dem Undo-Lambda. Doppelte GUIDs: nur der erste Eintrag zaehlt,
    //    sonst wuerde der zweite auf einem veralteten Ist-Stand aufbauen.
    C2DCaches caches;
    const short myUserId = MyTeamworkUserIdELM ();
    std::map<Int32, GS::UniString> layerCache;
    std::set<std::string> seen;
    std::vector<std::unique_ptr<C2DMod>> mods;
    for (const GS::ObjectState& item : items) {
        mods.push_back (std::unique_ptr<C2DMod> (new C2DMod ()));
        C2DMod& m = *mods.back ();
        C2DPrepareModify (item, m, caches, myUserId, layerCache);
        if (m.p.err == NoError && !seen.insert (std::string (APIGuidToString (m.p.created).ToCStr ().Get ())).second)
            C2DFail (m.p, APIERR_BADPARS, "Element steht mehrfach in der Liste - Aenderungen in EINEM Eintrag zusammenfassen");
    }

    const bool teamwork = reserve && ACAPI_Teamwork_HasConnection ();
    GS::Array<API_Guid> touched;
    for (const auto& m : mods)
        if (m->p.err == NoError) touched.Push (m->p.created);
    if (teamwork && !touched.IsEmpty ()) {
        GS::HashTable<API_Guid, short> conflicts;
        ACAPI_Teamwork_ReserveElements (touched, &conflicts, false);
    }

    // 2) Aendern in EINEM Undo-Schritt; Status liegt ausserhalb (Lehre 0.9.16).
    std::vector<GSErrCode> changeErr (mods.size (), NoError);
    bool lambdaStarted = false;
    ACAPI_CallUndoableCommand ("ELM_SAB Modify2DElements", [&] () -> GSErrCode {
        lambdaStarted = true;
        for (size_t i = 0; i < mods.size (); ++i) {
            C2DMod& m = *mods[i];
            if (m.p.err != NoError) continue;
            changeErr[i] = C2DChangeElement (m);
        }
        return NoError;
    });

    // 3) Memos freigeben.
    for (auto& m : mods) {
        ACAPI_DisposeElemMemoHdls (&m->p.memo);
        m->p.memo = {};
    }

    // 4) Ruecklese + Antwort NACH dem Lambda, in Eingabereihenfolge.
    GS::ObjectState response;
    const auto& results = response.AddList<GS::ObjectState> ("executionResults");
    for (size_t i = 0; i < mods.size (); ++i) {
        C2DMod& m = *mods[i];
        if (m.p.err != NoError) {
            results (CreateFailedExecutionResult (m.p.err, m.p.message));
            continue;
        }
        if (!lambdaStarted) {
            results (CreateFailedExecutionResult (APIERR_GENERAL, "Undo-Lambda wurde nicht gestartet - nichts geaendert"));
            continue;
        }
        if (changeErr[i] != NoError) {
            results (CreateFailedExecutionResult (changeErr[i], m.p.typeName + GS::UniString (" konnte nicht geaendert werden (ACAPI_Element_Change)")));
            continue;
        }
        results (C2DModReadBack (m));
    }

    if (teamwork && !touched.IsEmpty ())
        ACAPI_Teamwork_ReleaseElements (touched, false);

    GS::ObjectState undoScope;
    undoScope.Add ("lambdaStarted", lambdaStarted);
    response.Add ("undoScope", undoScope);
    return response;
}
