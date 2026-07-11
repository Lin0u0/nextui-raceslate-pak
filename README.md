# RaceSlate

RaceSlate is an independent, non-commercial Formula 1 season companion for the TrimUI Brick running NextUI. It presents the next session, current and historical season calendars, circuit diagrams and history, driver and constructor standings, and session weather in a controller-first 1024×768 interface.

RaceSlate is unofficial and is not associated with Formula 1, FIA, any team, driver, or circuit. It does not include official logos, photographs, typefaces, live timing, telemetry, radio, or scraped content.

## Controls

- `L1 / R1`: NEXT, CALENDAR, STANDINGS
- D-pad: navigate lists
- `A`: open circuit history, results, or statistical profile
- `B`: close
- `X`: switch local/track time on NEXT; switch result/profile views in details; switch driver/constructor standings
- `Y`: refresh through verified HTTPS
- `L2 / R2`: previous/next season from CALENDAR or STANDINGS
- `SELECT`: save one favorite driver and constructor from their profile
- `START`: settings, three text-size levels, haptics, cache controls, About and data licences
- `MENU`: exit

Host keyboard equivalents are Q/E, arrows, Return/Escape, X/Y, S, and M.

## Build and test

```sh
make test
make host
SDL_VIDEODRIVER=dummy ./build/host/raceslate --offline --screenshot next.bmp
```

The app starts from bundled current data and complete offline season snapshots from 1950 onward. Historical seasons switch locally without network access; only current-season increments and the independent Open-Meteo forecast use the refresh worker. Historical standings combine F1DB career totals for more than 1,000 driver and constructor identities with official per-round progression from the selected season. A compact F1DB atlas selects the circuit layout actually raced at each venue, year and round; 78 current and retired venues include geometry, records, winners and poles. Classifications retain up to 64 entries so early Indianapolis fields remain complete. Validated responses are committed atomically under the data directory. Weather responses tolerate unavailable far-future hours while retaining valid forecast points. TLS peer and hostname checks are never disabled.

For a device package:

```sh
make tg5040-bootstrap
make nextui-release
```

The output is `dist/RaceSlate.pakz`. Copy it to the SD-card root and let NextUI import it.

## Regenerating attributed reference assets

Reference assets are generated from F1DB commit `0921cd9a6f79029290b61544965f91201373e960`. On macOS, install Python 3 with PyYAML and use the system `qlmanage` and `sips` tools:

```sh
git clone https://github.com/f1db/f1db.git build/f1db-source
git -C build/f1db-source checkout 0921cd9a6f79029290b61544965f91201373e960
python3 -m pip install PyYAML
python3 scripts/generate_circuits.py build/f1db-source assets/circuits
python3 scripts/generate_profiles.py build/f1db-source assets/reference/profiles.tsv
python3 scripts/generate_offline_seasons.py build/f1db-source assets/reference/profiles.tsv assets/baseline/seasons
```

Profile generation also resolves the public Jolpica driver and constructor identifiers used by runtime snapshots.

## Data and licensing

Program code is GPL-3.0-only. Data, fonts, CA certificates, and circuit derivatives retain their own licences; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Modified distributions must use a different name and icon; see [TRADEMARKS.md](TRADEMARKS.md).
