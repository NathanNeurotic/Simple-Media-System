"""Per-frame decoder error series; NumPy accelerates long RGB captures.

Compares the same elementary stream's SMS capture and host RGB24 reference.
Metrics measure this controlled decoder path, not physical-console acceptance.
"""
import argparse
import json
from pathlib import Path


def compare(decoded, reference, width, height, frames, mode=32, crop_width=None, crop_height=None):
    import numpy as np
    crop_width = crop_width or width
    crop_height = crop_height or height
    if (width <= 0 or height <= 0 or width % 16 or height % 16 or frames <= 0
            or mode not in (16, 32) or not 0 < crop_width <= width or not 0 < crop_height <= height):
        raise ValueError("Use positive macroblock-aligned dimensions, frame count and in-bounds crop")
    decoded, reference = Path(decoded), Path(reference)
    bpp = mode // 8
    frame_bytes = width * height * bpp
    ref_bytes = crop_width * crop_height * 3
    if decoded.stat().st_size != frames * frame_bytes or reference.stat().st_size != frames * ref_bytes:
        raise ValueError("Capture/reference lengths do not match the requested frame layout")
    rows = []
    with decoded.open("rb") as native, reference.open("rb") as host:
        for index in range(frames):
            dtype = np.uint8 if mode == 32 else np.dtype("<u2")
            pixels = np.frombuffer(native.read(frame_bytes), dtype=dtype)
            if mode == 32:
                pixels = pixels.reshape(height // 16, width // 16, 16, 16, 4).transpose(0, 2, 1, 3, 4)
                pixels = pixels.reshape(height, width, 4)[:crop_height, :crop_width, :3]
            else:
                pixels = pixels.reshape(height // 16, width // 16, 16, 16).transpose(0, 2, 1, 3)
                pixels = pixels.reshape(height, width)[:crop_height, :crop_width]
                pixels = np.stack(((pixels & 31) << 3, ((pixels >> 5) & 31) << 3,
                                   ((pixels >> 10) & 31) << 3), axis=-1)
            expected = np.frombuffer(host.read(ref_bytes), dtype=np.uint8).reshape(crop_height, crop_width, 3)
            delta = pixels.astype(np.int16) - expected.astype(np.int16)
            absolute = np.abs(delta)
            rows.append({"frame": index,
                         "mean_absolute_channel_error": float(absolute.mean()),
                         "mean_absolute_rgb_error": [float(x) for x in absolute.mean(axis=(0, 1))],
                         "signed_rgb_bias": [float(x) for x in delta.mean(axis=(0, 1))],
                         "max_channel_error": int(absolute.max()),
                         "channels_with_error_over_32": int((absolute > 32).sum())})
    return {"mode": mode, "frames": frames, "width": width, "height": height,
            "mean_absolute_channel_error": sum(r["mean_absolute_channel_error"] for r in rows) / frames,
            "max_channel_error": max(r["max_channel_error"] for r in rows),
            "channels_with_error_over_32": sum(r["channels_with_error_over_32"] for r in rows),
            "per_frame": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decoded", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--mode", type=int, choices=(16, 32), default=32)
    parser.add_argument("--crop-width", type=int)
    parser.add_argument("--crop-height", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = compare(args.decoded, args.reference, args.width, args.height, args.frames,
                     args.mode, args.crop_width, args.crop_height)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "per_frame"}, indent=2))


if __name__ == "__main__":
    main()
