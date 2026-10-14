"""HTTP-Zugriff auf die Archicad-JSON-API (Ports 19723-19744), nur Standardbibliothek."""
from __future__ import annotations

import json
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor

PORTS = range(19723, 19745)


class ArchicadError(RuntimeError):
    pass


def post(port: int, command: str, parameters: dict | None = None, timeout: float = 600.0) -> dict:
    body = json.dumps({"command": command, "parameters": parameters or {}}).encode("utf-8")
    req = urllib.request.Request(f"http://127.0.0.1:{port}", data=body, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return json.loads(r.read().decode("utf-8"))
    except urllib.error.URLError as e:
        raise ArchicadError(f"Keine Verbindung zu Archicad auf Port {port}: {e.reason}") from e
    except TimeoutError as e:
        raise ArchicadError(f"Zeitueberschreitung auf Port {port} nach {timeout:.0f} s (Teamwork/Dialog offen?)") from e


def call(port: int, namespace: str, name: str, params: dict | None = None, timeout: float = 600.0):
    """namespace 'API' = offizielle JSON-API, sonst Add-On-Namensraum (TapirCommand, ELM_SAB)."""
    if namespace == "API":
        r = post(port, f"API.{name}", params, timeout)
        if not r.get("succeeded"):
            raise ArchicadError(_err(r))
        return r.get("result", {})
    r = post(port, "API.ExecuteAddOnCommand", {
        "addOnCommandId": {"commandNamespace": namespace, "commandName": name},
        "addOnCommandParameters": params or {}}, timeout)
    if not r.get("succeeded"):
        raise ArchicadError(_err(r))
    return r.get("result", {}).get("addOnCommandResponse", {})


def _err(r: dict) -> str:
    e = r.get("error") or {}
    return f"Archicad-Fehler {e.get('code')}: {e.get('message')}"


def _probe(port: int) -> dict | None:
    try:
        alive = post(port, "API.IsAlive", timeout=2.0)
    except ArchicadError:
        return None
    info: dict = {"port": port}
    if not alive.get("succeeded"):
        info["status"] = _err(alive)
        return info
    try:
        p = post(port, "API.GetProductInfo", timeout=5.0)
        if p.get("succeeded"):
            res = p.get("result", {})
            info["archicadVersion"] = res.get("version")
            info["buildNumber"] = res.get("buildNumber")
            info["languageCode"] = res.get("languageCode")
    except ArchicadError:
        pass
    try:
        pi = call(port, "TapirCommand", "GetProjectInfo", timeout=10.0)
        info["projectName"] = pi.get("projectName")
        info["isTeamwork"] = pi.get("isTeamwork")
        info["isUntitled"] = pi.get("isUntitled")
        info["status"] = "ok"
    except ArchicadError as e:
        info["status"] = str(e)
    try:
        v = call(port, "ELM_SAB", "GetAddOnVersion", timeout=5.0)
        info["elmonkeyVersion"] = v.get("version")
        info["tapirBaseVersion"] = v.get("tapirBaseVersion")
    except ArchicadError:
        info["elmonkeyVersion"] = None
    return info


def list_instances() -> list[dict]:
    with ThreadPoolExecutor(max_workers=len(PORTS)) as ex:
        found = [r for r in ex.map(_probe, PORTS) if r]
    return found
