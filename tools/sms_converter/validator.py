"""
Post-conversion inspection and packet scanning validator for SMS compliance.
"""
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from typing import List, Dict, Any, Tuple, Optional
from config import (
    MAX_WIDTH, MAX_HEIGHT, MAX_SMS_FILE_SIZE_BYTES, MAX_SMS_STREAMS,
    MAX_VIDEO_PACKET_SIZE, MAX_AUDIO_PACKET_SIZE, MPEG_ALLOWED_FPS_MAP,
    sms_supports
)
from ffmpeg_utils import get_media_info

@dataclass
class ValidationResult:
    status: str  # "PASS", "WARN", "FAIL"
    reasons: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    container: str = "Unknown"
    vcodec: str = "None"
    vtag: str = "N/A"
    resolution: str = "N/A"
    fps: str = "N/A"
    vbitrate_kbps: int = 0
    acodec: str = "None"
    sample_rate: int = 0
    channels: int = 0
    abitrate_kbps: int = 0
    streams_count: int = 0
    file_size_mib: float = 0.0
    max_video_packet: int = 0
    max_audio_packet: int = 0

    def generate_report(self) -> str:
        lines = []
        lines.append(f"SMS VALIDATION: {self.status}\n")
        if self.reasons:
            lines.append("FAILURE REASONS:")
            for r in self.reasons:
                lines.append(f"  • {r}")
            lines.append("")

        if self.warnings:
            lines.append("WARNINGS:")
            for w in self.warnings:
                lines.append(f"  • {w}")
            lines.append("")

        lines.append(f"Container:             {self.container}")
        lines.append(f"Video codec:           {self.vcodec}")
        lines.append(f"Video tag:             {self.vtag}")
        lines.append(f"Resolution:            {self.resolution}")
        lines.append(f"Frame rate:            {self.fps}")
        lines.append(f"Video bitrate:         {self.vbitrate_kbps:,} kbps" if self.vbitrate_kbps else "Video bitrate:         N/A")
        lines.append(f"Audio codec:           {self.acodec}")
        lines.append(f"Audio rate:            {self.sample_rate:,} Hz" if self.sample_rate else "Audio rate:            N/A")
        lines.append(f"Audio channels:        {self.channels}" if self.channels else "Audio channels:        N/A")
        lines.append(f"Audio bitrate:         {self.abitrate_kbps:,} kbps" if self.abitrate_kbps else "Audio bitrate:         N/A")
        lines.append(f"Streams:               {self.streams_count}")
        lines.append(f"File size:             {self.file_size_mib:.1f} MiB")
        lines.append(f"Largest video packet:  {self.max_video_packet:,} bytes" if self.max_video_packet else "Largest video packet:  N/A")
        lines.append(f"Largest audio packet:  {self.max_audio_packet:,} bytes" if self.max_audio_packet else "Largest audio packet:  N/A")

        return "\n".join(lines)

class PacketScanError(RuntimeError):
    """Packet inspection was incomplete; retain any packet limits already seen."""
    def __init__(self, message: str, max_video_packet: int = 0, max_audio_packet: int = 0):
        super().__init__(message)
        self.max_video_packet = max_video_packet
        self.max_audio_packet = max_audio_packet


def scan_packet_sizes(ffprobe_path: str, file_path: str) -> Tuple[int, int]:
    """Read only packet type/size records, keeping memory bounded for long media."""
    cmd = [
        ffprobe_path,
        "-v", "error",
        "-show_packets",
        "-show_entries", "packet=codec_type,size",
        "-of", "compact=p=0:nk=0",
        file_path
    ]
    max_video_packet = 0
    max_audio_packet = 0

    def incomplete(message):
        return PacketScanError(message, max_video_packet, max_audio_packet)

    try:
        # A pipe for stderr can deadlock if malformed media emits many errors.
        # Keep diagnostics on disk, then include only a bounded excerpt.
        with tempfile.TemporaryFile() as diagnostics:
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=diagnostics, text=True,
                encoding="utf-8", errors="replace",
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
            )
            try:
                while True:
                    line = proc.stdout.readline(4097)
                    if not line:
                        break
                    if len(line) > 4096:
                        raise incomplete("ffprobe returned an oversized packet record.")
                    if not line.strip():
                        continue
                    try:
                        packet = dict(part.split("=", 1) for part in line.strip().split("|") if part)
                        packet_type = packet["codec_type"]
                        size = int(packet["size"])
                        if size < 0:
                            raise ValueError("negative packet size")
                    except (KeyError, ValueError):
                        raise incomplete("ffprobe returned an invalid packet record.")
                    if packet_type == "video":
                        max_video_packet = max(max_video_packet, size)
                    elif packet_type == "audio":
                        max_audio_packet = max(max_audio_packet, size)

                return_code = proc.wait()
                diagnostics.seek(0)
                detail = diagnostics.read(2048).decode("utf-8", errors="replace").strip()
                if return_code:
                    raise incomplete(f"ffprobe exited with code {return_code}: {detail or 'no diagnostics'}")
                if detail:
                    raise incomplete(f"ffprobe reported errors: {detail}")
                if not (max_video_packet or max_audio_packet):
                    raise incomplete("ffprobe returned no audio/video packet payloads.")
            finally:
                proc.stdout.close()
                if proc.poll() is None:
                    proc.kill()
                proc.wait()
    except OSError as exc:
        raise incomplete(f"Could not complete ffprobe packet scan: {exc}") from exc

    return max_video_packet, max_audio_packet

def validate_converted_file(
    ffprobe_path: str,
    file_path: str,
    preset_name: Optional[str] = None
) -> ValidationResult:
    """Performs full post-conversion validation of the generated file."""
    result = ValidationResult(status="PASS")

    if not os.path.exists(file_path):
        result.status = "FAIL"
        result.reasons.append("Output file does not exist.")
        return result

    file_size_bytes = os.path.getsize(file_path)
    result.file_size_mib = file_size_bytes / (1024 * 1024)

    # 1. File Size check
    if file_size_bytes > MAX_SMS_FILE_SIZE_BYTES:
        result.status = "FAIL"
        result.reasons.append(
            f"File size ({file_size_bytes:,} bytes) exceeds the portable SMS seek limit "
            f"of {MAX_SMS_FILE_SIZE_BYTES:,} bytes (2 GiB minus 1 byte)."
        )
        return result

    probe_info = get_media_info(ffprobe_path, file_path)
    if "error" in probe_info:
        result.status = "FAIL"
        result.reasons.append(f"ffprobe failed: {probe_info['error']}")
        return result

    fmt = probe_info.get("format", {})
    streams = probe_info.get("streams", [])

    result.container = fmt.get("format_long_name", fmt.get("format_name", "Unknown")).split(",")[0].upper()

    # The TARGET container extension, for the per-container codec checks below. The file under
    # validation is still the encoder's ".partial", so that suffix has to come off first or
    # every extension lookup misses.
    _clean = file_path[:-8] if file_path.endswith(".partial") else file_path
    container_ext = os.path.splitext(_clean)[1].lower()
    result.streams_count = len(streams)

    if len(streams) > MAX_SMS_STREAMS:
        result.status = "FAIL"
        result.reasons.append(f"Stream count ({len(streams)}) exceeds the maximum SMS limit of {MAX_SMS_STREAMS}.")

    video_streams = [s for s in streams if s.get("codec_type") == "video"]
    audio_streams = [s for s in streams if s.get("codec_type") == "audio"]

    # Validate Video
    first_video_details = None
    for video_stream in video_streams:
        v_name = video_stream.get("codec_name", "unknown")
        result.vcodec = video_stream.get("codec_long_name", v_name)
        result.vtag = video_stream.get("codec_tag_string", "N/A")
        
        w = int(video_stream.get("width", 0))
        h = int(video_stream.get("height", 0))
        result.resolution = f"{w}×{h}"

        r_fps = video_stream.get("avg_frame_rate", "0/0")
        if r_fps == "0/0" or not r_fps:
            r_fps = video_stream.get("r_frame_rate", "0/0")
        result.fps = r_fps

        duration_sec = float(fmt.get("duration", 0.0))
        v_bitrate = int(video_stream.get("bit_rate", 0)) // 1000
        if v_bitrate <= 0 or v_bitrate > 50000:
            if duration_sec > 0:
                v_bitrate = int((file_size_bytes * 8) / (duration_sec * 1000))
            else:
                v_bitrate = int(fmt.get("bit_rate", 0)) // 1000
        result.vbitrate_kbps = v_bitrate

        # Unsupported video codecs
        if v_name in ("h264", "hevc", "av1", "vp9", "vp8"):
            result.status = "FAIL"
            result.reasons.append(f"Unsupported video codec: {v_name.upper()}. SMS requires MPEG-1, MPEG-2, or MPEG-4 Part 2.")
        elif not sms_supports(container_ext, "video", v_name):
            # CONTAINER-AWARE. The check above is a denylist, so anything not named in it
            # passed -- including combinations SMS genuinely cannot open, because the codec
            # table it consults belongs to the CONTAINER's reader. MPEG-2 video inside an AVI
            # is the clearest case: SMS_Codec.c's AVI FourCC table has no MPEG-1/MPEG-2 entry
            # at all, so the file is valid everywhere else and undecodable on the PS2.
            #
            # WARN, not FAIL, deliberately: a FAIL DELETES the output (see the worker's
            # cleanup), and this table is a statement about what SMS is known to read rather
            # than a proof of what it will refuse. Being wrong must not destroy the user's
            # conversion. Promote to FAIL only once it has been checked on hardware.
            if result.status != "FAIL":
                result.status = "WARN"
            result.warnings.append(
                f"{v_name.upper()} video inside {container_ext} is not in SMS's codec table "
                f"for that container and probably will not open on the PS2."
            )

        # Resolution limits
        if w > MAX_WIDTH or h > MAX_HEIGHT:
            result.status = "FAIL"
            result.reasons.append(f"Resolution {w}×{h} exceeds SMS maximum boundary of {MAX_WIDTH}×{MAX_HEIGHT}.")
        elif w > 640 or h > 480:
            if result.status != "FAIL":
                result.status = "WARN"
            result.warnings.append(f"Resolution {w}×{h} is above standard 640×480 and may cause performance drops on hardware.")

        # Frame rate checks
        fps_float = 0.0
        if "/" in r_fps:
            n, d = r_fps.split("/")
            if float(d) > 0:
                fps_float = float(n) / float(d)
            n, d = r_fps.split("/")
            if float(d) > 0:
                fps_float = float(n) / float(d)
        
        if v_name in ("mpeg1video", "mpeg2video"):
            valid_fps_values = [23.976, 24.0, 25.0, 29.97, 30.0, 50.0, 59.94, 60.0]
            if not any(abs(fps_float - target) < 0.05 for target in valid_fps_values):
                result.status = "FAIL"
                result.reasons.append(f"Invalid frame rate ({r_fps}) for {v_name.upper()}.")

        if fps_float > 30.0 and result.status != "FAIL":
            result.status = "WARN"
            result.warnings.append(f"High frame rate ({fps_float:.2f} fps) may impact smooth playback.")

        # Bitrate check
        if v_bitrate > 3000 and result.status != "FAIL":
            result.status = "WARN"
            result.warnings.append(f"High video bitrate ({v_bitrate} kbps) may strain PS2 USB/SMB read bandwidth.")
        if first_video_details is None:
            first_video_details = {key: getattr(result, key) for key in
                                   ("vcodec", "vtag", "resolution", "fps", "vbitrate_kbps")}

    if first_video_details:
        for key, value in first_video_details.items():
            setattr(result, key, value)

    # Validate Audio
    for audio_index, audio_stream in enumerate(audio_streams):
        a_name = audio_stream.get("codec_name", "unknown")
        if audio_index == 0:
            result.acodec = audio_stream.get("codec_long_name", a_name)
            result.sample_rate = int(audio_stream.get("sample_rate", 0))
            result.channels = int(audio_stream.get("channels", 0))
            result.abitrate_kbps = int(audio_stream.get("bit_rate", 0)) // 1000

        valid_audio = ("mp3", "mp2", "aac", "ac3", "vorbis", "flac", "wmav1", "wmav2", "pcm_s16le", "pcm_s16be")
        if a_name not in valid_audio:
            result.status = "FAIL"
            result.reasons.append(f"Unsupported audio codec: {a_name.upper()} (audio track {audio_index}).")
        elif not sms_supports(container_ext, "audio", a_name):
            # Same container-aware backstop, same WARN-not-FAIL reasoning. AAC in an AVI is
            # the common case: SMS's AVI wFormatTag table has no AAC entry.
            if result.status != "FAIL":
                result.status = "WARN"
            result.warnings.append(
                f"{a_name.upper()} audio inside {container_ext} is not in SMS's codec table "
                f"for that container and probably will not play on the PS2."
            )

    # Packet Scan
    try:
        max_v_pkt, max_a_pkt = scan_packet_sizes(ffprobe_path, file_path)
    except PacketScanError as exc:
        max_v_pkt, max_a_pkt = exc.max_video_packet, exc.max_audio_packet
        if result.status != "FAIL":
            result.status = "WARN"
        # An inspection failure does not prove the encoded file is unusable.
        # WARN preserves it in the worker; observed oversize packets still FAIL below.
        result.warnings.append(f"Packet-size validation incomplete: {exc}")
    else:
        if (video_streams and not max_v_pkt) or (audio_streams and not max_a_pkt):
            if result.status != "FAIL":
                result.status = "WARN"
            result.warnings.append("Packet-size validation incomplete: no payloads were found for an audio/video stream type.")
    result.max_video_packet = max_v_pkt
    result.max_audio_packet = max_a_pkt

    if max_v_pkt > MAX_VIDEO_PACKET_SIZE:
        result.status = "FAIL"
        result.reasons.append(f"Largest video packet ({max_v_pkt:,} bytes) exceeds maximum allowable 2 MiB ({MAX_VIDEO_PACKET_SIZE:,} bytes).")

    if max_a_pkt > MAX_AUDIO_PACKET_SIZE:
        result.status = "FAIL"
        result.reasons.append(f"Largest audio packet ({max_a_pkt:,} bytes) exceeds maximum allowable 512 KiB ({MAX_AUDIO_PACKET_SIZE:,} bytes).")

    return result
