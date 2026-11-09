#!/usr/bin/env python3
"""Build an Android RomFS overlay that makes the game's large-font profile the default."""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]  # dm/
IOS_TOOLS = ROOT / "ios" / "tools"
sys.path.insert(0, str(IOS_TOOLS))
import touch_layout


def build(romfs, output):
    romfs = Path(romfs).resolve()
    output = Path(output).resolve()
    if not (romfs / "data" / "build_manifest.psv").is_file():
        raise SystemExit(f"RomFS extraction manifest missing: {romfs / 'data' / 'build_manifest.psv'}")
    if output.exists():
        raise SystemExit(f"output already exists: {output}")

    with tempfile.TemporaryDirectory(prefix="d2r-android-font-") as temporary:
        replacements = touch_layout.build_profile(str(romfs), temporary)
        expected_profiles = {
            "profile_consolehd.json": ('"XMediumFontSize": 45', '"MediumFontSize": 42', '"SmallFontSize": 39'),
            "profile_handheldhd.json": ('"XMediumLargeFontSize": 54', '"MediumLargeFontSize": 54', '"XMediumFontSize": 51'),
        }
        for name, expected in expected_profiles.items():
            profile = (Path(temporary) / name).read_text(encoding="utf-8")
            if not all(value in profile for value in expected):
                raise SystemExit(f"1.5x source-relative font profile values missing from {name}")
        layout_path, layout = touch_layout.read_layout(romfs, "gameplayoptionspanelhd.json")
        patch = touch_layout._row_height(touch_layout._drop_rows("LowVisionMode"), 270)
        layout, changes = patch(layout)
        if changes == 0 or '"rowHeight": 270' not in layout or '"name": "Row LowVisionMode"' in layout:
            raise SystemExit("gameplay options layout did not apply the 1.5x font settings fix")
        settings = Path(temporary) / "gameplayoptions-large-font.json"
        settings.write_text(layout, encoding="utf-8")
        replacements[layout_path.lower()] = settings

        replacement_table = Path(temporary) / "replacements.tsv"
        with replacement_table.open("w", encoding="utf-8", newline="\n") as stream:
            for logical_path, source in sorted(replacements.items()):
                stream.write(f"{logical_path}\t{source}\n")
        subprocess.run(
            [sys.executable, ROOT / "tools" / "casc_overlay.py", romfs, output, replacement_table],
            check=True,
        )

    data = output / "data"
    archives = [path for path in data.glob("*.data") if path.is_file() and not path.is_symlink()]
    config = data / ".build.config"
    if not config.is_file() or config.is_symlink() or len(archives) != 1:
        raise SystemExit("generated font overlay must contain .build.config and exactly one new data archive")
    print(f"Large-font profile enabled; LowVisionMode removed; overlay archive={archives[0].name}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("romfs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.romfs, args.output)


if __name__ == "__main__":
    main()
