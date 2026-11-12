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


    def test_android_enlarges_scroll_speech_dialog_box(self):
        transforms = touch_layout.present_transforms("android")
        source = '''{
            "fields": { "rect": { "x": "$ScrollSpeechOffset", "y": 50, "width": 800, "height": 325 }, "scrollStartOffset": -80, },
            "children": [
                { "fields": { "rect": { "x": 0, "y": 0, "width": "$ScrollSpeechBorderWidth", "height": 345 }, } },
                { "fields": { "rect": { "x": 50, "y": 8, "width": "$ScrollSpeechWidth", "height": 330 }, } },
                { "fields": { "rect": { "x": 0, "y": 330, "width": "$ScrollSpeechWidth", "height": 2000 }, } },
                { "fields": { "rect": { "x": -33, "y": 28, "width": 26, "height": 290 }, } }
            ]
        }'''
        transformed, changes = transforms["scrollspeechpanelhd.json"](source)
        self.assertEqual(changes, 6)
        self.assertIn('"x": -810, "y": 320, "width": 1620, "height": 500', transformed)
        self.assertIn('"scrollStartOffset": -460,', transformed)
        self.assertIn('"x": 60, "y": 20, "width": 1460, "height": 460', transformed)
        self.assertIn('"x": 0, "y": 380, "width": 1460, "height": 2400', transformed)
        self.assertNotIn("scrollspeechpanelhd.json", touch_layout.PRESENT)

if __name__ == "__main__":
    unittest.main()
