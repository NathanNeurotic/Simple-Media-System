"""Portable seek policy: actual validator, worker and per-file UI boundaries.

Sizes are mocked over tiny files; no multi-GiB allocation or encoding is needed.
"""
import io
import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

from config import (
    MAX_SMS_FILE_SIZE_BYTES, FILE_SIZE_WARNING_BYTES, FILE_SIZE_ABORT_BYTES,
    FILE_SIZE_ABORT_HEADROOM_BYTES, MSG_FILE_SIZE_APPROACHING_LIMIT,
)
from qt_compat import QApplication, QLabel
from ui.main_window import MainWindow
from validator import validate_converted_file
from worker import ConversionWorker


class TestFileSizePolicy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.folder = Path(self.tmp.name)
        self.partial = self.folder / "output.mp4.partial"
        self.final = self.folder / "output.mp4"
        self.metadata = {"format": {"duration": "1", "format_name": "mp4"}, "streams": [
            {"codec_type": "video", "codec_name": "mpeg4", "width": 160,
             "height": 128, "avg_frame_rate": "30/1", "bit_rate": "600000"},
            {"codec_type": "audio", "codec_name": "aac", "sample_rate": "48000",
             "channels": 2, "bit_rate": "128000"},
        ]}

    def tearDown(self):
        self.tmp.cleanup()

    def test_exact_limit_and_distinct_warning_abort_thresholds(self):
        self.assertEqual(MAX_SMS_FILE_SIZE_BYTES, 2_147_483_647)
        self.assertEqual(FILE_SIZE_ABORT_HEADROOM_BYTES, 10 * 1024 * 1024)
        self.assertEqual(FILE_SIZE_ABORT_BYTES, MAX_SMS_FILE_SIZE_BYTES - 10 * 1024 * 1024)
        self.assertLess(FILE_SIZE_WARNING_BYTES, FILE_SIZE_ABORT_BYTES)
        self.assertLess(FILE_SIZE_ABORT_BYTES, MAX_SMS_FILE_SIZE_BYTES)

    def test_validator_accepts_exact_maximum_and_rejects_larger_files(self):
        self.partial.write_bytes(b"tiny fixture")
        for size in (FILE_SIZE_ABORT_BYTES, MAX_SMS_FILE_SIZE_BYTES - 1,
                     MAX_SMS_FILE_SIZE_BYTES, MAX_SMS_FILE_SIZE_BYTES + 1, 3 * 1024 ** 3):
            with self.subTest(size=size), \
                 patch("validator.os.path.getsize", return_value=size), \
                 patch("validator.get_media_info", return_value=self.metadata) as probe, \
                 patch("validator.scan_packet_sizes", return_value=(100, 20)) as scan:
                result = validate_converted_file("ffprobe", str(self.partial))
                if size <= MAX_SMS_FILE_SIZE_BYTES:
                    self.assertEqual(result.status, "PASS", result.generate_report())
                    probe.assert_called_once()
                    scan.assert_called_once()
                else:
                    self.assertEqual(result.status, "FAIL")
                    self.assertIn("2 GiB minus 1 byte", result.reasons[0])
                    self.assertIn("2,147,483,647", result.reasons[0])
                    probe.assert_not_called()
                    scan.assert_not_called()

    def test_banner_uses_largest_file_and_exact_threshold_boundaries(self):
        size_label, warning = QLabel(), QLabel()
        window = SimpleNamespace(lbl_estimated_size=size_label, lbl_size_warning=warning,
                                 queue_table=SimpleNamespace(items_dict={}))
        cases = [([], None), ([int(1.5 * 1024 ** 3)] * 3, None),
                 ([FILE_SIZE_WARNING_BYTES - 1], None),
                 ([FILE_SIZE_WARNING_BYTES], "WARNING"),
                 ([FILE_SIZE_ABORT_BYTES - 1], "WARNING"),
                 ([1, FILE_SIZE_ABORT_BYTES], "10 MiB"),
                 ([MAX_SMS_FILE_SIZE_BYTES + 1, 1], "10 MiB")]
        for sizes, message in cases:
            with self.subTest(sizes=sizes):
                window.queue_table.items_dict = {str(i): SimpleNamespace(estimated_size_bytes=value)
                                                 for i, value in enumerate(sizes)}
                MainWindow._update_total_estimated_size(window)
                self.assertEqual(warning.isHidden(), message is None)
                if message:
                    self.assertIn(message, warning.text())
                    self.assertIn("2 GiB", warning.text())
                if sizes:
                    self.assertNotEqual(size_label.text(), "Estimated output: --")
        size_label.close()
        warning.close()

    class Process:
        def __init__(self, partial, finish_after_poll=False):
            self.returncode = None
            self.terminated = False
            self.pid = 123
            if finish_after_poll:
                class FinalWrite(io.StringIO):
                    def readline(stream, *args):
                        partial.write_bytes(b"finalized fixture")
                        return super().readline(*args)
                self.stdout = FinalWrite("")
            else:
                partial.write_bytes(b"encoding fixture")
                self.stdout = io.StringIO("progress=end\n")

        def poll(self):
            return self.returncode

        def terminate(self):
            self.terminated = True
            self.returncode = -15

        def kill(self):
            self.terminate()

        def wait(self, timeout=None):
            if self.returncode is None:
                self.returncode = 0
            return self.returncode

    def make_worker(self):
        worker = ConversionWorker("size", "ffmpeg", "ffprobe", str(self.folder / "input.mp4"),
                                  str(self.partial), str(self.final), {"vcodec": "mpeg4"}, 1)
        worker._has_video_cached = True
        worker._source_info_cached = self.metadata
        return worker

    def test_worker_stops_at_abort_boundary_and_cleans_partial_and_passlogs(self):
        for size in (FILE_SIZE_ABORT_BYTES, MAX_SMS_FILE_SIZE_BYTES + 1):
            with self.subTest(size=size):
                worker = self.make_worker()
                process = self.Process(self.partial)
                passlog = str(self.partial) + ".passlog"
                logs = [Path(passlog + suffix) for suffix in ("-0.log", "-0.log.mbtree")]
                for path in logs:
                    path.write_text("statistics")
                failures = []
                worker.failed_signal.connect(lambda item, message: failures.append(message))
                with patch("worker.build_ffmpeg_cmd", return_value=["ffmpeg"]), \
                     patch("worker.subprocess.Popen", return_value=process), \
                     patch("worker.os.path.getsize", return_value=size), \
                     patch("worker.crash_log.breadcrumb"):
                    self.assertEqual(worker._run_pass(2, passlog, 1, 2), "failed")
                self.assertEqual(failures, [MSG_FILE_SIZE_APPROACHING_LIMIT])
                self.assertTrue(process.terminated and process.stdout.closed)
                self.assertFalse(self.partial.exists() or self.final.exists())
                self.assertTrue(all(not path.exists() for path in logs))

    def test_worker_below_abort_threshold_completes_and_analysis_pass_ignores_size(self):
        for pass_number in (0, 1):
            with self.subTest(pass_number=pass_number):
                worker = self.make_worker()
                process = self.Process(self.partial)
                with patch("worker.build_ffmpeg_cmd", return_value=["ffmpeg"]), \
                     patch("worker.subprocess.Popen", return_value=process), \
                     patch("worker.os.path.getsize", return_value=FILE_SIZE_ABORT_BYTES - 1) as size, \
                     patch("worker.crash_log.breadcrumb"):
                    self.assertEqual(worker._run_pass(pass_number, None, 0, 1), "ok")
                if pass_number == 1:
                    size.assert_not_called()
                self.assertFalse(process.terminated)
                self.assertTrue(process.stdout.closed)
                self.partial.unlink()

    def test_fast_completion_is_validated_before_final_name_promotion(self):
        for size in (MAX_SMS_FILE_SIZE_BYTES, MAX_SMS_FILE_SIZE_BYTES + 1, 3 * 1024 ** 3):
            with self.subTest(size=size):
                worker = self.make_worker()
                process = self.Process(self.partial, finish_after_poll=True)
                validations, failures = [], []
                worker.validation_signal.connect(lambda item, result, path: validations.append((result, path)))
                worker.failed_signal.connect(lambda item, message: failures.append(message))
                # Initial poll sees no partial. EOF then creates the tiny finished
                # fixture; the actual validator sees its mocked final byte count.
                with patch("worker.build_ffmpeg_cmd", return_value=["ffmpeg"]), \
                     patch("worker.subprocess.Popen", return_value=process), \
                     patch("worker.os.path.getsize", return_value=size), \
                     patch("validator.get_media_info", return_value=self.metadata), \
                     patch("validator.scan_packet_sizes", return_value=(100, 20)), \
                     patch("worker.crash_log.breadcrumb"), \
                     patch("worker.crash_log.write_report") as report:
                    worker.run()
                report.assert_not_called()
                self.assertFalse(failures or process.terminated)
                self.assertEqual(len(validations), 1)
                self.assertFalse(self.partial.exists())
                if size <= MAX_SMS_FILE_SIZE_BYTES:
                    self.assertEqual(validations[0][0].status, "PASS")
                    self.assertTrue(self.final.exists())
                    self.final.unlink()
                else:
                    self.assertEqual(validations[0][0].status, "FAIL")
                    self.assertFalse(self.final.exists())


if __name__ == "__main__":
    unittest.main()
