/**
 * ExportSurfaces — Paket `sab-oberflaechen/1` (Mac + Windows, AC27–30). ELM_SAB 0.11.12.
 *
 * Archicad kennt zu jeder Oberflaeche, was ein 3D-Modell ausserhalb von Archicad braucht:
 * Farbe, Glanz, Transparenz, das Texturbild UND dessen Groesse im Modell (xSize/ySize in
 * Metern = Kachelmass, das im IFC fehlt). Das IFC traegt davon nur den Namen.
 *
 * Vorlage: der Oberflaechen-Export des Plattform-Add-ons (05./06.11.2026), als ELM_SAB-Befehl
 * umgebaut; das Format ist unveraendert.
 *
 * Bild: Vor ACAPI_Attribute_Get wird `texture.fileLoc` angelegt — Archicad traegt dort den
 * Ort der Bilddatei in der geladenen Bibliothek ein. Liegt das Bild als Datei auf der Platte,
 * wird es kopiert; sonst (eingebettete Bibliothek, Container .lcf) ueber IO::File gelesen,
 * also ueber Archicads eigenes Dateisystem. Klappt beides nicht, steht die Oberflaeche
 * trotzdem im Paket, mit `bild_fehlt` und dem Texturnamen.
 *
 * Pfade: intern UTF-8-Text, in std::filesystem immer ueber PfadUtf8() — sonst gehen Umlaute
 * unter Windows (ANSI-Codepage) kaputt.
 *
 * Erfahrungen aus dem Vorlage-Add-on (AC29):
 *  - API_ProjectInfo gibt location/projectName/projectPath im eigenen Destruktor frei.
 *    NICHT selbst loeschen — doppeltes delete → Absturz.
 *  - Attribut-Indizes haben Luecken (geloeschte Oberflaechen): Fehler bei Get = weiter.
 *  - Ein Projekt mit ~250 texturierten Oberflaechen hatte ~70 Bilder nur in der eingebetteten
 *    Bibliothek — die kommen nur ueber IO::File mit.
 */

#include "ExportSurfacesCommand.hpp"
#include "MigrationHelper.hpp"   // AC27/28: ACAPI_ProjectOperation_Project, ACAPI_CreateAttributeIndex
#include "AddOnVersion.hpp"      // ELM_SAB_VERSION

#include "File.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {

namespace fs = std::filesystem;

constexpr double kPi = 3.14159265358979323846;

#if defined(ServerMainVers_3000)
constexpr int kArchicadVersion = 30;
#elif defined(ServerMainVers_2900)
constexpr int kArchicadVersion = 29;
#elif defined(ServerMainVers_2800)
constexpr int kArchicadVersion = 28;
#else
constexpr int kArchicadVersion = 27;
#endif

// ---------------------------------------------------------------------------
// Pfade + Text (portabel)
// ---------------------------------------------------------------------------

fs::path PfadUtf8 (const std::string& s)
{
#if defined(__cpp_char8_t)
    return fs::path (std::u8string (s.begin (), s.end ()));
#else
    return fs::u8path (s);
#endif
}

std::string Utf8 (const fs::path& p)
{
    const auto u = p.u8string ();   // C++17: std::string, C++20: std::u8string
    return std::string (u.begin (), u.end ());
}

std::string Text (const GS::UniString& s)
{
    return std::string (s.ToCStr (0, MaxUSize, GSCharCode::CC_UTF8).Get ());
}

GS::UniString Uni (const std::string& s)
{
    return GS::UniString (s.c_str (), CC_UTF8);
}

/** Auf hoechstens `max` Bytes kuerzen, ohne ein UTF-8-Zeichen zu zerteilen. */
std::string KuerzenUtf8 (std::string s, size_t max)
{
    if (s.size () <= max) return s;
    size_t n = max;
    while (n > 0 && (static_cast<unsigned char> (s[n]) & 0xC0) == 0x80) --n;   // s[n] = Folgebyte → zurueck
    s.resize (n);
    return s;
}

/** Letzter Teil eines Pfads (Mac „/“, Windows „\“). */
std::string DateiName (const std::string& pfad)
{
    const size_t p = pfad.find_last_of ("/\\");
    return p == std::string::npos ? pfad : pfad.substr (p + 1);
}

void StammUndEndung (const std::string& name, std::string& stamm, std::string& endung)
{
    const size_t p = name.find_last_of ('.');
    if (p == std::string::npos || p == 0) { stamm = name; endung.clear (); return; }
    stamm  = name.substr (0, p);
    endung = name.substr (p);
}

fs::path BenutzerOrdner ()
{
#ifdef _WIN32
    wchar_t* u = nullptr;
    size_t laenge = 0;
    fs::path ergebnis;
    if (_wdupenv_s (&u, &laenge, L"USERPROFILE") == 0 && u != nullptr)
        ergebnis = fs::path (u);
    free (u);
    return ergebnis;
#else
    if (const char* h = std::getenv ("HOME")) return fs::path (h);
    return {};
#endif
}

std::tm Ortszeit (std::time_t t)
{
    std::tm zeit {};
#ifdef _WIN32
    localtime_s (&zeit, &t);
#else
    localtime_r (&t, &zeit);
#endif
    return zeit;
}

std::string Zeit (const char* muster)
{
    const std::tm zeit = Ortszeit (std::time (nullptr));
    char b[40];
    std::strftime (b, sizeof (b), muster, &zeit);
    return b;
}

void Log (const std::string& msg)
{
    std::error_code ec;
    const fs::path tmp = fs::temp_directory_path (ec);
    if (ec) return;
    std::ofstream lf (tmp / "ELM_SAB_AddOn.log", std::ios::app);
    if (lf.is_open ()) lf << "[" << Zeit ("%H:%M:%S") << "] [ExportSurfaces] " << msg << "\n";
}

std::string Json (const std::string& s)
{
    std::string o;
    o.reserve (s.size () + 8);
    for (const char zeichen : s) {
        const unsigned char c = static_cast<unsigned char> (zeichen);
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf (b, sizeof (b), "\\u%04x", static_cast<unsigned> (c)); o += b; }
                else o += zeichen;
        }
    }
    return o;
}

std::string Zahl (double x, int stellen = 4)
{
    if (!std::isfinite (x)) return "null";
    std::ostringstream s;
    s.imbue (std::locale::classic ());                  // immer Punkt, nie Komma
    s.setf (std::ios::fixed);
    s.precision (stellen);
    s << x;
    std::string t = s.str ();
    while (t.find ('.') != std::string::npos && (t.back () == '0' || t.back () == '.')) {
        const bool punkt = t.back () == '.';
        t.pop_back ();
        if (punkt) break;
    }
    return (t.empty () || t == "-0") ? "0" : t;
}

int Kanal (double v)
{
    return static_cast<int> (std::lround (std::fmax (0.0, std::fmin (1.0, v)) * 255.0));
}

std::string Hex (const API_RGBColor& c)
{
    char h[8];
    std::snprintf (h, sizeof (h), "#%02x%02x%02x", Kanal (c.f_red), Kanal (c.f_green), Kanal (c.f_blue));
    return h;
}

const char* Art (API_MaterTypeID t)
{
    switch (t) {
        case APIMater_GeneralID:  return "allgemein";
        case APIMater_SimpleID:   return "einfach";
        case APIMater_MatteID:    return "matt";
        case APIMater_MetalID:    return "metall";
        case APIMater_PlasticID:  return "kunststoff";
        case APIMater_GlassID:    return "glas";
        case APIMater_GlowingID:  return "leuchtend";
        case APIMater_ConstID:    return "konstant";
        default:                  return "unbekannt";
    }
}

const char* JaNein (bool b)
{
    return b ? "true" : "false";
}

/** Dateiname sicher fuer den Ordner — Umlaute bleiben (UTF-8), Pfadzeichen nicht. */
std::string Sicher (std::string n)
{
    for (char& c : n)
        if (c == '/' || c == ':' || c == '\\' || c == '"' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|')
            c = '-';
    n = KuerzenUtf8 (n, 100);
    return n.empty () ? std::string ("textur") : n;
}

// ---------------------------------------------------------------------------
// Projekt + Bilder
// ---------------------------------------------------------------------------

struct Projekt {
    std::string name;
    std::string datei;
};

Projekt ProjektLesen ()
{
    Projekt p;
    API_ProjectInfo info = {};                         // gibt seine Zeiger selbst frei
    if (ACAPI_ProjectOperation_Project (&info) == NoError) {
        if (info.projectName != nullptr) p.name  = Text (*info.projectName);
        if (info.projectPath != nullptr) p.datei = Text (*info.projectPath);
    }
    if (p.name.empty () && !p.datei.empty ()) {
        std::string stamm, endung;
        StammUndEndung (DateiName (p.datei), stamm, endung);
        p.name = stamm;
    }
    if (p.name.empty ()) p.name = "Projekt";
    return p;
}

/** Bild ueber Archicads Dateisystem lesen (eingebettete Bibliothek, .lcf-Container). */
bool UeberArchicadKopieren (const IO::Location& ort, const fs::path& ziel, std::string& grund)
{
    IO::File f (ort, IO::File::Fail);
    if (f.GetStatus () != NoError) { grund = "Archicad kennt die Datei nicht"; return false; }
    if (f.Open (IO::File::ReadMode) != NoError) { grund = "Archicad oeffnet die Datei nicht"; return false; }
    UInt64 laenge = 0;
    if (f.GetDataLength (&laenge) != NoError || laenge == 0 || laenge > 300ull * 1024 * 1024) {
        f.Close ();
        grund = "Groesse unlesbar (" + std::to_string (laenge) + ")";
        return false;
    }
    std::string daten (static_cast<size_t> (laenge), '\0');
    USize gelesen = 0;
    const GSErrCode e = f.ReadBin (&daten[0], static_cast<USize> (laenge), &gelesen);
    f.Close ();
    if (e != NoError || gelesen != laenge) { grund = "Lesen abgebrochen"; return false; }
    std::ofstream aus (ziel, std::ios::binary);
    aus.write (daten.data (), static_cast<std::streamsize> (daten.size ()));
    if (!aus) { grund = "Schreiben fehlgeschlagen"; return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

struct Ergebnis {
    bool        ok = false;
    std::string fehler;
    std::string ordner;
    std::string projekt;
    int oberflaechen = 0, mitBild = 0, bildFehlt = 0, ohneTextur = 0, bilder = 0;
};

Ergebnis Exportieren (const fs::path& basis)
{
    Ergebnis r;
    const Projekt projekt = ProjektLesen ();
    r.projekt = projekt.name;

    std::string name = Sicher (projekt.name);
    for (char& c : name) if (c == ' ') c = '-';
    name = KuerzenUtf8 (name, 60);
    const fs::path ordner = basis / PfadUtf8 (name + "_oberflaechen_" + Zeit ("%Y%m%d-%H%M%S"));
    const fs::path texturen = ordner / "texturen";
    r.ordner = Utf8 (ordner);

    std::error_code ec;
    fs::create_directories (texturen, ec);
    if (ec) { r.fehler = "Ordner nicht anlegbar: " + r.ordner + " (" + ec.message () + ")"; return r; }
    Log ("Start " + projekt.name + " -> " + r.ordner);

    UInt32 anzahl = 0;
    if (ACAPI_Attribute_GetNum (API_MaterialID, anzahl) != NoError) {
        r.fehler = "Archicad liefert keine Oberflaechen.";
        return r;
    }

    std::map<std::string, std::string> bildNachQuelle;   // Quellpfad → Dateiname im Paket
    std::set<std::string> vergeben;
    std::string liste;

    auto freierName = [&vergeben] (const std::string& quelle) {
        std::string ziel = Sicher (DateiName (quelle));
        std::string stamm, endung;
        StammUndEndung (ziel, stamm, endung);
        // Gleicher Dateiname aus anderer Bibliothek: Nummer anhaengen
        for (int n = 2; vergeben.count (ziel) > 0; ++n) ziel = stamm + "_" + std::to_string (n) + endung;
        return ziel;
    };

    for (UInt32 i = 1; i <= anzahl; ++i) {
        API_Attribute a = {};
        a.header.typeID = API_MaterialID;
        a.header.index  = ACAPI_CreateAttributeIndex (static_cast<Int32> (i));
        GS::UniString attrName;
        a.header.uniStringNamePtr = &attrName;
        IO::Location* angelegt = new IO::Location ();
        a.material.texture.fileLoc = angelegt;
        const GSErrCode getErr = ACAPI_Attribute_Get (&a);
        // Archicad schreibt in den uebergebenen Ort; falls es den Zeiger doch ersetzt, beide freigeben.
        IO::Location* ort = a.material.texture.fileLoc;
        if (ort != angelegt && ort != nullptr) delete angelegt;   // ort gehoert jetzt uns, angelegt ist ungenutzt
        if (ort == nullptr) ort = angelegt;                        // Zeiger genullt: unseren freigeben
        a.material.texture.fileLoc = nullptr;
        if (getErr != NoError) {
            delete ort;
            continue;                                    // geloeschte Indizes haben Luecken
        }
        ++r.oberflaechen;
        const API_MaterialType& m = a.material;
        const API_Texture& t = m.texture;
        const std::string texName = Text (GS::UniString (t.texName));

        std::string tex = "null";
        if (!texName.empty ()) {
            std::string datei, fehlt;
            GS::UniString pfadU;
            if (ort->ToPath (&pfadU) == NoError && !pfadU.IsEmpty ()) {
                const std::string quelle = Text (pfadU);
                auto it = bildNachQuelle.find (quelle);
                if (it != bildNachQuelle.end ()) {
                    datei = it->second;
                } else {
                    const std::string ziel = freierName (quelle);
                    std::error_code e2;
                    const fs::path quellPfad = PfadUtf8 (quelle);
                    bool kopiert = false;
                    if (fs::is_regular_file (quellPfad, e2)) {
                        fs::copy_file (quellPfad, texturen / PfadUtf8 (ziel), fs::copy_options::overwrite_existing, e2);
                        kopiert = !e2;
                        if (e2) Log ("kopieren fehlgeschlagen: " + quelle + " (" + e2.message () + ")");
                    }
                    std::string grund;
                    if (!kopiert) kopiert = UeberArchicadKopieren (*ort, texturen / PfadUtf8 (ziel), grund);
                    if (kopiert) {
                        datei = ziel;
                        vergeben.insert (ziel);
                        bildNachQuelle[quelle] = ziel;
                    } else {
                        fehlt = "Datei fehlt: " + quelle;
                        Log ("nicht lesbar: " + quelle + " (" + grund + ")");
                    }
                }
            } else {
                fehlt = "kein Ort";
            }
            if (t.missingPict && fehlt.empty () && datei.empty ()) fehlt = "Archicad meldet fehlendes Bild";
            if (datei.empty ()) ++r.bildFehlt; else ++r.mitBild;

            tex = "{\"name\":\"" + Json (texName) + "\"";
            tex += datei.empty () ? ",\"datei\":null" : ",\"datei\":\"texturen/" + Json (datei) + "\"";
            if (!fehlt.empty ()) tex += ",\"bild_fehlt\":\"" + Json (fehlt) + "\"";
            tex += ",\"breite_m\":" + Zahl (t.xSize) + ",\"hoehe_m\":" + Zahl (t.ySize);
            tex += ",\"drehung_grad\":" + Zahl (t.rotAng * 180.0 / kPi, 2);
            tex += std::string (",\"alpha\":")                 + JaNein ((t.status & APITxtr_UseAlpha) != 0);
            tex += std::string (",\"bump\":")                  + JaNein ((t.status & APITxtr_BumpPattern) != 0);
            tex += std::string (",\"spiegeln_x\":")            + JaNein ((t.status & APITxtr_MirrorX) != 0);
            tex += std::string (",\"spiegeln_y\":")            + JaNein ((t.status & APITxtr_MirrorY) != 0);
            tex += std::string (",\"zufall_versatz\":")        + JaNein ((t.status & APITxtr_RandomShift) != 0);
            tex += std::string (",\"transparenz_aus_alpha\":") + JaNein ((t.status & APITxtr_TransPattern) != 0);
            tex += "}";
        } else {
            ++r.ohneTextur;
        }
        delete ort;

        if (!liste.empty ()) liste += ",\n";
        liste += "  {\"name\":\"" + Json (Text (attrName)) + "\"";
        liste += ",\"index\":" + std::to_string (i);
        liste += ",\"guid\":\"" + Json (Text (APIGuidToString (a.header.guid))) + "\"";
        liste += std::string (",\"art\":\"") + Art (m.mtype) + "\"";
        liste += ",\"farbe\":\"" + Hex (m.surfaceRGB) + "\"";
        liste += ",\"spiegelfarbe\":\"" + Hex (m.specularRGB) + "\"";
        liste += ",\"emission\":\"" + Hex (m.emissionRGB) + "\"";
        liste += ",\"umgebung_pc\":" + std::to_string (static_cast<int> (m.ambientPc));
        liste += ",\"diffus_pc\":" + std::to_string (static_cast<int> (m.diffusePc));
        liste += ",\"spiegelung_pc\":" + std::to_string (static_cast<int> (m.specularPc));
        liste += ",\"transparenz_pc\":" + std::to_string (static_cast<int> (m.transpPc));
        liste += ",\"glanz\":" + Zahl (m.shine / 100.0, 2);
        liste += ",\"textur\":" + tex + "}";
    }
    r.bilder = static_cast<int> (vergeben.size ());

    const std::string iso = Zeit ("%Y-%m-%dT%H:%M:%S");
    {
        std::ofstream f (ordner / "oberflaechen.json", std::ios::trunc | std::ios::binary);
        f << "{\"format\":\"sab-oberflaechen/1\",\n"
          << " \"projekt\":\"" << Json (projekt.name) << "\",\n"
          << " \"datei\":\"" << Json (projekt.datei) << "\",\n"
          << " \"plattform_projekt\":null,\n"
          << " \"archicad_version\":" << kArchicadVersion << ",\n"
          << " \"quelle\":\"ELM_SAB_Add-On ExportSurfaces " << ELM_SAB_VERSION << "\",\n"
          << " \"exportiert_am\":\"" << iso << "\",\n"
          << " \"zaehler\":{\"oberflaechen\":" << r.oberflaechen << ",\"mit_bild\":" << r.mitBild
          << ",\"bild_fehlt\":" << r.bildFehlt << ",\"ohne_textur\":" << r.ohneTextur
          << ",\"bilder\":" << r.bilder << "},\n"
          << " \"oberflaechen\":[\n" << liste << "\n]}\n";
        if (!f) { r.fehler = "oberflaechen.json nicht schreibbar"; return r; }
    }
    {
        std::ofstream f (ordner / "LIESMICH.md", std::ios::trunc | std::ios::binary);
        f << "# Oberflaechen aus Archicad - " << projekt.name << "\n\n"
          << "Format `sab-oberflaechen/1`, exportiert " << iso << " mit dem ELM_SAB_Add-On (ExportSurfaces "
          << ELM_SAB_VERSION << ", Archicad " << kArchicadVersion << ").\n\n"
          << "* `oberflaechen.json` - je Archicad-Oberflaeche Farbe, Glanz, Transparenz und Textur mit\n"
          << "  Groesse im Modell (breite_m/hoehe_m = Kachelmass), Drehung, Flags.\n"
          << "* `texturen/` - genau die Bilder, die diese Oberflaechen nutzen.\n\n"
          << "In die Plattform kommt der Ordner ueber deren Einlese-Werkzeug (erst Trockenlauf).\n\n"
          << "**Lizenz:** Bilder aus der Graphisoft-Standardbibliothek sind an Archicad gebunden -\n"
          << "nur buero-intern verwenden.\n";
    }

    Log ("Fertig: " + std::to_string (r.oberflaechen) + " Oberflaechen, " + std::to_string (r.bilder) +
         " Bilder, " + std::to_string (r.bildFehlt) + " ohne Bilddatei");
    r.ok = true;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// JSON-Befehl
// ---------------------------------------------------------------------------

GS::Optional<GS::UniString> ExportSurfacesCommand::GetInputParametersSchema () const
{
    return GS::UniString (R"({
        "type": "object",
        "properties": {
            "targetFolder": {
                "type": "string",
                "description": "Ordner, in dem der Exportordner angelegt wird. Ohne Angabe: <Benutzer>/Downloads."
            }
        },
        "additionalProperties": false
    })");
}

GS::Optional<GS::UniString> ExportSurfacesCommand::GetResponseSchema () const
{
    return ELMResponseSchemaWithError (GS::UniString (R"({
        "type": "object",
        "properties": {
            "success":         { "type": "boolean" },
            "folder":          { "type": "string" },
            "project":         { "type": "string" },
            "archicadVersion": { "type": "integer" },
            "surfaces":        { "type": "integer" },
            "withImage":       { "type": "integer" },
            "imageMissing":    { "type": "integer" },
            "withoutTexture":  { "type": "integer" },
            "images":          { "type": "integer" }
        },
        "additionalProperties": false,
        "required": ["success", "folder", "surfaces", "withImage", "imageMissing", "withoutTexture", "images"]
    })"));
}

GS::ObjectState ExportSurfacesCommand::Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const
{
    fs::path basis;
    GS::UniString zielU;
    if (parameters.Get ("targetFolder", zielU) && !zielU.IsEmpty ()) {
        basis = PfadUtf8 (Text (zielU));
    } else {
        const fs::path home = BenutzerOrdner ();
        if (home.empty ())
            return CreateErrorResponseELM (APIERR_GENERAL, GS::UniString ("ExportSurfaces: kein Benutzerordner gefunden - targetFolder angeben"));
        basis = home / "Downloads";
    }

    const Ergebnis r = Exportieren (basis);
    if (!r.ok)
        return CreateErrorResponseELM (APIERR_GENERAL, GS::UniString ("ExportSurfaces: ") + Uni (r.fehler));

    GS::ObjectState response;
    response.Add ("success", true);
    response.Add ("folder", Uni (r.ordner));
    response.Add ("project", Uni (r.projekt));
    response.Add ("archicadVersion", kArchicadVersion);
    response.Add ("surfaces", r.oberflaechen);
    response.Add ("withImage", r.mitBild);
    response.Add ("imageMissing", r.bildFehlt);
    response.Add ("withoutTexture", r.ohneTextur);
    response.Add ("images", r.bilder);
    return response;
}
