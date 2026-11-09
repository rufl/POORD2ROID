#!/usr/bin/env python3
import importlib.util
from pathlib import Path

source = Path(__file__).resolve().parents[2] / "ios" / "tools" / "touch_layout.py"
spec = importlib.util.spec_from_file_location("touch_layout", source)
touch_layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(touch_layout)

console = '''{
    "XMediumFontSize": 30,
    "MediumFontSize": 28,
    "SmallFontSize": 26
}'''
scaled = touch_layout._scale_profile_font_sizes(console)
assert '"XMediumFontSize": 45' in scaled
assert '"MediumFontSize": 42' in scaled
assert '"SmallFontSize": 39' in scaled

handheld = '''{
    "XMediumLargeFontSize": 36,
    "MediumLargeFontSize": 36,
    "XMediumFontSize": 34
}'''
scaled = touch_layout._scale_profile_font_sizes(handheld)
assert '"XMediumLargeFontSize": 54' in scaled
assert '"MediumLargeFontSize": 54' in scaled
assert '"XMediumFontSize": 51' in scaled

assert touch_layout._scale_profile_font_sizes('{"SmallFontSize": 1}') == '{"SmallFontSize": 2}'
print("PASS 1.5x font scaling preserves profile-specific hierarchy and avoids fixed-size overrides")
