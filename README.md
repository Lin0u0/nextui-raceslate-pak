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
- `START`: settings, haptics, cache controls, About and data licences
- `MENU`: exit

Host keyboard equivalents are Q/E, arrows, Return/Escape, X/Y, S, and M.

## Build and test

```sh
make test
make host
SDL_VIDEODRIVER=dummy ./build/host/raceslate --offline --screenshot next.bmp
```

The app starts from the bundled snapshot and refreshes Jolpica-F1 and Open-Meteo on a worker thread. Historical seasons load one year at a time back to 1950 and are retained as separate offline snapshots. Validated responses are committed atomically under the data directory. Weather responses tolerate unavailable far-future hours while retaining valid forecast points. TLS peer and hostname checks are never disabled.

For a device package:

```sh
make tg5040-bootstrap
make nextui-release
```

The output is `dist/RaceSlate.pakz`. Copy it to the SD-card root and let NextUI import it.

## Data and licensing

Program code is GPL-3.0-only. Data, fonts, CA certificates, and circuit derivatives retain their own licences; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Modified distributions must use a different name and icon; see [TRADEMARKS.md](TRADEMARKS.md).
