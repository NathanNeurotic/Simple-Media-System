"""Real-frame coverage of scaling modes, source SAR and preset geometry state."""
import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from pathlib import Path
import subprocess
import tempfile
import unittest

from config import DEFAULT_SCALING_MODE, PRESETS
from ffmpeg_utils import build_ffmpeg_cmd, find_ffmpeg_binaries, get_media_info


class TestGeometry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ff, cls.fp = find_ffmpeg_binaries()
        if not cls.ff:
            raise unittest.SkipTest("FFmpeg is required")
        cls.tmp = tempfile.TemporaryDirectory(prefix="sms-geometry-")
        cls.folder = Path(cls.tmp.name)
        cls.sources = {}
        for name, width, height, sar in (("wide", 1280, 720, "1/1"),
                                         ("small", 320, 180, "1/1"),
                                         ("ntsc", 720, 480, "32/27"),
                                         ("pal", 720, 576, "64/45"),
                                         ("narrow-pixels", 320, 240, "1/2")):
            source = cls.folder / (name + ".mkv")
            # Colored edge strips distinguish a center crop from a stretched frame.
            vf = (f"color=red:size={width}x{height}:rate=25:duration=0.16,"
                  f"drawbox=x=0:y=0:w={width // 10}:h=ih:color=blue:t=fill,"
                  f"drawbox=x=iw-{width // 10}:y=0:w={width // 10}:h=ih:color=green:t=fill,"
                  f"setsar={sar}")
            subprocess.run([cls.ff, "-v", "error", "-f", "lavfi", "-i", vf,
                            "-c:v", "ffv1", str(source)], check=True, capture_output=True)
            cls.sources[name] = source

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tmp"):
            cls.tmp.cleanup()

    def convert(self, source_name, mode, allow_upscale=False, keep_sd=False):
        source = self.sources[source_name]
        info = get_media_info(self.fp, str(source))
        settings = dict(vcodec="mpeg4", acodec=None, width=640, height=480,
                        vbitrate_kbps=1200, fps="25", limit_streams=True,
                        scaling_mode=mode, allow_upscale=allow_upscale,
                        keep_sd_resolution=keep_sd)
        out = self.folder / f"{source_name}-{mode}-{allow_upscale}-{keep_sd}.mp4"
        cmd = build_ffmpeg_cmd(self.ff, str(source), str(out), settings, source_info=info)
        result = subprocess.run(cmd, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        video = get_media_info(self.fp, str(out))["streams"][0]
        pixels = subprocess.check_output([self.ff, "-v", "error", "-i", str(out),
                                          "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"])
        return video, pixels

    @staticmethod
    def pixel(video, pixels, x, y):
        offset = (y * video["width"] + x) * 3
        return tuple(pixels[offset:offset + 3])

    def test_fit_has_no_padding_and_letterbox_has_borders(self):
        fit, fit_pixels = self.convert("wide", "Fit inside dimensions")
        boxed, boxed_pixels = self.convert("wide", DEFAULT_SCALING_MODE)
        self.assertEqual((fit["width"], fit["height"]), (640, 360))
        self.assertEqual((boxed["width"], boxed["height"]), (640, 480))
        for video in (fit, boxed):
            self.assertEqual(video["sample_aspect_ratio"], "1:1")
        self.assertGreater(self.pixel(fit, fit_pixels, 320, 4)[0], 180)
        self.assertLess(sum(self.pixel(boxed, boxed_pixels, 320, 4)), 20)
        self.assertGreater(self.pixel(boxed, boxed_pixels, 320, 80)[0], 180)

    def test_crop_removes_edges_while_stretch_keeps_them(self):
        cropped, crop_pixels = self.convert("wide", "Crop to fill", allow_upscale=True)
        stretched, stretch_pixels = self.convert("wide", "Stretch", allow_upscale=True)
        for video in (cropped, stretched):
            self.assertEqual((video["width"], video["height"]), (640, 480))
            self.assertEqual(video["sample_aspect_ratio"], "1:1")
            self.assertEqual(video["display_aspect_ratio"], "4:3")
        self.assertGreater(self.pixel(cropped, crop_pixels, 10, 240)[0], 180)
        self.assertGreater(self.pixel(stretched, stretch_pixels, 10, 240)[2], 180)
        self.assertGreater(self.pixel(cropped, crop_pixels, 620, 240)[0], 180)
        self.assertGreater(self.pixel(stretched, stretch_pixels, 620, 240)[1], 70)

    def test_upscaling_setting_changes_every_mode(self):
        for mode in (DEFAULT_SCALING_MODE, "Fit inside dimensions", "Crop to fill", "Stretch"):
            with self.subTest(mode=mode):
                small, _ = self.convert("small", mode)
                enlarged, _ = self.convert("small", mode, allow_upscale=True)
                small_dimensions = (320, 180) if mode == "Fit inside dimensions" else (240, 180)
                self.assertEqual((small["width"], small["height"]), small_dimensions)
                self.assertEqual(small["sample_aspect_ratio"], "1:1")
                expected = (640, 360) if mode == "Fit inside dimensions" else (640, 480)
                self.assertEqual((enlarged["width"], enlarged["height"]), expected)

    def test_anamorphic_fit_and_letterbox_preserve_picture_proportions(self):
        for source in ("ntsc", "pal"):
            with self.subTest(source=source):
                fitted, _ = self.convert(source, "Fit inside dimensions")
                self.assertEqual((fitted["width"], fitted["height"]), (640, 360))
                self.assertEqual(fitted["sample_aspect_ratio"], "1:1")
                boxed, pixels = self.convert(source, DEFAULT_SCALING_MODE)
                self.assertEqual((boxed["width"], boxed["height"]), (640, 480))
                self.assertEqual(boxed["sample_aspect_ratio"], "1:1")
                self.assertLess(sum(self.pixel(boxed, pixels, 320, 40)), 20)
                self.assertGreater(self.pixel(boxed, pixels, 320, 80)[0], 180)

    def test_crop_does_not_enlarge_anamorphic_source_pixels(self):
        cropped, _ = self.convert("narrow-pixels", "Crop to fill")
        enlarged, _ = self.convert("narrow-pixels", "Crop to fill", allow_upscale=True)
        self.assertEqual((cropped["width"], cropped["height"]), (160, 120))
        self.assertEqual((enlarged["width"], enlarged["height"]), (640, 480))
        self.assertEqual(cropped["sample_aspect_ratio"], "1:1")

    def test_keep_sd_preserves_dimensions_and_pixel_aspect(self):
        for source, height, sar in (("ntsc", 480, "32:27"), ("pal", 576, "64:45")):
            for mode in (DEFAULT_SCALING_MODE, "Stretch", "Crop to fill"):
                with self.subTest(source=source, mode=mode):
                    video, _ = self.convert(source, mode, allow_upscale=True, keep_sd=True)
                    self.assertEqual((video["width"], video["height"]), (720, height))
                    self.assertEqual(video["sample_aspect_ratio"], sar)
                    self.assertEqual(video["display_aspect_ratio"], "16:9")
        # HD sources still use the selected scaling mode when SD preservation is on.
        video, _ = self.convert("wide", "Fit inside dimensions", keep_sd=True)
        self.assertEqual((video["width"], video["height"]), (640, 360))


class TestGeometrySettings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from qt_compat import QApplication
        cls.app = QApplication.instance() or QApplication([])

    def test_geometry_changes_badge_and_loading_preset_restores_defaults(self):
        from ui.advanced_settings import AdvancedSettingsWidget
        from ui.main_window import MainWindow
        widget = AdvancedSettingsWidget()
        preset = next(p for p in PRESETS.values() if p.ext == ".mp4" and p.width == 640)
        try:
            widget.load_preset(preset)
            self.assertTrue(MainWindow._matches_preset(widget.get_settings_dict(), preset))
            for mode in ("Fit inside dimensions", "Crop to fill", "Stretch"):
                widget.combo_scaling.setCurrentText(mode)
                self.assertFalse(MainWindow._matches_preset(widget.get_settings_dict(), preset))
                widget.load_preset(preset)
                self.assertEqual(widget.combo_scaling.currentText(), preset.scaling_mode)
                self.assertTrue(MainWindow._matches_preset(widget.get_settings_dict(), preset))
            widget.chk_upscale.setChecked(True)
            self.assertFalse(MainWindow._matches_preset(widget.get_settings_dict(), preset))
            widget.load_preset(preset)
            self.assertFalse(widget.chk_upscale.isChecked())
            self.assertTrue(MainWindow._matches_preset(widget.get_settings_dict(), preset))
            keep_sd_preset = next(p for p in PRESETS.values() if p.keep_sd_resolution)
            widget.load_preset(keep_sd_preset)
            self.assertTrue(widget.chk_keep_sd.isChecked())
            self.assertTrue(MainWindow._matches_preset(widget.get_settings_dict(), keep_sd_preset))
            widget.chk_keep_sd.setChecked(False)
            self.assertFalse(MainWindow._matches_preset(widget.get_settings_dict(), keep_sd_preset))
            widget.load_preset(keep_sd_preset)
            self.assertTrue(widget.chk_keep_sd.isChecked())
            widget.load_preset(preset)
            self.assertFalse(widget.chk_keep_sd.isChecked())
            self.assertTrue(MainWindow._matches_preset(widget.get_settings_dict(), preset))
        finally:
            widget.close()


if __name__ == "__main__":
    unittest.main()
