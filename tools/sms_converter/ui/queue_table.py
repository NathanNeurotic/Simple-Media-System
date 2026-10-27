"""
Batch queue table view displaying items, duration, file sizes, and conversion statuses.
"""
from dataclasses import dataclass, field
from typing import Dict, Optional, List
from qt_compat import Qt, Signal, menu_exec
from qt_compat import (
    QTableWidget, QTableWidgetItem, QHeaderView, QAbstractItemView, QMenu
)
from validator import ValidationResult

@dataclass
class QueueItem:
    item_id: str
    file_path: str
    file_name: str
    duration_sec: float = 0.0
    size_bytes: int = 0
    # Source pixel dimensions from the ffprobe pass. They were being computed in
    # _on_inspect_finished and thrown away, which left build_ffmpeg_cmd's upscale clamp
    # with nothing to read and made the "Allow upscaling" checkbox inert.
    src_width: int = 0
    src_height: int = 0
    audio_track_count: int = 1
    status: str = "Ready"
    estimated_size_bytes: int = 0
    console_log: str = ""
    validation_result: Optional[ValidationResult] = None
    final_output_path: str = ""

class QueueTableWidget(QTableWidget):
    item_selected = Signal(str)           # item_id
    show_details_requested = Signal(str)  # item_id
    # Removal is REQUESTED, never performed here. Queue membership lives in two structures --
    # this widget's items_dict and MainWindow.queue_order -- and only MainWindow maintains
    # both. The context menu used to call self.remove_item directly, which popped items_dict
    # and left a dangling id in queue_order; the next Convert press then did
    # items_dict[stale_id] and raised KeyError, permanently, for the life of the queue.
    remove_requested = Signal(list)       # [item_id, ...]

    def __init__(self, parent=None):
        super().__init__(0, 4, parent)
        self.setHorizontalHeaderLabels(["File", "Duration", "Size", "Status"])
        self.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)

        self.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.setAlternatingRowColors(True)
        self.verticalHeader().hide()

        self.setStyleSheet("""
            QTableWidget {
                background-color: #1A202C;
                gridline-color: #2D3748;
                color: #E2E8F0;
                font-size: 13px;
                border: 1px solid #4A5568;
                border-radius: 4px;
            }
            QHeaderView::section {
                background-color: #2D3748;
                color: #A0AEC0;
                font-weight: bold;
                padding: 6px;
                border: none;
            }
            QTableWidget::item:selected {
                background-color: #3182CE;
                color: white;
            }
        """)

        self.items_dict: Dict[str, QueueItem] = {}

        self.doubleClicked.connect(self._on_double_click)
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self._show_context_menu)

    def add_queue_item(self, item: QueueItem):
        if item.item_id in self.items_dict:
            return

        self.items_dict[item.item_id] = item
        row = self.rowCount()
        self.insertRow(row)

        item_name = QTableWidgetItem(item.file_name)
        item_name.setData(Qt.ItemDataRole.UserRole, item.item_id)

        item_dur = QTableWidgetItem(self._format_duration(item.duration_sec))
        item_size = QTableWidgetItem(self._format_size(item.size_bytes))
        item_status = QTableWidgetItem(item.status)

        self._style_status_item(item_status, item.status)

        self.setItem(row, 0, item_name)
        self.setItem(row, 1, item_dur)
        self.setItem(row, 2, item_size)
        self.setItem(row, 3, item_status)

    def update_item_metadata(self, item_id: str, duration_sec: float, size_bytes: int, estimated_size: int):
        if item_id not in self.items_dict:
            return
        item = self.items_dict[item_id]
        item.duration_sec = duration_sec
        item.size_bytes = size_bytes
        item.estimated_size_bytes = estimated_size

        row = self._find_row(item_id)
        if row >= 0:
            self.item(row, 1).setText(self._format_duration(duration_sec))
            self.item(row, 2).setText(self._format_size(size_bytes))

    def update_item_status(self, item_id: str, status: str):
        if item_id not in self.items_dict:
            return
        item = self.items_dict[item_id]
        item.status = status

        row = self._find_row(item_id)
        if row >= 0:
            status_item = self.item(row, 3)
            status_item.setText(status)
            self._style_status_item(status_item, status)

    def get_selected_item_ids(self) -> List[str]:
        selected_rows = set(index.row() for index in self.selectedIndexes())
        ids = []
        for r in selected_rows:
            item_id = self.item(r, 0).data(Qt.ItemDataRole.UserRole)
            if item_id:
                ids.append(item_id)
        return ids

    def remove_item(self, item_id: str):
        row = self._find_row(item_id)
        if row >= 0:
            self.removeRow(row)
            self.items_dict.pop(item_id, None)

    def clear_all(self):
        self.setRowCount(0)
        self.items_dict.clear()

    def _find_row(self, item_id: str) -> int:
        for r in range(self.rowCount()):
            if self.item(r, 0).data(Qt.ItemDataRole.UserRole) == item_id:
                return r
        return -1

    def _format_duration(self, seconds: float) -> str:
        if seconds <= 0:
            return "--:--:--"
        total_sec = int(seconds)
        hours = total_sec // 3600
        minutes = (total_sec % 3600) // 60
        secs = total_sec % 60
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"

    def _format_size(self, size_bytes: int) -> str:
        if size_bytes <= 0:
            return "0 B"
        # Powers of 1024 -- label them as such. See the note in main_window.
        mib = size_bytes / (1024 * 1024)
        if mib >= 1024:
            return f"{mib / 1024:.2f} GiB"
        return f"{mib:.1f} MiB"

    def _style_status_item(self, item: QTableWidgetItem, status: str):
        if status in ("Passed", "Ready"):
            item.setForeground(Qt.GlobalColor.green)
        elif status in ("Warning", "Inspecting"):
            item.setForeground(Qt.GlobalColor.yellow)
        elif status in ("Converting", "Validating"):
            item.setForeground(Qt.GlobalColor.cyan)
        elif status in ("Failed", "Cancelled"):
            item.setForeground(Qt.GlobalColor.red)

    def _on_double_click(self, index):
        row = index.row()
        item_id = self.item(row, 0).data(Qt.ItemDataRole.UserRole)
        if item_id:
            self.show_details_requested.emit(item_id)

    def _show_context_menu(self, pos):
        item_ids = self.get_selected_item_ids()
        if not item_ids:
            return
        menu = QMenu(self)
        action_details = menu.addAction("View Details & Log")
        action_remove = menu.addAction("Remove Selected")
        
        chosen = menu_exec(menu, self.mapToGlobal(pos))
        if chosen == action_details and len(item_ids) == 1:
            self.show_details_requested.emit(item_ids[0])
        elif chosen == action_remove:
            self.remove_requested.emit(list(item_ids))
