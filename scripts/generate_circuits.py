#!/usr/bin/env python3
"""Create Brick-ready BMP circuit assets from pinned F1DB SVGs."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile

ALIASES = {
    "albert_park": "melbourne-2", "shanghai": "shanghai-1", "suzuka": "suzuka-2",
    "miami": "miami-1", "villeneuve": "montreal-6", "monaco": "monaco-6",
    "catalunya": "catalunya-6", "red_bull_ring": "spielberg-3", "silverstone": "silverstone-8",
    "spa": "spa-francorchamps-4", "hungaroring": "hungaroring-3", "zandvoort": "zandvoort-5",
    "monza": "monza-7", "madring": "madring-1", "baku": "baku-1",
    "marina_bay": "marina-bay-4", "americas": "austin-1", "rodriguez": "mexico-city-3",
    "interlagos": "interlagos-2", "vegas": "las-vegas-1", "losail": "lusail-1",
    "yas_marina": "yas-marina-2",
}

def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_circuits.py F1DB_ROOT OUTPUT_DIR", file=sys.stderr)
        return 2
    source = Path(sys.argv[1]) / "src/assets/circuits/white-outline"
    output = Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)
    for provider_id, layout_id in ALIASES.items():
        svg = source / f"{layout_id}.svg"
        if not svg.exists():
            raise FileNotFoundError(svg)
        source_text = svg.read_text()
        path_match = re.search(r'<path[^>]* d="([^"]+)"', source_text)
        if not path_match:
            raise ValueError(f"no path in {svg}")
        clean_svg = (
            '<svg width="500" height="500" viewBox="0 0 500 500" xmlns="http://www.w3.org/2000/svg">'
            '<rect width="500" height="500" fill="#000"/>'
            f'<path d="{path_match.group(1)}" fill="none" stroke="#fff" stroke-width="12" '
            'stroke-linecap="round" stroke-linejoin="round"/>'
            '</svg>'
        )
        with tempfile.TemporaryDirectory() as temporary:
            temporary_path = Path(temporary)
            clean = temporary_path / f"{layout_id}.svg"
            clean.write_text(clean_svg)
            subprocess.run(["qlmanage", "-t", "-s", "512", "-o", str(temporary_path), str(clean)], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            preview = temporary_path / f"{layout_id}.svg.png"
            bmp = output / f"{provider_id}.bmp"
            subprocess.run(["sips", "-s", "format", "bmp", str(preview), "--out", str(bmp)], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
