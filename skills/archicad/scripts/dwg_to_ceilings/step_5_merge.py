#!/usr/bin/env python3
"""
Text-basiertes Merge mehrerer IFC-Dateien.
IFC ist STEP-Format mit #N=ENTITY(...) Zeilen.

Vorgehen:
  - Aus jeder Quell-IFC die Data-Section extrahieren.
  - Entitaeten der ersten Datei unveraendert uebernehmen.
  - Fuer jede weitere Datei: alle IDs um max_id der bisher gemergten Datei verschieben.
  - Doppelte Singleton-Entities (Project, Site, Building, Storey, OwnerHistory) NICHT mergen --
    wir behalten nur die aus der Hauptdatei. Die zusaetzlichen Slabs werden NICHT explizit
    an Storey gebunden -- viewer zeigen alle IfcSlab trotzdem an. Aber fuer saubere Hierarchie
    fuegen wir nachtraeglich Rels manuell hinzu.

Diese Version ist primitiv: sie kopiert ALLE Entities aus jeder Datei mit ID-Offset.
Das Resultat hat redundante Project/Site/Building/Storey -- die meisten Viewer ignorieren das
oder zeigen mehrere Standorte.
"""
import sys, re, time

def parse_ifc(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    # split into header (before DATA;) and data
    m = re.search(r'\bDATA;\s*\n', text)
    if not m:
        raise ValueError("no DATA; section in "+path)
    header_end = m.end()
    m2 = re.search(r'\bENDSEC;\s*\n\s*END-ISO[^\n]*', text[header_end:])
    if not m2:
        raise ValueError("no ENDSEC after data in "+path)
    data = text[header_end : header_end + m2.start()]
    # parse entities
    entities = []   # list of (id, full_line)
    max_id = 0
    for ln in data.splitlines():
        ln = ln.strip()
        if not ln: continue
        m3 = re.match(r'#(\d+)\s*=\s*(\S.*?);$', ln)
        if not m3:
            continue
        eid = int(m3.group(1))
        max_id = max(max_id, eid)
        entities.append((eid, ln))
    return text[:header_end], entities, max_id, text[header_end + m2.start():]


def shift_ids(line, offset, drop_ids=None):
    """Shift all #N references in line by offset (except those in drop_ids)."""
    # we need to shift the LHS id AND all referenced ids
    def repl(m):
        eid = int(m.group(1))
        if drop_ids and eid in drop_ids:
            return f"#{eid}"  # keep original (will be replaced separately)
        return f"#{eid + offset}"
    return re.sub(r'#(\d+)', repl, line)


def remap_line(line, mapping):
    """Replace #N occurrences according to mapping dict."""
    def repl(m):
        eid = int(m.group(1))
        if eid in mapping:
            return f"#{mapping[eid]}"
        return m.group(0)
    return re.sub(r'#(\d+)', repl, line)


def find_entity_id(entities, typename):
    pat = re.compile(rf'^#(\d+)\s*=\s*{typename}\b', re.IGNORECASE)
    out = []
    for eid, ln in entities:
        m = pat.match(ln)
        if m:
            out.append(eid)
    return out


def main():
    out_path = sys.argv[1]
    main_path = sys.argv[2]
    add_paths = sys.argv[3:]

    t0 = time.time()
    print(f"[{time.time()-t0:.1f}s] Haupt: {main_path}", flush=True)
    header, ents, max_id, footer = parse_ifc(main_path)
    print(f"  Entities: {len(ents)}, max_id: {max_id}", flush=True)

    # Wichtige Singletons aus Hauptdatei
    def first_of(ents, typ):
        ids = find_entity_id(ents, typ)
        return ids[0] if ids else None

    main_singletons = {}
    for typ in ['IFCPROJECT','IFCSITE','IFCBUILDING','IFCBUILDINGSTOREY',
                'IFCOWNERHISTORY','IFCPERSON','IFCORGANIZATION',
                'IFCPERSONANDORGANIZATION','IFCAPPLICATION',
                'IFCUNITASSIGNMENT',
                'IFCGEOMETRICREPRESENTATIONCONTEXT',
                'IFCGEOMETRICREPRESENTATIONSUBCONTEXT']:
        first = first_of(ents, typ)
        if first is not None:
            main_singletons[typ] = first
    main_storey = main_singletons.get('IFCBUILDINGSTOREY')
    main_owner  = main_singletons.get('IFCOWNERHISTORY')
    if not main_storey or not main_owner:
        print("Fehler: keine Storey/OwnerHistory in Hauptdatei", file=sys.stderr); return 2
    print(f"  Hauptdatei Singletons: {main_singletons}", flush=True)

    output_lines = [ln for _, ln in ents]
    current_max = max_id

    for ap in add_paths:
        print(f"[{time.time()-t0:.1f}s] Lese {ap}...", flush=True)
        _, ae, _max, _ = parse_ifc(ap)
        offset = current_max  # neue IDs starten ab current_max+1
        new_max = 0
        # Singletons in der Zusatz-Datei finden und Mapping bauen.
        # Wir bauen mapping: ADD_ID -> MAIN_ID fuer alle Singleton-Typen.
        # Andere Entitaeten werden mit offset verschoben.
        SKIP_TYPES = {
            'IFCPROJECT','IFCSITE','IFCBUILDING','IFCBUILDINGSTOREY',
            'IFCRELAGGREGATES','IFCRELCONTAINEDINSPATIALSTRUCTURE',
            'IFCOWNERHISTORY','IFCPERSON','IFCORGANIZATION','IFCPERSONANDORGANIZATION',
            'IFCAPPLICATION','IFCUNITASSIGNMENT','IFCSIUNIT',
            'IFCGEOMETRICREPRESENTATIONCONTEXT','IFCGEOMETRICREPRESENTATIONSUBCONTEXT',
        }
        # Mapping ADD_ID -> MAIN_ID (nur fuer Typen mit korrespondierendem Singleton in Hauptdatei)
        id_remap = {}
        skipped_ids = set()
        type_pat = re.compile(r'^#(\d+)\s*=\s*([A-Z0-9]+)\b')
        for eid, ln in ae:
            m = type_pat.match(ln)
            if not m: continue
            typ = m.group(2).upper()
            if typ in SKIP_TYPES:
                skipped_ids.add(eid)
                if typ in main_singletons:
                    id_remap[eid] = main_singletons[typ]
                # IFCSIUNIT, IFCREL* haben keine main-singleton -> Referenzen bleiben ungemapped
                # (sollten kaum vorkommen ausserhalb von UnitAssignment)

        ent_lines_out = []
        for eid, ln in ae:
            if eid in skipped_ids:
                continue
            new_id = eid + offset
            new_max = max(new_max, new_id)
            def repl(mm):
                rid = int(mm.group(1))
                if rid in id_remap:
                    return f"#{id_remap[rid]}"
                return f"#{rid + offset}"
            new_ln = re.sub(r'#(\d+)', repl, ln)
            ent_lines_out.append(new_ln)

        # Slabs einsammeln
        new_slab_ids = []
        slab_pat = re.compile(r'^#(\d+)\s*=\s*IFCSLAB\b', re.IGNORECASE)
        for ln in ent_lines_out:
            ms = slab_pat.match(ln)
            if ms: new_slab_ids.append(int(ms.group(1)))

        output_lines.extend(ent_lines_out)
        current_max = new_max + 1  # next offset basis

        print(f"  -> {len(ent_lines_out)} Entities kopiert ({len(new_slab_ids)} Slabs)", flush=True)

        # neue IfcRelContainedInSpatialStructure: alle neuen Slabs an Hauptstorey haengen
        if new_slab_ids:
            current_max += 1
            rel_id = current_max
            slab_refs = ",".join(f"#{i}" for i in new_slab_ids)
            import uuid
            import ifcopenshell
            guid = ifcopenshell.guid.compress(uuid.uuid4().hex)
            rel_line = (f"#{rel_id}=IFCRELCONTAINEDINSPATIALSTRUCTURE('{guid}',#{main_owner},"
                        f"'MergeContainer',$,(${slab_refs[1:]}),#{main_storey});").replace("(${", "(")
            # Bau die richtige Form:
            rel_line = (f"#{rel_id}=IFCRELCONTAINEDINSPATIALSTRUCTURE('{guid}',#{main_owner},"
                        f"'MergeContainer',$,({slab_refs}),#{main_storey});")
            output_lines.append(rel_line)
            print(f"  -> RelContained #{rel_id} angelegt mit {len(new_slab_ids)} Slabs", flush=True)

    print(f"[{time.time()-t0:.1f}s] Schreibe {out_path}...", flush=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(header)
        for ln in output_lines:
            f.write(ln + "\n")
        f.write(footer if footer.startswith('ENDSEC') else 'ENDSEC;\nEND-ISO-10303-21;\n')
    print(f"[{time.time()-t0:.1f}s] Fertig: {len(output_lines)} Entities total", flush=True)


if __name__ == "__main__":
    main()
