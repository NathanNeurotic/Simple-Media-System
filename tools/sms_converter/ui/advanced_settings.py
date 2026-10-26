"""
Collapsible advanced video and audio settings panel with hints and tooltips.
"""
from pathlib import Path
from typing import Dict, Any
from qt_compat import Qt, Signal
from qt_compat import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QGroupBox, QFormLayout,
    QComboBox, QSpinBox, QCheckBox, QLabel, QFrame, QScrollArea
)

from config import (
    VIDEO_CODECS_MAP, AUDIO_CODECS_MAP, MPEG_ALLOWED_FPS_MAP,
    MAX_WIDTH, MAX_HEIGHT, WARNING_RESOLUTION_MSG, Preset, sms_supports
)

class AdvancedSettingsWidget(QWidget):
    settings_changed = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.is_custom_mode = False

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 5, 0, 5)

        # 1. Toggle Button
        self.btn_toggle = QPushButton("[ Advanced Settings ▼ ]")
        self.btn_toggle.setCheckable(True)
        self.btn_toggle.setChecked(False)
        self.btn_toggle.setCursor(Qt.PointingHandCursor)
        self.btn_toggle.setToolTip("Click to expand or collapse fine-grained custom video and audio encoding options.")
        self.btn_toggle.setStyleSheet("""
            QPushButton {
                background: transparent;
                color: #63B3ED;
                border: none;
                text-align: left;
                font-weight: bold;
                padding: 4px 0px;
            }
            QPushButton:hover {
                color: #90CDF4;
            }
        """)
        self.btn_toggle.clicked.connect(self._toggle_collapse)

        # 2. Collapsible Container
        self.container = QFrame()
        self.container.setFrameShape(QFrame.StyledPanel)
        self.container.setStyleSheet("""
            QFrame {
                background-color: #2D3748;
                border-radius: 6px;
                border: 1px solid #4A5568;
            }
            QGroupBox {
                color: #E2E8F0;
                font-weight: bold;
                border: 1px solid #4A5568;
                border-radius: 4px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QLabel {
                color: #CBD5E0;
            }
        """)
        # visibility is owned by self.scroll ( created below ); the container itself stays shown

        container_layout = QHBoxLayout(self.container)

        # --- Video Group ---
        grp_video = QGroupBox("Video Settings")
        self.grp_video = grp_video
        v_layout = QFormLayout(grp_video)

        self.combo_vcodec = QComboBox()
        self.combo_vcodec.setToolTip(
            "Select output video codec supported natively by SMS on PS2:\n"
            "• Xvid-compatible MPEG-4 (Recommended): Best performance and compatibility.\n"
            "• MPEG-2 Video: Standard DVD video stream format.\n"
            "• MPEG-1 Video: VCD video format with minimal CPU load."
        )
        for k in VIDEO_CODECS_MAP.keys():
            self.combo_vcodec.addItem(k)
        self.combo_vcodec.currentIndexChanged.connect(self._on_control_changed)

        self.spin_width = QSpinBox()
        self.spin_width.setRange(2, MAX_WIDTH)
        self.spin_width.setSingleStep(2)
        self.spin_width.setValue(640)
        self.spin_width.setToolTip("Target width in pixels (max 1024). Standard PS2 resolution is 640. Automatically rounded to even numbers.")
        self.spin_width.valueChanged.connect(self._on_resolution_changed)

        self.spin_height = QSpinBox()
        self.spin_height.setRange(2, MAX_HEIGHT)
        self.spin_height.setSingleStep(2)
        self.spin_height.setValue(480)
        self.spin_height.setToolTip("Target height in pixels (max 1024). Standard PS2 resolution is 480. Automatically rounded to even numbers.")
        self.spin_height.valueChanged.connect(self._on_resolution_changed)

        res_layout = QHBoxLayout()
        res_layout.addWidget(self.spin_width)
        res_layout.addWidget(QLabel("×"))
        res_layout.addWidget(self.spin_height)

        self.chk_aspect = QCheckBox("Preserve aspect ratio (Recommended)")
        self.chk_aspect.setChecked(True)
        self.chk_aspect.setToolTip("Preserves original video proportions by applying black letterbox bars when scaling.")
        self.chk_aspect.stateChanged.connect(self._on_control_changed)

        self.combo_scaling = QComboBox()
        self.combo_scaling.setToolTip(
            "Choose scaling mode:\n"
            "• Letterbox to exact dimensions (Recommended): Pad black bars to fit target resolution.\n"
            "• Fit inside dimensions: Scale down without adding black bars.\n"
            "• Crop to fill: Scale up and crop edges to fill entire screen.\n"
            "• Stretch: Force stretch image to target resolution."
        )
        self.combo_scaling.addItems([
            "Letterbox to exact dimensions",
            "Fit inside dimensions",
            "Crop to fill",
            "Stretch"
        ])
        self.combo_scaling.currentIndexChanged.connect(self._on_control_changed)

        self.chk_upscale = QCheckBox("Allow upscaling (default off)")
        self.chk_upscale.setChecked(False)
        self.chk_upscale.setToolTip("Disabled by default. Upscaling low-res video increases file size without improving quality.")
        self.chk_upscale.stateChanged.connect(self._on_control_changed)

        self.combo_fps = QComboBox()
        self.combo_fps.setToolTip("Target video frame rate. 30 FPS or 24 FPS recommended for PS2 hardware. Restricted for MPEG-1/2.")
        for fps_val in MPEG_ALLOWED_FPS_MAP.keys():
            self.combo_fps.addItem(fps_val)
        self.combo_fps.setCurrentText("30")
        self.combo_fps.currentIndexChanged.connect(self._on_control_changed)

        self.spin_vbitrate = QSpinBox()
        self.spin_vbitrate.setRange(100, 10000)
        self.spin_vbitrate.setSingleStep(100)
        self.spin_vbitrate.setSuffix(" kbps")
        self.spin_vbitrate.setValue(1500)
        self.spin_vbitrate.setToolTip("Target video bitrate. 1500 kbps (1.5 Mbps) recommended for smooth USB 1.1 / SMB network playback.")
        self.spin_vbitrate.valueChanged.connect(self._on_control_changed)

        self.chk_deinterlace = QCheckBox("Deinterlace video")
        self.chk_deinterlace.setChecked(False)
        self.chk_deinterlace.setToolTip("Converts interlaced video to progressive frames. Recommended for TV rips or DVD source material.")
        self.chk_deinterlace.stateChanged.connect(self._on_control_changed)

        self.spin_gop = QSpinBox()
        self.spin_gop.setRange(1, 600)
        self.spin_gop.setValue(50)
        self.spin_gop.setSuffix(" frames")
        self.spin_gop.setToolTip(
            "Keyframe interval (GOP size): how often a complete, self-contained frame is "
            "written. Everything between keyframes is stored as differences.\n\n"
            "This is what decides how fast SEEKING feels on the PS2. Jumping to any point "
            "means decoding forward from the previous keyframe, so at 25 fps an interval of "
            "300 can mean chewing through 12 seconds of video before playback resumes -- on a "
            "294 MHz CPU that is a long stare at a frozen screen.\n\n"
            "Measured at a fixed 1000 kbps on footage with normal, smooth motion:\n"
            "     12 frames  ->  2.18 MB   SSIM 0.924    0.5 s seek\n"
            "     50 frames  ->  1.95 MB   SSIM 0.940    2 s seek\n"
            "    300 frames  ->  1.86 MB   SSIM 0.945   12 s seek\n\n"
            "Going from 12 to 50 is the big win: 11% smaller AND better quality, for 1.5 s of "
            "extra seek delay. Past that it flattens -- 300 saves only another 4% but costs ten "
            "more seconds every time you jump.\n\n"
            "50 sits at that knee. Raise it toward 300 if you only ever watch straight through "
            "and want the smallest files; lower it if you scrub around a lot."
        )
        self.spin_gop.valueChanged.connect(self._on_control_changed)

        self.spin_bframes = QSpinBox()
        self.spin_bframes.setRange(0, 4)
        self.spin_bframes.setValue(0)
        self.spin_bframes.setToolTip(
            "B-frames: extra frames predicted from BOTH the previous and the next frame. They "
            "shrink the file a little but cost decoding work and add latency.\n\n"
            "NOT the same thing as the keyframe interval -- that is the 'Keyframe interval' "
            "box above. B-frames is 0-4; the keyframe interval is in the hundreds.\n\n"
            "Recommended: 0. The PS2 decodes MPEG-4 in software, and B-frames are the first "
            "thing to cost it frames."
        )
        self.spin_bframes.valueChanged.connect(self._on_control_changed)

        self.chk_qpel = QCheckBox("QPel (Quarter-Pixel, default off)")
        self.chk_qpel.setChecked(False)
        self.chk_qpel.setToolTip("Quarter-Pixel motion estimation. Recommended: OFF for PS2 hardware decoding compatibility.")
        self.chk_qpel.stateChanged.connect(self._on_control_changed)

        # --- Multi-pass encoding -------------------------------------------------------
        self.chk_two_pass = QCheckBox("Multi-pass encoding (better quality, takes twice as long)")
        self.chk_two_pass.setChecked(False)
        self.chk_two_pass.setToolTip(
            "Encodes the video twice: the first run measures the material, the second uses "
            "what it learned to spend the bitrate where it is actually needed.\n\n"
            "Worth it because these presets aim at a FIXED bitrate. A single pass has to guess "
            "while it is still reading the file, so it commonly overshoots -- measured on a "
            "test clip asking for 500 kbps, one pass produced 689 and two produced 516.\n\n"
            "On a PS2 an overshoot is not just a larger file: it is dropped frames over USB or "
            "a network share. Cost is roughly double the conversion time."
        )
        self.chk_two_pass.stateChanged.connect(self._on_two_pass_toggled)

        self.spin_passes = QSpinBox()
        self.spin_passes.setRange(2, 2)
        self.spin_passes.setValue(2)
        self.spin_passes.setEnabled(False)
        self.spin_passes.setToolTip(
            "Number of passes.\n\n"
            "Fixed at 2 -- a limitation of FFMPEG, not of the codec.\n\n"
            "MPEG-4 Part 2 (DivX / Xvid) itself supports as many passes as you like, and the "
            "commercial DivX encoder does exactly that. But this app encodes through ffmpeg, "
            "and ffmpeg's pass 2 never writes back to the statistics log -- verified: the log "
            "is byte-for-byte identical before and after it runs. With nothing new written, a "
            "third pass has nothing to refine.\n\n"
            "Worse, ffmpeg treats -pass as a bitmask, so asking for 3 means pass 1 AND pass 2 "
            "together and the encoder falls back to first-pass behaviour. Measured at a 700 "
            "kbps target: pass 1 gave 5022 kbps, pass 2 gave 712, pass 3 gave 5022 again -- "
            "seven times over target, which on a PS2 means dropped frames.\n\n"
            "So 2 is what is achievable here, not what the format is capable of."
        )

        self.chk_gmc = QCheckBox("GMC (Global Motion Comp, default off)")
        self.chk_gmc.setChecked(False)
        self.chk_gmc.setToolTip("Global Motion Compensation. Recommended: OFF for PS2 hardware decoding compatibility.")
        self.chk_gmc.stateChanged.connect(self._on_control_changed)

        v_layout.addRow("Codec:", self.combo_vcodec)
        v_layout.addRow("Dimensions:", res_layout)
        v_layout.addRow("", self.chk_aspect)
        v_layout.addRow("Scaling Mode:", self.combo_scaling)
        v_layout.addRow("", self.chk_upscale)
        v_layout.addRow("Frame Rate:", self.combo_fps)
        v_layout.addRow("Video Bitrate:", self.spin_vbitrate)
        v_layout.addRow("", self.chk_deinterlace)
        v_layout.addRow("Keyframe interval:", self.spin_gop)
        v_layout.addRow("B-Frames (not keyframes):", self.spin_bframes)
        v_layout.addRow("", self.chk_two_pass)
        v_layout.addRow("Passes:", self.spin_passes)
        v_layout.addRow("", self.chk_qpel)
        v_layout.addRow("", self.chk_gmc)

        # --- Audio Group ---
        grp_audio = QGroupBox("Audio Settings")
        a_layout = QFormLayout(grp_audio)

        self.combo_acodec = QComboBox()
        self.combo_acodec.setToolTip(
            "Select output audio codec supported natively by SMS:\n"
            "• MP3 (Recommended for AVI): Broadest playback support.\n"
            "• AAC-LC (Recommended for MP4): High quality audio at lower bitrates.\n"
            "• MP2: Standard for MPEG-1/2 streams."
        )
        for k in AUDIO_CODECS_MAP.keys():
            self.combo_acodec.addItem(k)
        self.combo_acodec.currentIndexChanged.connect(self._on_control_changed)

        self.combo_abitrate = QComboBox()
        self.combo_abitrate.setToolTip("Target audio bitrate. 128 kbps recommended for MP3/AAC, 192 kbps for MP2.")
        for b in [64, 96, 128, 160, 192, 256, 320]:
            label = f"{b} kbps (Recommended)" if b == 128 else f"{b} kbps"
            self.combo_abitrate.addItem(label, b)
        # A target bitrate is meaningless for a lossless codec, which is why the FLAC preset
        # declares abitrate_kbps=0. Without an entry carrying that value it matched nothing,
        # so the combo kept whatever the previously viewed preset had left there and reported
        # it as FLAC's -- and build_ffmpeg_cmd skips -b:a for a falsy value anyway, so the
        # number shown had no effect on the output either.
        self.combo_abitrate.addItem("Lossless / encoder default", 0)
        self.combo_abitrate.setCurrentIndex(2) # 128 kbps
        self.combo_abitrate.currentIndexChanged.connect(self._on_control_changed)

        self.combo_sample = QComboBox()
        self.combo_sample.setToolTip("Audio sampling rate. 48000 Hz (48 kHz) is the hardware recommended baseline for PS2 audio output.")
        for sr in [48000, 44100, 32000, 22050]:
            label = f"{sr} Hz (Recommended)" if sr == 48000 else f"{sr} Hz"
            self.combo_sample.addItem(label, sr)
        # "Keep source" entries, carrying None. Without them a preset had no way to say
        # "do not touch this", and the FLAC preset -- described as "Keeps every bit of the
        # original" -- was forcing 48 kHz stereo, which is neither lossless nor required:
        # SMS reads the rate from FLAC STREAMINFO and accepts mono
        # (SMS-v1/src/SMS_ContainerFLAC.c). Resampling also made the file BIGGER.
        self.combo_sample.addItem("Keep source rate", None)
        self.combo_sample.setCurrentIndex(0) # 48000 Hz
        self.combo_sample.currentIndexChanged.connect(self._on_control_changed)

        self.combo_channels = QComboBox()
        self.combo_channels.setToolTip("Audio channel layout. Stereo (2 ch) recommended for PlayStation 2.")
        self.combo_channels.addItem("Stereo (2 ch) — Recommended", 2)
        self.combo_channels.addItem("Mono (1 ch)", 1)
        self.combo_channels.addItem("Keep source layout", None)
        self.combo_channels.currentIndexChanged.connect(self._on_control_changed)

        self.chk_normalize = QCheckBox("Normalize audio (Loudnorm)")
        self.chk_normalize.setChecked(False)
        self.chk_normalize.setToolTip("Applies EBU R128 audio normalization filter to equalize volume levels between quiet and loud scenes.")
        self.chk_normalize.stateChanged.connect(self._on_control_changed)

        # Source audio track (#22): a file can carry several audio streams
        # (commentary, additional dubs). 0 is the first -- the long-standing
        # default -- so existing presets and queue items behave identically.
        self.spin_atrack = QSpinBox()
        self.spin_atrack.setRange(0, 7)
        self.spin_atrack.setValue(0)
        self.spin_atrack.setToolTip(
            "Which source audio track to keep: 0 = first (default), 1 = second, ...\n"
            "Use it for commentary tracks and additional dubs."
        )
        self.spin_atrack.valueChanged.connect(self._on_control_changed)

        # Subtitle burn-in (#22): the player has no soft-subtitle support, so
        # subtitles only exist as burned-in pixels. Optional; empty = none.
        self.subtitle_path = ""
        self.btn_subtitle = QPushButton("Subtitle file…")
        self.btn_subtitle.setToolTip(
            "Optional: burn a subtitle file into the video.\n"
            "The PS2 player cannot read soft subtitles, so this draws them over the frame."
        )
        self.btn_subtitle.clicked.connect(self._pick_subtitle)
        self.lbl_subtitle = QLabel("none")
        self.lbl_subtitle.setStyleSheet("color: #A0AEC0;")

        a_layout.addRow("Codec:", self.combo_acodec)
        a_layout.addRow("Bitrate:", self.combo_abitrate)
        a_layout.addRow("Sample Rate:", self.combo_sample)
        a_layout.addRow("Channels:", self.combo_channels)
        a_layout.addRow("Source audio track:", self.spin_atrack)
        sub_row = QHBoxLayout()
        sub_row.addWidget(self.btn_subtitle)
        sub_row.addWidget(self.lbl_subtitle, 1)
        a_layout.addRow("Subtitles:", sub_row)
        a_layout.addRow("", self.chk_normalize)

        container_layout.addWidget(grp_video)
        container_layout.addWidget(grp_audio)

        # Warning label for custom resolution
        self.label_res_warning = QLabel(WARNING_RESOLUTION_MSG)
        self.label_res_warning.setStyleSheet("color: #ED8936; font-size: 11px; font-style: italic;")
        self.label_res_warning.setWordWrap(True)
        self.label_res_warning.hide()

        main_layout.addWidget(self.btn_toggle)

        # The settings container goes inside a scroll area rather than straight into the
        # layout. Added directly, its full sizeHint height became part of the WINDOW's
        # minimum: expanded, the window could not be smaller than 958px tall, while a
        # 1366x768 laptop has roughly 728px of usable height -- so the Convert button sat
        # below the bottom of the screen and the window could not be shrunk to reach it,
        # because that was a floor and not a preference.
        # Wrapped, the panel keeps its natural size when there is room and scrolls when there
        # is not, which is the behaviour a settings panel should have had anyway.
        self.scroll = QScrollArea()
        self.scroll.setWidget(self.container)
        self.scroll.setWidgetResizable(True)
        self.scroll.setFrameShape(QFrame.NoFrame)
        self.scroll.setVisible(False)
        main_layout.addWidget(self.scroll)
        main_layout.addWidget(self.label_res_warning)

    @staticmethod
    def _select_by_data(combo, value) -> bool:
        """Select the entry whose userData equals `value`. True if one existed.

        Compares with `is` for None so that a genuine "keep source" entry is matched rather
        than colliding with 0, which is a real bitrate value elsewhere.
        """
        for i in range(combo.count()):
            data = combo.itemData(i)
            if (data is None and value is None) or (data is not None and data == value):
                combo.setCurrentIndex(i)
                return True
        return False

    def _restrict_codec_choices(self, ext: str):
        """Repopulate the codec combos with the entries valid for `ext`.

        Rebuilding rather than disabling: a disabled-but-visible entry still invites the
        question "why can't I pick that?", and the honest answer is that this container
        cannot carry it. An empty result would leave the user no choice at all, so the map
        is left untouched in that case and the validator remains the backstop.
        """
        for combo, kind, source in (
            (self.combo_vcodec, "video", VIDEO_CODECS_MAP),
            (self.combo_acodec, "audio", AUDIO_CODECS_MAP),
        ):
            allowed = [d for d, ff in source.items() if sms_supports(ext, kind, ff)]
            if not allowed:
                allowed = list(source.keys())
            current = combo.currentText()
            blocked = combo.blockSignals(True)
            combo.clear()
            for d in allowed:
                combo.addItem(d)
            if current in allowed:
                combo.setCurrentText(current)
            combo.blockSignals(blocked)

    def load_preset(self, preset: Preset):
        """Populates control values from preset data."""
        self._block_signals(True)
        # Audio-only is a property of the PRESET, so it is remembered as one. The combo box
        # below gets parked at index 0 purely to neutralise a hidden control, and index 0 is
        # the Xvid entry -- so reading the answer back off that widget said "mpeg4 + XVID" for
        # every audio preset. See get_settings_dict.
        self._audio_only = preset.vcodec is None

        # Offer only what SMS can DECODE in this preset's container. The container is
        # fixed by the preset (Preset.ext) while both codec combos used to be filled
        # from the entire map for every preset, and nothing reconciled the two -- so 20
        # of 54 preset x audio-codec pairs killed the job outright. On the MP3 preset
        # the audio group is the ONLY panel visible and 5 of its 6 choices failed.
        # A combination the target cannot play should not be selectable.
        self._restrict_codec_choices(preset.ext)

        if preset.vcodec is None:
            # Audio-only preset: hide the entire Video Settings group, not just neutralise the
            # codec combo. Eleven video controls that cannot affect the output were still
            # taking their full width and height, which is a quarter of the presets paying a
            # ~170px penalty for dead controls -- and that height is what pushes the expanded
            # window past the bottom of a 1366x768 screen.
            self.grp_video.setVisible(False)
            # Multi-pass analyses VIDEO; with no video track there is nothing for it to do.
            self.chk_two_pass.setChecked(False)
            self.combo_vcodec.setCurrentIndex(0)
        else:
            self.grp_video.setVisible(True)
            # TWO entries in VIDEO_CODECS_MAP map to "mpeg4" -- "Xvid-compatible MPEG-4
            # [RECOMMENDED]" and "MPEG-4 Part 2" -- so matching on vcodec ALONE always picked
            # the Xvid one, whichever preset was selected. That mattered because vtag is
            # recovered from this combo's TEXT in get_settings_dict, so every .mp4 preset
            # silently came back carrying vtag="XVID": the command became
            # `-c:v libxvid -vtag XVID ... -f mp4`, and the MP4 muxer has no tag for it
            # ("Could not find tag for codec mpeg4 in stream #0"). ffmpeg exited non-zero the
            # moment it wrote the header, so EVERY MP4 conversion failed, on every platform.
            # It also defeated resolve_video_encoder, whose whole point is that the .mp4
            # presets keep ffmpeg's own mpeg4 encoder rather than libxvid.
            #
            # So select on the pair (vcodec, vtag), which is what actually identifies a codec
            # choice here, and fall back to the first vcodec match only if no exact pair
            # exists.
            want_xvid = preset.vtag == "XVID"
            fallback = None
            for display_name, ff_name in VIDEO_CODECS_MAP.items():
                if ff_name != preset.vcodec:
                    continue
                if fallback is None:
                    fallback = display_name
                if ("Xvid-compatible" in display_name) == want_xvid:
                    fallback = display_name
                    break
            if fallback is not None:
                self.combo_vcodec.setCurrentText(fallback)

        if preset.width:
            self.spin_width.setValue(preset.width)
        if preset.height:
            self.spin_height.setValue(preset.height)
        if preset.fps:
            self.combo_fps.setCurrentText(str(preset.fps))
        if preset.vbitrate_kbps:
            self.spin_vbitrate.setValue(preset.vbitrate_kbps)

        for display_name, ff_name in AUDIO_CODECS_MAP.items():
            if ff_name == preset.acodec:
                self.combo_acodec.setCurrentText(display_name)
                break

        # Match abitrate. These "match or silently keep whatever was there" loops are the
        # display-string defect class in miniature: a preset value with no matching entry
        # leaves the PREVIOUS preset's choice in the widget, and get_settings_dict then
        # reports it as this preset's. FLAC declares abitrate_kbps=0, which matches nothing,
        # so its reported audio bitrate was whichever preset the user happened to view first.
        # Fall back to the first entry so the answer is at least deterministic.
        if not self._select_by_data(self.combo_abitrate, preset.abitrate_kbps):
            self.combo_abitrate.setCurrentIndex(0)

        # Sample rate and channels: None means "keep the source", which has its own entry.
        if not self._select_by_data(self.combo_sample, preset.sample_rate):
            self.combo_sample.setCurrentIndex(0)
        if not self._select_by_data(self.combo_channels, preset.channels):
            self.combo_channels.setCurrentIndex(0)

        self.spin_bframes.setValue(0)
        self.chk_qpel.setChecked(False)
        self.chk_gmc.setChecked(False)
        self.chk_deinterlace.setChecked(False)
        self.chk_normalize.setChecked(False)

        self._block_signals(False)
        self._check_resolution_warning()

    def get_settings_dict(self) -> Dict[str, Any]:
        # An audio-only preset has NO video codec, and the video combo is a hidden control
        # parked at index 0 -- which happens to be the Xvid entry. Reading it back as though
        # it meant something returned vcodec="mpeg4", vtag="XVID", so converting a VIDEO file
        # with the MP3 / FLAC / Ogg / AAC presets built `-c:v libxvid ... -f mp3` and ffmpeg
        # died: "Error submitting a packet to the muxer: Invalid argument", exit 127. All four
        # audio presets failed on any source that had a video track.
        #
        # build_ffmpeg_cmd already does the right thing with vcodec=None -- it emits -vn and
        # drops the video stream -- so reporting the preset's actual state is the whole fix.
        # Same root cause as the vtag bug: a display widget is not where the model lives.
        if getattr(self, "_audio_only", False):
            ff_vcodec = None
            vtag = None
        else:
            vcodec_display = self.combo_vcodec.currentText()
            ff_vcodec = VIDEO_CODECS_MAP.get(vcodec_display, "mpeg4")
            vtag = "XVID" if "Xvid-compatible" in vcodec_display else None

        acodec_display = self.combo_acodec.currentText()
        ff_acodec = AUDIO_CODECS_MAP.get(acodec_display, "aac")

        w = self.spin_width.value()
        h = self.spin_height.value()
        if w % 2 != 0:
            w -= 1
        if h % 2 != 0:
            h -= 1

        return {
            "vcodec": ff_vcodec,
            "vtag": vtag,
            "width": w,
            "height": h,
            "preserve_aspect": self.chk_aspect.isChecked(),
            "scaling_mode": self.combo_scaling.currentText(),
            "allow_upscale": self.chk_upscale.isChecked(),
            "fps": self.combo_fps.currentText(),
            "vbitrate_kbps": self.spin_vbitrate.value(),
            "deinterlace": self.chk_deinterlace.isChecked(),
            "bframes": self.spin_bframes.value(),
            "gop": self.spin_gop.value(),
            "two_pass": self.chk_two_pass.isChecked(),
            "passes": self.spin_passes.value() if self.chk_two_pass.isChecked() else 1,
            "qpel": self.chk_qpel.isChecked(),
            "gmc": self.chk_gmc.isChecked(),
            "acodec": ff_acodec,
            "abitrate_kbps": self.combo_abitrate.currentData(),
            "sample_rate": self.combo_sample.currentData(),
            "channels": self.combo_channels.currentData(),
            "normalize_audio": self.chk_normalize.isChecked(),
            "audio_track": self.spin_atrack.value(),
            "subtitle_file": self.subtitle_path,
            "limit_streams": True
        }

    def _pick_subtitle(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Choose subtitle file", "",
            "Subtitle files (*.srt *.ass *.ssa *.vtt *.sub);;All files (*)"
        )
        if path:
            self.subtitle_path = path
            self.lbl_subtitle.setText(Path(path).name)
            self._on_control_changed()

    def _on_two_pass_toggled(self):
        """The pass count is only meaningful when multi-pass is on."""
        self.spin_passes.setEnabled(self.chk_two_pass.isChecked())
        self._on_control_changed()

    def _toggle_collapse(self):
        # Toggle the SCROLL AREA, not the container: the container now lives inside it, so
        # hiding the container alone would leave an empty scroll frame occupying the space.
        if self.btn_toggle.isChecked():
            self.btn_toggle.setText("[ Advanced Settings ▲ ]")
            self.scroll.show()
        else:
            self.btn_toggle.setText("[ Advanced Settings ▼ ]")
            self.scroll.hide()

    def _on_resolution_changed(self):
        w = self.spin_width.value()
        h = self.spin_height.value()
        if w % 2 != 0:
            self.spin_width.setValue(w - 1)
        if h % 2 != 0:
            self.spin_height.setValue(h - 1)
        self._check_resolution_warning()
        self._on_control_changed()

    def _check_resolution_warning(self):
        w = self.spin_width.value()
        h = self.spin_height.value()
        if w > 640 or h > 480:
            self.label_res_warning.show()
        else:
            self.label_res_warning.hide()

    def _on_control_changed(self):
        self.settings_changed.emit(self.get_settings_dict())

    def _block_signals(self, block: bool):
        self.combo_vcodec.blockSignals(block)
        self.spin_width.blockSignals(block)
        self.spin_height.blockSignals(block)
        self.chk_aspect.blockSignals(block)
        self.combo_scaling.blockSignals(block)
        self.chk_upscale.blockSignals(block)
        self.combo_fps.blockSignals(block)
        self.spin_vbitrate.blockSignals(block)
        self.chk_deinterlace.blockSignals(block)
        self.spin_bframes.blockSignals(block)
        self.chk_qpel.blockSignals(block)
        self.chk_gmc.blockSignals(block)
        self.combo_acodec.blockSignals(block)
        self.combo_abitrate.blockSignals(block)
        self.combo_sample.blockSignals(block)
        self.combo_channels.blockSignals(block)
        self.chk_normalize.blockSignals(block)
