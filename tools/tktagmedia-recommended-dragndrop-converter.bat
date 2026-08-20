@echo off
REM ============================================================
REM  PS2 SMS - MPEG2 + MP2 encoder (960x544, mod-16 safe)
REM  Two-pass encoding
REM ============================================================
REM  Usage:
REM    encode_ps2_mpeg2.bat "input.mkv"
REM    (or just drag-and-drop a file onto this .bat)
REM
REM  Why 960x544, not 960x540:
REM    MPEG2 (like MPEG4 ASP) codes video in 16x16 macroblocks.
REM    960 divides evenly by 16 (60 MBs) but 540 does not
REM    (540 / 16 = 33.75). Encoders/decoders that mishandle that
REM    non-mod-16 padding/crop are the likely cause of the
REM    chroma-misalignment you were seeing. 544 = 34 * 16 exactly,
REM    so both dimensions land on a clean macroblock boundary.
REM
REM    Rather than stretching to 544 (which would slightly alter
REM    aspect ratio), this script pads your original 540-line
REM    picture up to 544 with 2 black lines top/bottom and then
REM    forces the container's display aspect ratio to 16:9, so the
REM    picture itself is untouched and undistorted.
REM
REM  Why sc_threshold is set so high:
REM    ffmpeg's mpeg2video encoder does not support combining
REM    closed-GOP (+cgop) with scene-change detection. Setting
REM    sc_threshold to a huge value effectively disables scene-cut
REM    insertion, which is what ffmpeg itself suggests when this
REM    combination is used.
REM ============================================================

if "%~1"=="" (
    echo Usage: %~nx0 "input_file"
    echo   ^(or drag and drop a video file onto this script^)
    pause
    exit /b 1
)

set "INPUT=%~1"
set "OUTDIR=%~dp1"
set "OUTNAME=%~n1"
set "OUTPUT=%OUTDIR%%OUTNAME%_ps2.mpg"
set "PASSLOG=%OUTDIR%%OUTNAME%_2pass"

REM ---- Tunables -------------------------------------------------
set VIDEO_BITRATE=3000k
set MAXRATE=3400k
set BUFSIZE=1700k
set GOP_SIZE=15
set BFRAMES=2
set AUDIO_BITRATE=224k
set AUDIO_RATE=48000
set "VF=scale=960:540:flags=lanczos,pad=960:544:0:2:black,setsar=1"
REM ----------------------------------------------------------------

echo.
echo ==== Pass 1/2 ====
ffmpeg -y -i "%INPUT%" ^
  -vf "%VF%" ^
  -c:v mpeg2video ^
  -pix_fmt yuv420p ^
  -b:v %VIDEO_BITRATE% -maxrate %MAXRATE% -bufsize %BUFSIZE% ^
  -g %GOP_SIZE% -bf %BFRAMES% -flags +cgop -sc_threshold 1000000000 ^
  -mbd rd -trellis 1 -mpeg_quant 1 -intra_vlc 1 -dc 9 -qmin 1 -qmax 28 ^
  -aspect 16:9 ^
  -pass 1 -passlogfile "%PASSLOG%" ^
  -an ^
  -f null NUL

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Pass 1 failed - check the ffmpeg output above.
    pause
    exit /b 1
)

echo.
echo ==== Pass 2/2 ====
ffmpeg -y -i "%INPUT%" ^
  -vf "%VF%" ^
  -c:v mpeg2video ^
  -pix_fmt yuv420p ^
  -b:v %VIDEO_BITRATE% -maxrate %MAXRATE% -bufsize %BUFSIZE% ^
  -g %GOP_SIZE% -bf %BFRAMES% -flags +cgop -sc_threshold 1000000000 ^
  -mbd rd -trellis 1 -mpeg_quant 1 -intra_vlc 1 -dc 9 -qmin 1 -qmax 28 ^
  -aspect 16:9 ^
  -pass 2 -passlogfile "%PASSLOG%" ^
  -c:a mp2 -b:a %AUDIO_BITRATE% -ar %AUDIO_RATE% -ac 2 ^
  -f vob ^
  "%OUTPUT%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Pass 2 failed - check the ffmpeg output above.
    pause
    exit /b 1
)

REM Clean up pass-log files
del /q "%PASSLOG%-0.log" 2>nul
del /q "%PASSLOG%-0.log.mbtree" 2>nul

echo.
echo Done: %OUTPUT%
pause
