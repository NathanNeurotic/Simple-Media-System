"""
FFmpeg and ffprobe helper utilities.
"""
import os
import sys
import shutil
import json
import subprocess
from pathlib import Path
from typing import Optional, Tuple, Dict, Any, List
from config import DEFAULT_SCALING_MODE, MPEG_ALLOWED_FPS_MAP, MAX_CONVERTER_STREAMS, MULTI_AUDIO_CONTAINERS

def get_app_dir() -> Path:
    """Returns the base directory of the application."""
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent
    return Path(__file__).resolve().parent

def find_ffmpeg_binaries(custom_dir: Optional[str] = None) -> Tuple[Optional[str], Optional[str]]:
    """
    Search for ffmpeg and ffprobe binaries in:
    1. <application directory>/ffmpeg/bin/
    2. <application directory>/ffmpeg.exe
    3. Custom directory specified by user
    4. System PATH
    """
    app_dir = get_app_dir()
    exe_ext = ".exe" if sys.platform == "win32" else ""
    ffmpeg_name = f"ffmpeg{exe_ext}"
    ffprobe_name = f"ffprobe{exe_ext}"

    # Search candidates
    search_paths = [
        app_dir / "ffmpeg" / "bin",
        app_dir / "ffmpeg",
        app_dir,
    ]
    if custom_dir:
        custom_p = Path(custom_dir)
        search_paths.insert(0, custom_p / "bin" if (custom_p / "bin").exists() else custom_p)

    for p in search_paths:
        ff = p / ffmpeg_name
        fp = p / ffprobe_name
        if ff.exists() and fp.exists():
            return str(ff), str(fp)

    # Check system PATH
    ff_sys = shutil.which("ffmpeg")
    fp_sys = shutil.which("ffprobe")
    if ff_sys and fp_sys:
        return ff_sys, fp_sys

    return None, None

_LIBXVID_CACHE: Dict[str, bool] = {}


def resolve_video_encoder(ffmpeg_path: str, vcodec: str, vtag: str) -> str:
    """Pick the encoder to actually run for a requested codec.

    When the user asks for Xvid, use REAL Xvid (libxvid) if this ffmpeg has it, instead of
    ffmpeg's own built-in mpeg4. Both emit MPEG-4 Part 2 that SMS decodes.

    THE REASON IS ROBUSTNESS, NOT QUALITY. An earlier version of this comment claimed libxvid
    was measurably better (SSIM 0.982 vs 0.968); that came from a single synthetic testsrc2
    clip and does not generalise. Re-measured across four content types it is a coin flip --
    mpeg4 won two, libxvid won two, and the gaps were tiny (0.005-0.014 SSIM). libxvid is also
    consistently 1.4x-3.7x SLOWER.

    What decided it: ffmpeg's mpeg4 encoder can FAIL OUTRIGHT in two-pass mode on
    high-entropy material -- "Nothing was written into output file... Conversion failed", a
    zero-byte result -- where the same source single-passes fine and libxvid two-passes fine.
    Reproduced on a 640x480 noise-like clip at 1200 kbps. Since two-pass is offered in the UI,
    an encoder that can silently produce nothing is the worse trade, and paying encode time to
    avoid a failed conversion is the right way round for a tool whose job is to make files
    that work.

    The preset is called "Xvid-Compatible", so using real Xvid also makes the name honest.

    Only the AVI/XVID path is switched. The .mp4 presets keep ffmpeg's mpeg4 deliberately --
    MP4 video support in SMS is new and its sample-entry handling was only just fixed, so
    there is no reason to vary the encoder underneath it for a quality delta.

    Falls back silently when libxvid is absent: many ffmpeg builds ship without it, and that
    must degrade to a working encode rather than an error.
    """
    if vcodec != "mpeg4" or vtag != "XVID":
        return vcodec
    if ffmpeg_path not in _LIBXVID_CACHE:
        _LIBXVID_CACHE[ffmpeg_path] = check_libxvid_support(ffmpeg_path)
    return "libxvid" if _LIBXVID_CACHE[ffmpeg_path] else vcodec


def check_libxvid_support(ffmpeg_path: str) -> bool:
    """Checks if the given ffmpeg binary supports the libxvid encoder."""
    try:
        res = subprocess.run(
            [ffmpeg_path, "-encoders"],
            capture_output=True,
            text=True,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
        )
        return "libxvid" in res.stdout
    except Exception:
        return False

def get_media_info(ffprobe_path: str, file_path: str) -> Dict[str, Any]:
    """Retrieves media format and stream metadata using ffprobe JSON output."""
    cmd = [
        ffprobe_path,
        "-v", "quiet",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        file_path
    ]
    try:
        res = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
        )
        return json.loads(res.stdout)
    except Exception as e:
        return {"error": str(e)}

def has_real_video_stream(probe_info: Dict[str, Any]) -> bool:
    """True only if the source has a video stream that is actual VIDEO.

    Cover art in an audio file is carried as a video stream flagged
    disposition.attached_pic -- a single still. Treating it as video is what broke both MP4
    presets on any music file with album art: `-map 0:v:0?` selected the still, the encoder
    turned it into mpeg4, and the mp4 muxer refused the result --

        [mp4] Could not find tag for codec mpeg4 in stream #0
        [out#0/mp4] Could not write header (incorrect codec parameters ?)

    -- exit -22 and a zero-byte output, for a file the user thinks of as audio. Album art is
    the normal case for a music library, not an edge case.

    An unreadable probe returns True, which preserves the previous behaviour: the encoder
    then decides, rather than this silently stripping video from a real film.
    """
    if not probe_info or "error" in probe_info:
        return True
    streams = probe_info.get("streams")
    if not isinstance(streams, list):
        return True
    for st in streams:
        if st.get("codec_type") != "video":
            continue
        if not st.get("disposition", {}).get("attached_pic"):
            return True   # a genuine video track
    # No video streams at all, or nothing but cover art.
    return False

def parse_media_summary(probe_info: Dict[str, Any]) -> Dict[str, Any]:
    """Extracts duration, video specs, audio specs from ffprobe dictionary."""
    if "error" in probe_info:
        return {"error": probe_info["error"]}

    fmt = probe_info.get("format", {})
    streams = probe_info.get("streams", [])

    duration = float(fmt.get("duration", 0.0))
    size_bytes = int(fmt.get("size", 0))

    video_stream = next((s for s in streams if s.get("codec_type") == "video"), None)
    audio_stream = next((s for s in streams if s.get("codec_type") == "audio"), None)

    v_codec = video_stream.get("codec_name") if video_stream else None
    v_width = int(video_stream.get("width", 0)) if video_stream else 0
    v_height = int(video_stream.get("height", 0)) if video_stream else 0
    
    # Calculate FPS
    v_fps = 0.0
    if video_stream:
        r_fps = video_stream.get("r_frame_rate", "0/0")
        if "/" in r_fps:
            num, den = r_fps.split("/")
            if float(den) > 0:
                v_fps = float(num) / float(den)

    a_codec = audio_stream.get("codec_name") if audio_stream else None
    a_sample_rate = int(audio_stream.get("sample_rate", 0)) if audio_stream else 0
    a_channels = int(audio_stream.get("channels", 0)) if audio_stream else 0

    return {
        "duration": duration,
        "size": size_bytes,
        "video_codec": v_codec,
        "width": v_width,
        "height": v_height,
        "fps": v_fps,
        "audio_codec": a_codec,
        "sample_rate": a_sample_rate,
        "channels": a_channels,
        "num_streams": len(streams),
        "audio_tracks": sum(s.get("codec_type") == "audio" for s in streams)
    }

def calculate_estimated_output_size(duration_sec: float, vbitrate_kbps: float, abitrate_kbps: float) -> int:
    """Calculates estimated output size in bytes including minor container overhead (~1%)."""
    if duration_sec <= 0:
        return 0
    total_bitrate_bps = (vbitrate_kbps + abitrate_kbps) * 1000.0
    payload_bytes = (duration_sec * total_bitrate_bps) / 8.0
    return int(payload_bytes * 1.015)

def make_even(val: int) -> int:
    """Ensures integer dimension is an even number."""
    val = int(val)
    if val % 2 != 0:
        val -= 1
    return max(2, val)

def build_scale_filter(
    src_w: int,
    src_h: int,
    target_w: int,
    target_h: int,
    scaling_mode: str,
    allow_upscale: bool = False,
    deinterlace: bool = False
) -> List[str]:
    """Generates FFmpeg video filter (-vf) chain for scaling and aspect ratio handling."""
    filters = []

    if deinterlace:
        filters.append("yadif")

    target_w = make_even(target_w)
    target_h = make_even(target_h)

    if not allow_upscale and src_w > 0 and src_h > 0 and scaling_mode != "Fit inside dimensions":
        # Shrink the canvas proportionally: separate axis clamps would turn a
        # requested 4:3 crop/stretch into 16:9 on a small widescreen source.
        factor = min(1.0, src_w / target_w, src_h / target_h)
        target_w, target_h = make_even(target_w * factor), make_even(target_h * factor)

    def even(expression):
        return f"max(2,trunc(({expression})/2)*2)"

    # Use display aspect (dar), including anamorphic sample aspect, then emit
    # square pixels. scale's default SAR adjustment alone would undo Stretch,
    # and pixel-width/height ratios alone distort anamorphic sources.
    if scaling_mode == "Stretch":
        width, height = str(target_w), str(target_h)
        if not allow_upscale:
            factor = f"min(1,min(iw/{target_w},ih/{target_h}))"
            width, height = even(f"{target_w}*{factor}"), even(f"{target_h}*{factor}")
        filters.extend([f"scale=w='{width}':h='{height}'", "setsar=1"])
    elif scaling_mode in ("Fit inside dimensions", DEFAULT_SCALING_MODE):
        width = f"min({target_w},{target_h}*dar)"
        height = f"min({target_h},{target_w}/dar)"
        if not allow_upscale:
            width = f"min({width},min(iw,ih*dar))"
            height = f"min({height},min(ih,iw/dar))"
        filters.extend([f"scale=w='{even(width)}':h='{even(height)}'", "setsar=1"])
        if scaling_mode == DEFAULT_SCALING_MODE:
            filters.append(f"pad={target_w}:{target_h}:(ow-iw)/2:(oh-ih)/2:black")
    elif scaling_mode == "Crop to fill":
        # Crop first to avoid huge intermediate images for very wide/tall input.
        # SAR belongs in the crop ratio too; the final canvas uses square pixels.
        width = even(f"min(iw,ih*{target_w}/{target_h}/sar)")
        height = even(f"min(ih,iw*{target_h}/{target_w}*sar)")
        filters.append(f"crop=w='{width}':h='{height}'")
        width, height = str(target_w), str(target_h)
        if not allow_upscale:
            factor = f"min(1,min(iw/{target_w},ih/{target_h}))"
            width, height = even(f"{target_w}*{factor}"), even(f"{target_h}*{factor}")
        filters.extend([f"scale=w='{width}':h='{height}'", "setsar=1"])
    else:
        raise ValueError(f"Unknown scaling mode: {scaling_mode}")

    return filters

def build_ffmpeg_cmd(
    ffmpeg_path: str,
    input_file: str,
    output_file: str,
    settings: Dict[str, Any],
    has_video_stream: bool = True,
    pass_num: int = 0,
    passlog: str = None,
    source_info: Optional[Dict[str, Any]] = None
) -> List[str]:
    """Builds complete FFmpeg command line arguments list.

    pass_num / passlog drive TWO-PASS encoding. pass_num == 0 means single pass.

    Two-pass exists because these presets target a FIXED bitrate, and a single pass has to
    guess how to spend it while it is still reading the file. Measured on a test clip at a
    500 kbps target: one pass landed at 689 kbps, two passes at 516. On a PS2, overshooting
    the bitrate is not just a bigger file -- it is the difference between smooth playback and
    dropped frames over USB or a network share.

    NOTE the deliberate absence of a "3 or more" path, and note WHY -- the reason is ffmpeg,
    not the format. MPEG-4 Part 2 (DivX / Xvid) supports arbitrary multi-pass, and the
    commercial DivX encoder implements it. ffmpeg does not: its pass 2 never writes back to
    the statistics log (verified -- the log is byte-identical before and after), so a third
    pass has nothing new to work from. And because -pass is a bitmask, asking for 3 sets
    PASS1|PASS2 and the encoder reverts to first-pass behaviour: measured at a 700 kbps
    target, pass 1 gave 5022 kbps, pass 2 gave 712, pass 3 gave 5022 again. Exposing 3+ would
    hand the user a file seven times over target, so the UI caps at 2 and explains itself.
    """
    cmd = [ffmpeg_path, "-y", "-i", input_file]

    # The TARGET CONTAINER, worked out up front because the video section below needs it and
    # it used to be derived only further down, at the muxer selection. Taken from the output
    # filename with the encoder's ".partial" suffix stripped, which would otherwise defeat
    # any extension-based decision.
    _out_clean = output_file[:-8] if output_file.endswith(".partial") else output_file
    out_ext = Path(_out_clean).suffix.lower()

    vcodec = settings.get("vcodec")
    acodec = settings.get("acodec")

    # Track 0 stays optional for silent video. An explicit alternate track must
    # exist: FFmpeg reports a missing required map instead of producing mute video.
    if settings.get("limit_streams", True):
        audio_track = int(settings.get("audio_track", 0) or 0)
        if audio_track < 0:
            raise ValueError("Audio track index must be nonnegative")
        if settings.get("all_audio_tracks"):
            if out_ext not in MULTI_AUDIO_CONTAINERS or not vcodec or not has_video_stream:
                raise ValueError("Preserving multiple audio tracks requires a video source and an AVI, MPEG or MP4 target")
            streams = (source_info or {}).get("streams")
            if not isinstance(streams, list):
                raise ValueError("Cannot inspect source tracks to preserve all audio tracks")
            audio_streams = [s for s in streams if s.get("codec_type") == "audio"]
            if len(audio_streams) + int(has_video_stream) > MAX_CONVERTER_STREAMS:
                raise ValueError(f"Output exceeds the {MAX_CONVERTER_STREAMS}-stream SMS limit; select one audio track")
            audio_map = "0:a?"
        else:
            audio_map = f"0:a:{audio_track}" + ("?" if audio_track == 0 else "")
        cmd.extend(["-map", "0:v:0?", "-map", audio_map, "-sn", "-dn", "-map_metadata", "-1"])
        if settings.get("all_audio_tracks"):
            for i, stream in enumerate(audio_streams):
                for key in ("language", "title"):
                    value = stream.get("tags", {}).get(key)
                    if value:
                        cmd.extend([f"-metadata:s:a:{i}", f"{key}={value}"])
                        if key == "title" and out_ext == ".mp4":
                            cmd.extend([f"-metadata:s:a:{i}", f"handler_name={value}"])

    # Video Settings
    if vcodec and has_video_stream and vcodec != "none":
        # Real Xvid when available; see resolve_video_encoder.
        cmd.extend(["-c:v", resolve_video_encoder(ffmpeg_path, vcodec, settings.get("vtag"))])

        if pass_num and passlog:
            cmd.extend(["-pass", str(pass_num), "-passlogfile", passlog])

        # VTAG for XVID -- AVI ONLY.
        #
        # "XVID" is an AVI FourCC. The MP4 muxer has no mapping for it and ffmpeg aborts while
        # writing the header ("Could not find tag for codec mpeg4 in stream #0, codec not
        # currently supported in container"), so a stray vtag does not degrade the output, it
        # destroys the job. A UI bug fed exactly that combination to every .mp4 preset for as
        # long as the MP4 presets have existed.
        #
        # The container is the authority on which tags are legal, so the check lives here
        # rather than only at the place that got it wrong: no amount of preset or
        # advanced-settings confusion can put an AVI FourCC into an MP4 again.
        vtag = settings.get("vtag")
        if vtag and out_ext == ".avi":
            cmd.extend(["-vtag", vtag])

        # Video bitrate
        vbitrate = settings.get("vbitrate_kbps")
        if vbitrate:
            cmd.extend(["-b:v", f"{vbitrate}k"])

        # Scaling and Filters
        src_w = settings.get("src_width", 0)
        src_h = settings.get("src_height", 0)
        if (not src_w or not src_h) and source_info:
            video = next((s for s in source_info.get("streams") or []
                          if s.get("codec_type") == "video" and not s.get("disposition", {}).get("attached_pic")), {})
            src_w, src_h = int(video.get("width", 0)), int(video.get("height", 0))
        target_w = settings.get("width", 640)
        target_h = settings.get("height", 480)
        scaling_mode = settings.get("scaling_mode", DEFAULT_SCALING_MODE)
        allow_upscale = settings.get("allow_upscale", False)
        deinterlace = settings.get("deinterlace", False)
        if settings.get("keep_sd_resolution") and 0 < src_w <= 720 and 0 < src_h <= 576:
            # Preserve coded SD dimensions AND sample aspect (e.g. 720x576 PAL
            # widescreen). The selected scaling mode applies to larger sources.
            vf_chain = ["yadif"] if deinterlace else []
            vf_chain.append(f"scale={make_even(src_w)}:{make_even(src_h)}")
        else:
            vf_chain = build_scale_filter(
                src_w, src_h, target_w, target_h, scaling_mode, allow_upscale, deinterlace
            )

        # Optional permanent subtitle burn-in; SMS also reads external SRT/SUB.
        subtitle_file = (settings.get("subtitle_file") or "").strip()
        subtitle_track = int(settings.get("embedded_subtitle_track", -1))
        if subtitle_file and subtitle_track >= 0:
            raise ValueError("Select either an external or embedded subtitle track")
        if subtitle_track >= 0:
            streams = (source_info or {}).get("streams")
            subtitles = [s for s in streams or [] if s.get("codec_type") == "subtitle"]
            if subtitle_track >= len(subtitles):
                raise ValueError("The selected embedded subtitle track is absent")
            if subtitles[subtitle_track].get("codec_name") not in {"subrip", "ass", "ssa", "webvtt", "mov_text", "text", "microdvd"}:
                raise ValueError("Embedded subtitle burn-in requires a text track; bitmap subtitles are not supported")
            subtitle_file = input_file
        if subtitle_file:
            # Two parsers consume escapes: filter options, then filtergraph.
            esc = "".join("\\" + c if c in "\\':" else c for c in subtitle_file)
            esc = "".join("\\" + c if c in "\\'[],;" else c for c in esc)
            suffix = f":si={subtitle_track}" if subtitle_track >= 0 else ":charenc=UTF-8"
            vf_chain.append(f"subtitles=filename={esc}{suffix}")

        # Frame rate
        fps = str(settings.get("fps", ""))
        if fps in MPEG_ALLOWED_FPS_MAP:
            cmd.extend(["-r", MPEG_ALLOWED_FPS_MAP[fps]])
            vf_chain.append(f"fps=fps={MPEG_ALLOWED_FPS_MAP[fps]}")
        elif fps:
            cmd.extend(["-r", fps])
            vf_chain.append(f"fps=fps={fps}")

        # B-frames, QPel, GMC default off
        bframes = settings.get("bframes", 0)
        cmd.extend(["-bf", str(bframes)])

        # Keyframe interval (GOP). Distinct from B-frames, and routinely confused with it:
        # B-frames is 0-4 and controls prediction; this is in the tens/hundreds and controls
        # how often a self-contained frame is written -- which is what makes seeking on the
        # PS2 fast or slow, since a seek decodes forward from the previous keyframe.
        #
        # On real footage a LONGER interval is genuinely better for both size and quality
        # (measured at 1000 kbps on smooth motion: 12 -> 2.18 MB/SSIM 0.924, 300 -> 1.86 MB/
        # SSIM 0.945). An earlier note here claimed the opposite because it was measured only
        # on testsrc2, whose hard edges and constant motion are pathological for prediction.
        # The default of 50 is therefore a SEEK-LATENCY compromise, not a quality win: 12->50
        # buys 11% size for 1.5 s of seek, while 50->300 buys only 4% more for another 10 s.
        gop = settings.get("gop")
        if gop:
            cmd.extend(["-g", str(int(gop))])

        if settings.get("qpel", False):
            cmd.extend(["-qpel", "1"])
        
        if settings.get("gmc", False):
            cmd.extend(["-gmc", "1"])

        # Ensure pixel format is yuv420p for standard compatibility
        vf_chain.append("format=yuv420p")

        if vf_chain:
            cmd.extend(["-vf", ",".join(vf_chain)])

    else:
        cmd.append("-vn")

    # Audio Settings
    if acodec and acodec != "none":
        cmd.extend(["-c:a", acodec])

        abitrate = settings.get("abitrate_kbps")
        if abitrate and not acodec.startswith("pcm_"):
            cmd.extend(["-b:a", f"{abitrate}k"])

        sample_rate = settings.get("sample_rate", 48000)
        if sample_rate:
            cmd.extend(["-ar", str(sample_rate)])

        channels = settings.get("channels", 2)
        if channels:
            cmd.extend(["-ac", str(channels)])

        # Audio resampling parameters
        pass

        if settings.get("normalize_audio", False):
            cmd.extend(["-filter:a", "loudnorm"])
    else:
        cmd.append("-an")

    ext = out_ext   # computed at the top; the video section needs it too

    format_map = {
        ".avi": "avi",
        ".mpg": "mpeg",
        ".mpeg": "mpeg",
        ".mp4": "mp4",
        ".m4a": "mp4",
        # Audio-only targets. Each of these is a container SMS opens directly, so the muxer
        # is named explicitly rather than left to ffmpeg's extension guess -- the output path
        # carries a ".partial" suffix while encoding, which defeats that guess entirely.
        ".mp3": "mp3",
        ".flac": "flac",
        ".ogg": "ogg",
        ".ac3": "ac3",
    }
    fmt_flag = format_map.get(ext)
    # The analysis pass appends its own "-f" for the null device below, so emitting one here
    # too put a duplicate -f on the command line and left the two spellings free to disagree.
    if fmt_flag and pass_num != 1:
        cmd.extend(["-f", fmt_flag])

    # MP4/M4A: move the moov atom to the FRONT of the file.
    #
    # This is not a micro-optimisation on PS2 -- it is the difference between a file that
    # opens and one the user assumes has hung. ffmpeg's default writes moov (the index)
    # AFTER mdat (the data). SMS cannot seek past mdat to reach it: its skip helper is a
    # sequential READ loop through a 4 KiB buffer, so a 700 MB movie costs ~170,000 device
    # reads -- over USB 1.1 or SMB that is minutes of a seemingly frozen browser -- before
    # a single frame is parsed. With +faststart the index is read immediately and playback
    # starts at once.
    #
    # Cost: ffmpeg does a second pass at the end to relocate the atom, so encoding finishes
    # slightly slower. That trade is overwhelmingly worth it for the target hardware.
    #
    # Skipped on the ANALYSIS pass: that pass writes to the null device, and relocating an
    # atom requires seeking back into the output to rewrite it. Asking for it on a stream
    # that cannot be seeked is at best wasted work and at worst a muxer error, and the
    # analysis pass discards its output anyway -- only the statistics log survives it.
    if fmt_flag == "mp4" and pass_num != 1:
        cmd.extend(["-movflags", "+faststart"])

    # Progress output via pipe:1
    cmd.extend(["-progress", "pipe:1", "-nostats"])

    if pass_num == 1:
        # The analysis pass only produces the statistics log; its encoded output is thrown
        # away. Writing it to the null device instead of a real file avoids creating a
        # throwaway the user would see appear and vanish, and skipping audio (-an) saves
        # encoding a track that is discarded either way.
        # -f is mandatory here: with no filename to infer from, ffmpeg cannot pick a muxer.
        cmd.extend(["-an", "-f", fmt_flag or "avi", os.devnull])
    else:
        cmd.append(output_file)

    return cmd

def generate_unique_output_path(
    source_file: str,
    extension: str,
    custom_save_dir: Optional[str] = None
) -> Tuple[str, str]:
    """
    Generates non-overwriting destination path with SMS suffix and partial path.
    Example: Movie.mp4 -> Movie_SMS.avi, Movie_SMS_2.avi if Movie_SMS.avi exists.
    """
    source_p = Path(source_file)
    target_dir = Path(custom_save_dir) if custom_save_dir and Path(custom_save_dir).is_dir() else source_p.parent
    base_name = source_p.stem

    ext = extension if extension.startswith(".") else f".{extension}"
    candidate = target_dir / f"{base_name}_SMS{ext}"
    
    counter = 2
    while candidate.exists():
        candidate = target_dir / f"{base_name}_SMS_{counter}{ext}"
        counter += 1

    final_path = str(candidate)
    partial_path = f"{final_path}.partial"
    return final_path, partial_path
