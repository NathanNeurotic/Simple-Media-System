"""
Dialog showing complete FFmpeg console log and post-conversion SMS validation report.
"""
from qt_compat import Qt
from qt_compat import (
    QDialog, QVBoxLayout, QTabWidget, QTextEdit, QHBoxLayout, QPushButton, QFileDialog, QLabel
)
from ui.queue_table import QueueItem

# On-hardware display hint.
#
# SMS applies NO aspect-ratio signalling to AVI or MP4: MPEG-4's m_SampleAspectRatio is parsed
# and never read (SMS-v1/src/SMS_MPEG4.c), libmpeg discards aspect_ratio_information, and the
# only widescreen flag in the tree is set by the MPEG-PS demuxer alone. So the console cannot
# know how a converted file is meant to be framed, and the picture can come up stretched,
# squashed or cropped through no fault of the conversion.
#
# The remedy already exists on the console: SQUARE cycles the pan/scan modes
# (SMS-v1/src/SMS_Player.c -- eight of them, 0..7, persisted in g_Config.m_PlayerFlags >> 28
# and applied via the IPU's ChangeMode). Because it is stored in the global config it is a
# console-wide setting, not a per-file one: set it once and it sticks across files and reboots.
#
# This is deliberately a quiet footer rather than a warning. It is not a defect report and it
# is not conditional on anything -- it is the one piece of playback knowledge a user cannot
# derive from the file, placed where someone looks AFTER something looks wrong.
SQUARE_HINT = (
    "On the PS2: if the picture looks stretched, squashed or cut off, press SQUARE during "
    "playback to cycle the display modes. It is a console-wide setting and it is remembered."
)

class DetailsDialog(QDialog):
    def __init__(self, item: QueueItem, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Details & Log — {item.file_name}")
        self.resize(700, 500)
        self.item = item

        layout = QVBoxLayout(self)

        tabs = QTabWidget()

        # Tab 1: Validation Report
        self.txt_val = QTextEdit()
        self.txt_val.setReadOnly(True)
        self.txt_val.setStyleSheet("font-family: monospace; background-color: #1A202C; color: #E2E8F0;")
        
        if item.validation_result:
            self.txt_val.setPlainText(item.validation_result.generate_report())
        else:
            self.txt_val.setPlainText("No validation report available for this item.")

        tabs.addTab(self.txt_val, "Validation Report")

        # Tab 2: Console Log & Command Line
        self.txt_log = QTextEdit()
        self.txt_log.setReadOnly(True)
        self.txt_log.setStyleSheet("font-family: monospace; background-color: #1A202C; color: #A0AEC0;")
        self.txt_log.setPlainText(item.console_log if item.console_log else "No conversion log available.")

        tabs.addTab(self.txt_log, "FFmpeg Command & Log")

        layout.addWidget(tabs)

        # Quiet footer, always present -- see SQUARE_HINT. It sits below the tabs rather than
        # inside the validation text so it is visible on BOTH tabs, and so it still shows for an
        # item that has no validation report yet (a failure, or a queue item never converted),
        # which is exactly when someone is most likely to be looking for it.
        self.lbl_hint = QLabel(SQUARE_HINT)
        self.lbl_hint.setWordWrap(True)
        self.lbl_hint.setStyleSheet("color: #718096; font-size: 11px; font-style: italic; padding: 2px;")
        layout.addWidget(self.lbl_hint)

        # Bottom Buttons
        btn_layout = QHBoxLayout()
        btn_export = QPushButton("Export Log…")
        btn_export.clicked.connect(self._export_log)

        btn_close = QPushButton("Close")
        btn_close.clicked.connect(self.accept)

        btn_layout.addWidget(btn_export)
        btn_layout.addStretch()
        btn_layout.addWidget(btn_close)

        layout.addLayout(btn_layout)

    def _export_log(self):
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Conversion Log",
            f"{self.item.file_name}_log.txt",
            "Text Files (*.txt);;All Files (*.*)"
        )
        if file_path:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(f"FILE: {self.item.file_path}\n")
                f.write(f"STATUS: {self.item.status}\n\n")
                f.write("=== VALIDATION REPORT ===\n")
                f.write(self.txt_val.toPlainText())
                f.write("\n\n=== CONSOLE LOG ===\n")
                f.write(self.txt_log.toPlainText())
                # The exported log is what gets pasted into a bug report, so it carries the
                # hint too -- otherwise the one thing that most often explains "it looks wrong
                # on my TV" is the one thing missing from the evidence.
                f.write("\n\n=== NOTE ===\n")
                f.write(SQUARE_HINT + "\n")
