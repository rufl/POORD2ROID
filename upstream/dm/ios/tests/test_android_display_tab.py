import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("touch_layout", ROOT / "tools" / "touch_layout.py")
touch_layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(touch_layout)


class AndroidDisplayTabTests(unittest.TestCase):
    def test_display_route_stays_native_and_controls_becomes_custom(self):
        source = '''{
            "fields": {
                "tabCount": 5,
                "textStrings": [ "@OptionsMenuVideo", "@Audio", "@Gameplay", "@GameSettingsTab", "@Controls" ],
                "tabMessages": [
                    "SettingsPanelMessage:CheckChanges:VideoOptionsPanel",
                    "SettingsPanelMessage:CheckChanges:SoundOptionsPanel",
                    "SettingsPanelMessage:CheckChanges:GameplayOptionsPanel",
                    "SettingsPanelMessage:CheckChanges:OnlineGameOptionsPanel",
                    "SettingsPanelMessage:CheckChanges:KeyBindingMenu"
                ]
            }
        }'''
        transformed, changes = touch_layout._settings_tabs_android(source)

        self.assertEqual(changes, 2)
        self.assertIn('"@OptionsMenuVideo"', transformed)
        self.assertIn('"SettingsPanelMessage:CheckChanges:VideoOptionsPanel"', transformed)
        self.assertIn('"@KeyBindingTypeCustom"', transformed)
        self.assertIn('"SettingsPanelMessage:CheckChanges:KeyBindingMenu"', transformed)

    def test_custom_page_uses_keybinding_route_without_replacing_display(self):
        transforms = touch_layout.present_transforms("android")
        page, _ = transforms["keybindingmenuhd.json"]('{ "name": "KeyBindingMenu" }')

        self.assertNotIn("controller/console/videooptionspanelhd.json", transforms)
        self.assertIn('"type": "VideoOptionsPanel", "name": "KeyBindingMenu"', page)
        self.assertIn('"name": "TouchCustomRows"', page)


if __name__ == "__main__":
    unittest.main()
