"""
Main Application Window for SMS Media Converter.
"""
from pathlib import Path
from typing import Dict, Optional, List
from qt_compat import Qt, QTimer, Signal, dialog_exec
from qt_compat import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QProgressBar, QPushButton, QMessageBox
)

import crash_log
from config import (
    PRESETS, SIZE_3_8_GIB, SIZE_4_0_GIB, MSG_FILE_SIZE_APPROACHING_LIMIT, Preset
)
from ffmpeg_utils import calculate_estimated_output_size, generate_unique_output_path
from validator import ValidationResult
from worker import InspectWorker, ConversionWorker

from ui.drop_zone import DropZoneWidget
from ui.preset_selector import PresetSelectorWidget
from ui.advanced_settings import AdvancedSettingsWidget
from ui.queue_table import QueueTableWidget, QueueItem
from ui.details_dialog import DetailsDialog

class MainWindow(QMainWindow):
    def __init__(self, ffmpeg_path: str, ffprobe_path: str):
        super().__init__()
        self.setWindowTitle("SMS Media Converter")
        self.resize(750, 680)

        self.ffmpeg_path = ffmpeg_path
        self.ffprobe_path = ffprobe_path

        # State management
        self.queue_order: List[str] = []
        self.current_worker: Optional[ConversionWorker] = None
        # Workers handed off but not yet destroyed. See _advance_queue_soon: dropping the last
        # reference to a QThread that has not finished aborts the process.
        self._retired_workers: List[ConversionWorker] = []
        self.inspect_workers: Dict[str, InspectWorker] = {}
        # Inspect workers awaiting destruction. Same hazard as _retired_workers above:
        # _on_inspect_finished used to pop the worker straight out of inspect_workers,
        # dropping the last reference to a QThread that had emitted its signal but not
        # yet FINISHED. Qt aborts the process for that (0xC0000409) with no Python
        # exception and no crash-log entry -- the 'app just vanished, empty log' report.
        # It needs a batch to show up: ~2 runs in 10 at 8 files, never at 1-3.
        self._retired_inspects: List[InspectWorker] = []
        self.is_converting = False

        # Main Widget & Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(16, 16, 16, 16)
        main_layout.setSpacing(12)

        # 0. Guidance Banner
        self.banner = QLabel("💡 Drop in any video or audio file and it comes out ready to play on a PlayStation 2 running SMS. The recommended settings are already chosen.")
        # Without word wrap this single long line sets the window's minimum WIDTH to its own
        # rendered width -- 820px measured -- so the resize(750, 680) below could never take
        # effect and the window could not be narrowed. Wrapping drops that floor to ~512.
        self.banner.setWordWrap(True)
        self.banner.setStyleSheet("""
            QLabel {
                background-color: #2B6CB0;
                color: #FFFFFF;
                font-weight: bold;
                font-size: 12px;
                padding: 8px 12px;
                border-radius: 6px;
            }
        """)
        main_layout.addWidget(self.banner)

        # 1. Drop Zone
        self.drop_zone = DropZoneWidget()
        self.drop_zone.files_dropped.connect(self._on_files_added)

        # Accept drops on the WHOLE window, not just the drop zone. Once the queue has files
        # the user's attention is on the table, and dropping there is the natural gesture --
        # it previously did nothing at all, silently, because drop_zone was the only widget
        # with setAcceptDrops. The affordance was obvious for the first file and misleading
        # for every one after it.
        self.setAcceptDrops(True)

        # 2. Preset Selector
        self.preset_selector = PresetSelectorWidget()
        self.preset_selector.preset_changed.connect(self._on_preset_changed)
        self.preset_selector.destination_changed.connect(self._on_destination_changed)

        # 3. Advanced Settings
        self.advanced_settings = AdvancedSettingsWidget()
        self.advanced_settings.settings_changed.connect(self._on_settings_changed)

        # 4. Queue Table
        self.queue_table = QueueTableWidget()
        self.queue_table.show_details_requested.connect(self._show_details)
        # The context menu asks; _remove_selected is the ONLY place that removes, because
        # it is the only code that maintains BOTH items_dict and queue_order.
        self.queue_table.remove_requested.connect(self._remove_requested)

        # 5. Bottom Status Bar & Controls
        self.lbl_estimated_size = QLabel("Estimated output: --")
        self.lbl_estimated_size.setStyleSheet("font-weight: bold; font-size: 13px; color: #E2E8F0;")

        self.lbl_size_warning = QLabel("")
        self.lbl_size_warning.setStyleSheet("font-size: 11px; font-weight: bold;")
        self.lbl_estimated_size.setToolTip("Estimated output file size calculated from target video and audio bitrates.")
        self.lbl_size_warning.hide()

        size_layout = QHBoxLayout()
        size_layout.addWidget(self.lbl_estimated_size)
        size_layout.addWidget(self.lbl_size_warning)
        size_layout.addStretch()

        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)
        self.progress_bar.setToolTip("Real-time encoding progress for the active media file.")
        self.progress_bar.setStyleSheet("""
            QProgressBar {
                border: 1px solid #4A5568;
                border-radius: 4px;
                background-color: #1A202C;
                text-align: center;
                color: white;
                font-weight: bold;
                height: 18px;
            }
            QProgressBar::chunk {
                background-color: #3182CE;
                border-radius: 3px;
            }
        """)

        self.lbl_status_msg = QLabel("Ready")
        self.lbl_status_msg.setStyleSheet("color: #A0AEC0; font-size: 12px;")

        # Action Buttons
        btn_layout = QHBoxLayout()

        self.btn_remove = QPushButton("Remove")
        self.btn_remove.setToolTip("Remove selected item(s) from the conversion queue.")
        self.btn_remove.clicked.connect(self._remove_selected)

        self.btn_clear = QPushButton("Clear")
        self.btn_clear.setToolTip("Clear all files from the queue.")
        self.btn_clear.clicked.connect(self._clear_queue)

        self.btn_details = QPushButton("Details")
        self.btn_details.setToolTip("Open conversion log, FFmpeg command details, and post-conversion SMS validation report.")
        self.btn_details.clicked.connect(self._show_selected_details)

        self.btn_cancel = QPushButton("Cancel")
        self.btn_cancel.setToolTip("Cancel the active encoding job.")
        self.btn_cancel.setEnabled(False)
        self.btn_cancel.clicked.connect(self._cancel_conversion)

        self.btn_convert = QPushButton("Convert")
        self.btn_convert.setToolTip("Start converting queued media files into SMS hardware-compatible format.")
        self.btn_convert.setStyleSheet("""
            QPushButton {
                background-color: #38A169;
                color: white;
                font-weight: bold;
                padding: 6px 20px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background-color: #2F855A;
            }
            QPushButton:disabled {
                background-color: #4A5568;
                color: #A0AEC0;
            }
        """)
        self.btn_convert.clicked.connect(self._start_conversion)

        btn_layout.addWidget(self.btn_remove)
        btn_layout.addWidget(self.btn_clear)
        btn_layout.addWidget(self.btn_details)
        btn_layout.addStretch()
        btn_layout.addWidget(self.btn_cancel)
        btn_layout.addWidget(self.btn_convert)

        # Assembly
        main_layout.addWidget(self.drop_zone)
        main_layout.addWidget(self.preset_selector)
        main_layout.addWidget(self.advanced_settings)
        main_layout.addWidget(self.queue_table, 1)

        # Status, progress and the size estimate share ONE row instead of three stacked
        # full-width rows. They are three short left-aligned strings that are idle most of the
        # time, and each was consuming a full row plus spacing -- roughly 90px of chrome that
        # the queue table (the only widget allowed to grow) was paying for. Merged, that space
        # goes to the file list, which is what the user is actually reading.
        status_row = QHBoxLayout()
        status_row.addLayout(size_layout)
        status_row.addWidget(self.progress_bar, 1)
        status_row.addWidget(self.lbl_status_msg)
        main_layout.addLayout(status_row)

        main_layout.addLayout(btn_layout)

        # Initialize defaults
        self._on_preset_changed(
            self.preset_selector.current_preset_name(),
            PRESETS.get(self.preset_selector.current_preset_name())
        )

    # --- Queue Management ---
    def closeEvent(self, event):
        """Shut the encoder down before the window goes.

        There was no closeEvent at all. Closing mid-encode returned cleanly from the event
        loop and then aborted in teardown with empty stderr, while the ffmpeg child SURVIVED
        its dead parent -- still writing a .partial that nothing was left to clean up. On a
        feature-length encode that is gigabytes of orphaned temp file and a CPU-bound process
        the user has no obvious way to find, let alone stop.

        Killing the child directly rather than via worker.cancel(): cancel() only sets a flag
        the run loop notices at its next readline, and a thread blocked on a pipe from a
        process nobody is reading may never get there.
        """
        if self.is_converting and self.current_worker is not None:
            answer = QMessageBox.question(
                self, "Conversion in progress",
                "A conversion is still running. Quit and discard it?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            if answer != QMessageBox.StandardButton.Yes:
                event.ignore()
                return

        crash_log.breadcrumb("window closing; shutting down workers")

        w = self.current_worker
        if w is not None:
            try:
                w.cancel()          # sets the flag AND terminates the child
            except Exception:
                pass
            w.wait(5000)
            # Only now is the file unlocked: on Windows the unlink inside the worker fails
            # while ffmpeg still holds the handle, and that failure is swallowed.
            try:
                w._cleanup_partial()
            except Exception:
                pass

        for lst in (self._retired_workers, self._retired_inspects):
            for other in lst:
                try:
                    other.wait(2000)
                except Exception:
                    pass
        for insp in list(self.inspect_workers.values()):
            try:
                insp.wait(2000)
            except Exception:
                pass

        event.accept()

    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
        else:
            event.ignore()

    def dropEvent(self, event):
        """Window-level drop. Reuses the same path as the drop zone so behaviour is identical."""
        import os as _os
        files = [
            u.toLocalFile() for u in event.mimeData().urls()
            if u.toLocalFile() and _os.path.isfile(u.toLocalFile())
        ]
        if files:
            self._on_files_added(files)
            event.acceptProposedAction()
        else:
            event.ignore()

    def _on_files_added(self, file_paths: List[str]):
        for fp in file_paths:
            item_id = str(Path(fp).resolve())
            if item_id in self.queue_table.items_dict:
                continue

            item = QueueItem(
                item_id=item_id,
                file_path=fp,
                file_name=Path(fp).name,
                status="Inspecting"
            )
            self.queue_table.add_queue_item(item)
            self.queue_order.append(item_id)

            # Spawn non-blocking background inspection
            inspect_worker = InspectWorker(self.ffprobe_path, fp)
            inspect_worker.finished_signal.connect(self._on_inspect_finished)
            self.inspect_workers[item_id] = inspect_worker
            inspect_worker.start()

    def _on_inspect_finished(self, file_path: str, summary: dict):
        item_id = str(Path(file_path).resolve())
        _w = self.inspect_workers.pop(item_id, None)
        if _w is not None:
            # Hold it until Qt is done with it; released by the drain below.
            self._retired_inspects.append(_w)
            QTimer.singleShot(0, self._drain_retired_inspects)

        duration = summary.get("duration", 0.0)
        size = summary.get("size", 0)
        v_width = summary.get("width", 0)
        v_height = summary.get("height", 0)

        # Calculate estimated size based on current settings
        settings = self.advanced_settings.get_settings_dict()
        est_bytes = self._estimate_bytes(duration, settings)

        # Keep the probed dimensions: build_ffmpeg_cmd's upscale clamp reads src_width /
        # src_height, and nothing in the app produced them.
        _item = self.queue_table.items_dict.get(item_id)
        if _item is not None:
            _item.src_width = int(v_width or 0)
            _item.src_height = int(v_height or 0)

        self.queue_table.update_item_metadata(item_id, duration, size, est_bytes)
        self.queue_table.update_item_status(item_id, "Ready")

        self._update_total_estimated_size()

    def _on_preset_changed(self, name: str, preset: Optional[Preset]):
        if preset:
            self.advanced_settings.load_preset(preset)
        self._on_settings_changed(self.advanced_settings.get_settings_dict())

    def _on_destination_changed(self, mode: str, path: str):
        pass

    def _on_settings_changed(self, settings: dict):
        preset_name = self.preset_selector.current_preset_name()
        preset = PRESETS.get(preset_name)
        if preset:
            self.preset_selector.set_badge_state(self._matches_preset(settings, preset))

        self._recalculate_estimates()

    def _recalculate_estimates(self):
        settings = self.advanced_settings.get_settings_dict()
        for item_id, item in self.queue_table.items_dict.items():
            if item.duration_sec > 0:
                item.estimated_size_bytes = self._estimate_bytes(item.duration_sec, settings)
        self._update_total_estimated_size()

    @staticmethod
    def _matches_preset(settings, preset) -> bool:
        """Is the current configuration still this preset's, in every respect that reaches ffmpeg?

        The old test looked at six controls out of nineteen, so thirteen of them could be
        changed with the badge still reading "Using the recommended settings" -- including the
        codecs. It also wrote `preset.width or settings.get("width")`, which compares a value
        to ITSELF whenever the preset declares None, so on an audio-only preset the badge could
        never go amber at all.

        Video fields are skipped for an audio-only preset: those controls are hidden and
        cannot affect the output, so counting them would report a difference the user has no
        way to see or undo.
        """
        fields = [("acodec", preset.acodec), ("abitrate_kbps", preset.abitrate_kbps),
                  ("sample_rate", preset.sample_rate), ("channels", preset.channels)]
        if preset.vcodec is not None:
            fields += [("vcodec", preset.vcodec), ("vtag", preset.vtag),
                       ("width", preset.width), ("height", preset.height),
                       ("vbitrate_kbps", preset.vbitrate_kbps)]
            if preset.fps is not None:
                fields.append(("fps", str(preset.fps)))

        for key, expected in fields:
            actual = settings.get(key)
            if key == "fps":
                actual = str(actual)
            if actual != expected:
                return False

        # Extras that are off in every preset; turning one on IS a custom setting.
        return (
            settings.get("bframes") == 0
            and not settings.get("qpel")
            and not settings.get("gmc")
            and not settings.get("deinterlace")
            and not settings.get("normalize_audio")
            and not settings.get("two_pass")
            and not settings.get("allow_upscale")
            and not settings.get("audio_track", 0)
            and (preset.vcodec is None or not settings.get("subtitle_file"))
        )

    def _estimate_bytes(self, duration_sec, settings):
        """Estimated output size for one file under `settings`.

        The video bitrate is only counted when a video codec is actually being ENCODED. Both
        call sites used to pass settings.get("vbitrate_kbps", 1500) unconditionally, so every
        audio-only preset was estimated as if it were also writing 1500 kbps of video --
        roughly 9x too large for a 128 kbps MP3, which then dragged the 4 GiB banner along
        with it.
        """
        vbitrate = settings.get("vbitrate_kbps", 1500) if settings.get("vcodec") else 0
        return calculate_estimated_output_size(
            duration_sec, vbitrate, settings.get("abitrate_kbps", 128)
        )

    def _update_total_estimated_size(self):
        total_bytes = sum(item.estimated_size_bytes for item in self.queue_table.items_dict.values())
        if total_bytes <= 0:
            self.lbl_estimated_size.setText("Estimated output: --")
            self.lbl_size_warning.hide()
            return

        # GiB/MiB: these are powers of 1024, and the 4 GiB ceiling they are compared against
        # is too. Labelling them GB/MB understated the number by ~7% against the limit the
        # user is being warned about.
        mib = total_bytes / (1024 * 1024)
        if mib >= 1024:
            size_str = f"{mib / 1024:.2f} GiB"
        else:
            size_str = f"{mib:.1f} MiB"

        self.lbl_estimated_size.setText(f"Estimated output: {size_str}")

        # The 4 GiB ceiling is a PER-FILE limit -- it is what SMS itself can open, and what
        # the worker's own size safeguard aborts a single encode on. Testing it against the
        # SUM of the queue meant five perfectly fine 1 GB files raised a red "BLOCKED"
        # banner, while one genuinely oversized file among small ones could be missed. And
        # "BLOCKED" was a lie in both directions: nothing consults this label, Convert stays
        # enabled, and the worker aborts on real size mid-encode regardless.
        largest = max(
            (item.estimated_size_bytes for item in self.queue_table.items_dict.values()),
            default=0,
        )
        if largest >= SIZE_4_0_GIB:
            self.lbl_size_warning.setText("[A file is projected over the 4.0 GiB limit and will be stopped]")
            self.lbl_size_warning.setStyleSheet("color: #E53E3E; font-weight: bold;")
            self.lbl_size_warning.show()
        elif largest >= SIZE_3_8_GIB:
            self.lbl_size_warning.setText("[WARNING: Projected file size approaching 4.0 GiB ceiling]")
            self.lbl_size_warning.setStyleSheet("color: #ED8936; font-weight: bold;")
            self.lbl_size_warning.show()
        else:
            self.lbl_size_warning.hide()

    # --- Batch Conversion Execution ---
    def _start_conversion(self):
        if self.is_converting:
            return

        # Check if queue has ready items
        # .get() rather than [] throughout: queue_order and items_dict are separate
        # structures, and a desync used to turn every later Convert press into a KeyError.
        # Single ownership is fixed above; this makes a future desync degrade instead.
        _items = self.queue_table.items_dict
        ready_ids = [i_id for i_id in self.queue_order
                     if _items.get(i_id) is not None
                     and _items[i_id].status in ("Ready", "Failed", "Cancelled")]
        if not ready_ids:
            QMessageBox.information(self, "No Files Ready", "There are no files in the queue ready for conversion.")
            return

        # Pressing Convert is what re-arms a previous failure. _process_next_in_queue only ever
        # advances through "Ready" items, so this is the single place a retry can be asked for,
        # and it takes a deliberate press rather than happening on its own.
        for i_id in ready_ids:
            if self.queue_table.items_dict[i_id].status in ("Failed", "Cancelled"):
                self.queue_table.update_item_status(i_id, "Ready")

        self.is_converting = True
        self.btn_convert.setEnabled(False)
        self.btn_cancel.setEnabled(True)
        self.btn_remove.setEnabled(False)
        self.btn_clear.setEnabled(False)

        self._process_next_in_queue()

    def _process_next_in_queue(self):
        # "Ready" ONLY, and that is load-bearing.
        #
        # This used to also pick up "Failed" and "Cancelled". Since a failure sets the item to
        # "Failed" and then calls straight back into here, the very same item was selected
        # again, converted again, failed again -- forever. Every lap started another QThread
        # and another ffmpeg, so the app spun up threads as fast as ffmpeg could fail until
        # the process died. No Python exception is raised on that path, which is exactly why
        # a tester reporting "the app just closes" got an empty crash log.
        #
        # A success never showed it: the item becomes "Passed", which this scan ignores, so it
        # returned here and stopped. That asymmetry is why the failure looked codec-specific --
        # whichever preset failed on the user's machine was the one that appeared to crash.
        #
        # Retrying is still available, but only when the USER asks: _start_conversion promotes
        # failed and cancelled items back to "Ready". An automatic retry of something that just
        # failed for a deterministic reason can only fail again.
        next_id = None
        for i_id in self.queue_order:
            _it = self.queue_table.items_dict.get(i_id)
            if _it is not None and _it.status == "Ready":
                next_id = i_id
                break

        if not next_id:
            # Queue Finished!
            self.is_converting = False
            self.btn_convert.setEnabled(True)
            self.btn_cancel.setEnabled(False)
            self.btn_remove.setEnabled(True)
            self.btn_clear.setEnabled(True)
            # Say what actually happened. This reported "Batch conversion completed." and
            # slammed the bar to 100% without ever looking at how anything ended -- so a batch
            # in which every single file failed announced success, which is the worst possible
            # thing for a tool whose failures are already hard to notice.
            # "Passed" and "Warning" are both outcomes where the file was produced and kept;
            # see _on_validation_finished. Anything else did not deliver a file.
            done = failed = cancelled = 0
            for i_id in self.queue_order:
                it = self.queue_table.items_dict.get(i_id)
                if it is None:
                    continue
                if it.status in ("Passed", "Warning"):
                    done += 1
                elif it.status == "Failed":
                    failed += 1
                elif it.status == "Cancelled":
                    cancelled += 1

            bits = []
            if done:
                bits.append(f"{done} converted")
            if failed:
                bits.append(f"{failed} failed")
            if cancelled:
                bits.append(f"{cancelled} cancelled")
            summary = "Batch finished: " + ", ".join(bits) + "." if bits else "Batch finished."
            self.lbl_status_msg.setText(summary)
            # Only claim a full bar when something actually got there.
            self.progress_bar.setValue(100 if done else 0)
            return

        item = self.queue_table.items_dict[next_id]
        self.queue_table.update_item_status(next_id, "Converting")

        crash_log.breadcrumb("Convert pressed; building settings")
        settings = self.advanced_settings.get_settings_dict()

        # Add preset name metadata to settings
        preset_name = self.preset_selector.current_preset_name()
        settings["preset_name"] = preset_name

        # The source size, so the "Allow upscaling" setting can actually apply. Without
        # these the clamp in build_scale_filter never runs and a 320x240 clip is blown up
        # to 640x480 -- bigger file, no added detail, and four times the macroblocks for
        # the PS2 to decode in software.
        settings["src_width"] = item.src_width
        settings["src_height"] = item.src_height

        # Determine extension
        ext = PRESETS[preset_name].ext if preset_name in PRESETS else ".avi"
        save_mode = self.preset_selector.get_save_mode()
        custom_dir = self.preset_selector.get_custom_dir() if save_mode == "custom" else None

        final_path, partial_path = generate_unique_output_path(
            item.file_path,
            ext,
            custom_dir
        )
        item.final_output_path = final_path

        self.current_worker = ConversionWorker(
            item_id=next_id,
            ffmpeg_path=self.ffmpeg_path,
            ffprobe_path=self.ffprobe_path,
            input_file=item.file_path,
            partial_file=partial_path,
            final_file=final_path,
            settings=settings,
            duration_sec=item.duration_sec
        )
        self.current_worker.progress_signal.connect(self._on_worker_progress)
        self.current_worker.log_signal.connect(self._on_worker_log)
        self.current_worker.validation_signal.connect(self._on_worker_validation)
        self.current_worker.failed_signal.connect(self._on_worker_failed)
        self.current_worker.cancelled_signal.connect(self._on_worker_cancelled)

        crash_log.breadcrumb(f"starting worker thread for {Path(item.file_path).name}")
        self.current_worker.start()

    def _on_worker_progress(self, item_id: str, pct: float, status_msg: str):
        self.progress_bar.setValue(int(pct))
        self.lbl_status_msg.setText(status_msg)

    # Kept bounded. This appends to a str on the GUI thread once per forwarded line, and a str
    # += reallocates the whole buffer each time -- so an unbounded log is quadratic in both time
    # and memory over a long encode. The worker no longer forwards the per-second progress
    # block, which removes most of the traffic; this caps what is left. Only the tail is ever
    # shown, so trimming the front loses nothing a user would look for.
    _LOG_MAX_CHARS = 64 * 1024

    def _on_worker_log(self, item_id: str, log_line: str):
        if item_id in self.queue_table.items_dict:
            item = self.queue_table.items_dict[item_id]
            item.console_log += log_line + "\n"
            if len(item.console_log) > self._LOG_MAX_CHARS:
                item.console_log = item.console_log[-self._LOG_MAX_CHARS:]

    def _on_worker_validation(self, item_id: str, result: ValidationResult, output_path: str):
        if item_id in self.queue_table.items_dict:
            item = self.queue_table.items_dict[item_id]
            item.validation_result = result
            if result.status == "PASS":
                self.queue_table.update_item_status(item_id, "Passed")
            elif result.status == "WARN":
                self.queue_table.update_item_status(item_id, "Warning")
            else:
                self.queue_table.update_item_status(item_id, "Failed")

        self._advance_queue_soon()

    def _on_worker_failed(self, item_id: str, err_msg: str):
        if item_id in self.queue_table.items_dict:
            item = self.queue_table.items_dict[item_id]
            item.console_log += f"\nFAILURE: {err_msg}\n"
            self.queue_table.update_item_status(item_id, "Failed")

        # Continue queue even if individual file failed
        self._advance_queue_soon()

    def _advance_queue_soon(self):
        """Move to the next item from the event loop, never from inside a worker's signal.

        Both callers are slots invoked by the worker that is finishing, and advancing the queue
        assigns a NEW ConversionWorker over self.current_worker -- which drops the last
        reference to the QThread whose signal is still on the stack. Destroying a QThread that
        has not finished is a hard abort in Qt, with no Python exception to catch: the process
        simply disappears, which is the symptom being chased here.

        Deferring with a zero timer lets the signal return and the thread finish first, and
        holding the outgoing worker in _retired_workers keeps it alive until Qt is genuinely
        done with it. This matters on the ORDINARY path too, not just after a failure -- a
        two-file batch hands over exactly the same way.
        """
        if self.current_worker is not None:
            self._retired_workers.append(self.current_worker)
            self.current_worker = None
        QTimer.singleShot(0, self._drain_retired_and_advance)

    def _drain_retired_inspects(self):
        """Release inspect threads that have genuinely finished; keep holding the rest."""
        still_running = []
        for w in self._retired_inspects:
            if w.isRunning():
                still_running.append(w)
            else:
                w.deleteLater()
        self._retired_inspects = still_running
        if still_running:
            QTimer.singleShot(16, self._drain_retired_inspects)

    def _drain_retired_and_advance(self):
        """Release finished workers, then start the next item."""
        still_running = []
        for w in self._retired_workers:
            if w.isRunning():
                still_running.append(w)
            else:
                w.deleteLater()
        self._retired_workers = still_running
        self._process_next_in_queue()

    def _on_worker_cancelled(self, item_id: str):
        if item_id in self.queue_table.items_dict:
            self.queue_table.update_item_status(item_id, "Cancelled")
        self.is_converting = False
        self.btn_convert.setEnabled(True)
        self.btn_cancel.setEnabled(False)
        self.btn_remove.setEnabled(True)
        self.btn_clear.setEnabled(True)
        self.lbl_status_msg.setText("Conversion cancelled by user.")

    def _cancel_conversion(self):
        if self.current_worker and self.current_worker.isRunning():
            self.current_worker.cancel()

    # --- UI Actions ---
    def _remove_requested(self, item_ids):
        """Removal asked for by the queue's context menu.

        Routed here rather than done in the widget so that queue_order and items_dict are
        only ever changed together. Removing while a batch is running would strand the
        running item, so it is refused for the same reason btn_remove is disabled then.
        """
        if self.is_converting:
            return
        self._remove_ids(item_ids)

    def _remove_selected(self):
        self._remove_ids(self.queue_table.get_selected_item_ids())

    def _remove_ids(self, item_ids):
        for i_id in list(item_ids):
            self.queue_table.remove_item(i_id)
            if i_id in self.queue_order:
                self.queue_order.remove(i_id)
        self._update_total_estimated_size()

    def _clear_queue(self):
        self.queue_table.clear_all()
        self.queue_order.clear()
        self._update_total_estimated_size()

    def _show_selected_details(self):
        selected_ids = self.queue_table.get_selected_item_ids()
        if selected_ids:
            self._show_details(selected_ids[0])

    def _show_details(self, item_id: str):
        if item_id in self.queue_table.items_dict:
            item = self.queue_table.items_dict[item_id]
            dlg = DetailsDialog(item, self)
            dialog_exec(dlg)
