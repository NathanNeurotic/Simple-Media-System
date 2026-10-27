"""
End-to-end integration test creating sample media and testing all 6 hardware-confirmed presets.
"""
import subprocess
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ffmpeg_utils import find_ffmpeg_binaries, build_ffmpeg_cmd, generate_unique_output_path
from validator import validate_converted_file
from config import PRESETS

def test_all_presets():
    ff, fp = find_ffmpeg_binaries()
    assert ff and fp, "FFmpeg binaries missing"

    test_dir = Path(__file__).resolve().parent / "test_tmp"
    test_dir.mkdir(exist_ok=True)

    input_file = str(test_dir / "test_input.mp4")
    
    # 1. Create a 3-second dummy MP4 video file
    gen_cmd = [
        ff, "-y",
        "-f", "lavfi", "-i", "testsrc=duration=3:size=640x480:rate=30",
        "-f", "lavfi", "-i", "sine=frequency=440:duration=3",
        "-c:v", "libx264", "-c:a", "aac",
        input_file
    ]
    print("[E2E] Generating test input media...")
    subprocess.run(gen_cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    for preset_name, preset in PRESETS.items():
        print(f"\n==========================================")
        print(f"[TEST PRESET] {preset_name}")
        print(f"==========================================")

        settings = {
            "vcodec": preset.vcodec,
            "vtag": preset.vtag,
            "width": preset.width,
            "height": preset.height,
            "fps": preset.fps,
            "vbitrate_kbps": preset.vbitrate_kbps,
            "acodec": preset.acodec,
            "abitrate_kbps": preset.abitrate_kbps,
            "sample_rate": preset.sample_rate,
            "channels": preset.channels,
            "scaling_mode": "Letterbox to exact dimensions",
            "limit_streams": True,
            "preset_name": preset.name,
            "gop": preset.gop, "keep_sd_resolution": preset.keep_sd_resolution
        }

        final_path, partial_path = generate_unique_output_path(input_file, preset.ext)

        conv_cmd = build_ffmpeg_cmd(ff, input_file, partial_path, settings, has_video_stream=(preset.vcodec is not None))
        subprocess.run(conv_cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        val_result = validate_converted_file(fp, partial_path, preset.name)
        print(val_result.generate_report())

        assert val_result.status in ("PASS", "WARN"), f"Preset '{preset_name}' failed validation: {val_result.reasons}"

        if os.path.exists(final_path):
            os.remove(final_path)
        os.rename(partial_path, final_path)
        os.remove(final_path)

    # Clean up test input file
    os.remove(input_file)
    test_dir.rmdir()

    # Counted, not hardcoded: the loop above already iterates PRESETS, so a literal here goes
    # stale the moment a preset is added and quietly under-reports what was actually tested.
    print(f"\n[E2E] ALL {len(PRESETS)} PRESETS CONVERTED AND VALIDATED SUCCESSFULLY!")

if __name__ == "__main__":
    test_all_presets()
