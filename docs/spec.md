# RaceSlate v1 specification

RaceSlate is a public, English-only, non-commercial and unofficial Formula 1 season companion for TrimUI Brick running NextUI. It uses an independent racing-inspired visual system and must not imply endorsement.

The product exposes NEXT, CALENDAR and STANDINGS surfaces; offline-first current-season schedules and standings; F1DB circuit geometry and history; Open-Meteo session weather; driver/constructor statistical details; verified HTTPS refresh with atomic local snapshots; visible attribution; zero analytics; and controller-first navigation. Live timing, telemetry, radio, scraping, background notifications and official brand assets are out of scope.

The runtime target is tg5040 at 1024×768. Code is C11 with SDL2, SDL2_ttf, libcurl and cJSON. The distributable is `RaceSlate.pakz`.
