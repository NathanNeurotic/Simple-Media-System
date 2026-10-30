"""Real FFmpeg acceptance of multi-audio, subtitle selection and SD sizing."""
import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from config import PRESETS
from ffmpeg_utils import build_ffmpeg_cmd, find_ffmpeg_binaries, get_media_info
from validator import validate_converted_file


def avi_audio_formats(path):
    """Read WAVE tags/config from the small, valid AVI fixtures FFmpeg creates."""
    data = path.read_bytes()

    def chunks(start, end):
        while start + 8 <= end:
            kind, size = struct.unpack_from("<4sI", data, start)
            payload = start + 8
            yield kind, payload, payload + size
            start = payload + size + (size & 1)

    if data[:4] != b"RIFF" or data[8:12] != b"AVI ":
        raise AssertionError("FFmpeg did not produce an AVI RIFF header")
    formats = []
    for kind, start, end in chunks(12, len(data)):
        if kind != b"LIST" or data[start:start + 4] != b"hdrl":
            continue
        for kind, start, end in chunks(start + 4, end):
            if kind != b"LIST" or data[start:start + 4] != b"strl":
                continue
            fields = {kind: data[first:last] for kind, first, last in chunks(start + 4, end)}
            if fields.get(b"strh", b"")[:4] != b"auds":
                continue
            wave = fields[b"strf"]
            tag, channels, sample_rate = struct.unpack_from("<HHI", wave)
            extra_size = struct.unpack_from("<H", wave, 16)[0] if len(wave) > 16 else 0
            extra = wave[18:18 + extra_size]
            if len(extra) != extra_size:
                raise AssertionError("FFmpeg produced incomplete WAVE extra data")
            formats.append((tag, channels, sample_rate, extra))
    return formats


class TestTrackConversion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ff, cls.fp = find_ffmpeg_binaries()
        if not cls.ff:
            raise unittest.SkipTest("FFmpeg is required")
        from qt_compat import QApplication
        cls.app = QApplication.instance() or QApplication([])
        temp_root = Path(__file__).resolve().parents[2] / "build" / "converter-tests"
        temp_root.mkdir(parents=True, exist_ok=True)
        cls.tmp = tempfile.TemporaryDirectory(dir=str(temp_root))
        cls.folder = Path(cls.tmp.name)
        cls.first, cls.second = cls.folder / "first.srt", cls.folder / "second.srt"
        for path, text in ((cls.first, "FIRST"), (cls.second, "SECOND")):
            path.write_text("1\n00:00:00,000 --> 00:00:01,000\n" + text + "\n", encoding="utf-8")
        cls.source = cls.folder / "movie's source.mkv"
        subprocess.run([cls.ff, "-v", "error", "-f", "lavfi", "-i", "color=black:size=720x480:rate=25:duration=0.6",
                        "-f", "lavfi", "-i", "sine=frequency=440:duration=0.6",
                        "-f", "lavfi", "-i", "sine=frequency=880:duration=0.6",
                        "-i", str(cls.first), "-i", str(cls.second),
                        "-map", "0:v", "-map", "1:a", "-map", "2:a", "-map", "3:s", "-map", "4:s",
                        "-c:v", "mpeg4", "-c:a", "aac", "-c:s", "srt",
                        "-metadata:s:a:0", "language=eng", "-metadata:s:a:1", "language=jpn",
                        "-metadata:s:a:1", "title=Commentary 日本語",
                        str(cls.source)], check=True, capture_output=True)
        cls.info = get_media_info(cls.fp, str(cls.source))

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "tmp"):
            cls.tmp.cleanup()

    def settings(self, **changes):
        values = dict(vcodec="mpeg4", acodec="aac", width=640, height=480, fps="25",
                      vbitrate_kbps=600, abitrate_kbps=128, sample_rate=48000, channels=2,
                      limit_streams=True, all_audio_tracks=True, keep_sd_resolution=True)
        values.update(changes)
        return values

    def convert(self, name, settings):
        out = self.folder / name
        cmd = build_ffmpeg_cmd(self.ff, str(self.source), str(out), settings, source_info=self.info)
        result = subprocess.run(cmd, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        return out

    def test_worker_keeps_both_audio_tracks_and_languages(self):
        from worker import ConversionWorker
        final, partial = self.folder / "worker.mp4", self.folder / "worker.mp4.partial"
        job = ConversionWorker("tracks", self.ff, self.fp, str(self.source), str(partial), str(final), self.settings(), .6)
        failures, results = [], []
        job.failed_signal.connect(lambda item, reason: failures.append(reason))
        job.validation_signal.connect(lambda item, result, path: results.append(result))
        job.run()
        self.assertFalse(failures, failures)
        self.assertTrue(final.exists())
        self.assertTrue(results and results[0].status in ("PASS", "WARN"))
        streams = get_media_info(self.fp, str(final))["streams"]
        self.assertEqual(len(streams), 3)
        audio = [s for s in streams if s["codec_type"] == "audio"]
        self.assertEqual([s["tags"]["language"] for s in audio], ["eng", "jpn"])
        self.assertEqual(self.info["streams"][2]["tags"]["title"], "Commentary 日本語")
        self.assertEqual(audio[1]["tags"]["handler_name"], "Commentary 日本語")

    def test_worker_preserves_output_when_packet_validation_is_incomplete(self):
        from validator import PacketScanError
        from worker import ConversionWorker
        final, partial = self.folder / "packet-warning.mp4", self.folder / "packet-warning.mp4.partial"
        job = ConversionWorker("packet-warning", self.ff, self.fp, str(self.source), str(partial),
                               str(final), self.settings(), .6)
        failures, results = [], []
        job.failed_signal.connect(lambda item, reason: failures.append(reason))
        job.validation_signal.connect(lambda item, result, path: results.append((result, path)))
        with patch("validator.scan_packet_sizes", side_effect=PacketScanError("ffprobe packet read failed")):
            job.run()
        self.assertFalse(failures, failures)
        self.assertTrue(final.exists())
        self.assertFalse(partial.exists())
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0][0].status, "WARN")
        self.assertIn("packet read failed", results[0][0].generate_report())
        self.assertEqual(results[0][1], str(final))
        self.assertEqual(sum(s["codec_type"] == "audio" for s in get_media_info(self.fp, str(final))["streams"]), 2)

    def test_embedded_subtitle_selection_changes_the_picture(self):
        hashes = []
        for track in (0, 1):
            out = self.convert(f"subtitle{track}.mp4", self.settings(embedded_subtitle_track=track))
            result = subprocess.run([self.ff, "-v", "error", "-i", str(out), "-map", "0:v:0", "-frames:v", "1", "-f", "framemd5", "-"], capture_output=True, check=True)
            hashes.append(result.stdout.splitlines()[-1])
        self.assertNotEqual(*hashes)

    def test_two_pass_multitrack_worker_probes_source_once(self):
        from worker import ConversionWorker
        final, partial = self.folder / "two-pass.mp4", self.folder / "two-pass.mp4.partial"
        job = ConversionWorker("two-pass", self.ff, self.fp, str(self.source), str(partial), str(final),
                               self.settings(two_pass=True), .6)
        failures = []
        job.failed_signal.connect(lambda item, reason: failures.append(reason))
        with patch("worker.get_media_info", wraps=get_media_info) as probe:
            job.run()
            self.assertEqual(probe.call_count, 1)
        self.assertFalse(failures, failures)
        self.assertTrue(final.exists())
        self.assertEqual(sum(s["codec_type"] == "audio" for s in get_media_info(self.fp, str(final))["streams"]), 2)

    def test_multitrack_avi_and_mpeg_outputs(self):
        for ext, video, audio in (("avi", "mpeg4", "libmp3lame"), ("mpg", "mpeg2video", "mp2")):
            with self.subTest(container=ext):
                out = self.convert("multi." + ext, self.settings(vcodec=video, acodec=audio))
                streams = get_media_info(self.fp, str(out))["streams"]
                self.assertEqual(sum(s["codec_type"] == "audio" for s in streams), 2)
                if ext == "avi":
                    audio_streams = [s for s in streams if s["codec_type"] == "audio"]
                    self.assertEqual(audio_streams[1]["tags"]["title"], "Commentary 日本語")
                self.assertIn(validate_converted_file(self.fp, str(out)).status, ("PASS", "WARN"))
                decoded = subprocess.run([self.ff, "-v", "error", "-i", str(out), "-map", "0:a", "-f", "null", "-"], capture_output=True)
                self.assertEqual(decoded.returncode, 0, decoded.stderr.decode(errors="replace"))

    def test_avi_aac_and_vorbis_emitted_tags_and_validation(self):
        # Compare actual muxer output with the owning player's table rather than copying
        # the converter allowlist into this test. SMS_CodecGetID ignores ASCII tag case.
        source = (Path(__file__).resolve().parents[2] / "src" / "SMS_Codec.c").read_text()
        table = source.split("s_CodecAudioTags[] =", 1)[1].split("};", 1)[0]
        tags = {}
        for codec, value in re.findall(r"SMS_CodecID_(\w+)\s*,\s*(0x[0-9a-fA-F]+)", table):
            tags.setdefault(codec, set()).add(int(value, 16).to_bytes(4, "little").upper())

        cases = [("aac", "AAC", True, rate) for rate in (48000, 44100, 32000, 22050)]
        cases.append(("libvorbis", "OGGV", False, 48000))
        for encoder, codec_id, supported, requested_rate in cases:
            with self.subTest(encoder=encoder, sample_rate=requested_rate):
                out = self.convert(f"mapping-{encoder}-{requested_rate}.avi", self.settings(
                    acodec=encoder, sample_rate=requested_rate,
                    width=160, height=128, keep_sd_resolution=False))
                formats = avi_audio_formats(out)
                audio = [s for s in get_media_info(self.fp, str(out))["streams"]
                         if s["codec_type"] == "audio"]
                self.assertEqual(len(formats), 2)
                self.assertEqual(len(audio), len(formats))
                for stream, (tag, channels, rate, extra) in zip(audio, formats):
                    self.assertEqual(tag, int(stream["codec_tag"], 16))
                    self.assertEqual(tag.to_bytes(4, "little").upper() in tags[codec_id], supported)
                    self.assertEqual((channels, rate), (2, requested_rate))
                    self.assertTrue(extra)
                    if encoder == "aac":
                        # WAVE cbSize reaches SMS _NeAACDecInit2. Leading ASC fields:
                        # objectType(5), frequencyIndex(4), channelConfig(4), GASpecific(3).
                        asc = int.from_bytes(extra[:2], "big")
                        frequency_index = {48000: 3, 44100: 4, 32000: 5, 22050: 7}[requested_rate]
                        self.assertEqual((asc >> 11, (asc >> 7) & 15, (asc >> 3) & 15),
                                         (2, frequency_index, 2))
                        self.assertEqual(asc & 7, 0)  # ordinary 1024-sample AAC-LC frames
                result = validate_converted_file(self.fp, str(out))
                self.assertEqual(result.status, "PASS" if supported else "WARN", result.generate_report())
                mapping_warnings = [w for w in result.warnings if "converter output mapping" in w]
                if supported:
                    self.assertFalse(mapping_warnings, result.generate_report())
                else:
                    self.assertTrue(mapping_warnings, result.generate_report())
                    self.assertTrue(all("VORBIS audio inside .avi" in w for w in mapping_warnings))
                    self.assertTrue(out.exists())

    def test_avi_audio_choices_include_aac_and_exclude_ffmpeg_vorbis(self):
        from config import AUDIO_CODECS_MAP
        from ui.advanced_settings import AdvancedSettingsWidget
        widget = AdvancedSettingsWidget()
        try:
            widget.load_preset(next(p for p in PRESETS.values() if p.ext == ".avi"))
            offered = {AUDIO_CODECS_MAP[widget.combo_acodec.itemText(i)]
                       for i in range(widget.combo_acodec.count())}
            self.assertIn("aac", offered)
            self.assertIn("libmp3lame", offered)
            self.assertIn("pcm_s16le", offered)
            self.assertNotIn("libvorbis", offered)
            widget.load_preset(next(p for p in PRESETS.values() if p.ext == ".ogg"))
            offered = {AUDIO_CODECS_MAP[widget.combo_acodec.itemText(i)]
                       for i in range(widget.combo_acodec.count())}
            self.assertIn("libvorbis", offered)
        finally:
            widget.close()

    def test_source_sd_resolution_is_preserved(self):
        out = self.convert("sd.mp4", self.settings())
        video = next(s for s in get_media_info(self.fp, str(out))["streams"] if s["codec_type"] == "video")
        self.assertEqual((video["width"], video["height"]), (720, 480))
        scaled = self.convert("scaled.mp4", self.settings(keep_sd_resolution=False))
        video = next(s for s in get_media_info(self.fp, str(scaled))["streams"] if s["codec_type"] == "video")
        self.assertEqual((video["width"], video["height"]), (640, 480))

    def test_worker_reports_bad_track_selection_without_encoding_or_crash(self):
        from worker import ConversionWorker
        final, partial = self.folder / "invalid.mp4", self.folder / "invalid.mp4.partial"
        job = ConversionWorker("invalid", self.ff, self.fp, str(self.source), str(partial), str(final),
                               self.settings(embedded_subtitle_track=9), .6)
        job._source_info_cached, job._has_video_cached = self.info, True
        failures = []
        job.failed_signal.connect(lambda item, reason: failures.append(reason))
        with patch("worker.subprocess.Popen") as launch, patch("worker.crash_log.write_report") as report:
            job.run()
            launch.assert_not_called()
            report.assert_not_called()
        self.assertEqual(len(failures), 1)
        self.assertIn("Settings cannot be applied", failures[0])
        self.assertIn("absent", failures[0])
        self.assertFalse(final.exists() or partial.exists())

    def test_all_tracks_rejects_uninspectable_excess_and_incompatible_outputs(self):
        cases = [(".mp4", None), (".mp4", {"streams": [{"codec_type": "audio"}] * 7}), (".mp3", self.info)]
        for ext, info in cases:
            with self.subTest(ext=ext, info=info), self.assertRaises(ValueError):
                build_ffmpeg_cmd(self.ff, "in.mkv", "out" + ext, self.settings(), source_info=info)
        with self.assertRaises(ValueError):
            build_ffmpeg_cmd(self.ff, "in.mkv", "out.mp4", self.settings(), has_video_stream=False, source_info=self.info)
        with self.assertRaises(ValueError):
            build_ffmpeg_cmd(self.ff, "in.mkv", "out.mp4", self.settings(embedded_subtitle_track=2), source_info=self.info)
        with self.assertRaises(ValueError):
            build_ffmpeg_cmd(self.ff, "in.mkv", "out.mp4", self.settings(embedded_subtitle_track=0),
                             source_info={"streams": [{"codec_type": "subtitle", "codec_name": "hdmv_pgs_subtitle"}]})

    def test_validator_checks_secondary_audio_and_video(self):
        media = self.folder / "validator.avi"
        media.write_bytes(b"dummy")
        for bad in ({"codec_type": "audio", "codec_name": "opus"},
                    {"codec_type": "video", "codec_name": "h264", "width": 320, "height": 240}):
            probe = {"format": {"duration": ".6"}, "streams": [
                {"codec_type": "audio", "codec_name": "mp3"}, bad]}
            with patch("validator.get_media_info", return_value=probe), patch("validator.scan_packet_sizes", return_value=(0, 0)):
                self.assertEqual(validate_converted_file(self.fp, str(media)).status, "FAIL")

    def test_high_quality_preset_controls_and_pcm_size(self):
        from ui.advanced_settings import AdvancedSettingsWidget
        from ui.main_window import MainWindow
        preset = next(p for p in PRESETS.values() if p.vbitrate_kbps == 10000)
        widget = AdvancedSettingsWidget()
        widget.load_preset(preset)
        settings = widget.get_settings_dict()
        self.assertEqual((settings["gop"], settings["acodec"], settings["abitrate_kbps"]), (6, "pcm_s16le", 0))
        self.assertTrue(settings["keep_sd_resolution"])
        self.assertTrue(MainWindow._matches_preset(settings, preset))
        estimate = MainWindow._estimate_bytes(None, 1, settings)
        self.assertGreater(estimate, (10000+1536)*1000/8)
        widget.chk_all_audio.setChecked(True)
        self.assertFalse(widget.spin_atrack.isEnabled())
        widget.spin_strack.setValue(1)
        self.assertEqual(widget.get_settings_dict()["embedded_subtitle_track"], 1)
        widget.btn_clear_subtitle.click()
        self.assertEqual(widget.spin_strack.value(), -1)
        widget.close()


if __name__ == "__main__":
    unittest.main()
