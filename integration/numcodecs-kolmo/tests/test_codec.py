"""numcodecs API: round trips, out=, non-contiguous input, config, entry point, errors."""
import numpy as np
import pytest

import kolmo
import numcodecs
from numcodecs_kolmo import Kolmo

DTYPES = ["u1", "i1", "<u2", "<i2", "<i4", "<u4", "<i8", "<u8", "<f2", "<f4", "<f8", "<c8", "<c16", "?",
          ">i2", ">i4", ">i8", ">f4", ">f8", "<M8[ns]", ">m8[s]", "S7", "<U3"]
SHAPES = [(0,), (1,), (1000,), (0, 5), (17, 31), (8, 9, 10), (64, 64, 4)]


def sample(dtype, shape, seed=0):
    rng = np.random.default_rng(seed)
    n = int(np.prod(shape))
    dt = np.dtype(dtype)
    if dt.kind == "f" or dt.kind == "c":
        base = np.cumsum(rng.normal(size=n)) / 10           # smooth, like a measured series
        a = base.astype(dt) if dt.kind == "f" else (base + 1j * base[::-1]).astype(dt)
    elif dt.kind == "b":
        a = rng.random(n) < 0.3
    elif dt.kind in "SU":
        words = np.array(["abc", "temp", "x", "pressure", ""], dtype=dt)
        a = words[rng.integers(0, len(words), n)]
    elif dt.kind in "Mm":
        a = (np.arange(n, dtype="i8") * 1000 + 1_700_000_000).astype(dt)
    else:
        info = np.iinfo(dt)
        a = rng.integers(max(info.min, -1000), min(info.max, 1000), n, endpoint=True).astype(dt)
    return a.reshape(shape)


@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize("shape", SHAPES)
def test_round_trip(dtype, shape):
    a = sample(dtype, shape)
    codec = Kolmo(level=1, threads=1)
    enc = codec.encode(a)
    assert isinstance(enc, bytes)
    dec = codec.decode(enc)
    assert np.array_equal(np.frombuffer(dec, dtype=a.dtype).reshape(shape), a)
    out = np.empty_like(a)
    res = codec.decode(enc, out=out)
    assert res is out or np.shares_memory(res, out)
    assert np.array_equal(out, a)
    assert out.dtype == a.dtype                               # big-endian stays big-endian


@pytest.mark.parametrize("level", [1, 2, 3, 4, 5, 6, 7, 8, 9])
def test_levels(level):
    a = sample("<f4", (200, 300), seed=level)
    codec = Kolmo(level=level)
    enc = codec.encode(a)
    assert np.array_equal(np.frombuffer(codec.decode(enc), "<f4").reshape(a.shape), a)
    assert enc == kolmo.compress(a.tobytes(), level=level)    # a plain Kolmo frame, the same bytes as the binding


def test_ultra():
    a = sample("<i2", (4096,))
    codec = Kolmo(level="ultra", threads=1)
    assert np.array_equal(np.frombuffer(codec.decode(codec.encode(a)), "<i2"), a)


def test_threads_do_not_change_bytes():
    a = sample("<f8", (512, 512))
    assert Kolmo(level=5, threads=1).encode(a) == Kolmo(level=5, threads=0).encode(a) == Kolmo(level=5, threads=4).encode(a)


def test_big_endian_bytes_kept():
    a = np.arange(1000, dtype=">i4")
    dec = Kolmo().decode(Kolmo().encode(a))
    assert dec == a.tobytes()                                 # the bytes as stored, not swapped


@pytest.mark.parametrize("make", [
    lambda a: a[:, ::2],              # strided columns
    lambda a: a.T,                    # Fortran order (contiguous, other order)
    lambda a: a[::3, 1:-1],
    lambda a: a[::-1],                # negative stride
])
def test_non_contiguous_input(make):
    base = sample("<f4", (60, 40))
    v = make(base)
    codec = Kolmo(level=3, threads=1)
    dec = np.frombuffer(codec.decode(codec.encode(v)), "<f4")
    if v.flags.f_contiguous and not v.flags.c_contiguous:
        expect = v.ravel(order="F")                         # numcodecs encodes memory order
    else:
        expect = np.ascontiguousarray(v).ravel()
    assert np.array_equal(dec, expect)


@pytest.mark.parametrize("buf", [b"hello kolmo" * 100, bytearray(b"\x00\x01" * 500), memoryview(b"abc" * 333)])
def test_bytes_like_input(buf):
    codec = Kolmo()
    enc = codec.encode(buf)
    assert codec.decode(enc) == bytes(buf)
    assert codec.decode(bytearray(enc)) == bytes(buf)
    assert codec.decode(memoryview(enc)) == bytes(buf)
    assert codec.decode(np.frombuffer(enc, "u1")) == bytes(buf)


def test_out_is_bytes_buffer():
    a = sample("<i4", (100,))
    out = bytearray(a.nbytes)
    Kolmo().decode(Kolmo().encode(a), out=out)
    assert bytes(out) == a.tobytes()


def test_out_too_small_or_too_large():
    a = sample("<f8", (100,))
    enc = Kolmo().encode(a)
    with pytest.raises(ValueError):
        Kolmo().decode(enc, out=np.empty(99, "<f8"))
    with pytest.raises(ValueError):
        Kolmo().decode(enc, out=np.empty(101, "<f8"))


def test_corrupt_input_raises():
    enc = bytearray(Kolmo().encode(sample("<f4", (1000,))))
    with pytest.raises(ValueError):
        Kolmo().decode(bytes(enc[:10]))
    enc[len(enc) // 2] ^= 0xFF
    with pytest.raises(ValueError):
        Kolmo().decode(bytes(enc))


def test_config_round_trip():
    for level, threads in [(3, 0), (9, 1), ("ultra", 2), (1, 8)]:
        c = Kolmo(level=level, threads=threads)
        cfg = c.get_config()
        assert cfg == {"id": "kolmo", "level": level, "threads": threads}
        c2 = Kolmo.from_config(dict(cfg))
        assert c2 == c
        c3 = numcodecs.get_codec(dict(cfg))
        assert isinstance(c3, Kolmo) and c3 == c


def test_entry_point_registration():
    # a fresh lookup by id goes through the numcodecs.codecs entry point
    numcodecs.registry.codec_registry.pop("kolmo", None)
    assert isinstance(numcodecs.get_codec({"id": "kolmo"}), Kolmo)


def test_default_config():
    assert Kolmo().get_config() == {"id": "kolmo", "level": 3, "threads": 0}
    assert repr(Kolmo(level=7, threads=2)) == "Kolmo(level=7, threads=2)"


@pytest.mark.parametrize("level", [0, 10, -1, "fast", "max", 3.0, True, None])
def test_bad_level(level):
    with pytest.raises(ValueError):
        Kolmo(level=level)


@pytest.mark.parametrize("threads", [-1, 1.5, "2", True, None])
def test_bad_threads(threads):
    with pytest.raises(ValueError):
        Kolmo(threads=threads)


def test_object_arrays_refused():
    with pytest.raises((TypeError, ValueError)):
        Kolmo().encode(np.array([1, "a", None], dtype=object))
