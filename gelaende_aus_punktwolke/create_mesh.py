"""
Zweck: Mesh-JSON (von make_mesh.py) via Tapir CreateMeshes in Archicad anlegen
       und per Rücklese verifizieren (Sublines-/Punktzahl).
Aufruf: python3 create_mesh.py <mesh.json> <port>
Abhängigkeiten: keine (nur stdlib: urllib, json).
Live-Verifikation: 2026-09-07 (THN Außenanlage, Mesh-GUID
  08EDCBD4-B85E-5143-9319-07A683D26334, Untitled, Port 19724, EG).
"""
import json
import sys
import urllib.request

TIMEOUT = 600


def call(port, cmd, params=None, timeout=TIMEOUT):
    body = {"command": cmd, "parameters": params or {}}
    req = urllib.request.Request(f"http://127.0.0.1:{port}/", json.dumps(body).encode(),
                                  headers={"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=timeout).read())


def addon(port, ns, cmd, params=None):
    r = call(port, "API.ExecuteAddOnCommand",
             {"addOnCommandId": {"commandNamespace": ns, "commandName": cmd},
              "addOnCommandParameters": params or {}})
    return r["result"]["addOnCommandResponse"]


def tapir(port, cmd, params=None):
    return addon(port, "TapirCommand", cmd, params)


def main():
    mesh_path = sys.argv[1]
    port = int(sys.argv[2])
    data = json.load(open(mesh_path))

    result = tapir(port, "CreateMeshes", data)
    elems = result.get("elements", [])
    if not elems:
        print("Keine Elemente zurückgegeben:", result)
        sys.exit(1)
    guid = elems[0]["elementId"]["guid"]
    print("Mesh-GUID:", guid)

    # Rücklese-Verifikation
    details = tapir(port, "GetDetailsOfElements", {"elements": [{"elementId": {"guid": guid}}]})
    detail = details["detailsOfElements"][0]
    sublines = detail.get("meshGeometryData", {}).get("sublines", [])
    npts = sum(len(s.get("coordinates", [])) for s in sublines)
    print("Rücklese: Sublines =", len(sublines), "Punkte =", npts)


if __name__ == "__main__":
    main()
