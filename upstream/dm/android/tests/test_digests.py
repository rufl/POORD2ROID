#!/usr/bin/env python3
"""资源摘要对照 Python 标准库，含填充边界。| Compare asset digests against Python's standard library, including padding boundaries."""
import ctypes
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "digests.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-Wall", "-Wextra", "-Werror", str(ROOT / "native/legacy_digest.c"), "-o", str(output)], check=True)
    library = ctypes.CDLL(str(output))
    for kind in ["md5", "sha1"]:
        function = getattr(library, "dm_android_" + kind)
        function.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p]
        for n in [0, 1, 3, 55, 56, 63, 64, 65, 127, 128, 1024, 65536]:
            data = bytes(i % 251 for i in range(n))
            result = ctypes.create_string_buffer(16 if kind == "md5" else 20)
            function(data, n, result)
            assert result.raw == getattr(hashlib, kind)(data).digest(), (kind, n)
    class Context(ctypes.Structure):
        _fields_ = [("state", ctypes.c_uint32 * 8), ("length", ctypes.c_uint64), ("used", ctypes.c_uint), ("buffer", ctypes.c_ubyte * 64)]
    library.dm_android_sha256_init.argtypes = [ctypes.POINTER(Context)]
    library.dm_android_sha256_update.argtypes = [ctypes.POINTER(Context), ctypes.c_void_p, ctypes.c_uint32]
    library.dm_android_sha256_final.argtypes = [ctypes.c_void_p, ctypes.POINTER(Context)]
    for n in [0, 1, 3, 55, 56, 63, 64, 65, 127, 128, 1024, 65536]:
        data = bytes(i % 251 for i in range(n)); context = Context(); result = ctypes.create_string_buffer(32)
        library.dm_android_sha256_init(ctypes.byref(context))
        for offset in range(0, n, 13):
            chunk = data[offset:offset + 13]
            library.dm_android_sha256_update(ctypes.byref(context), chunk, len(chunk))
        library.dm_android_sha256_final(result, ctypes.byref(context))
        assert result.raw == hashlib.sha256(data).digest(), ("sha256", n)
import sys
sys.path.insert(0, str(ROOT / "tools"))
import build_shaders
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    work = root / "work"
    out = root / "spirv"
    work.mkdir(); out.mkdir()
    jobs = [{"key": "0123456789abcdef", "reduced": False, "host_subgroup_size": 16}]
    build_shaders.record_build_config(work, out, jobs, "full")
    (out / "0123456789abcdef.spv").write_bytes(b"\x03\x02\x23\x07\x00")
    (out / "0123456789abcdef.meta").write_bytes(b"DMVK")
    d1, c1 = build_shaders.shader_output_digest(out)
    assert c1 == 1
    try:
        build_shaders.record_build_config(work, out, [dict(jobs[0], reduced=True)], "reduced")
        raise AssertionError("expected precision mismatch rejection")
    except ValueError:
        pass
    (out / "0123456789abcdef.spv").write_bytes(b"\x03\x02\x23\x07\x01")
    d2, c2 = build_shaders.shader_output_digest(out)
    assert c2 == 1 and d1 != d2
print("Resource digests: PASS (36 reference comparisons)")
