"""
Asynchronous worker threads for file inspection, conversion execution, file size monitoring, and validation.
"""
import os
import sys
import time
import shutil
import subprocess
from collections import deque
from pathlib import Path
from typing import Dict, Any, Optional
from qt_compat import QThread, Signal

import crash_log
from config import SIZE_4_0_GIB, MSG_FILE_SIZE_APPROACHING_LIMIT

# The keys "-progress pipe:1" writes once per block. Recognised so they can drive the progress
# bar without also being forwarded to the log pane -- see the note at the parse site.
_PROGRESS_KEYS = frozenset({
    "frame", "fps", "stream_0_0_q", "bitrate", "total_size", "out_time_us", "out_time_ms",
    "out_time", "dup_frames", "drop_frames", "speed", "progress",
})

# Only the tail is ever used (the failure message quotes the last 20 lines), but this list used
# to grow for the entire encode -- unbounded, on a build that may well be 32-bit. Keeping a
# bounded window costs nothing and removes a slow leak from every long conversion.
_CONSOLE_LOG_MAX = 400
from ffmpeg_utils import (
    get_media_info, parse_media_summary, build_ffmpeg_cmd, generate_unique_output_path,
    has_real_video_stream
)
from validator import validate_converted_file, ValidationResult

class InspectWorker(QThread):
    """Worker thread for non-blocking file inspection via ffprobe."""
    finished_signal = Signal(str, dict)  # file_path, summary_dict

    def __init__(self, ffprobe_path: str, file_path: str):
        super().__init__()
        self.ffprobe_path = ffprobe_path
        self.file_path = file_path

    def run(self):
        probe_data = get_media_info(self.ffprobe_path, self.file_path)
        summary = parse_media_summary(probe_data)
        self.finished_signal.emit(self.file_path, summary)


class ConversionWorker(QThread):
    """
    Worker thread that executes FFmpeg with pipe:1 progress reporting,
    monitors partial file size against the 4 GiB SMS ceiling,
    and runs post-conversion validation.
    """
    progress_signal = Signal(str, float, str)  # item_id, percentage, status_msg
    log_signal = Signal(str, str)             # item_id, log_line
    validation_signal = Signal(str, ValidationResult, str) # item_id, result, final_path
    failed_signal = Signal(str, str)          # item_id, error_message
    cancelled_signal = Signal(str)            # item_id

    def __init__(
        self,
        item_id: str,
        ffmpeg_path: str,
        ffprobe_path: str,
        input_file: str,
        partial_file: str,
        final_file: str,
        settings: Dict[str, Any],
        duration_sec: float
    ):
        super().__init__()
        self.item_id = item_id
        self.ffmpeg_path = ffmpeg_path
        self.ffprobe_path = ffprobe_path
        self.input_file = input_file
        self.partial_file = partial_file
        self.final_file = final_file
        self.settings = settings
        self.duration_sec = duration_sec

        self._is_cancelled = False
        self._has_video_cached: Optional[bool] = None
        self._has_audio_cached: Optional[bool] = None
        self._source_info_cached: Optional[dict] = None
        self._process: Optional[subprocess.Popen] = None
        self.console_log = deque(maxlen=_CONSOLE_LOG_MAX)
        self.cmd_str = ""

    def _has_audio(self) -> bool:
        """Does the source carry an audio stream? Probed once, cached alongside _has_video."""
        if self._has_audio_cached is None:
            try:
                info = self._source_info()
                streams = info.get("streams") if isinstance(info, dict) else None
                if not isinstance(streams, list):
                    self._has_audio_cached = True   # unreadable: let ffmpeg decide, as before
                else:
                    self._has_audio_cached = any(
                        st.get("codec_type") == "audio" for st in streams
                    )
            except Exception:
                self._has_audio_cached = True
        return self._has_audio_cached

    def _has_video(self) -> bool:
        """Does the SOURCE carry a real video track?

        build_ffmpeg_cmd has always taken has_video_stream, and nothing ever passed it -- so
        it sat at its default of True for every job, and the encoder was pointed at whatever
        `-map 0:v:0?` happened to select. For a music file with album art that is a single
        still flagged attached_pic, and encoding it as mpeg4 into an .mp4 fails outright.

        Probed once and cached: this runs per pass, and a two-pass job would otherwise pay
        for two identical ffprobe calls.
        """
        if self._has_video_cached is None:
            try:
                self._has_video_cached = has_real_video_stream(
                    self._source_info()
                )
            except Exception:
                self._has_video_cached = True   # unreadable: behave as before, let ffmpeg decide
            crash_log.breadcrumb(f"source has real video: {self._has_video_cached}")
        return self._has_video_cached

    def _source_info(self) -> dict:
        if self._source_info_cached is None:
            self._source_info_cached = get_media_info(self.ffprobe_path, self.input_file)
        return self._source_info_cached

    def cancel(self):
        """Requests worker cancellation and terminates running process."""
        self._is_cancelled = True
        if self._process and self._process.poll() is None:
            try:
                self._process.terminate()
                self._process.wait(timeout=2)
            except Exception:
                try:
                    self._process.kill()
                except Exception:
                    pass

    @crash_log.guard("ConversionWorker.run")
    def run(self):
        """Encode, in one or two passes, then validate and promote the result.

        NOTE the guard. This runs in a QThread, which PySide invokes from C++ -- so an
        exception escaping here reaches NEITHER sys.excepthook NOR threading.excepthook, and
        on a --windowed build that means it vanishes without a trace. That gap is why the
        first crash-logging attempt produced an empty log on the Windows 7 report.

        Two-pass runs ffmpeg twice over the same input: the first measures the material and
        writes a statistics log, the second encodes using it. That is worth having because
        these presets target a FIXED bitrate and a single pass must guess how to spend it
        while it is still reading the file -- measured on a test clip at a 500 kbps target,
        one pass landed at 689 kbps and two at 516. On a PS2 an overshoot is not merely a
        bigger file, it is dropped frames over USB or a network share.
        """
        two_pass = bool(self.settings.get("two_pass")) and bool(self.settings.get("vcodec"))

        # An audio-only preset on a source with no audio produces a command with NO output
        # streams at all -- `-vn` plus an optional `-map 0:a:0?` that matches nothing -- and
        # ffmpeg exits -22 with "at least one of its streams received no packets", which tells
        # the user nothing about what they actually did wrong. It is knowable before launching
        # anything, so say it plainly instead.
        if not self.settings.get("vcodec") and not self._has_audio():
            self.failed_signal.emit(
                self.item_id,
                "This file has no audio track, so an audio-only preset has nothing to "
                "convert. Choose a video preset instead."
            )
            return

        crash_log.breadcrumb(
            "worker.run: preset=%r vcodec=%r acodec=%r ext=%r two_pass=%s"
            % (self.settings.get("preset_name"), self.settings.get("vcodec"),
               self.settings.get("acodec"), Path(self.final_file).suffix, two_pass)
        )

        # The stats log sits beside the partial output, so it lands on the same volume and is
        # swept up by the same cleanup whatever happens.
        passlog = self.partial_file + ".passlog" if two_pass else None
        passes = [1, 2] if two_pass else [0]

        try:
            for idx, pass_num in enumerate(passes):
                outcome = self._run_pass(pass_num, passlog, idx, len(passes))
                if outcome == "cancelled":
                    self._cleanup_partial(passlog)
                    self.cancelled_signal.emit(self.item_id)
                    return
                if outcome != "ok":
                    return          # _run_pass already emitted the failure

            self._remove_passlog(passlog)

            # Cancel is honoured HERE too, not just inside the encode loop. _is_cancelled was
            # only ever tested between ffmpeg reads, so a press once the last pass had finished
            # did nothing at all: the file was validated, promoted to its final name, and the
            # rest of the batch carried on. Validation of a long file is not instant, so this
            # is a real window, and "Cancel" appearing to do nothing is worse than slow.
            if self._is_cancelled:
                self._cleanup_partial(passlog)
                self.cancelled_signal.emit(self.item_id)
                return

            # Validation stage
            crash_log.breadcrumb("encode finished; validating")
            self.progress_signal.emit(self.item_id, 100.0, f"Validating {Path(self.input_file).name}...")
            val_result = validate_converted_file(
                self.ffprobe_path,
                self.partial_file,
                self.settings.get("preset_name")
            )

            crash_log.breadcrumb(f"validation: {val_result.status}")

            # And again after validation, which is the other half of the same window: the
            # move is the point of no return, so a cancel that arrived during the probe must
            # still stop the file being promoted.
            if self._is_cancelled:
                self._cleanup_partial(passlog)
                self.cancelled_signal.emit(self.item_id)
                return

            if val_result.status in ("PASS", "WARN"):
                target_path = self.final_file
                if os.path.exists(target_path):
                    target_path, _ = generate_unique_output_path(
                        self.input_file,
                        Path(self.final_file).suffix,
                        str(Path(self.final_file).parent)
                    )
                crash_log.breadcrumb(f"moving partial -> {target_path}")
                shutil.move(self.partial_file, target_path)
                crash_log.breadcrumb("job complete")
                self.validation_signal.emit(self.item_id, val_result, target_path)
            else:
                self._cleanup_partial()
                self.validation_signal.emit(self.item_id, val_result, self.partial_file)

        except Exception as e:
            # WRITE THE TRACEBACK. The @crash_log.guard on run() cannot see anything this
            # blanket except catches -- which is everything realistic -- so a failure after
            # the encode (validation blowing up, a denied move, a full disk) produced a status
            # of "Failed", a one-line message, and NOTHING in the crash log. The trail simply
            # stopped at "encode finished; validating", making a death during validation, a
            # death during the move, and a clean success indistinguishable in the file testers
            # send back.
            crash_log.write_report(*sys.exc_info(), context="ConversionWorker.run")
            self._cleanup_partial(passlog)
            # Name the stage: str(e) alone does not say WHERE it died. PermissionError on a
            # move and a validator crash both arrive here reading like nothing in particular.
            self.failed_signal.emit(
                self.item_id, f"Failed after encoding ({type(e).__name__}): {e}"
            )

    def _run_pass(self, pass_num, passlog, pass_idx, total_passes):
        """Run one ffmpeg invocation. Returns "ok", "cancelled" or "failed".

        Progress is reported across the WHOLE job rather than per pass, so a two-pass encode
        moves 0->50% then 50->100% instead of filling the bar twice, which would read as the
        work having restarted.
        """
        cmd = build_ffmpeg_cmd(
            self.ffmpeg_path,
            self.input_file,
            self.partial_file,
            self.settings,
            has_video_stream=self._has_video(),
            pass_num=pass_num,
            passlog=passlog,
            source_info=self._source_info()
        )
        self.cmd_str = " ".join(f'"{arg}"' if " " in arg else arg for arg in cmd)
        label = f"pass {pass_num} of {total_passes}" if pass_num else "encode"
        crash_log.breadcrumb(f"ffmpeg cmd ({label}): {self.cmd_str}")
        self.log_signal.emit(self.item_id, f"Executing {label}:\n{self.cmd_str}\n")
        crash_log.breadcrumb("about to Popen ffmpeg")

        self._process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            # ffmpeg writes its output -- including any metadata it echoes from the source --
            # as UTF-8, but text=True decodes using the LOCALE codepage. On a Central European
            # Windows that is cp1250/cp1252, whose undefined byte slots (0x81, 0x8D, 0x8F,
            # 0x90, 0x9D) are perfectly ordinary UTF-8 continuation bytes: a file tagged with
            # "Ł" (U+0141 -> C5 81) would raise UnicodeDecodeError mid-read. Decoding as UTF-8
            # and replacing anything malformed means a stray byte can never take down a
            # conversion -- the log is for humans, and an unreadable character in it is not
            # worth failing over.
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
        )

        start_time = time.time()
        out_time_us = 0
        span = 100.0 / total_passes
        base = span * pass_idx
        crash_log.breadcrumb(f"ffmpeg started, pid={self._process.pid}; reading progress")
        _crumbed_first_line = False

        while True:
            if self._is_cancelled:
                break

            # Size safeguard, only meaningful while a real file is being written -- the
            # analysis pass sends its output to the null device.
            if pass_num != 1 and os.path.exists(self.partial_file):
                if os.path.getsize(self.partial_file) >= SIZE_4_0_GIB:
                    self.cancel()
                    try:
                        self._process.wait(timeout=5)
                    except Exception:
                        pass
                    self.failed_signal.emit(self.item_id, MSG_FILE_SIZE_APPROACHING_LIMIT)
                    self._cleanup_partial(passlog)
                    return "failed"

            line = self._process.stdout.readline()
            if not line:
                break

            if not _crumbed_first_line:
                # Proves ffmpeg launched AND produced output. If the trail stops between the
                # "ffmpeg started" crumb and this one, the problem is the launch itself; if it
                # stops just after, it is the first emit into the GUI thread.
                crash_log.breadcrumb("first ffmpeg output line received")
                _crumbed_first_line = True

            self.console_log.append(line)

            if "=" in line:
                key, _, val = line.strip().partition("=")
                key = key.strip()
                val = val.strip()

# Machine-readable progress is for the progress bar, not for the log pane. "-progress pipe:1"
# emits this block roughly once a second, so forwarding it was sending ~12 signals per second
# across threads and appending every one to a string on the GUI thread -- tens of thousands of
# lines over a feature-length encode, for text nobody reads. Real ffmpeg messages, which are
# the ones worth showing, still go through. Anything unrecognised is forwarded too, so a new
# ffmpeg version cannot silently swallow an error by adding a key.
                if key not in _PROGRESS_KEYS:
                    self.log_signal.emit(self.item_id, line.rstrip())

                if key == "out_time_us":
                    try:
                        out_time_us = int(val)
                    except ValueError:
                        pass
                elif key == "progress":
                    current_sec = out_time_us / 1_000_000.0
                    inner = 0.0
                    if self.duration_sec > 0:
                        inner = min(100.0, (current_sec / self.duration_sec) * 100.0)
                    pct = min(100.0, base + inner * span / 100.0)

                    elapsed = time.time() - start_time

                    remaining_msg = ""
                    if inner > 0:
                        # Estimate from THIS pass, then add the passes still to run -- a
                        # two-pass job must not advertise half the time it actually needs.
                        total_est = (elapsed / inner) * 100.0
                        rem_sec = int(total_est - elapsed) + int(total_est) * (total_passes - pass_idx - 1)
                        rem_min = rem_sec // 60
                        rem_s = rem_sec % 60
                        if rem_min > 0:
                            remaining_msg = f" \u2014 approximately {rem_min} min remaining"
                        else:
                            remaining_msg = f" \u2014 approximately {rem_s} sec remaining"

                    stage = f" [{label}]" if pass_num else ""
                    status_msg = f"Encoding {Path(self.input_file).name}{stage}{remaining_msg}"
                    self.progress_signal.emit(self.item_id, pct, status_msg)
            else:
                self.log_signal.emit(self.item_id, line.rstrip())

        if self._is_cancelled:
            try:
                self._process.wait(timeout=5)
            except Exception:
                pass
            return "cancelled"

        if self._process.stdout:
            self._process.stdout.close()
        return_code = self._process.wait()

        if return_code != 0:
            # console_log is a bounded deque, which cannot be sliced.
            #
            # Drop the machine-readable progress lines before taking the tail. "-progress
            # pipe:1" emits a ~12-line key=value block every second, so a raw last-20 was
            # usually two progress blocks and none of the actual error -- ffmpeg prints its
            # complaint and exits, and the surrounding spam pushed it out of the window.
            _err_lines = [
                l for l in self.console_log
                if "=" not in l or l.strip().partition("=")[0].strip() not in _PROGRESS_KEYS
            ]
            err_text = "".join(_err_lines[-25:])

            # AND put it in the breadcrumb trail, not only in the GUI. The trail is the file
            # testers actually send back, and it recorded the launch and then simply stopped
            # -- a failed conversion left no reason anywhere in it. A log that says a job
            # failed but not why costs a whole round-trip with someone else's hardware.
            crash_log.breadcrumb(f"ffmpeg exited {return_code} during {label}")
            for _l in _err_lines[-25:]:
                _l = _l.rstrip()
                if _l:
                    crash_log.breadcrumb(f"  ffmpeg: {_l}")

            self._cleanup_partial(passlog)
            self.failed_signal.emit(
                self.item_id,
                f"FFmpeg exited with code {return_code} during {label}:\n{err_text}"
            )
            return "failed"

        return "ok"

    def _remove_passlog(self, passlog):
        """Delete the two-pass statistics log(s).

        ffmpeg appends its own suffixes (-0.log, and -0.log.mbtree for some encoders), so
        remove by PREFIX rather than assuming one exact filename -- otherwise stray logs pile
        up next to the user's output.
        """
        if not passlog:
            return
        d = os.path.dirname(passlog) or "."
        base = os.path.basename(passlog)
        try:
            for n in os.listdir(d):
                if n.startswith(base):
                    try:
                        os.remove(os.path.join(d, n))
                    except Exception:
                        pass
        except Exception:
            pass

    def _cleanup_partial(self, passlog=None):
        self._remove_passlog(passlog)
        if os.path.exists(self.partial_file):
            try:
                os.remove(self.partial_file)
            except Exception:
                pass
