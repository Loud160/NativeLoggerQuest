#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: © 2026 Loud160 (AKA Whisp) and the Big Screen contributors

from __future__ import annotations

import hashlib
import json
import pathlib
import re
import sys
import urllib.request


root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
manifest = json.loads(
    (root / "native-logger-current.json").read_text(encoding="utf-8")
)
assert manifest["schemaVersion"] == 1
assert manifest["name"] == "Native Logger Quest"
assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.-]+)?", manifest["version"])
assert (root / "VERSION").read_text(encoding="utf-8").strip() == manifest["version"]
revision = manifest["revision"]
assert re.fullmatch(r"[0-9a-f]{40}", revision)
assert manifest["archiveUrl"] == (
    f"https://github.com/Loud160/NativeLoggerQuest/archive/{revision}.zip"
)
assert re.fullmatch(r"[0-9a-f]{64}", manifest["archiveSha256"])
assert manifest["sourceDirectory"] == f"NativeLoggerQuest-{revision}"

request = urllib.request.Request(
    manifest["archiveUrl"],
    headers={"User-Agent": "NativeLoggerQuest-manifest-test"},
)
with urllib.request.urlopen(request, timeout=30) as response:
    archive = response.read()
assert hashlib.sha256(archive).hexdigest() == manifest["archiveSha256"]
print(
    f"Native Logger Quest {manifest['version']} current-source manifest "
    f"validated at {revision}."
)
