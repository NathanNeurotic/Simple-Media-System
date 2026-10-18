"""Native C conversion tests with Python's independent codec oracle (AFL-3.0)."""
import ctypes as C
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
checks = 0
with tempfile.TemporaryDirectory() as build:
    for signedness in ('-fsigned-char', '-funsigned-char'):
        library = pathlib.Path(build) / (signedness[2:] + '.so')
        subprocess.run(['cc', '-shared', '-fPIC', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                        signedness, '-I' + str(ROOT / 'src'), str(ROOT / 'tests/cp1251_bridge.c'),
                        '-o', str(library)], check=True)
        lib = C.CDLL(str(library))
        lib.decode_byte.argtypes = [C.c_ubyte]
        lib.decode_byte.restype = C.c_ushort
        lib.encode_word.argtypes = [C.c_ushort]
        lib.encode_word.restype = C.c_ubyte
        lib.encode_path.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_int]
        lib.decode_path.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_uint, C.c_int]
        lib.join_path.argtypes = [C.c_void_p, C.c_uint, C.c_char_p, C.c_char_p]
        for byte in range(256):
            expected = ord(bytes([byte]).decode('cp1251')) if byte != 0x98 else ord('?')
            assert lib.decode_byte(byte) == expected
            if byte != 0x98:
                assert lib.encode_word(expected) == byte
            checks += 1

        def guarded(function, capacity, *args):
            global checks
            memory = C.create_string_buffer(b'Z' * (capacity + 8), capacity + 8)
            result = function(C.byref(memory, 4), capacity, *args)
            assert memory.raw[:4] == b'ZZZZ' and memory.raw[4 + capacity:] == b'ZZZZ'
            checks += 1
            return result, memory.raw[4:4 + capacity]

        cases = ['', 'Movies', 'Фильмы', 'Сериалы', 'Ёжик', 'Test123', 'Новый фильм 01.mp3',
                 'Фильм (2026)/Серия 01.mpg', 'Ёжик и ёлка', '— – № € Ў І Ї Ґ џ',
                 'я' * 255, 'Каталог/' * 60 + 'Фильм.avi']
        cases += [bytes([b]).decode('cp1251') for b in range(1, 256) if b != 0x98]
        for text in cases:
            local, wire = text.encode('cp1251'), text.encode('utf-16le')
            result, out = guarded(lib.encode_path, len(wire) + 2, local, 1)
            assert result == len(wire) + 2 and out == wire + b'\0\0'
            for terminated in (False, True):
                source = wire + (b'\0\0' if terminated else b'')
                result, out = guarded(lib.decode_path, len(local) + 1, source, len(source), 1)
                assert result == len(local) + 1 and out == local + b'\0'
            for capacity in (0, 1, len(wire) + 1):
                result, _ = guarded(lib.encode_path, capacity, local, 1)
                assert result == -1
            if local:
                result, _ = guarded(lib.decode_path, len(local), wire, len(wire), 1)
                assert result == -1
        for wire in (b'\x10', b'\0\xd8', b'\0\xdc', b'\x3d\xd8\0\xde', b'\xff\xff',
                     '中'.encode('utf-16le'), b'A\0\x10'):
            result, _ = guarded(lib.decode_path, 16, wire, len(wire), 1)
            assert result == -1
        result, _ = guarded(lib.encode_path, 16, b'\x98', 1)
        assert result == -1
        result, out = guarded(lib.decode_path, 2, b'A\0B\0', 2, 1)
        assert result == 2 and out == b'A\0'
        result, out = guarded(lib.decode_path, 2, b'A\0\0\0B\0', 6, 1)
        assert result == 2 and out == b'A\0'
        for byte in range(1, 256):
            result, out = guarded(lib.encode_path, 4, bytes([byte]), 0)
            assert result == 4 and out == bytes([byte, 0, 0, 0])
            result, out = guarded(lib.decode_path, 2, bytes([byte, 0]), 2, 0)
            assert result == 2 and out == bytes([byte if byte <= 128 else ord('?'), 0])
        result, out = guarded(lib.decode_path, 2, b'\x3d\xd8\0\xde', 4, 0)
        assert result == 2 and out == b'?\0'
        for directory, path, expected in [(b'', b'', b'\\'), (b'', b'///', b'\\'),
                                          (b'root', b'/nested/file/', b'root\\nested\\file'),
                                          (b'root', b'x' * 1000, b'root\\' + b'x' * 1000)]:
            for capacity in (0, len(expected), len(expected) + 1):
                result, out = guarded(lib.join_path, capacity, directory, path)
                if capacity <= len(expected):
                    assert result == -1
                else:
                    assert result == 0 and out == expected + b'\0'
        for length in (1022, 1023, 4096):
            result, out = guarded(lib.join_path, 1024, b'', b'x' * length)
            assert (result == 0) == (length == 1022)
            if result == 0:
                assert out == b'\\' + b'x' * length + b'\0'
print('PASS: %d checks; signed/unsigned char, codec oracle, roundtrip, canaries, invalid UTF-16, capacities, long/nested paths, legacy mode' % checks)
