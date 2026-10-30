"""
Configuration and constants for SMS Media Converter.
"""
from dataclasses import dataclass, field
from typing import Optional, List, Dict

# Settings-state badge.
#
# This deliberately does NOT claim anything about hardware compatibility. Every format this
# tool can output is already known-good for SMS -- that is precisely why the output list is
# restricted to these presets -- so a "works on real hardware" badge told the user nothing
# and merely implied the opposite might be possible.
#
# What IS worth surfacing is whether the user has edited away from the preset in Advanced
# Settings, because that is the only way to land on values the presets would not have chosen.
LABEL_PRESET_DEFAULTS = "✓ Using the recommended settings for this format"
LABEL_CUSTOM_SETTINGS = "✎ Custom settings — edited from the recommended preset"
DEFAULT_SCALING_MODE = "Letterbox to exact dimensions"

# Resolution Limits for SMS
MAX_WIDTH = 1024
MAX_HEIGHT = 1024
RECOMMENDED_MAX_WIDTH = 640
RECOMMENDED_MAX_HEIGHT = 480

WARNING_RESOLUTION_MSG = (
    "⚠️ Resolution exceeds standard 640×480. It satisfies SMS format rules but may drop frames on real PS2 hardware."
)

# Cross-device output limits. STIO uses the signed 32-bit fioLseek/fileXioLseek
# interface, including transports that internally support 64-bit file positions.
# Keep generated files seekable through that common path. This is independent
# of FAT32/exFAT filesystem limits and is not a claim about sequential reads.
MAX_SMS_FILE_SIZE_BYTES = (1 << 31) - 1
FILE_SIZE_WARNING_BYTES = int(1.9 * 1024 * 1024 * 1024)
# Stop an active encode early to leave room for buffered data/muxer finalization.
# Post-conversion validation separately enforces the exact hard ceiling above.
FILE_SIZE_ABORT_HEADROOM_BYTES = 10 * 1024 * 1024
FILE_SIZE_ABORT_BYTES = MAX_SMS_FILE_SIZE_BYTES - FILE_SIZE_ABORT_HEADROOM_BYTES

MSG_FILE_SIZE_APPROACHING_LIMIT = (
    "Conversion stopped because the output reached the stop threshold "
    "(10 MiB below the portable 2 GiB minus 1 byte seek limit). "
    "Reduce the bitrate, resolution, or duration."
)

# Packet Size Validation Thresholds (Bytes)
MAX_VIDEO_PACKET_SIZE = 2_097_088  # 2 MiB
MAX_AUDIO_PACKET_SIZE = 524_224   # 512 KiB

# Stream Limits
MAX_SMS_STREAMS = 8
# The player's stream walkers require a trailing NULL entry.
MAX_CONVERTER_STREAMS = MAX_SMS_STREAMS - 1
MULTI_AUDIO_CONTAINERS = {".avi", ".mpg", ".mpeg", ".mp4"}

# Allowed Frame Rates for MPEG-1 and MPEG-2
MPEG_ALLOWED_FPS_MAP: Dict[str, str] = {
    "23.976": "24000/1001",
    "24": "24/1",
    "25": "25/1",
    "29.97": "30000/1001",
    "30": "30/1",
    "50": "50/1",
    "59.94": "60000/1001",
    "60": "60/1"
}

@dataclass
class Preset:
    name: str
    vcodec: Optional[str]  # e.g., 'mpeg4', 'mpeg2video', 'mpeg1video', None for audio only
    vtag: Optional[str]    # e.g., 'XVID'
    width: Optional[int]
    height: Optional[int]
    fps: Optional[str]     # e.g., '30'
    vbitrate_kbps: Optional[int]
    acodec: str           # e.g., 'libmp3lame', 'mp2', 'aac'
    abitrate_kbps: int    # e.g., 128, 192
    sample_rate: int = 48000
    channels: int = 2
    ext: str = ".avi"
    description: str = ""
    gop: int = 50
    keep_sd_resolution: bool = False
    scaling_mode: str = DEFAULT_SCALING_MODE

# Shorter MPEG-1/2 prediction chains are a mitigation candidate for #20/#23.
# The presets refresh with an I-frame every 15 frames rather than the generic 50.
# Reporter playback of re-converted files is still needed; neither the specific
# reconstruction-drift mechanism nor a complete color fix is established.
MPEG_IPU_GOP = 15

PRESETS: Dict[str, Preset] = {
    "Xvid-Compatible AVI [RECOMMENDED - Best PS2 Performance]": Preset(
        name="Xvid-Compatible AVI [RECOMMENDED - Best PS2 Performance]",
        vcodec="mpeg4",
        vtag="XVID",
        width=640,
        height=480,
        fps="30",
        vbitrate_kbps=1500,
        acodec="libmp3lame",
        abitrate_kbps=128,
        sample_rate=48000,
        channels=2,
        ext=".avi",
        description="Start here. Best picture per megabyte, so files stay small and stream well over USB or a network share. Decoded in software, which the PS2 handles comfortably at 640x480."
    ),
    "MPEG-2 [High Compatibility - DVD Standard]": Preset(
        name="MPEG-2 [High Compatibility - DVD Standard]",
        vcodec="mpeg2video",
        vtag=None,
        width=640,
        height=480,
        fps="30",
        vbitrate_kbps=2500,
        acodec="mp2",
        abitrate_kbps=192,
        sample_rate=48000,
        channels=2,
        ext=".mpg",
        gop=MPEG_IPU_GOP,
        description="Use if the Xvid preset stutters. The PS2's IPU chip decodes MPEG-2 in hardware, so the CPU barely works and playback is the smoothest available. Trade-off: noticeably larger files for the same quality."
    ),
    "MPEG-1 [Maximum Compatibility - VCD Standard]": Preset(
        name="MPEG-1 [Maximum Compatibility - VCD Standard]",
        vcodec="mpeg1video",
        vtag=None,
        width=640,
        height=480,
        fps="30",
        vbitrate_kbps=1150,
        acodec="mp2",
        abitrate_kbps=192,
        sample_rate=48000,
        channels=2,
        ext=".mpg",
        gop=MPEG_IPU_GOP,
        description="The most forgiving option, also decoded by the IPU chip rather than the CPU. Picture quality tops out lower, but it is the lightest load of all - try it if everything else struggles."
    ),
    "MPEG-4 MP4 [RECOMMENDED for MP4 files]": Preset(
        name="MPEG-4 MP4 [RECOMMENDED for MP4 files]",
        vcodec="mpeg4",
        vtag=None,
        width=640,
        height=480,
        fps="30",
        vbitrate_kbps=1500,
        acodec="aac",
        abitrate_kbps=128,
        sample_rate=48000,
        channels=2,
        ext=".mp4",
        description="The same video as the Xvid preset, in a modern .mp4 container with AAC audio. Choose this if you want .mp4 files; choose Xvid AVI if you want the longest-proven path."
    ),
    "MPEG-4 MP4 [Small - 320x240 Low Bitrate]": Preset(
        name="MPEG-4 MP4 [Small - 320x240 Low Bitrate]",
        vcodec="mpeg4",
        vtag=None,
        width=320,
        height=240,
        fps="30",
        vbitrate_kbps=600,
        acodec="aac",
        abitrate_kbps=128,
        sample_rate=48000,
        channels=2,
        ext=".mp4",
        description="320x240 at a low bitrate - roughly a quarter the file size of the 640x480 presets. For memory cards, small USB drives, or when a full-size file will not play smoothly."
    ),
    "Xvid-Compatible AVI [High Quality - Fast Storage]": Preset(
        name="Xvid-Compatible AVI [High Quality - Fast Storage]",
        vcodec="mpeg4", vtag="XVID", width=640, height=480, fps="30",
        vbitrate_kbps=10000, acodec="pcm_s16le", abitrate_kbps=0,
        sample_rate=48000, channels=2, ext=".avi", gop=6,
        keep_sd_resolution=True,
        description="10 Mbps Xvid, uncompressed 16-bit stereo PCM at 48 kHz and keyframes every 6 frames. Keeps source SD resolution. Requires fast storage; smooth playback is content and transport dependent."
    ),
    # ----------------------------------------------------------------------------------
    # AUDIO-ONLY targets.
    #
    # Every container here is one SMS opens directly (verified against the PS2 source:
    # .mp3/.mpa/.mp2, .ogg, .wma, .m4a/.aac, .ac3 and .flac are all accepted by the
    # browser and have a decoder). Video is dropped with -vn, which also strips any
    # embedded cover art -- SMS would not show it and some muxers choke on it.
    #
    # Bitrates are higher than the video presets' audio tracks on purpose: with no video
    # competing for space there is no reason to economise, and these are the files people
    # listen to closely.
    # ----------------------------------------------------------------------------------
    "MP3 Audio [RECOMMENDED for music]": Preset(
        name="MP3 Audio [RECOMMENDED for music]",
        vcodec=None,
        vtag=None,
        width=None,
        height=None,
        fps=None,
        vbitrate_kbps=None,
        acodec="libmp3lame",
        abitrate_kbps=192,
        sample_rate=48000,
        channels=2,
        ext=".mp3",
        description="Start here for music. Plays everywhere, small files, and the format SMS handles most cheaply. 192 kbps is transparent for almost all listening."
    ),
    "FLAC Audio [Lossless - exact copy]": Preset(
        name="FLAC Audio [Lossless - exact copy]",
        vcodec=None,
        vtag=None,
        width=None,
        height=None,
        fps=None,
        vbitrate_kbps=None,
        acodec="flac",
        abitrate_kbps=0,
        # None = keep the source. This preset promises "keeps every bit of the original" and
        # was force-resampling to 48 kHz and upmixing to stereo, so a 44.1 kHz mono source
        # came out 48 kHz stereo -- not bit-identical, and LARGER than leaving it alone.
        # Neither is required by the target: SMS reads the rate from FLAC STREAMINFO and
        # accepts mono (SMS-v1/src/SMS_ContainerFLAC.c).
        sample_rate=None,
        channels=None,
        ext=".flac",
        description="Keeps every bit of the original - no quality is thrown away. Files are roughly 4x an MP3, so use it when you have the space and care about the difference."
    ),
    "Ogg Vorbis Audio [Open format]": Preset(
        name="Ogg Vorbis Audio [Open format]",
        vcodec=None,
        vtag=None,
        width=None,
        height=None,
        fps=None,
        vbitrate_kbps=None,
        acodec="libvorbis",
        abitrate_kbps=192,
        sample_rate=48000,
        channels=2,
        ext=".ogg",
        description="Slightly better quality than MP3 at the same size, and patent-free. Pick it if you prefer open formats; pick MP3 if you want the most universally playable file."
    ),
    "AAC Audio (.m4a)": Preset(
        name="AAC Audio (.m4a)",
        vcodec=None,
        vtag=None,
        width=None,
        height=None,
        fps=None,
        vbitrate_kbps=None,
        acodec="aac",
        abitrate_kbps=192,
        sample_rate=48000,
        channels=2,
        ext=".m4a",
        description="Music only, in the same family as the MP4 video presets. Good quality per megabyte; choose MP3 instead if the file also needs to play on older devices."
    ),
}

# Export target video codecs supported natively by SMS hardware decoder
VIDEO_CODECS_MAP = {
    "Xvid-compatible MPEG-4 [RECOMMENDED]": "mpeg4",
    "MPEG-4 Part 2": "mpeg4",
    "MPEG-2 Video (DVD Stream)": "mpeg2video",
    "MPEG-1 Video (VCD Stream)": "mpeg1video",
    "Microsoft MPEG-4 v3": "msmpeg4v3",
}

# Export target audio codecs supported natively by SMS hardware decoder
AUDIO_CODECS_MAP = {
    "MP3 [RECOMMENDED for AVI]": "libmp3lame",
    "AAC-LC [RECOMMENDED for MP4]": "aac",
    "MP2 [RECOMMENDED for MPEG-1/2]": "mp2",
    "AC-3 / Dolby Digital": "ac3",
    "FLAC [lossless]": "flac",
    "Ogg Vorbis": "libvorbis",
    "PCM 16-bit [uncompressed for AVI]": "pcm_s16le",
}

# WHAT SMS CAN ACTUALLY DEMUX, PER CONTAINER.
#
# A codec is not enough on its own -- SMS looks the codec up in a table that belongs to the
# CONTAINER's reader, so a stream can be perfectly valid, play everywhere else, and still be
# undecodable on the PS2 because that container's table has no entry for it. Read straight
# out of the SMS sources, not assumed:
#
#   AVI      SMS-v1/src/SMS_Codec.c:33-54 (video FourCCs) and :58-65 (audio wFormatTag).
#            Video is MPEG-4 Part 2 and MS-MPEG4v3 ONLY -- there is no MPEG-1 or MPEG-2
#            FourCC in that table at all, and no AAC in the audio one.
#   MP4/M4A  SMS-v1/src/SMS_ContainerMOV.c:346-356, by MPEG-4 object type id.
#   MPEG-PS  SMS-v1/src/SMS_ContainerMPEG_PS.c:673-718, by stream id.
#
# Used two ways: Advanced Settings will not OFFER a combination that lands outside this, and
# the validator warns if one reaches an output anyway.
#
# Names are ffmpeg DECODER names as ffprobe reports them (codec_name), which is not always the
# encoder name -- libmp3lame encodes to "mp3", libvorbis to "vorbis".
SMS_CONTAINER_CODECS = {
    ".avi": {
        "video": {"mpeg4", "msmpeg4v3"},
        "audio": {"mp3", "mp2", "ac3", "dts", "vorbis", "wmav1", "wmav2", "pcm_s16le"},
    },
    ".mpg": {
        "video": {"mpeg1video", "mpeg2video"},
        "audio": {"mp2", "mp3", "ac3", "dts", "pcm_s16be"},
    },
    ".mpeg": {
        "video": {"mpeg1video", "mpeg2video"},
        "audio": {"mp2", "mp3", "ac3", "dts", "pcm_s16be"},
    },
    ".mp4": {
        "video": {"mpeg4", "mpeg1video", "mpeg2video"},
        "audio": {"aac", "mp3", "ac3", "vorbis"},
    },
    ".m4a": {
        "video": set(),
        "audio": {"aac", "mp3", "ac3", "vorbis"},
    },
    # Audio-only containers, each with its own reader in SMS.
    ".mp3":  {"video": set(), "audio": {"mp3"}},
    ".flac": {"video": set(), "audio": {"flac"}},
    ".ogg":  {"video": set(), "audio": {"vorbis"}},
    ".ac3":  {"video": set(), "audio": {"ac3"}},
}

# ffmpeg ENCODER name -> the decoder name ffprobe reports for its output.
ENCODER_TO_CODEC_NAME = {
    "libmp3lame": "mp3",
    "libvorbis": "vorbis",
    "libxvid": "mpeg4",
}

def codec_name_for(encoder: str) -> str:
    """Normalise an encoder name to the codec name ffprobe will report."""
    return ENCODER_TO_CODEC_NAME.get(encoder, encoder)

def sms_supports(ext: str, kind: str, encoder_or_codec: str) -> bool:
    """Can SMS decode `encoder_or_codec` as `kind` ("video"/"audio") inside `ext`?

    Unknown containers return True: this table is a statement about what SMS is KNOWN to
    read, and an unlisted extension means "no information", which must not be reported as a
    problem.
    """
    table = SMS_CONTAINER_CODECS.get(str(ext).lower())
    if table is None:
        return True
    return codec_name_for(encoder_or_codec) in table.get(kind, set())
