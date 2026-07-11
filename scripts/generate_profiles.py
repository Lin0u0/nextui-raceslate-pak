#!/usr/bin/env python3
"""Generate compact current-grid profiles from a pinned F1DB checkout."""

from collections import defaultdict
from pathlib import Path
import csv
import sys
import yaml
import json
import re
import unicodedata
import urllib.request

DRIVERS = {
    "antonelli": "kimi-antonelli", "russell": "george-russell",
    "hamilton": "lewis-hamilton", "leclerc": "charles-leclerc",
    "norris": "lando-norris", "piastri": "oscar-piastri",
    "max_verstappen": "max-verstappen", "hadjar": "isack-hadjar",
    "gasly": "pierre-gasly", "lawson": "liam-lawson",
    "arvid_lindblad": "arvid-lindblad", "bearman": "oliver-bearman",
    "colapinto": "franco-colapinto", "bortoleto": "gabriel-bortoleto",
    "sainz": "carlos-sainz-jr", "albon": "alexander-albon",
    "ocon": "esteban-ocon", "alonso": "fernando-alonso",
    "hulkenberg": "nico-hulkenberg", "bottas": "valtteri-bottas",
    "perez": "sergio-perez", "stroll": "lance-stroll",
}
CONSTRUCTORS = {
    "mercedes": "mercedes", "ferrari": "ferrari", "mclaren": "mclaren",
    "red_bull": "red-bull", "alpine": "alpine", "rb": "racing-bulls",
    "haas": "haas", "williams": "williams", "audi": "audi",
    "aston_martin": "aston-martin", "cadillac": "cadillac",
}


def load(path):
    with path.open() as stream:
        return yaml.safe_load(stream)


def position(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return 99


def normalized(value):
    ascii_value = unicodedata.normalize("NFKD", value or "").encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]", "", ascii_value)


def jolpica(kind):
    table = "DriverTable" if kind == "drivers" else "ConstructorTable"; key = "Drivers" if kind == "drivers" else "Constructors"
    rows, offset = [], 0
    while True:
        url = f"https://api.jolpi.ca/ergast/f1/{kind}.json?limit=100&offset={offset}"
        with urllib.request.urlopen(url) as response: document = json.load(response)["MRData"]
        batch = document[table][key]; rows.extend(batch); offset += len(batch)
        if offset >= int(document["total"]): return rows


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_profiles.py F1DB_ROOT OUTPUT_TSV")
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    driver_ids = {path.stem for path in (root / "src/data/drivers").glob("*.yml")}
    constructor_ids = {path.stem for path in (root / "src/data/constructors").glob("*.yml")}
    stats = defaultdict(lambda: [0, 0, 0, 0, 0])  # starts, wins, podiums, poles, titles

    for path in (root / "src/data/seasons").glob("*/races/*/race-results.yml"):
        rows = load(path) or []
        seen_constructors = set()
        for row in rows:
            driver, constructor, place = row.get("driverId"), row.get("constructorId"), position(row.get("position"))
            if driver in driver_ids:
                stats[("D", driver)][0] += 1
                stats[("D", driver)][1] += place == 1
                stats[("D", driver)][2] += place <= 3
            if constructor in constructor_ids:
                seen_constructors.add(constructor)
                stats[("C", constructor)][1] += place == 1
                stats[("C", constructor)][2] += place <= 3
        for constructor in seen_constructors:
            stats[("C", constructor)][0] += 1

    for path in (root / "src/data/seasons").glob("*/races/*/qualifying-results.yml"):
        rows = load(path) or []
        if rows and rows[0].get("driverId") in driver_ids:
            stats[("D", rows[0]["driverId"])][3] += 1
        if rows and rows[0].get("constructorId") in constructor_ids:
            stats[("C", rows[0]["constructorId"])][3] += 1

    for kind, filename, ids in (("D", "driver-standings.yml", driver_ids), ("C", "constructor-standings.yml", constructor_ids)):
        for path in (root / "src/data/seasons").glob(f"*/{filename}"):
            rows = load(path) or []
            key = "driverId" if kind == "D" else "constructorId"
            if rows and rows[0].get(key) in ids:
                stats[(kind, rows[0][key])][4] += 1

    source_drivers = {path.stem: load(path) for path in (root / "src/data/drivers").glob("*.yml")}
    source_constructors = {path.stem: load(path) for path in (root / "src/data/constructors").glob("*.yml")}
    driver_names = {}
    drivers_by_last = defaultdict(list)
    for source_id, data in source_drivers.items():
        for name in (data.get("name"), f"{data.get('firstName','')} {data.get('lastName','')}".strip()):
            if name: driver_names[normalized(name)] = source_id
        drivers_by_last[normalized(data.get("lastName"))].append(source_id)
    constructor_names = {}
    for source_id, data in source_constructors.items():
        for name in (data.get("name"), data.get("fullName")):
            if name: constructor_names[normalized(name)] = source_id
    mappings = []
    for item in jolpica("drivers"):
        provider_id=item["driverId"]; name=f"{item.get('givenName','')} {item.get('familyName','')}".strip(); source_id=DRIVERS.get(provider_id) or driver_names.get(normalized(name))
        if not source_id:
            candidates=drivers_by_last.get(normalized(item.get("familyName")),[]); given=normalized(item.get("givenName"))
            matches=[candidate for candidate in candidates if normalized(source_drivers[candidate].get("firstName")).startswith(given)]
            if len(matches)==1: source_id=matches[0]
        if source_id: mappings.append(("D",provider_id,source_id,source_drivers[source_id]))
    for item in jolpica("constructors"):
        provider_id=item["constructorId"]; name=item["name"]; source_id=CONSTRUCTORS.get(provider_id) or constructor_names.get(normalized(name))
        if not source_id and "-" in name: source_id=constructor_names.get(normalized(name.split("-",1)[0]))
        if source_id: mappings.append(("C",provider_id,source_id,source_constructors[source_id]))
    current_ids=set(DRIVERS)|set(CONSTRUCTORS)
    provider_keys={(kind,provider_id) for kind,provider_id,_,_ in mappings}
    for source_id,data in source_drivers.items():
        if ("D",source_id) not in provider_keys:mappings.append(("D",source_id,source_id,data))
    for source_id,data in source_constructors.items():
        if ("C",source_id) not in provider_keys:mappings.append(("C",source_id,source_id,data))
    for path in (root / "src/data/seasons").glob("*/constructor-standings.yml"):
        for item in load(path) or []:
            source_id=item.get("constructorId");engine=item.get("engineManufacturerId");provider_id=f"{source_id}-{engine}" if source_id and engine and source_id!=engine else source_id
            if item.get("position") and not str(item.get("position")).isdigit():provider_id=f"{provider_id}-excluded"
            if source_id in source_constructors and ("C",provider_id) not in provider_keys:mappings.append(("C",provider_id,source_id,source_constructors[source_id]));provider_keys.add(("C",provider_id))
    mappings.sort(key=lambda value:(value[1] not in current_ids,value[0],value[1]))

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(["type", "provider_id", "source_id", "name", "country", "starts", "wins", "podiums", "poles", "championships", "series"])
        for kind, provider_id, source_id, data in mappings:
            country = data.get("nationalityCountryId") or data.get("countryOfBirthCountryId") or data.get("countryId") or "UNKNOWN"
            writer.writerow([kind, provider_id, source_id, data.get("name", source_id), country, *stats[(kind, source_id)], ""])


if __name__ == "__main__":
    main()
