#!/usr/bin/env python3
"""Generate compact current-grid profiles from a pinned F1DB checkout."""

from collections import defaultdict
from pathlib import Path
import csv
import sys
import yaml

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


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_profiles.py F1DB_ROOT OUTPUT_TSV")
    root, output = Path(sys.argv[1]), Path(sys.argv[2])
    driver_ids, constructor_ids = set(DRIVERS.values()), set(CONSTRUCTORS.values())
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

    for kind, filename, ids in (("D", "driver-standings.yml", driver_ids), ("C", "constructor-standings.yml", constructor_ids)):
        for path in (root / "src/data/seasons").glob(f"*/{filename}"):
            rows = load(path) or []
            key = "driverId" if kind == "D" else "constructorId"
            if rows and rows[0].get(key) in ids:
                stats[(kind, rows[0][key])][4] += 1

    series = defaultdict(list)
    for race_path in sorted((root / "src/data/seasons/2026/races").glob("*/race.yml"), key=lambda p: int(load(p).get("round", 0))):
        race = load(race_path)
        for kind, filename, ids in (("D", "driver-standings.yml", driver_ids), ("C", "constructor-standings.yml", constructor_ids)):
            path = race_path.parent / filename
            if not path.exists():
                continue
            key = "driverId" if kind == "D" else "constructorId"
            for row in load(path) or []:
                if row.get(key) in ids:
                    series[(kind, row[key])].append(f"{race['round']}:{row.get('position', 0)}:{row.get('points', 0)}")

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t")
        writer.writerow(["type", "provider_id", "source_id", "name", "country", "starts", "wins", "podiums", "poles", "championships", "series"])
        for kind, mappings, folder in (("D", DRIVERS, "drivers"), ("C", CONSTRUCTORS, "constructors")):
            for provider_id, source_id in mappings.items():
                path = root / f"src/data/{folder}/{source_id}.yml"
                data = load(path) if path.exists() else {"name": source_id}
                country = data.get("nationalityCountryId") or data.get("countryOfBirthCountryId") or data.get("countryId") or "UNKNOWN"
                writer.writerow([kind, provider_id, source_id, data.get("name", source_id), country, *stats[(kind, source_id)], ",".join(series[(kind, source_id)])])


if __name__ == "__main__":
    main()
