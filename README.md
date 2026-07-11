# RaceSlate

RaceSlate is an independent, non-commercial Formula 1 season companion for the TrimUI Brick running NextUI. It presents the next session, current-season calendar, circuit diagrams and history, driver and constructor standings, and session weather in a controller-first 1024×768 interface.

RaceSlate is unofficial and is not associated with Formula 1, FIA, any team, driver, or circuit. It does not include official logos, photographs, typefaces, live timing, telemetry, radio, or scraped content.

## Controls

- `L1 / R1`: NEXT, CALENDAR, STANDINGS
- D-pad: navigate lists
- `A`: open circuit history or statistical profile
- `B`: close
- `X`: switch driver/constructor standings
- `Y`: refresh through verified HTTPS
- `START`: About and data licences
- `MENU`: exit

Host keyboard equivalents are Q/E, arrows, Return/Escape, X/Y, S, and M.

## Build and test

```sh
make test
make host
SDL_VIDEODRIVER=dummy ./build/host/raceslate --offline --screenshot next.bmp
```

The app starts from the bundled snapshot and refreshes Jolpica-F1 and Open-Meteo on a worker thread. Validated responses are committed atomically under the data directory. TLS peer and hostname checks are never disabled.

For a device package:

```sh
make tg5040-bootstrap
make nextui-release
```

The output is `dist/RaceSlate.pakz`. Copy it to the SD-card root and let NextUI import it.

## Data and licensing

Program code is GPL-3.0-only. Data, fonts, CA certificates, and circuit derivatives retain their own licences; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Modified distributions must use a different name and icon; see [TRADEMARKS.md](TRADEMARKS.md).
