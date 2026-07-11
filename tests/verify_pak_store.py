#!/usr/bin/env python3
import json
import sys
import zipfile
from pathlib import Path

root = Path(sys.argv[1])
archive = Path(sys.argv[2])
version = (root / "VERSION").read_text().strip()
metadata = json.loads((root / "pak.json").read_text())
required = ("name", "version", "type", "description", "author", "repo_url", "release_filename", "platforms")
assert all(metadata.get(field) for field in required), "pak.json is missing required fields"
assert metadata["version"] == f"v{version}"
assert metadata["type"] == "TOOL"
assert metadata["release_filename"] == archive.name
assert metadata["platforms"] == ["tg5040"]
assert metadata["repo_url"] == "https://github.com/Lin0u0/nextui-raceslate-pak"

with zipfile.ZipFile(archive) as package:
    names = set(package.namelist())
    for name in ("launch.sh", "pak.json", "bin/tg5040/raceslate", "res/fonts/BarlowCondensed-SemiBold.ttf"):
        assert name in names, f"store archive missing {name}"
    assert not any(name.startswith("Tools/") for name in names), "store archive must contain the Pak contents at its root"
    packaged = json.loads(package.read("pak.json"))
    for field in ("name", "version", "type", "author", "repo_url", "release_filename", "platforms"):
        assert packaged[field] == metadata[field], f"packaged pak.json differs at {field}"

print(f"ok: Pak Store metadata and {archive.name}")
