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
assert '"XMediumFontSize": 52' in scaled
assert '"MediumFontSize": 48' in scaled
assert '"SmallFontSize": 45' in scaled

handheld = '''{
    "XMediumLargeFontSize": 36,
    "MediumLargeFontSize": 36,
    "XMediumFontSize": 34,
    "FriendsListMediumFontSize": 32,
    "StyleSettingsNumericGameCreator": { "pointSize": 34 }
}'''
scaled = touch_layout._scale_profile_font_sizes(handheld)
assert '"XMediumLargeFontSize": 62' in scaled
assert '"MediumLargeFontSize": 62' in scaled
assert '"XMediumFontSize": 59' in scaled
assert '"FriendsListMediumFontSize": 55' in scaled
assert '"pointSize": 59' in scaled

assert touch_layout._scale_profile_font_sizes('{"SmallFontSize": 1}') == '{"SmallFontSize": 2}'
android_hh = '''{
    "basedOn": "ConsoleHD",
    "XMediumLargeFontSize": 36,
    "MediumLargeFontSize": 36,
    "XMediumFontSize": 34,
    "StyleLFButtonTextSmall": { "pointSize": "$LootFilterSmallFontSize" },
    "FriendsListMediumFontSize": 32,
    "FriendsListSmallFontSize": 28,
    "StyleSettingsNumericGameCreator": { "pointSize": 34 }
}'''
boosted = touch_layout._apply_android_profile_fonts(android_hh, handheld=True)
assert '"MediumFontSize": 60' in boosted
assert '"SmallFontSize": 54' in boosted
assert '"TooltipFontSize": 58' in boosted
assert '"LargeFontSize": 72' in boosted
assert '"StyleNPCDialogueSize": 64' in boosted
assert '"XMediumLargeFontSize": 70' in boosted
assert '"pointSize": "$LootFilterSmallFontSize"' in boosted
assert '"pointSize": 66' in boosted
print("PASS 1.725x source-relative font scaling includes named sizes and numeric pointSize overrides")
print("PASS font scaling preserves profile hierarchy and scales numeric pointSize overrides")
