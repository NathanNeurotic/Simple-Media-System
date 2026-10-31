#!/usr/bin/env python3
"""Host regression for the player-visible AAC sample rate.

The selected production source supplies the real ASC parser, initialization,
rate lookup, and _aac_decode functions. FFmpeg generates short valid stereo AVI
AAC fixtures; all generated media, C source, and executables stay in an isolated
TemporaryDirectory. No Git lookup or repository writes are required.

Host adapters replace PS2 structures, bit reading, spectral reconstruction, DSP
filter banks, channel-position description, PCM conversion, and ring buffers.
The production code still selects decoded PCM frames/bytes and initialization
rate, so the tests check their agreement with the context copied by the player.
ADTS/ADIF/PCE adapters abort if reached. Explicit SBR cases exercise valid ASC
parsing and production output sizing, with spectral content behind the adapter.
This does not test PCM quality, SBR/DSP numerical accuracy, actual SPU resampling,
performance, or console playback. ASan, UBSan and leak checking cover host code.
"""
import argparse
import os
import shlex
import shutil
import pathlib
import re
import struct
import subprocess
import tempfile

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]



def function(source, name):
    match = re.search(r'^static [^;\n]*\b' + re.escape(name) + r'\s*\(', source, re.M)
    if not match:
        raise RuntimeError(f'Cannot find {name}')
    marker = '/* end ' + name + ' */'
    end = source.find(marker, match.start())
    if end < 0:
        raise RuntimeError(f'Missing "{marker}" marker after {name}')
    return source[match.start():source.index('\n', end)]


def asc(*parts):
    bits = ''.join(format(value, f'0{width}b') for value, width in parts)
    bits += '0' * (-len(bits) % 8)
    return int(bits, 2).to_bytes(len(bits) // 8, 'big')


def chunks(data, start, end):
    while start + 8 <= end:
        tag, size = struct.unpack_from('<4sI', data, start)
        payload = start + 8
        assert payload + size <= end
        yield tag, payload, payload + size
        start = payload + size + (size & 1)


def fixture(rate, workspace, ffmpeg):
    """Return actual FFmpeg AVI AAC WAVE config and its first raw access unit."""
    path = workspace / f'lc-{rate}.avi'
    subprocess.run([ffmpeg, '-y', '-v', 'error', '-f', 'lavfi', '-i',
                    f'sine=frequency=440:sample_rate={rate}:duration=0.15',
                    '-c:a', 'aac', '-ac', '2', '-ar', str(rate), '-b:a', '128k',
                    '-f', 'avi', str(path)], check=True, capture_output=True)
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'AVI '
    extra = packet = None
    for kind, start, end in chunks(data, 12, len(data)):
        if kind != b'LIST':
            continue
        if data[start:start + 4] == b'hdrl':
            for kind, first, last in chunks(data, start + 4, end):
                if kind != b'LIST' or data[first:first + 4] != b'strl':
                    continue
                fields = {k: data[a:b] for k, a, b in chunks(data, first + 4, last)}
                if fields.get(b'strh', b'')[:4] != b'auds':
                    continue
                wave = fields[b'strf']
                tag, channels, emitted_rate = struct.unpack_from('<HHI', wave)
                assert (tag, channels, emitted_rate) == (255, 2, rate)
                length = struct.unpack_from('<H', wave, 16)[0]
                extra = wave[18:18 + length]
                assert len(extra) == length
        if data[start:start + 4] == b'movi':
            for kind, first, last in chunks(data, start + 4, end):
                if kind == b'00wb' and packet is None:
                    packet = data[first:last]
    assert extra and packet
    return extra, packet


ADAPTERS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMS_INLINE
#define MAX_CHANNELS 64
#define MAX_SYNTAX_ELEMENTS 48
#define FAAD_LC 2
#define FAAD_MAIN 1
#define FAAD_NO_SBR 0
#define FAAD_RAW 0
#define FAAD_ADIF 1
#define FAAD_ADTS 2

typedef struct {
    const uint8_t *data;
    uint32_t m_Idx, m_szInBits;
} SMS_BitContext;
static void SMS_InitGetBits(SMS_BitContext *b, const uint8_t *data, uint32_t bits) {
    b->data = data; b->m_Idx = 0; b->m_szInBits = bits;
}
static uint32_t SMS_GetBits(SMS_BitContext *b, unsigned count) {
    uint32_t result = 0;
    assert(b->m_Idx + count <= b->m_szInBits);
    while (count--) {
        result = (result << 1) | ((b->data[b->m_Idx / 8] >> (7 - b->m_Idx % 8)) & 1);
        b->m_Idx++;
    }
    return result;
}
static uint32_t SMS_GetBit(SMS_BitContext *b) { return SMS_GetBits(b, 1); }
static uint32_t SMS_ShowBits(SMS_BitContext *b, unsigned count) {
    SMS_BitContext copy = *b;
    return SMS_GetBits(&copy, count);
}
static void SMS_AlignBits(SMS_BitContext *b) { b->m_Idx = (b->m_Idx + 7) & ~7U; }
static uint32_t bit2byte(uint32_t bits) { return (bits + 7) / 8; }

typedef struct { uint8_t sf_index, object_type, channels; } program_config;
typedef int drc_info;
typedef struct { int initialized; } fb_info;
typedef struct { program_config pce[1]; } adif_header;
typedef struct { uint8_t old_format, sf_index, profile, channel_configuration; } adts_header;
typedef struct {
    uint8_t objectTypeIndex, samplingFrequencyIndex, channelsConfiguration;
    uint32_t samplingFrequency;
    uint8_t frameLengthFlag, dependsOnCoreCoder, extensionFlag;
    uint16_t coreCoderDelay;
    int8_t sbr_present_flag, forceUpSampling, downSampledSBR;
} _mp4AudioSpecificConfig;
typedef struct {
    uint8_t defObjectType;
    uint64_t defSampleRate;
    uint8_t useOldADTSFormat, dontUpSampleImplicitSBR;
} _NeAACDecConfiguration;
typedef struct {
    uint32_t bytesconsumed, samples, samplerate;
    uint8_t channels, error, sbr, object_type, header_type;
} _NeAACDecFrameInfo;
typedef struct {
    uint8_t adts_header_present, adif_header_present, sf_index, object_type;
    uint8_t channelConfiguration, pce_set, fr_channels, fr_ch_ele;
    uint8_t downMatrix, upMatrix, internal_channel[MAX_CHANNELS];
    int8_t sbr_present_flag, forceUpSampling, downSampledSBR;
    uint32_t frame;
    _NeAACDecConfiguration config;
    program_config pce;
    fb_info fb;
    drc_info drc;
    float *fb_intermed[MAX_CHANNELS], *time_out[MAX_CHANNELS];
    void *sbr[MAX_SYNTAX_ELEMENTS];
} _NeAACDecStruct, *_NeAACDecHandle;
typedef struct { void *m_pCtx; } SMS_Codec;
typedef struct {
    int32_t m_SampleRate;
    SMS_Codec *m_pCodec;
    void *m_pUserData;
    short m_UserDataLen, m_Channels;
} SMS_CodecContext;
typedef struct { uint8_t *m_pData; int32_t m_Size; } SMS_AVPacket;
typedef struct SMS_RingBuffer {
    uint8_t *m_pOut;
    void (*UserCB)(struct SMS_RingBuffer *);
    uint8_t storage[20000] __attribute__((aligned(16)));
    int allocated, callbacks;
} SMS_RingBuffer;

/* Only the normal ASC (non-PCE) and no-initial-packet paths are tested.
 * Fail loudly if an unmodelled ADTS/ADIF/PCE route is accidentally exercised. */
static uint8_t program_config_element(program_config *p, SMS_BitContext *b) {
    (void)p; (void)b; abort();
}
static void get_adif_header(adif_header *a, SMS_BitContext *b) {
    (void)a; (void)b; abort();
}
static uint8_t adts_frame(adts_header *a, SMS_BitContext *b) {
    (void)a; (void)b; abort();
}
static void filter_bank_init(fb_info *fb) { fb->initialized++; }
static void sbrReset(void *sbr) { (void)sbr; }

/* DSP boundary adapter: a successful stereo AAC access unit has been
 * reconstructed. The production _aac_decode still selects PCM frame count,
 * bytes, upsampling behavior, output rate and ring-buffer callback itself. */
static void raw_data_block(_NeAACDecHandle h, _NeAACDecFrameInfo *info,
                           SMS_BitContext *bits, program_config *pce, drc_info *drc) {
    (void)pce; (void)drc;
    assert(bits->m_szInBits > 0);
    h->fr_channels = h->channelConfiguration;
    h->fr_ch_ele = 1;
    h->sbr[0] = h;
    info->error = 0;
    bits->m_Idx = bits->m_szInBits;
}
static void create_channel_config(_NeAACDecHandle h, _NeAACDecFrameInfo *info) {
    (void)h; (void)info;
}
static int observed_frames, observed_channels;
static void output_to_PCM(_NeAACDecHandle h, float **input, short *out,
                           uint8_t channels, int frames) {
    (void)h; (void)input;
    observed_frames = frames;
    observed_channels = channels;
    memset(out, 0, frames * channels * sizeof(short));
}
static void *SMS_RingBufferAlloc(SMS_RingBuffer *rb, int size) {
    assert(size > 0 && (size_t)size <= sizeof(rb->storage));
    rb->allocated = size;
    return rb->storage;
}
static void on_output(SMS_RingBuffer *rb) { rb->callbacks++; }
'''


CHECKS = r'''
static int checks, failures;
#define CHECK(condition, name) do { \
    checks++; \
    if (!(condition)) { \
        failures++; fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); \
    } \
} while (0)

static void check_case(const char *name, const uint8_t *extra, size_t extra_len,
                        const uint8_t *packet, size_t packet_len, uint32_t core_rate,
                        uint32_t output_rate, int output_frames, int disabled) {
    _NeAACDecStruct decoder = {0};
    decoder.config.defObjectType = FAAD_LC;
    decoder.config.defSampleRate = core_rate;
    decoder.config.dontUpSampleImplicitSBR = disabled;
    SMS_Codec codec = {.m_pCtx = &decoder};
    SMS_CodecContext ctx = {.m_SampleRate = core_rate, .m_pCodec = &codec,
                            .m_pUserData = (void *)extra, .m_UserDataLen = extra_len,
                            .m_Channels = 2};
    SMS_AVPacket pkt = {.m_pData = (uint8_t *)packet, .m_Size = packet_len};
    SMS_RingBuffer input = {.m_pOut = (uint8_t *)&pkt};
    SMS_RingBuffer output = {.UserCB = on_output};
    for (int repeat = 0; repeat < 2; repeat++) {
        CHECK(_aac_init(&ctx) == 1, name);
        CHECK(ctx.m_SampleRate == (int32_t)output_rate, name);
        CHECK(ctx.m_Channels == 2, name);
        CHECK(decoder.fb.initialized == repeat + 1, name);
        observed_frames = observed_channels = output.allocated = output.callbacks = 0;
        CHECK(_aac_decode(&ctx, &output, &input) == 0, name);
        CHECK(observed_frames == output_frames, name);
        CHECK(observed_channels == 2, name);
        CHECK(output.allocated == output_frames * 2 * (int)sizeof(short) + 80, name);
        CHECK(output.callbacks == 1, name);
        /* Independent consistency: 1024 core samples and emitted PCM must
         * represent the same time interval at the player/SPU context rate. */
        CHECK((uint64_t)observed_frames * core_rate ==
              (uint64_t)1024 * ctx.m_SampleRate, name);
    }
    printf("%s: source=%u output=%d Hz, frames=%d, repeats=2\n",
           name, core_rate, ctx.m_SampleRate, observed_frames);
}

static void check_failure(const char *name, const uint8_t *bad, size_t length,
                           const uint8_t *good, size_t good_len) {
    _NeAACDecStruct decoder = {0};
    decoder.config.defObjectType = FAAD_LC;
    decoder.config.defSampleRate = 22050;
    SMS_Codec codec = {.m_pCtx = &decoder};
    SMS_CodecContext ctx = {.m_SampleRate = 32000, .m_pCodec = &codec,
                            .m_pUserData = (void *)bad, .m_UserDataLen = length,
                            .m_Channels = 2};
    CHECK(_aac_init(&ctx) == 0, name);
    CHECK(ctx.m_SampleRate == 32000, name);
    CHECK(decoder.fb.initialized == 0, name);
    ctx.m_pUserData = (void *)good; ctx.m_UserDataLen = good_len;
    CHECK(_aac_init(&ctx) == 1, name);
    CHECK(ctx.m_SampleRate == 44100, name);
    CHECK(decoder.fb.initialized == 1, name);
    ctx.m_pUserData = (void *)bad; ctx.m_UserDataLen = length;
    CHECK(_aac_init(&ctx) == 0, name);
    CHECK(ctx.m_SampleRate == 44100, name);
    CHECK(decoder.fb.initialized == 1, name);
    CHECK(ctx.m_Channels == 2, name);
    printf("%s: failed init preserves context rate before/after success\n", name);
}
'''


def byte_array(name, data):
    return f'static const uint8_t {name}[] = {{' + ','.join(str(b) for b in data) + '};\n'


def create_harness(source, fixtures):
    result = ADAPTERS
    table = re.search(r'static const uint8_t ObjectTypesTable\[.*?\n};', source, re.S).group()
    result += '\n' + table + '\n'
    for name in ('get_sr_index', 'get_sample_rate', 'can_decode_ot', '_NeAACDecInit',
                 'GASpecificConfig', 'AudioSpecificConfig2', '_NeAACDecInit2',
                 '_aac_decode', '_aac_init'):
        result += '\n' + function(source, name) + '\n'
    result += CHECKS
    for rate, (extra, packet) in fixtures.items():
        result += byte_array(f'lc_{rate}', extra)
        result += byte_array(f'packet_{rate}', packet)
    # Complete, valid ASC configs; failure cases are valid but unsupported AAC
    # profiles/frame lengths, never truncated or attacker-controlled buffers.
    configs = {
        'sbr_up': asc((5, 5), (7, 4), (2, 4), (4, 4), (2, 5), (0, 3)),
        'sbr_down': asc((5, 5), (7, 4), (2, 4), (7, 4), (2, 5), (0, 3)),
        'ssr': asc((3, 5), (7, 4), (2, 4), (0, 3)),
        'lc_960': asc((2, 5), (7, 4), (2, 4), (4, 3)),
    }
    for name, data in configs.items():
        result += byte_array(name, data)
    result += '\nint main(void) {\n'
    for rate in fixtures:
        effective = rate * 2 if rate <= 24000 else rate
        frames = 2048 if rate <= 24000 else 1024
        result += (f'check_case("ffmpeg-lc-{rate}", lc_{rate}, sizeof(lc_{rate}), '
                   f'packet_{rate}, sizeof(packet_{rate}), {rate}, {effective}, {frames}, 0);\n')
    for rate in (22050, 24000):
        result += (f'check_case("ffmpeg-lc-{rate}-implicit-disabled", lc_{rate}, sizeof(lc_{rate}), '
                   f'packet_{rate}, sizeof(packet_{rate}), {rate}, {rate}, 1024, 1);\n')
    for disabled in (0, 1):
        result += (f'check_case("no-config-22050-disabled-{disabled}", NULL, 0, '
                   f'packet_22050, sizeof(packet_22050), 22050, '
                   f'{22050 if disabled else 44100}, {1024 if disabled else 2048}, {disabled});\n')
        result += (f'check_case("explicit-sbr-up-disabled-{disabled}", sbr_up, sizeof(sbr_up), '
                   f'packet_22050, sizeof(packet_22050), 22050, 44100, 2048, {disabled});\n')
        result += (f'check_case("explicit-sbr-down-disabled-{disabled}", sbr_down, sizeof(sbr_down), '
                   f'packet_22050, sizeof(packet_22050), 22050, 22050, 1024, {disabled});\n')
    for name in ('ssr', 'lc_960'):
        result += (f'check_failure("unsupported-{name}", {name}, sizeof({name}), '
                   f'lc_22050, sizeof(lc_22050));\n')
    result += 'printf("checks=%d failures=%d\\n", checks, failures); return failures ? 1 : 0;\n}\n'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=pathlib.Path,
                        default=REPO_ROOT / 'src/SMS_AAC.c',
                        help='AAC source to check (default: repository src/SMS_AAC.c)')
    args = parser.parse_args()
    source = args.source.read_text()
    ffmpeg = shutil.which(os.environ.get('FFMPEG', 'ffmpeg'))
    if not ffmpeg:
        parser.error('FFmpeg is required to generate valid AAC fixtures')
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    if not compiler:
        parser.error('CC must name a C compiler')
    # SDK-independent adapters permit the production rate/control code to run
    # under host sanitizers. Keep LeakSanitizer enabled alongside ASan/UBSan.
    run_env = os.environ.copy()
    run_env['ASAN_OPTIONS'] = (run_env.get('ASAN_OPTIONS', '') +
                               ':detect_leaks=1:halt_on_error=1').lstrip(':')
    run_env['UBSAN_OPTIONS'] = (run_env.get('UBSAN_OPTIONS', '') +
                                ':halt_on_error=1:print_stacktrace=1').lstrip(':')
    with tempfile.TemporaryDirectory(prefix='sms-aac-sample-rate-') as temp:
        workspace = pathlib.Path(temp)
        fixtures = {r: fixture(r, workspace, ffmpeg)
                    for r in (22050, 24000, 32000, 44100, 48000)}
        harness = workspace / 'aac_sample_rate.c'
        harness.write_text(create_harness(source, fixtures))
        binary = workspace / 'aac_sample_rate'
        subprocess.run(compiler + ['-std=c99', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                        '-Wno-unused-but-set-variable',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-fno-pie', '-no-pie', str(harness), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], env=run_env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end='')
        raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
