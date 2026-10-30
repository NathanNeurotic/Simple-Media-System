"""
Unit and integration tests for SMS Media Converter core utilities and validator.
"""
import unittest
import os
import sys
from pathlib import Path

# Add sms_converter to sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from config import (
    PRESETS, MAX_WIDTH, MAX_HEIGHT, SIZE_4_0_GIB, MAX_VIDEO_PACKET_SIZE,
    MAX_AUDIO_PACKET_SIZE, MPEG_ALLOWED_FPS_MAP
)
from ffmpeg_utils import (
    find_ffmpeg_binaries, check_libxvid_support, get_media_info,
    parse_media_summary, calculate_estimated_output_size, build_ffmpeg_cmd,
    generate_unique_output_path
)
from validator import validate_converted_file

class TestSMSConverter(unittest.TestCase):

    def test_binary_detection(self):
        ff, fp = find_ffmpeg_binaries()
        self.assertIsNotNone(ff, "FFmpeg binary should be found on system PATH or app dir")
        self.assertIsNotNone(fp, "ffprobe binary should be found on system PATH or app dir")

    def test_presets(self):
        self.assertTrue(any("Xvid-Compatible AVI" in k for k in PRESETS.keys()))
        self.assertTrue(any("MPEG-2" in k for k in PRESETS.keys()))
        self.assertTrue(any("MPEG-1" in k for k in PRESETS.keys()))
        self.assertTrue(any("MPEG-4 MP4" in k for k in PRESETS.keys()))

        xvid_preset = PRESETS["Xvid-Compatible AVI [RECOMMENDED - Best PS2 Performance]"]
        self.assertEqual(xvid_preset.vtag, "XVID")
        self.assertEqual(xvid_preset.width, 640)
        self.assertEqual(xvid_preset.height, 480)

    def test_ipu_presets_use_shorter_prediction_chains(self):
        # Preserve the merged 15-frame preset and its generated FFmpeg argument.
        # This command check does not establish the cause of console color changes.
        for preset in PRESETS.values():
            if preset.vcodec in ("mpeg1video", "mpeg2video"):
                self.assertEqual(preset.gop, 15, preset.name)
                cmd = build_ffmpeg_cmd("ffmpeg", "in.mp4", "out.mpg", {
                    "vcodec": preset.vcodec, "acodec": preset.acodec,
                    "gop": preset.gop, "fps": preset.fps, "ext": preset.ext})
                self.assertEqual(cmd[cmd.index("-g") + 1], str(preset.gop))

    def test_fps_mapping(self):
        self.assertEqual(MPEG_ALLOWED_FPS_MAP["23.976"], "24000/1001")
        self.assertEqual(MPEG_ALLOWED_FPS_MAP["29.97"], "30000/1001")
        self.assertEqual(MPEG_ALLOWED_FPS_MAP["59.94"], "60000/1001")

    def test_estimated_size_calculation(self):
        est_bytes = calculate_estimated_output_size(600, 1500, 128)
        self.assertGreater(est_bytes, 100 * 1024 * 1024)
        self.assertLess(est_bytes, 150 * 1024 * 1024)

    def test_output_path_generation(self):
        source = "sample_movie.mp4"
        final_p, partial_p = generate_unique_output_path(source, ".avi")
        self.assertTrue(final_p.endswith("sample_movie_SMS.avi"))
        self.assertTrue(partial_p.endswith("sample_movie_SMS.avi.partial"))

    def test_cmd_building(self):
        ff, _ = find_ffmpeg_binaries()
        settings = {
            "vcodec": "mpeg4",
            "vtag": "XVID",
            "width": 640,
            "height": 480,
            "fps": "30",
            "vbitrate_kbps": 1500,
            "acodec": "libmp3lame",
            "abitrate_kbps": 128,
            "sample_rate": 48000,
            "channels": 2,
            "scaling_mode": "Letterbox to exact dimensions",
            "limit_streams": True
        }
        cmd = build_ffmpeg_cmd(ff, "input.mp4", "output.avi.partial", settings)

        self.assertIn("-c:v", cmd)
        # Either encoder is correct here: both produce MPEG-4 Part 2 that SMS decodes, and
        # which one is chosen depends on whether THIS ffmpeg build has libxvid (real Xvid is
        # preferred when present -- measurably better at the same bitrate). Asserting one
        # specific name would make the suite fail on half the machines it runs on.
        self.assertTrue(
            "mpeg4" in cmd or "libxvid" in cmd,
            f"expected an MPEG-4 Part 2 encoder, got: {cmd}",
        )
        self.assertIn("-vtag", cmd)
        self.assertIn("XVID", cmd)
        self.assertIn("-map", cmd)
        self.assertIn("0:v:0?", cmd)
        self.assertIn("0:a:0?", cmd)
        self.assertIn("-sn", cmd)
        self.assertIn("-progress", cmd)

    def test_vtag_is_avi_only(self):
        """An AVI FourCC must never reach a non-AVI container.

        This is a REGRESSION TEST for a bug that broke every .mp4 conversion for as long as
        the MP4 presets existed, on every platform, and reported only "Failed". Two entries
        in VIDEO_CODECS_MAP map to "mpeg4", so the advanced-settings panel matched the Xvid
        one for any mpeg4 preset; vtag is recovered from that combo's TEXT, so the .mp4
        presets came back carrying vtag="XVID". ffmpeg then got
        `-c:v libxvid -vtag XVID ... -f mp4` and aborted while writing the header:
        "Could not find tag for codec mpeg4 in stream #0, codec not currently supported in
        container".

        Asserted at the command builder because that is the layer that knows the container,
        and it is the layer that has to stay right however the settings were assembled.
        """
        ff, _ = find_ffmpeg_binaries()
        polluted = {
            "vcodec": "mpeg4", "vtag": "XVID", "width": 640, "height": 480, "fps": "30",
            "vbitrate_kbps": 1500, "acodec": "aac", "abitrate_kbps": 128,
            "sample_rate": 48000, "channels": 2, "limit_streams": True,
        }

        mp4 = build_ffmpeg_cmd(ff, "input.avi", "output.mp4.partial", polluted)
        self.assertNotIn("-vtag", mp4, f"AVI FourCC leaked into an MP4: {mp4}")
        self.assertIn("mp4", mp4)

        # ...while the AVI path, which is the proven one, keeps it.
        avi = build_ffmpeg_cmd(ff, "input.avi", "output.avi.partial", polluted)
        self.assertIn("-vtag", avi)
        self.assertIn("XVID", avi)

        # And no shipped preset may produce the bad pairing on its own.
        for name, preset in PRESETS.items():
            if not preset.vcodec:
                continue
            s = dict(polluted, vcodec=preset.vcodec, vtag=preset.vtag, acodec=preset.acodec)
            c = build_ffmpeg_cmd(ff, "input.avi", f"output{preset.ext}.partial", s)
            if preset.ext != ".avi":
                self.assertNotIn("-vtag", c, f"preset '{name}' emits a vtag into {preset.ext}")

class TestPresetRoundTrip(unittest.TestCase):
    """Every preset must survive a trip through the settings widget unchanged.

    TWO shipped bugs came from reading the model back out of a display widget, and both were
    invisible until someone tried a conversion:

    1. Two VIDEO_CODECS_MAP keys map to "mpeg4", so the video combo always landed on the Xvid
       one and vtag was recovered from its TEXT -- every .mp4 preset came back as vtag="XVID"
       and ffmpeg refused the container ("Tag XVID incompatible with output codec id '12'").
    2. Audio-only presets park that same hidden combo at index 0, which IS the Xvid entry, so
       they came back claiming vcodec="mpeg4". MP3 and Ogg then failed outright, and the AAC
       .m4a preset silently wrote a VIDEO STREAM into an audio file -- exit 0, no error shown.

    Asserting the round-trip catches the whole class, including the next display name someone
    adds that happens to collide with an existing one.
    """

    @classmethod
    def setUpClass(cls):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        from qt_compat import QtWidgets
        cls._app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    def test_presets_round_trip_through_the_widget(self):
        from ui.advanced_settings import AdvancedSettingsWidget

        w = AdvancedSettingsWidget()
        for name, preset in PRESETS.items():
            with self.subTest(preset=name):
                w.load_preset(preset)
                got = w.get_settings_dict()
                self.assertEqual(got.get("vcodec"), preset.vcodec, f"vcodec changed for '{name}'")
                self.assertEqual(got.get("vtag"), preset.vtag, f"vtag changed for '{name}'")

    def test_audio_presets_never_encode_video(self):
        from ui.advanced_settings import AdvancedSettingsWidget

        ff, _ = find_ffmpeg_binaries()
        w = AdvancedSettingsWidget()
        for name, preset in PRESETS.items():
            if preset.vcodec is not None:
                continue
            with self.subTest(preset=name):
                w.load_preset(preset)
                # has_video_stream=True is the case that broke: an audio preset applied to a
                # source that HAS video. It must drop the video, not try to encode it.
                cmd = build_ffmpeg_cmd(
                    ff, "input.avi", f"output{preset.ext}.partial",
                    w.get_settings_dict(), has_video_stream=True,
                )
                self.assertIn("-vn", cmd, f"audio preset '{name}' does not drop video: {cmd}")
                self.assertNotIn("-c:v", cmd, f"audio preset '{name}' encodes video: {cmd}")

class TestOnlyPlayableCombinationsAreOffered(unittest.TestCase):
    """Advanced Settings must not offer a codec the preset's container cannot carry.

    The container is fixed by the preset (Preset.ext); both codec combos were filled from the
    entire map for every preset and nothing reconciled them. 20 of 54 preset x audio-codec
    pairs killed the job -- and on the MP3 preset the audio group is the ONLY visible panel,
    with 5 of its 6 choices fatal.

    The reference table is read out of the SMS demuxer sources; see SMS_CONTAINER_CODECS.
    """

    @classmethod
    def setUpClass(cls):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        from qt_compat import QtWidgets
        cls._app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    def test_every_offered_codec_is_decodable_in_that_container(self):
        from config import VIDEO_CODECS_MAP, AUDIO_CODECS_MAP, sms_supports
        from ui.advanced_settings import AdvancedSettingsWidget

        w = AdvancedSettingsWidget()
        for name, preset in PRESETS.items():
            w.load_preset(preset)
            for combo, kind, source in (
                (w.combo_vcodec, "video", VIDEO_CODECS_MAP),
                (w.combo_acodec, "audio", AUDIO_CODECS_MAP),
            ):
                if kind == "video" and preset.vcodec is None:
                    continue  # hidden for audio-only presets
                for i in range(combo.count()):
                    display = combo.itemText(i)
                    ff = source[display]
                    with self.subTest(preset=name, kind=kind, codec=display):
                        self.assertTrue(
                            sms_supports(preset.ext, kind, ff),
                            f"'{name}' offers {kind} codec {ff!r}, which SMS cannot decode "
                            f"inside {preset.ext}",
                        )

    def test_table_matches_the_sms_sources_it_was_read_from(self):
        """Guards the specific facts the table encodes, so a careless edit is caught."""
        from config import sms_supports

        # SMS_Codec.c AVI FourCC table has no MPEG-1/MPEG-2 entry, and no AAC in its audio one.
        self.assertFalse(sms_supports(".avi", "video", "mpeg2video"))
        self.assertFalse(sms_supports(".avi", "video", "mpeg1video"))
        self.assertFalse(sms_supports(".avi", "audio", "aac"))
        self.assertTrue(sms_supports(".avi", "video", "mpeg4"))
        self.assertTrue(sms_supports(".avi", "audio", "libmp3lame"))   # encoder name normalised
        # MPEG-PS carries MPEG-1/2 video; MP4 has no msmpeg4v3 object type.
        self.assertTrue(sms_supports(".mpg", "video", "mpeg2video"))
        self.assertFalse(sms_supports(".mp4", "video", "msmpeg4v3"))
        # An unlisted container means "no information", never "unsupported".
        self.assertTrue(sms_supports(".mkv", "video", "anything"))

class TestUpscaleClamp(unittest.TestCase):
    """"Allow upscaling (default off)" has to actually be off by default.

    build_scale_filter clamps against settings["src_width"]/["src_height"], and NOTHING in the
    app ever produced them -- _on_inspect_finished computed the values from the ffprobe summary
    and dropped them on the floor. So the clamp never ran, the checkbox changed nothing, and
    every sub-640x480 source was silently blown up: measured on a 320x240 clip, 227,782 bytes
    at 640x480 versus 132,030 at its native size, for no added detail and four times the
    macroblocks for the PS2's SOFTWARE decoder.
    """

    def test_clamp_engages_only_when_upscaling_is_disallowed(self):
        from ffmpeg_utils import build_scale_filter

        small = dict(src_w=320, src_h=240, target_w=640, target_h=480,
                     scaling_mode="Letterbox to exact dimensions")

        off = " ".join(build_scale_filter(allow_upscale=False, **small))
        on = " ".join(build_scale_filter(allow_upscale=True, **small))

        self.assertIn("320", off, f"source was upscaled with upscaling disallowed: {off}")
        self.assertIn("640", on, f"upscaling was requested and refused: {on}")
        self.assertNotEqual(off, on, "the allow_upscale setting changes nothing")

    def test_settings_reaching_the_worker_carry_source_dimensions(self):
        """The producer side: a QueueItem must be able to carry what the clamp consumes."""
        from ui.queue_table import QueueItem

        item = QueueItem(item_id="x", file_path="/tmp/x.avi", file_name="x.avi")
        self.assertTrue(hasattr(item, "src_width"))
        self.assertTrue(hasattr(item, "src_height"))

class TestCoverArtIsNotVideo(unittest.TestCase):
    """Album art must not be mistaken for a video track.

    build_ffmpeg_cmd has always accepted has_video_stream and NOTHING ever passed it, so it
    sat at its default of True for every job. On a music file with embedded art, `-map 0:v:0?`
    selected the still, the encoder turned it into mpeg4, and the mp4 muxer refused it --
    exit -22, zero-byte output, both MP4 presets, for the most ordinary music file there is.
    """

    def test_attached_pic_is_not_a_video_stream(self):
        from ffmpeg_utils import has_real_video_stream

        cover_art = {"streams": [
            {"codec_type": "audio"},
            {"codec_type": "video", "disposition": {"attached_pic": 1}},
        ]}
        real_video = {"streams": [
            {"codec_type": "video", "disposition": {"attached_pic": 0}},
            {"codec_type": "audio"},
        ]}
        audio_only = {"streams": [{"codec_type": "audio"}]}

        self.assertFalse(has_real_video_stream(cover_art), "cover art counted as video")
        self.assertTrue(has_real_video_stream(real_video), "real video track missed")
        self.assertFalse(has_real_video_stream(audio_only))
        # An unreadable probe must NOT silently strip video from a real film.
        self.assertTrue(has_real_video_stream({"error": "boom"}))
        self.assertTrue(has_real_video_stream({}))

    def test_cover_art_source_drops_video(self):
        from ffmpeg_utils import has_real_video_stream

        ff, _ = find_ffmpeg_binaries()
        settings = {
            "vcodec": "mpeg4", "vtag": None, "acodec": "aac", "width": 640, "height": 480,
            "fps": "30", "vbitrate_kbps": 1500, "abitrate_kbps": 128,
            "sample_rate": 48000, "channels": 2, "limit_streams": True,
        }
        cover_art = {"streams": [
            {"codec_type": "audio"},
            {"codec_type": "video", "disposition": {"attached_pic": 1}},
        ]}
        cmd = build_ffmpeg_cmd(
            ff, "music.mp3", "out.mp4.partial", settings,
            has_video_stream=has_real_video_stream(cover_art),
        )
        self.assertIn("-vn", cmd)
        self.assertNotIn("-c:v", cmd)

class TestQueueOwnership(unittest.TestCase):
    """Removing via the context menu must not desync the two queue structures.

    Membership lives in QueueTableWidget.items_dict AND MainWindow.queue_order, and only
    MainWindow maintained both. The context menu called remove_item directly, so queue_order
    kept a dangling id and the next Convert press did items_dict[stale_id] -> KeyError, on
    every press thereafter. The shipped build catches it in an excepthook and shows a modal,
    so the app stayed open and permanently broken.

    These tests CALL THE SLOT. The existing UI test only imports the modules, which is exactly
    why `QMenu.exec` (PySide2-incompatible) and this shipped together in the same handler.
    """

    @classmethod
    def setUpClass(cls):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
        from qt_compat import QtWidgets
        cls._app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    def test_context_menu_removal_keeps_queue_order_in_sync(self):
        from ui.queue_table import QueueTableWidget, QueueItem

        table = QueueTableWidget()
        order = []

        def on_remove(ids):
            for i in ids:
                table.remove_item(i)
                if i in order:
                    order.remove(i)

        table.remove_requested.connect(on_remove)

        for n in ("a.avi", "b.avi", "c.avi"):
            table.add_queue_item(QueueItem(item_id=n, file_path=f"/tmp/{n}", file_name=n))
            order.append(n)

        table.remove_requested.emit(["b.avi"])

        self.assertNotIn("b.avi", table.items_dict)
        self.assertNotIn("b.avi", order, "queue_order kept a dangling id")
        self.assertEqual(set(order), set(table.items_dict.keys()))
        # The KeyError this caused, reproduced directly.
        for i_id in order:
            table.items_dict[i_id]

    def test_context_menu_handler_requests_rather_than_removes(self):
        """Drive the REAL _show_context_menu, not just the signal it is supposed to emit.

        Stubs the menu runner to choose "Remove Selected", then asserts the handler emitted
        remove_requested and did NOT call remove_item itself. Emitting the signal by hand
        would pass even if the handler went back to mutating the dict directly.
        """
        import ui.queue_table as qt_mod
        from ui.queue_table import QueueTableWidget, QueueItem

        table = QueueTableWidget()
        table.add_queue_item(QueueItem(item_id="a.avi", file_path="/tmp/a.avi", file_name="a.avi"))
        table.selectRow(0)

        got = []
        table.remove_requested.connect(lambda ids: got.extend(ids))
        table.remove_item = lambda *a, **k: self.fail(
            "context menu removed the item itself; queue_order would now be stale"
        )

        real_exec = qt_mod.menu_exec
        # Choose the LAST action ("Remove Selected") without showing anything.
        qt_mod.menu_exec = lambda menu, pos: menu.actions()[-1]
        try:
            table._show_context_menu(table.rect().center())
        finally:
            qt_mod.menu_exec = real_exec

        self.assertEqual(got, ["a.avi"], "remove_requested was not emitted")

    def test_menu_exec_works_on_this_binding(self):
        """PySide2 has only QMenu.exec_; PySide6 added .exec. The shim must cover both."""
        from qt_compat import menu_exec, QMenu

        menu = QMenu()
        self.assertTrue(
            getattr(menu, "exec", None) or getattr(menu, "exec_", None),
            "neither exec nor exec_ on QMenu",
        )
        self.assertTrue(callable(menu_exec))

class TestQtCompatShim(unittest.TestCase):
    """Guard the Qt binding shim.

    The Windows 7 build routes every Qt import through qt_compat, so a name the shim
    forgets to re-export is not a lint nit -- the app dies at import on BOTH bindings.
    That is exactly what happened: QMenu and QTabWidget were missed, and nothing caught
    it because the suite never imported the UI modules. It does now.
    """

    def test_every_imported_name_is_exported(self):
        import io
        import os
        import re
        import qt_compat

        base = os.path.dirname(os.path.abspath(__file__))
        wanted = set()
        for root, _dirs, names in os.walk(base):
            if "__pycache__" in root:
                continue
            for n in names:
                if not n.endswith(".py") or n == "qt_compat.py":
                    continue
                src = io.open(os.path.join(root, n), encoding="utf-8").read()
                for m in re.finditer(
                    r"from qt_compat import \(([^)]*)\)|from qt_compat import ([^\(\n]+)", src
                ):
                    body = m.group(1) or m.group(2)
                    for tok in body.replace("\n", " ").split(","):
                        tok = tok.strip()
                        # Identifiers only: this scan reads THIS file too, and the regex
                        # literal just above would otherwise be collected as a "name".
                        if tok.isidentifier():
                            wanted.add(tok)

        self.assertTrue(wanted, "found no qt_compat imports -- the scan is broken")
        missing = sorted(w for w in wanted if not hasattr(qt_compat, w))
        self.assertEqual(missing, [], f"qt_compat is missing re-exports: {missing}")

    def test_ui_modules_actually_import(self):
        """Import every UI module. A missing shim export only shows up here."""
        import importlib

        for mod in (
            "ui.main_window", "ui.queue_table", "ui.preset_selector",
            "ui.drop_zone", "ui.advanced_settings", "ui.details_dialog",
            "ui.setup_dialog",
        ):
            with self.subTest(module=mod):
                importlib.import_module(mod)


class TestMultiTrackAudioAndSubtitles(unittest.TestCase):
    """#22: the stream map honors audio_track, and a subtitle file is burned in."""

    def _settings(self, **over):
        s = {
            "vcodec": "mpeg2video",
            "width": 640,
            "height": 480,
            "scaling_mode": "Letterbox to exact dimensions",
            "limit_streams": True,
        }
        s.update(over)
        return s

    def test_default_is_first_audio_track(self):
        ff, _ = find_ffmpeg_binaries()
        cmd = build_ffmpeg_cmd(ff, "in.mp4", "out.mpg.partial", self._settings())
        self.assertIn("0:a:0?", cmd)

    def test_audio_track_index_maps(self):
        ff, _ = find_ffmpeg_binaries()
        cmd = build_ffmpeg_cmd(ff, "in.mp4", "out.mpg.partial", self._settings(audio_track=2))
        self.assertIn("0:a:2", cmd)
        self.assertNotIn("0:a:2?", cmd)

    def test_subtitle_burn_in_and_escaping(self):
        ff, _ = find_ffmpeg_binaries()
        # Windows path + a quote in the folder name: the filtergraph must not break.
        sub = "C:\\subs\\it's here\\movie.srt"
        cmd = build_ffmpeg_cmd(ff, "in.mp4", "out.mpg.partial", self._settings(subtitle_file=sub))
        vf = cmd[cmd.index("-vf") + 1]
        self.assertIn("subtitles=", vf)
        self.assertIn("charenc=UTF-8", vf)
        self.assertIn("\\\\", vf, "backslashes must be escaped for the filter parser")
        self.assertNotIn(sub, vf, "the raw path must never reach the filtergraph")

    def test_real_ffmpeg_subtitle_paths_and_missing_audio(self):
        import subprocess
        import tempfile
        ff, fp = find_ffmpeg_binaries()
        if not ff or not fp:
            self.skipTest("FFmpeg and ffprobe required")
        with tempfile.TemporaryDirectory(dir=str(Path.cwd())) as tmp:
            folder = Path(tmp) / "it's here [subs],;"
            folder.mkdir()
            sub = folder / "movie.srt"
            sub.write_text("1\n00:00:00,000 --> 00:00:01,000\nSubtitle test\n", encoding="utf-8")
            source = str(Path(tmp) / "source.mkv")
            subprocess.run([ff, "-v", "error", "-f", "lavfi", "-i",
                            "color=size=160x120:rate=25:duration=1", "-f", "lavfi",
                            "-i", "sine=duration=1", "-c:v", "mpeg2video",
                            "-c:a", "mp2", source], check=True, capture_output=True)
            output = str(Path(tmp) / "output.mpg")
            settings = self._settings(subtitle_file=str(sub), acodec="mp2", width=160, height=120)
            result = subprocess.run(build_ffmpeg_cmd(ff, source, output, settings), capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
            self.assertTrue(any(x["codec_type"] == "audio" for x in get_media_info(fp, output)["streams"]))
            settings["audio_track"] = 1
            result = subprocess.run(build_ffmpeg_cmd(ff, source, output, settings), capture_output=True)
            self.assertNotEqual(result.returncode, 0, "missing explicit audio track must fail")
            self.assertIn(b"matches no streams", result.stderr)

    def test_subtitle_picker_and_clear(self):
        from unittest.mock import patch
        from qt_compat import QApplication
        from ui.advanced_settings import AdvancedSettingsWidget
        app = QApplication.instance() or QApplication([])
        widget = AdvancedSettingsWidget()
        changes = []
        widget.settings_changed.connect(changes.append)
        with patch("ui.advanced_settings.QFileDialog.getOpenFileName", return_value=("C:/subs/movie.srt", "")):
            widget.btn_subtitle.click()
        self.assertEqual(widget.subtitle_path, "C:/subs/movie.srt")
        with patch("ui.advanced_settings.QFileDialog.getOpenFileName", return_value=("", "")):
            widget.btn_subtitle.click()
        self.assertEqual(widget.subtitle_path, "C:/subs/movie.srt")
        widget.btn_clear_subtitle.click()
        self.assertEqual(widget.subtitle_path, "")
        self.assertEqual(widget.lbl_subtitle.text(), "none")
        self.assertTrue(changes)
        widget.close()

    def test_later_source_tracks_and_preset_changes_preserve_custom_state(self):
        from qt_compat import QApplication
        from ui.main_window import MainWindow
        from ui.preset_selector import LABEL_CUSTOM_SETTINGS
        app = QApplication.instance() or QApplication([])
        ff, fp = find_ffmpeg_binaries()
        window = MainWindow(ff, fp)
        widget = window.advanced_settings
        video_names = [name for name, preset in PRESETS.items() if preset.vcodec]
        first, second = video_names[:2]
        window.preset_selector.combo_preset.setCurrentText(first)
        widget.spin_atrack.setValue(12)
        self.assertEqual(widget.get_settings_dict()["audio_track"], 12)
        settings = widget.get_settings_dict()
        cmd = build_ffmpeg_cmd(ff, "source.mkv", "output.mpg", settings)
        self.assertIn("0:a:12", cmd)
        self.assertEqual(window.preset_selector.label_badge.text(), LABEL_CUSTOM_SETTINGS)
        window.preset_selector.combo_preset.setCurrentText(second)
        self.assertEqual(widget.get_settings_dict()["audio_track"], 12)
        self.assertEqual(window.preset_selector.label_badge.text(), LABEL_CUSTOM_SETTINGS)
        widget.spin_atrack.setValue(0)
        widget.subtitle_path = "movie.srt"
        widget._on_control_changed()
        self.assertEqual(window.preset_selector.label_badge.text(), LABEL_CUSTOM_SETTINGS)
        window.preset_selector.combo_preset.setCurrentText(first)
        self.assertEqual(widget.subtitle_path, "movie.srt")
        self.assertEqual(window.preset_selector.label_badge.text(), LABEL_CUSTOM_SETTINGS)
        window.close()

    def test_no_subtitle_by_default(self):
        ff, _ = find_ffmpeg_binaries()
        cmd = build_ffmpeg_cmd(ff, "in.mp4", "out.mpg.partial", self._settings())
        vf = cmd[cmd.index("-vf") + 1]
        self.assertNotIn("subtitles=", vf)


class TestPacketValidation(unittest.TestCase):
    """Incomplete ffprobe scans must not certify a conversion or discard its output."""

    class Reader:
        def __init__(self, records):
            self.records = iter(records)
            self.closed = False

        def readline(self, limit):
            # The scanner must consume bounded records, never the complete packet list.
            assert 0 < limit <= 8192
            return next(self.records, "")

        def close(self):
            self.closed = True

    class Process:
        def __init__(self, records, exit_code=0):
            self.stdout = TestPacketValidation.Reader(records)
            self.exit_code = exit_code
            self.returncode = None
            self.killed = False

        def wait(self):
            if self.returncode is None:
                self.returncode = self.exit_code
            return self.returncode

        def poll(self):
            return self.returncode

        def kill(self):
            self.killed = True
            self.returncode = -9

    def setUp(self):
        import tempfile
        self.tmp = tempfile.TemporaryDirectory()
        self.media = Path(self.tmp.name) / "output.avi.partial"
        self.media.write_bytes(b"encoded output")
        self.metadata = {"format": {"duration": "1"}, "streams": [
            {"codec_type": "video", "codec_name": "mpeg4", "width": 160,
             "height": 128, "avg_frame_rate": "25/1", "bit_rate": "600000"},
            {"codec_type": "audio", "codec_name": "mp3", "sample_rate": "48000",
             "channels": 2, "bit_rate": "128000"},
        ]}

    def tearDown(self):
        self.tmp.cleanup()

    def validate_probe(self, records=(), exit_code=0, diagnostics=b""):
        from unittest.mock import patch
        proc = self.Process(records, exit_code)
        self.last_process = proc

        def launch(cmd, **kwargs):
            self.assertIn("packet=codec_type,size", cmd)
            kwargs["stderr"].write(diagnostics)
            return proc

        with patch("validator.get_media_info", return_value=self.metadata), \
             patch("validator.subprocess.Popen", side_effect=launch):
            result = validate_converted_file("ffprobe", str(self.media))
        self.assertTrue(proc.stdout.closed)
        self.assertIsNotNone(proc.returncode)
        return result

    def test_failed_packet_probe_warns_instead_of_passing(self):
        result = self.validate_probe(exit_code=17, diagnostics=b"packet scan failed")
        self.assertEqual(result.status, "WARN")
        self.assertIn("Packet-size validation incomplete", result.warnings[0])
        self.assertIn("code 17", result.warnings[0])
        self.assertIn("packet scan failed", result.generate_report())

    def test_missing_ffprobe_and_empty_scan_warn(self):
        from unittest.mock import patch
        with patch("validator.get_media_info", return_value=self.metadata), \
             patch("validator.subprocess.Popen", side_effect=FileNotFoundError("ffprobe missing")):
            result = validate_converted_file("ffprobe", str(self.media))
        self.assertEqual(result.status, "WARN")
        self.assertIn("ffprobe missing", result.warnings[0])
        self.assertEqual(self.validate_probe().status, "WARN")

    def test_malformed_packet_records_warn_and_stop_the_process(self):
        for record in ("not a packet\n", "codec_type=video\n", "codec_type=video|size=-1\n",
                       "codec_type=audio|size=N/A\n", "x" * 4097):
            with self.subTest(record=record[:40]):
                self.assertEqual(self.validate_probe([record]).status, "WARN")
                self.assertTrue(self.last_process.killed)

    def test_diagnostics_or_a_missing_stream_type_cannot_pass(self):
        records = ["codec_type=video|size=100\n", "codec_type=audio|size=20\n"]
        self.assertEqual(self.validate_probe(records, diagnostics=b"damaged packet").status, "WARN")
        self.assertEqual(self.validate_probe(records[:1]).status, "WARN")

    def test_observed_oversize_packet_still_fails_an_incomplete_scan(self):
        result = self.validate_probe([
            f"codec_type=video|size={MAX_VIDEO_PACKET_SIZE + 1}\n", "bad record\n"])
        self.assertEqual(result.status, "FAIL")
        self.assertEqual(result.max_video_packet, MAX_VIDEO_PACKET_SIZE + 1)
        self.assertTrue(result.reasons and result.warnings)
        self.metadata["streams"][0]["codec_name"] = "h264"
        self.assertEqual(self.validate_probe(exit_code=17).status, "FAIL")

    def test_packet_scan_streams_records_and_tracks_every_packet(self):
        def records():
            for i in range(20000):
                yield f"codec_type=video|size={i + 1}\n"
            yield "codec_type=audio|size=321\n"
        result = self.validate_probe(records())
        self.assertEqual(result.status, "PASS")
        self.assertEqual((result.max_video_packet, result.max_audio_packet), (20000, 321))

    def test_real_ffmpeg_packet_maxima_match_the_reference(self):
        import json
        import subprocess
        from validator import scan_packet_sizes
        ff, fp = find_ffmpeg_binaries()
        if not ff or not fp:
            self.skipTest("FFmpeg and ffprobe are required")
        media = Path(self.tmp.name) / "real.mpg"
        subprocess.run([ff, "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=160x128:rate=25",
                        "-f", "lavfi", "-i", "sine=sample_rate=48000", "-t", "0.2",
                        "-c:v", "mpeg2video", "-c:a", "mp2", "-f", "mpeg", str(media)],
                       check=True, capture_output=True)
        ref = subprocess.run([fp, "-v", "error", "-show_packets", "-of", "json", str(media)],
                             check=True, capture_output=True, text=True, encoding="utf-8")
        packets = json.loads(ref.stdout)["packets"]
        expected = tuple(max(int(p["size"]) for p in packets if p["codec_type"] == kind)
                         for kind in ("video", "audio"))
        self.assertEqual(scan_packet_sizes(fp, str(media)), expected)
        self.assertEqual(validate_converted_file(fp, str(media)).status, "PASS")


if __name__ == "__main__":
    unittest.main()
