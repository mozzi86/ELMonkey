---
phase: quick-261119-wfq
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py
  - core/tests/test_plattform_abgleich_skript.py
autonomous: true
requirements: [QUICK-261119-wfq]

must_haves:
  truths:
    - "After zerlegen(), no returned body has an edge shared by more than 2 triangles (pinched tube profiles from BFW07 become buildable)"
    - "Meshes without >2 edges behave exactly as before (closed cube: 1 body, 12 faces, 8 points; existing two-cubes-on-one-edge test unchanged)"
    - "Full core test suite stays green"
  artifacts:
    - path: "skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py"
      provides: "mannigfaltig(P, F) fan-split + zerlegen() that applies it per component and re-splits"
      contains: "def mannigfaltig"
    - path: "core/tests/test_plattform_abgleich_skript.py"
      provides: "tests for pinched-edge mesh and plain cube"
      contains: "mannigfaltig"
  key_links:
    - from: "zerlegen()"
      to: "mannigfaltig()"
      via: "called per component; component re-split only when vertex count grew"
      pattern: "mannigfaltig\\("
    - from: "morph_body()"
      to: "zerlegen()"
      via: "existing call (line ~493), unchanged"
      pattern: "zerlegen\\(P, F\\)"
---

<objective>
Port the live-proven (BFW07, 19.11.2026) non-manifold-edge fix into the plattform_abgleich script: split vertex fans around edges shared by >2 triangles so Archicad accepts the morph bodies.

Purpose: 6 platform parts (Zuluftstutzen Ø160, Steigrohr Ø315, Kondensat-Fallrohr …) failed with "Failed to create morph" (Solid and Surface). After zerlegen() each still had one edge with 4 triangles (tube cross-section passes through one point twice), and the component stayed connected through other edges. With the fan split all 6 built live, read-back exact, 5 closed solids.
Output: modified plattform_abgleich.py + 2 new tests, one local commit (no push).
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@./CLAUDE.md
@skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py (lines ~442-478 zerlegen, ~487 morph_body)
@/Users/ap/.scratch/bfw07/tools/bauen_surface_probe.py (prototype: port ONLY lines 65-101 fan part + lines 107-112 re-split logic)
@core/tests/test_plattform_abgleich_skript.py (lines ~220-256: _ECKEN, _WUERFEL, _wuerfel, test_zerlegen_trennt_koerper_die_nur_eine_kante_teilen)

<interfaces>
Current (plattform_abgleich.py, stdlib only, `collections` already imported):
- def zerlegen(P: list, F: list) -> list[tuple[list, list]]
  drops degenerate triangles, union-find over triangles joined by edges with exactly 2 faces, returns per group (Pk, Fk) with re-indexed points.
- def volumen(P, F) -> float  (signed)
- morph_body(netze) calls zerlegen(P, F) per mesh — the only call site; do not change it.
Tests import the script as module `pa` (pa.zerlegen, pa.volumen, pa._wuerfel helper is in the test file).
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: mannigfaltig() fan split integrated into zerlegen() + tests</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <behavior>
    - test_zerlegen_loest_mehrfachkante_auf: pinched voxel mesh (see action) -> zerlegen returns exactly 1 body; in that body no edge (sorted vertex pair) is used by more than 2 faces; face count equals input face count; point count = number of distinct grid points used + 2 (one extra copy at each endpoint of the pinch edge); abs(pa.volumen(Pk, Fk)) == pytest.approx(6.0).
    - test_mannigfaltig_laesst_wuerfel_unveraendert: _wuerfel() -> zerlegen gives 1 body with 12 faces and 8 points; pa.mannigfaltig(P, F) on the cube returns the same point count (8) and identical face list.
    - existing test_zerlegen_trennt_koerper_die_nur_eine_kante_teilen still passes unchanged (2 bodies, 12 faces / 8 points each).
  </behavior>
  <action>
RED first: add the two tests to core/tests/test_plattform_abgleich_skript.py right after test_zerlegen_trennt_koerper_die_nur_eine_kante_teilen. Build the pinched mesh with a small local voxel helper (e.g. _voxel_netz(zellen)): points are the integer grid points (x,y,z) indexed via a dict so shared corners get one index; for every occupied unit cell and each of its 6 sides whose neighbouring cell is NOT occupied, emit the side quad as 2 triangles with outward orientation (consistent winding; volume sign does not matter for the test since abs() is used, but winding must be consistent so adjacent quads share edges normally). Occupied cells: bottom layer z=0 only the diagonal cells (0,0,0) and (1,1,0); top layer z=1 all four cells (0,0,1),(1,0,1),(0,1,1),(1,1,1). The vertical edge (1,1,0)-(1,1,1) is then shared by 4 triangles while the body stays connected through the top slab — exactly the BFW07 situation (one component, one 4-face edge). Add an assert in the test that the INPUT mesh really has one edge with 4 faces (guards the fixture). Count edges with a tiny helper over sorted index pairs. Run tests: the new pinch test must fail (current zerlegen leaves the 4-face edge in one body).

GREEN in plattform_abgleich.py:
1. Rename the current body of zerlegen() (degenerate filter + union-find + re-index) to a private helper _komponenten(P, F) with identical code and docstring adapted.
2. Add public mannigfaltig(P, F) -> tuple[list, list], ported from the prototype's fan part (prototype lines 65-101) ONLY. Do NOT port paar_von, the angle sorting, math/_sub/_cross/_dot — that code is dead. Algorithm: build edge->faces map from the input faces; build vertex->incident faces; for each vertex v, union-find over its incident faces, connecting face n with the other face of edge (v, x) only if that edge has exactly 2 faces (edges with >2 faces do not connect); the first fan group keeps v, every further group gets a new point appended (copy of P[v], same coordinates) and v is replaced by the new index in those faces. Improvement over prototype (intentional, document it in the comment): perform all adjacency lookups on an untouched copy of the original faces (F0) and write substitutions into a separate output face list, so earlier vertex substitutions cannot make later edge lookups miss (prototype looked up edges in the already-modified F via a defaultdict, which can silently split extra fans). Use plain dict.get(edge, ()) rather than defaultdict lookups that insert keys. Return (new points list incl. copies, new faces list); points list must be new lists (do not mutate the caller's P/F).
3. New zerlegen(P, F): for each (Pk, Fk) in _komponenten(P, F): P2, F2 = mannigfaltig(Pk, Fk); if len(P2) != len(Pk) extend result with _komponenten(P2, F2) else append (Pk, Fk) unchanged. This keeps behaviour bit-identical for meshes without >2 edges. Keep the signature and keep the existing docstring meaning, extending it with the fan-split step.
4. Add a short comment (German, like the rest of the file) with date marker 2026-11-19 above mannigfaltig or in zerlegen explaining why: live BFW07, 6 Plattform-Teile (Zuluftstutzen Ø160, Steigrohr Ø315, Kondensat-Fallrohr …) scheiterten mit "Failed to create morph" (Solid und Surface), weil nach dem Trennen noch eine Kante mit 4 Dreiecken übrig blieb (Rohrprofil läuft zweimal durch einen Punkt); Fächer je Eckpunkt trennen löst es, live alle 6 gebaut, Rücklese exakt.
Stdlib only (project constraint); no new imports needed beyond collections.
Run the full suite; all green.
  </action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && PYTHONPATH=core uv run --no-project --with pytest python -m pytest core/tests -q</automated>
  </verify>
  <done>New tests pass, previous tests unchanged and green; grep -c "paar_von\|atan2" on plattform_abgleich.py returns 0; mannigfaltig and _komponenten exist; morph_body untouched.</done>
</task>

<task type="auto">
  <name>Task 2: Local commit (no push)</name>
  <files>skills/archicad/scripts/plattform_abgleich/plattform_abgleich.py, core/tests/test_plattform_abgleich_skript.py</files>
  <action>Stay on branch fix-aggregation-eltern (do not switch, do not stash). Stage ONLY the two files. Commit with local email override: git -c user.email=mozzi666@gmail.com commit, message e.g. "fix(plattform_abgleich): Mehrfachkanten per Fächer-Trennung auflösen" with a short body (BFW07 19.11., 6 Teile, Fan-Split statt Winkelpaarung) and the trailer line "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>". Do not change global git config. No push.</action>
  <verify>
    <automated>cd /Users/ap/.scratch/elmonkey/diff-plattform-wt && git log -1 --format='%ae %s%n%b' && git show --stat HEAD | tail -4 && git rev-parse --abbrev-ref HEAD</automated>
  </verify>
  <done>HEAD commit author email mozzi666@gmail.com, touches exactly the two files, has the Co-Authored-By trailer, branch still fix-aggregation-eltern, nothing pushed.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| platform IFC -> mesh -> Archicad morph | geometry from platform download is turned into morph bodies |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-wfq-01 | Tampering | mannigfaltig() | mitigate | works on copies (no mutation of caller data); behaviour identical for clean meshes via len(P2)!=len(Pk) guard; covered by cube + existing tests |
| T-wfq-02 | Denial of Service | mannigfaltig() on large meshes | accept | linear-ish per vertex fan, same complexity class as existing zerlegen; live BFW07 run completed |
</threat_model>

<verification>
Full core test suite green; pinched-mesh test proves no >2 edge remains while the body stays one piece; cube and existing two-cube tests prove unchanged behaviour; dead angle-pairing code absent.
</verification>

<success_criteria>
- zerlegen() resolves >2-face edges by vertex fan duplication, per component, with re-split
- Date-marked (2026-11-19) comment explains the BFW07 cause
- Only the two files changed, one local commit with required email and trailer, no push
</success_criteria>

<output>
Create `.planning/quick/261119-wfq-mehrfachkanten-aufloesen/261119-wfq-SUMMARY.md` when done
</output>
