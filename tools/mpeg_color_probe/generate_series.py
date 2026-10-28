"""Generate matched converter GOP controls and independent FFmpeg references.

Outputs only local research files; provide a source you have permission to use.
"""
import argparse
import dataclasses
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "sms_converter"))
from config import PRESETS
from ffmpeg_utils import build_ffmpeg_cmd, get_media_info


def generate(source, output, extended=False):
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("FFmpeg and FFprobe must be on PATH")
    source, output = Path(source).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    info = get_media_info(ffprobe, str(source))
    cases = []
    if extended:
        for codec, bframes in (("mpeg1video", 0), ("mpeg1video", 2), ("mpeg2video", 2)):
            cases.append((f"{codec}-b{bframes}", codec, 0, 50, bframes, False))
        cases.append(("mpeg2video-interlaced-b2", "mpeg2video", 0, 50, 2, True))
    else:
        for seek in (0, 30):
            for gop in (50, 15):
                cases.append((f"samsung-{seek}-g{gop}", "mpeg2video", seek, gop, 0, False))
    manifest = {"ffmpeg": subprocess.check_output([ffmpeg, "-version"], text=True).splitlines()[0],
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "frames": 150, "cases": []}
    for name, codec, seek, gop, bframes, interlaced in cases:
        folder = output / name
        folder.mkdir(exist_ok=True)
        ps, es, ref = folder / "converted.mpg", folder / "probe.m2v", folder / "reference.rgb"
        if interlaced:
            command = [ffmpeg, "-y", "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=60",
                       "-vf", "tinterlace=interleave_top,setfield=tff", "-frames:v", "156", "-an",
                       "-c:v", codec, "-flags", "+ilme+ildct",
                       "-b:v", "2500k", "-bf", str(bframes), "-g", str(gop), str(ps)]
        else:
            preset = next(p for p in PRESETS.values() if p.vcodec == codec)
            settings = dataclasses.asdict(preset)
            settings.update(acodec=None, gop=gop, bframes=bframes, allow_upscale=True, limit_streams=True)
            command = build_ffmpeg_cmd(ffmpeg, str(source), str(ps), settings, source_info=info)
            if seek:
                at = command.index("-i")
                command[at:at] = ["-ss", str(seek)]
            command[-1:-1] = ["-frames:v", "156"]
        with (folder / "encode.log").open("wb") as log:
            subprocess.run(command, stdout=log, stderr=log, check=True)
        subprocess.run([ffmpeg, "-y", "-v", "error", "-i", str(ps), "-map", "0:v:0",
                        "-c:v", "copy", "-f", codec, str(es)], check=True)
        subprocess.run([ffmpeg, "-y", "-v", "error", "-i", str(es), "-frames:v", "150",
                        "-pix_fmt", "rgb24", "-f", "rawvideo", str(ref)], check=True)
        probe = json.loads(subprocess.check_output([ffprobe, "-v", "error", "-select_streams", "v:0",
                           "-show_streams", "-show_frames", "-show_entries",
                           "stream=width,height,field_order:frame=pict_type", "-of", "json", str(es)], text=True))
        stream = probe["streams"][0]
        width, height = stream["width"], stream["height"]
        if len(probe["frames"]) < 150 or ref.stat().st_size != width * height * 3 * 150:
            raise RuntimeError("Incomplete or unexpected reference layout")
        if es.stat().st_size > 8 * 1024 * 1024:
            raise RuntimeError("Input exceeds the raw decoder probe's memory bound")
        entry = {"name": name, "seek_seconds": seek, "gop": gop, "bframes": bframes,
                 "interlaced": interlaced, "field_order": stream.get("field_order"),
                 "width": width, "height": height, "aligned_width": (width + 15) // 16 * 16,
                 "aligned_height": (height + (31 if interlaced else 15)) // (32 if interlaced else 16) * (32 if interlaced else 16),
                 "command": command, "elementary_sha256": hashlib.sha256(es.read_bytes()).hexdigest(),
                 "elementary_bytes": es.stat().st_size, "reference_bytes": ref.stat().st_size,
                 "picture_types": [frame["pict_type"] for frame in probe["frames"][:150]]}
        manifest["cases"].append(entry)
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"{name}: {es.stat().st_size} bytes, 150 reference frames", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--extended", action="store_true", help="MPEG-1, B-frame and synthetic interlaced controls")
    args = parser.parse_args()
    generate(args.source, args.output, args.extended)
