"""Untile decoder output and compare to RGB24 from a host decode of the same ES."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("decoded", type=Path)
parser.add_argument("reference", type=Path)
parser.add_argument("--width", type=int, required=True, help="aligned decoder width")
parser.add_argument("--height", type=int, required=True, help="aligned decoder height")
parser.add_argument("--crop-width", type=int)
parser.add_argument("--crop-height", type=int)
parser.add_argument("--frames", type=int, default=5)
parser.add_argument("--mode", type=int, choices=(16, 32), default=32)
args = parser.parse_args()
width, height = args.width, args.height
crop_w, crop_h = args.crop_width or width, args.crop_height or height
if args.frames <= 0 or width <= 0 or height <= 0 or width % 16 or height % 16 or not (0 < crop_w <= width and 0 < crop_h <= height):
    parser.error("Use positive macroblock-aligned dimensions and an in-bounds crop")
raw, reference = args.decoded.read_bytes(), args.reference.read_bytes()
bpp = args.mode // 8
if len(raw) != width * height * args.frames * bpp or len(reference) != crop_w * crop_h * args.frames * 3:
    parser.error("File lengths do not match the specified frame layout")
total = maximum = large = 0
for frame in range(args.frames):
    for y in range(crop_h):
        for x in range(crop_w):
            block = (y // 16) * (width // 16) + x // 16
            pos = (frame * width * height + block * 256 + (y % 16) * 16 + x % 16) * bpp
            if args.mode == 16:
                value = raw[pos] | raw[pos + 1] << 8
                rgb = ((value & 31) << 3, ((value >> 5) & 31) << 3, ((value >> 10) & 31) << 3)
            else:
                rgb = raw[pos:pos + 3]
            ref = (frame * crop_w * crop_h + y * crop_w + x) * 3
            for actual, expected in zip(rgb, reference[ref:ref + 3]):
                error = abs(actual - expected)
                total += error
                maximum = max(maximum, error)
                large += error > 32
print(json.dumps({"mode": args.mode, "frames": args.frames,
                  "mean_absolute_channel_error": total / len(reference),
                  "max_channel_error": maximum, "channels_with_error_over_32": large}, indent=2))
