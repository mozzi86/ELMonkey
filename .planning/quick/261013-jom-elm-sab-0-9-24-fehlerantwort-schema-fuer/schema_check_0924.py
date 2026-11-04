#!/usr/bin/env python3
"""Pruefskript Quick 261013-jom (ELM_SAB 0.9.24, Fehlerantwort schemakonform fuer sechs Befehle).

Aufruf:  python3 -I schema_check_0924.py <repo> <basis-rev> <sollversion>
         z. B. python3 -I schema_check_0924.py . 92ab02a 0.9.24

Nur Standardbibliothek. Basis-Dateien kommen per `git -C <repo> show <basis-rev>:<pfad>`,
der aktuelle Stand aus dem Arbeitsverzeichnis <repo>.
Ausgabe: je Pruefpunkt eine Zeile `OK ...` bzw. `FEHLER ...`. Exit 1 bei mindestens einem Fehler.
Es wird nichts geschrieben und kein Netzwerk benutzt.
"""
import difflib
import json
import re
import subprocess
import sys

SRC = "ELM_SAB_Tapir/Sources/"

CWO_NOTSUP = (
    "CreateWallOpenings braucht die ACAPI::Element::Opening-API und damit Archicad 29. "
    "Unter AC27/28 liegt API_OpeningType noch in der API_Element-Union, die Felder fuer "
    "Anker, Grenze und Grundrissdarstellung sind im vorliegenden DevKit aber nicht "
    "nachlesbar - deshalb wird hier nichts geraten."
)

# (Klasse, Datei, Registrierungs-Klasse, erwartete Top-Level-Fehler [(Konstante, Text, Variable|None)])
COMMANDS = [
    ("CreateWallOpeningsCommand", "CreateWallOpeningsCommand.cpp", [
        ("APIERR_BADPARS", "openings ist leer.", None),
        ("APIERR_NOTSUPPORTED", CWO_NOTSUP, None),
    ]),
    ("SetLayerOfElementsCommand", "SetLayerCommand.cpp", [
        ("APIERR_BADPARS", "layerId oder layerName erforderlich.", None),
        ("APIERR_BADPARS", "Ebene nicht gefunden: ", "what"),
    ]),
    ("SetObjectParametersForceCommand", "SetObjectParametersForceCommand.cpp", [
        ("APIERR_BADPARS", "elements ist leer.", None),
    ]),
    ("SetPenOfElementsCommand", "SetPenOfElementsCommand.cpp", [
        ("APIERR_BADPARS",
         "Mindestens ein Stift-Parameter oder clearForegroundRGB/clearBackgroundRGB muss gesetzt sein.", None),
    ]),
    ("SetStoryVisibilityOfElementsCommand", "StoryVisibilityCommands.cpp", [
        ("APIERR_BADPARS", "Unbekannter visibility-Wert: ", "preset"),
        ("APIERR_BADPARS", "visibility oder custom erforderlich.", None),
    ]),
    ("SetTextSizeCommand", "SetTextSizeCommand.cpp", [
        ("APIERR_BADPARS", "Genau einer von sizeMm oder factor muss angegeben sein (> 0).", None),
    ]),
]

LIT = r'"(?:[^"\\]|\\.)*"'

failures = 0


def ok(msg):
    print("OK     " + msg)


def fail(msg):
    global failures
    failures += 1
    print("FEHLER " + msg)


def check(cond, good, bad):
    if cond:
        ok(good)
    else:
        fail(bad)
    return cond


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    repo, base_rev, soll = sys.argv[1], sys.argv[2], sys.argv[3]

    def read_base_bytes(path):
        return subprocess.run(
            ["git", "-C", repo, "show", base_rev + ":" + path],
            check=True, capture_output=True).stdout

    def read_cur_bytes(path):
        with open(repo.rstrip("/") + "/" + path, "rb") as fh:
            return fh.read()

    def dec(b):
        return b.decode("utf-8", errors="surrogateescape")

    # ---- Fehlerzweig aus dem echten Header ziehen --------------------------------------
    hpp = dec(read_cur_bytes(SRC + "ELMCommandBase.hpp"))
    m = re.search(r"ELMResponseSchemaWithError \(const GS::UniString& successSchema\)\s*\{(.*?)\n\}", hpp, re.S)
    if not m:
        fail("ELMResponseSchemaWithError in ELMCommandBase.hpp nicht gefunden")
        return 1
    lits = [json.loads(l) for l in re.findall(LIT, m.group(1))]
    wrap_head, wrap_tail = lits[0], "".join(lits[1:])

    def compose(success_text):
        return wrap_head + success_text + wrap_tail

    for cls, fname, expected in COMMANDS:
        path = SRC + fname
        print("--- " + cls + " (" + fname + ")")
        base = dec(read_base_bytes(path))
        cur = dec(read_cur_bytes(path))
        n = len(expected)

        # ---- GetResponseSchema ---------------------------------------------------------
        sch_re = re.compile(
            r"(?<![A-Za-z0-9_])" + cls + r"::GetResponseSchema \(\) const\s*\{(.*?)\n\}", re.S)
        mb, mc = sch_re.search(base), sch_re.search(cur)
        if not (mb and mc):
            fail("GetResponseSchema nicht gefunden (Basis=%s, aktuell=%s)" % (bool(mb), bool(mc)))
            continue
        raw_b = re.search(r'R"\((.*?)\)"', mb.group(1), re.S)
        raw_c = re.search(r'R"\((.*?)\)"', mc.group(1), re.S)
        if not (raw_b and raw_c):
            fail("Raw-String im GetResponseSchema nicht gefunden")
            continue

        check(re.search(r'\n    return GS::UniString \(R"\(', mb.group(1)) is not None,
              "Basis: return GS::UniString (R\"(...)\") vorhanden",
              "Basis hat nicht die erwartete Form return GS::UniString (R\"(")
        wrapped = re.search(
            r'\n    return ELMResponseSchemaWithError \(GS::UniString \(R"\(.*?\)"\)\);\s*$',
            mc.group(1), re.S) is not None
        check(wrapped,
              "aktuell: return ELMResponseSchemaWithError (GS::UniString (R\"(...)\"));",
              "aktuell: GetResponseSchema ist NICHT in ELMResponseSchemaWithError gefasst")
        check(mc.group(1).count("ELMResponseSchemaWithError") == 2,
              "aktuell: ELMResponseSchemaWithError je Schema 1x im Code + 1x im Kommentar",
              "aktuell: erwartet 2 Vorkommen (Code + Kommentar), gefunden %d"
              % mc.group(1).count("ELMResponseSchemaWithError"))
        comment = ("// ELM_SAB %s: Erfolgszweig unveraendert; ELMResponseSchemaWithError erlaubt zusaetzlich "
                   "{error:{code,message}} (sonst 4009 statt Meldung)." % soll)
        check(comment in mc.group(1),
              "aktuell: Kommentarzeile ELM_SAB %s vorhanden" % soll,
              "aktuell: Kommentarzeile fehlt oder weicht ab")
        check(raw_b.group(1) == raw_c.group(1),
              "Erfolgsschema (Raw-String) byte-gleich zur Basis (%d Zeichen)" % len(raw_c.group(1)),
              "Erfolgsschema (Raw-String) WEICHT von der Basis ab")

        # ---- JSON-Pruefung des Erfolgsschemas und des zusammengesetzten oneOf -------------
        try:
            succ = json.loads(raw_c.group(1))
        except ValueError as exc:
            fail("Erfolgsschema ist kein gueltiges JSON: %s" % exc)
            continue
        req = succ.get("required", [])
        check(len(req) > 0, "Erfolgsschema: required nicht leer %s" % req,
              "Erfolgsschema: required ist leer")
        check("error" not in req and "error" not in succ.get("properties", {}),
              "Erfolgsschema: 'error' weder in required noch in properties",
              "Erfolgsschema: 'error' kommt vor (oneOf waere mehrdeutig)")
        try:
            combined = json.loads(compose(raw_c.group(1)))
            branches = combined["oneOf"]
            check(len(branches) == 2, "oneOf-Schema ist gueltiges JSON mit genau 2 Zweigen",
                  "oneOf hat %d statt 2 Zweigen" % len(branches))
            check(branches[0] == succ, "oneOf[0] == Erfolgsschema", "oneOf[0] != Erfolgsschema")
            check(branches[1].get("required") == ["error"], "oneOf[1].required == ['error']",
                  "oneOf[1].required ist nicht ['error']")
        except Exception as exc:  # noqa: BLE001
            fail("zusammengesetztes oneOf-Schema nicht pruefbar: %s" % exc)
            continue
        err_example = {"error": {"code": -2130313112, "message": "x"}}
        succ_example = {k: [] for k in req}
        check(not all(k in err_example for k in req),
              "Fehlerbeispiel erfuellt required des Erfolgszweigs NICHT",
              "Fehlerbeispiel passt auch auf den Erfolgszweig (4009 moeglich)")
        check(all(k in err_example for k in branches[1]["required"]),
              "Fehlerbeispiel erfuellt required des Fehlerzweigs",
              "Fehlerbeispiel passt nicht auf den Fehlerzweig")
        check(not all(k in succ_example for k in branches[1]["required"]),
              "Erfolgsbeispiel erfuellt required des Fehlerzweigs NICHT",
              "Erfolgsbeispiel passt auch auf den Fehlerzweig")

        # ---- Execute: Top-Level-Fehlerrueckgaben ----------------------------------------
        ex_re = re.compile(r"(?<![A-Za-z0-9_])" + cls + r"::Execute \(.*?\n\}", re.S)
        eb, ec = ex_re.search(base), ex_re.search(cur)
        if not (eb and ec):
            fail("Execute nicht gefunden")
            continue
        check(ec.group(0).count("CreateErrorResponse (") == 0,
              "Execute: 0 x 'CreateErrorResponse ('",
              "Execute: noch %d x 'CreateErrorResponse ('" % ec.group(0).count("CreateErrorResponse ("))
        check(ec.group(0).count("return CreateErrorResponseELM (") == n,
              "Execute: genau %d x 'return CreateErrorResponseELM ('" % n,
              "Execute: %d x 'return CreateErrorResponseELM (' statt %d"
              % (ec.group(0).count("return CreateErrorResponseELM ("), n))
        check(cur.count("CreateErrorResponse (") == 0 and cur.count("CreateErrorResponseELM (") == n,
              "Datei: keine Tapir-CreateErrorResponse-Reste, %d x CreateErrorResponseELM" % n,
              "Datei: unerwartete CreateErrorResponse-Aufrufe ausserhalb des Execute")

        def parse_returns(text, elm):
            out = []
            name = "CreateErrorResponseELM" if elm else "CreateErrorResponse"
            for mm in re.finditer(r"return " + name + r" \((APIERR_\w+),(.*?)\);", text, re.S):
                const, args = mm.group(1), mm.group(2).strip()
                if elm:
                    mm2 = re.match(r"^GS::UniString \(\s*((?:" + LIT + r"\s*)+)\)(?:\s*\+\s*(\w+))?$", args, re.S)
                else:
                    mm2 = re.match(r"^((?:" + LIT + r"\s*)+?)(?:\+\s*(\w+))?$", args, re.S)
                if not mm2:
                    out.append((const, None, "<Form nicht erkannt: %r>" % args))
                    continue
                text_lit = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', mm2.group(1)))
                out.append((const, text_lit, mm2.group(2)))
            return out

        rb = parse_returns(eb.group(0), False)
        rc = parse_returns(ec.group(0), True)
        check(rb == expected, "Basis: %d Top-Level-Fehler entsprechen der Plan-Tabelle" % n,
              "Basis weicht von der Plan-Tabelle ab: %r" % (rb,))
        check(rc == rb, "aktuell: Konstante, Meldungstext und Verkettungsvariable je Fehler unveraendert",
              "aktuell weicht ab:\n         Basis    %r\n         aktuell  %r" % (rb, rc))

        # ---- Ganze Datei: ausser Schema-Funktion und Fehlerrueckgaben identisch ----------
        def normalize(src):
            src = sch_re.sub("SCHEMA_FUNC", src, count=1)
            return re.sub(r"return CreateErrorResponse(?:ELM)? \(.*?\);", "ERRRET;", src, flags=re.S)

        check(normalize(base) == normalize(cur),
              "Rest der Datei (ohne GetResponseSchema + Fehlerrueckgaben) byte-gleich zur Basis",
              "Datei weicht ausserhalb von GetResponseSchema/Fehlerrueckgaben von der Basis ab")

        # ---- GetStoryVisibilityOfElements unangetastet -----------------------------------
        if cls == "SetStoryVisibilityOfElementsCommand":
            g = "GetStoryVisibilityOfElementsCommand"
            gs_re = re.compile(r"(?<![A-Za-z0-9_])" + g + r"::GetResponseSchema \(\) const\s*\{.*?\n\}", re.S)
            ge_re = re.compile(r"(?<![A-Za-z0-9_])" + g + r"::Execute \(.*?\n\}", re.S)
            for label, rx in (("GetResponseSchema", gs_re), ("Execute", ge_re)):
                x, y = rx.search(base), rx.search(cur)
                check(bool(x and y) and x.group(0) == y.group(0),
                      "%s::%s byte-gleich zur Basis" % (g, label),
                      "%s::%s fehlt oder WEICHT ab" % (g, label))

    # ---- AddOnMain.cpp: genau 6 Beschreibungszeilen ------------------------------------
    print("--- AddOnMain.cpp")
    am_path = SRC + "AddOnMain.cpp"
    base_am = dec(read_base_bytes(am_path)).split("\n")
    cur_am = dec(read_cur_bytes(am_path)).split("\n")
    suffix = " %s: Aufruffehler kommen schemakonform beim Client an (statt 4009)." % soll
    changed = [(i1, i2, j1, j2) for tag, i1, i2, j1, j2 in
               difflib.SequenceMatcher(None, base_am, cur_am, autojunk=False).get_opcodes() if tag != "equal"]
    nchanged = sum(i2 - i1 for i1, i2, _, _ in changed)
    check(nchanged == 6 and all((i2 - i1) == (j2 - j1) for i1, i2, j1, j2 in changed),
          "AddOnMain.cpp: genau 6 Zeilen geaendert, keine eingefuegt/entfernt",
          "AddOnMain.cpp: %d Zeilen geaendert (Bloecke %r)" % (nchanged, changed))
    for cls, _f, _e in COMMANDS:
        ib = [i for i, l in enumerate(base_am) if ("RegisterCommand<" + cls + "> (") in l]
        ic = [i for i, l in enumerate(cur_am) if ("RegisterCommand<" + cls + "> (") in l]
        if len(ib) != 1 or len(ic) != 1:
            fail("AddOnMain: Registrierung von %s nicht eindeutig (Basis %d, aktuell %d)" % (cls, len(ib), len(ic)))
            continue
        b, c = ib[0], ic[0]
        ver_same = base_am[b + 1] == cur_am[c + 1]
        old_desc, new_desc = base_am[b + 2], cur_am[c + 2]
        exp_desc = old_desc[:old_desc.rindex('"')] + suffix + '"' if old_desc.rstrip().endswith('"') else None
        check(ver_same and new_desc == exp_desc and cur_am[c + 3] == base_am[b + 3],
              "AddOnMain: %s Versionsargument unveraendert, Beschreibung = alt + Suffix" % cls,
              "AddOnMain: %s weicht ab (Version gleich=%s, Beschreibung erwartet/ist:\n         %r\n         %r)"
              % (cls, ver_same, exp_desc, new_desc))

    # ---- AddOnVersion.hpp ----------------------------------------------------------------
    print("--- AddOnVersion.hpp")
    v_path = SRC + "AddOnVersion.hpp"
    base_v = dec(read_base_bytes(v_path)).split("\n")
    cur_v = dec(read_cur_bytes(v_path)).split("\n")
    vchg = [(i1, i2, j1, j2) for tag, i1, i2, j1, j2 in
            difflib.SequenceMatcher(None, base_v, cur_v, autojunk=False).get_opcodes() if tag != "equal"]
    only_version = (sum(i2 - i1 for i1, i2, _, _ in vchg) <= 1 and
                    all((i2 - i1) == (j2 - j1) for i1, i2, j1, j2 in vchg))
    check(only_version, "AddOnVersion.hpp: hoechstens die ELM_SAB_VERSION-Zeile geaendert",
          "AddOnVersion.hpp: mehr als eine Zeile geaendert: %r" % (vchg,))
    check(('#define ELM_SAB_VERSION "%s"' % soll) in cur_v,
          '#define ELM_SAB_VERSION "%s"' % soll, "ELM_SAB_VERSION ist nicht %s" % soll)
    check('#define ADDON_VERSION "1.5.4"' in cur_v, 'ADDON_VERSION bleibt "1.5.4"',
          "ADDON_VERSION ist nicht mehr 1.5.4")

    # ---- Nicht-ASCII-Bytes je Datei unveraendert -----------------------------------------
    print("--- Nicht-ASCII-Bytes")
    files = [SRC + f for _c, f, _e in COMMANDS] + [am_path, v_path]
    for p in files:
        nb = sum(1 for x in read_base_bytes(p) if x > 127)
        nc = sum(1 for x in read_cur_bytes(p) if x > 127)
        check(nb == nc, "%s: %d Nicht-ASCII-Bytes (unveraendert)" % (p.split("/")[-1], nc),
              "%s: Nicht-ASCII-Bytes Basis %d -> aktuell %d" % (p.split("/")[-1], nb, nc))

    print()
    if failures:
        print("ERGEBNIS: %d FEHLER" % failures)
        return 1
    print("ERGEBNIS: alle Pruefpunkte OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
