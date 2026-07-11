#!/usr/bin/env python3
"""Compose pixel-accurate RaceSlate screenshots into 16:9 marketing artwork."""

from base64 import b64encode
from pathlib import Path
import subprocess
import sys


def data_uri(path: Path) -> str:
    return "data:image/png;base64," + b64encode(path.read_bytes()).decode("ascii")


def shell(title: str, kicker: str, copy: str, images: str, accent: str = "#62d6c5") -> str:
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="900" viewBox="0 0 1600 900">
<defs><linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#080a0e"/><stop offset="1" stop-color="#151820"/></linearGradient><filter id="shadow"><feDropShadow dx="0" dy="20" stdDeviation="22" flood-color="#000" flood-opacity=".65"/></filter></defs>
<rect width="1600" height="900" fill="url(#bg)"/><path d="M0 760 L480 900 H0Z" fill="{accent}" opacity=".09"/><rect x="96" y="88" width="72" height="7" fill="{accent}"/>
<text x="96" y="145" fill="#f5f2ee" font-family="Helvetica Neue,Arial" font-size="34" font-weight="700" letter-spacing="3">RACESLATE</text>
<text x="96" y="258" fill="{accent}" font-family="Helvetica Neue,Arial" font-size="22" font-weight="700" letter-spacing="3">{kicker}</text>
<text x="96" y="340" fill="#f5f2ee" font-family="Helvetica Neue,Arial" font-size="60" font-weight="700">{title}</text>
<text x="100" y="402" fill="#999da6" font-family="Helvetica Neue,Arial" font-size="23">{copy}</text>{images}
<text x="98" y="820" fill="#737883" font-family="Helvetica Neue,Arial" font-size="19" letter-spacing="2">UNOFFICIAL · NON-COMMERCIAL · BUILT FOR TRIMUI BRICK + NEXTUI</text></svg>'''


def framed(uri: str, x: int, y: int, width: int, height: int, radius: int = 24) -> str:
    return f'''<g filter="url(#shadow)"><rect x="{x-12}" y="{y-12}" width="{width+24}" height="{height+24}" rx="{radius+8}" fill="#262a33"/><image href="{uri}" x="{x}" y="{y}" width="{width}" height="{height}" preserveAspectRatio="xMidYMid meet"/></g>'''


def render(svg: Path, size: int, output: Path) -> None:
    del size
    subprocess.run(["sips", "-s", "format", "png", str(svg), "--out", str(output)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: generate_marketing.py GALLERY_DIR OUTPUT_DIR")
    gallery, output = Path(sys.argv[1]), Path(sys.argv[2]); output.mkdir(parents=True, exist_ok=True)
    next_uri = data_uri(gallery / "01-next-local.png")
    race_uri = data_uri(gallery / "08-race-classification.png")
    standings_uri = data_uri(gallery / "04-driver-standings.png")
    profile_uri = data_uri(gallery / "12-driver-profile-points.png")
    artworks = [
        ("promo-01-weekend", shell("THE RACE WEEKEND", "EVERY SESSION. ONE GLANCE.", "Schedule, circuit and forecast—offline-first.", framed(next_uri, 680, 120, 820, 615))),
        ("promo-02-results", shell("EVERY RESULT", "FROM LIGHTS OUT TO THE FLAG.", "Classification, gaps, laps and team identity.", framed(race_uri, 650, 118, 850, 638))),
        ("promo-03-standings", shell("FOLLOW THE SEASON", "POINTS. POSITION. PROGRESSION.", "Driver and constructor stories across every round.", framed(standings_uri, 700, 70, 720, 540) + framed(profile_uri, 830, 390, 650, 488))),
    ]
    svg_documents = []
    for name, svg_text in artworks:
        svg = output / f"{name}.svg"
        png = output / f"{name}.png"
        svg.write_text(svg_text)
        render(svg, 1600, png)
        svg_documents.append(svg_text.split(">",1)[1].rsplit("</svg>",1)[0])
    panels = "".join(f'<svg x="0" y="{index*900}" width="1600" height="900" viewBox="0 0 1600 900">{document}</svg>' for index,document in enumerate(svg_documents))
    collage = output / "raceslate-marketing-collage.svg"; collage.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="2700" viewBox="0 0 1600 2700">{panels}</svg>')
    render(collage, 2700, output / "raceslate-marketing-collage.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
