"""Builds the kolmo native library from bundled C sources and ships it inside the wheel.

The package carries its own copy of the native core under ``native/`` (src + vendored zstd, lz4,
brotli, libbsc, xxhash, the liblzma subset), refreshed from the repo root by ``sync-native.sh``. ``setup.py``
compiles it into ``libkolmo`` with the system C/C++ compiler — no ``make``, no system codec libraries — so both the
sdist and per-platform wheels build anywhere. The compiled
library sits next to ``kolmo.py`` and is loaded by ctypes, so the installed package needs no
toolchain at run time. Portable flags (no ``-march=native``) are used so a wheel built in CI runs
on any CPU of that architecture; faster kernels are chosen at run time (src/arch/), with C references.

Set ``KOLMO_NO_LZMA=1`` to build without the LZMA2 coder (the ratio tiers then use the other coders).
"""
import glob, os, platform, subprocess, sys, sysconfig
from setuptools import setup
from setuptools.command.build_py import build_py
from setuptools.dist import Distribution

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
# Prefer the bundled copy (present in the sdist); fall back to the repo tree for in-repo builds.
NATIVE = HERE if os.path.exists(os.path.join(HERE, "native", "src", "kolmo.c")) else ROOT
SRC = os.path.join(NATIVE, "native", "src") if NATIVE == HERE else os.path.join(ROOT, "src")
VENDOR = os.path.join(NATIVE, "native", "vendor") if NATIVE == HERE else os.path.join(ROOT, "vendor")

IS_MAC = sys.platform == "darwin"
LIBNAME = "libkolmo.dylib" if IS_MAC else "libkolmo.so"
NO_LZMA = os.environ.get("KOLMO_NO_LZMA") == "1"


def _sources():
    z, b, l4 = (os.path.join(VENDOR, d) for d in ("zstd", "brotli", "lz4"))
    bsc = os.path.join(VENDOR, "libbsc")
    c = [os.path.join(SRC, "kolmo.c")]
    for sub in ("common", "compress", "decompress", "dictBuilder"):
        c += sorted(glob.glob(os.path.join(z, sub, "*.c")))
    for sub in ("common", "dec", "enc"):
        c += sorted(glob.glob(os.path.join(b, sub, "*.c")))
    c += [os.path.join(l4, "lz4.c"), os.path.join(l4, "lz4hc.c")]
    c += [os.path.join(SRC, "arch", "xxh3.c")]          # compiled apart from src/kolmo.c (docs/LAYOUT.md)
    c += [os.path.join(bsc, "libbsc", "bwt", "libsais", "libsais.c")]
    cpp = sorted(glob.glob(os.path.join(bsc, "libbsc", "**", "*.cpp"), recursive=True))
    return c, cpp, z, b, l4, bsc


def _xz(vendor):
    """The vendored liblzma subset (vendor/xz, raw LZMA2 only): its sources and its compile flags."""
    x = os.path.join(vendor, "xz")
    srcs = sorted(glob.glob(os.path.join(x, "*.c")) + glob.glob(os.path.join(x, "liblzma", "*", "*.c")))
    inc = [x] + [os.path.join(x, "liblzma", d) for d in ("api", "common", "check", "lz", "lzma", "rangecoder", "simple", "delta")] \
        + [os.path.join(x, "common")]
    flags = ["-DHAVE_CONFIG_H", "-include", os.path.join(x, "kolmo_lzma_namespace.h")] + ["-I" + d for d in inc]
    return srcs, flags


def build_library(dest_dir):
    """Compile libkolmo into dest_dir. Returns the full path to the built library."""
    c_src, cpp_src, z, b, l4, bsc = _sources()

    # Honor cibuildwheel's cross-compile request (ARCHFLAGS, e.g. "-arch arm64" when building an
    # Apple-silicon wheel on an Intel runner). The *target* arch — not the host — decides which
    # SIMD asm is compiled.
    archflags = os.environ.get("ARCHFLAGS", "").split()
    host = "x86_64" if platform.machine().lower() in ("x86_64", "amd64") else platform.machine().lower()
    if "arm64" in archflags and "x86_64" not in archflags:
        target = "arm64"
    elif "x86_64" in archflags and "arm64" not in archflags:
        target = "x86_64"
    else:
        target = host
    is_x86_64 = target == "x86_64"
    asm = os.path.join(z, "decompress", "huf_decompress_amd64.S")
    # Kolmo's own assembly kernels for the target ISA (src/arch/<isa>/*.S); KOLMO_NO_ASM=1 builds the C references only.
    no_asm = os.environ.get("KOLMO_NO_ASM") == "1"
    isa = "x86_64" if is_x86_64 else "aarch64" if target in ("arm64", "aarch64") else None
    kasm = [] if no_asm or not isa else sorted(glob.glob(os.path.join(SRC, "arch", isa, "*.S")))

    cc = os.environ.get("CC", "cc")
    cxx = os.environ.get("CXX", "c++")
    workdir = os.path.join(dest_dir, "_kolmo_build")
    os.makedirs(workdir, exist_ok=True)

    includes = ["-I" + p for p in (SRC, z, l4, bsc, os.path.join(b, "include"))]
    defs = ["-DXXH_NAMESPACE=ZSTD_", "-DZSTD_MULTITHREAD", "-DZSTD_LEGACY_SUPPORT=0", "-DKOLMO_NO_GPU"]
    if no_asm:
        defs.append("-DKOLMO_NO_ASM")
    if not is_x86_64:
        defs.append("-DZSTD_DISABLE_ASM")

    # liblzma: the vendored subset (vendor/xz), built like the other codecs, so every wheel has the LZMA2 path with no
    # system library, cross-compiled ones included. KOLMO_NO_LZMA=1 still leaves it out.
    xz_src, xz_flags = _xz(VENDOR)
    includes.append("-I" + os.path.join(VENDOR, "xz"))
    if NO_LZMA:
        defs.append("-DKOLMO_NO_LZMA")
        xz_src = []

    objs = []

    def compile_one(compiler, f, extra):
        obj = os.path.join(workdir, "o%d.o" % len(objs))
        cmd = [compiler, "-O3", "-fPIC", "-c"] + archflags + defs + includes + extra + [f, "-o", obj]
        subprocess.check_call(cmd)
        objs.append(obj)

    # -ffp-contract=off on every C and C++ file, not only kolmo.c: Brotli's encoder (-m eco) decides with float costs,
    # and FMA contraction (arm64, or x86-64 with FMA enabled) could change its choices and so the frame bytes.
    fp = ["-ffp-contract=off"]
    compile_one(cc, c_src[0], fp)  # kolmo.c: deterministic across compilers
    for f in c_src[1:]:
        compile_one(cc, f, fp)
    for f in xz_src:
        compile_one(cc, f, fp + xz_flags)
    if is_x86_64 and os.path.exists(asm):
        compile_one(cc, asm, [])
    for f in kasm:
        compile_one(cc, f, [])
    # dispatched kernels (src/arch/cpu.c): C references always, x86-64 versions unless KOLMO_NO_ASM=1 (Makefile: KSRC)
    for f in ["arch/cpu.c", "algo/predict.c", "algo/planes.c", "algo/mixer.c", "algo/video.c"]:
        compile_one(cc, os.path.join(SRC, f), fp)
    if is_x86_64 and not no_asm:
        for f in ["med8_sse2.c", "planes_sse2.c", "mixer_avx2.c", "video_sse2.c", "img16_sse2.c", "med8f_avx2.c"]:
            compile_one(cc, os.path.join(SRC, "arch", "x86_64", f), fp)
    for f in cpp_src:
        compile_one(cxx, f, fp + ["-I" + bsc])

    out = os.path.join(dest_dir, LIBNAME)
    link = [cxx, "-shared"] + archflags + objs + ["-o", out, "-lpthread", "-lm"]
    if IS_MAC:
        link += ["-install_name", "@loader_path/" + LIBNAME]
    subprocess.check_call(link)
    return out


class BinaryDistribution(Distribution):
    def has_ext_modules(self):   # force a platform-tagged wheel (it bundles a compiled library)
        return True


class build_py_with_lib(build_py):
    def run(self):
        super().run()
        build_library(self.build_lib)  # libkolmo.{so,dylib} lands next to kolmo.py


_cmd = {"build_py": build_py_with_lib}

# kolmo.py talks to the library through ctypes and never touches the Python C-API, so a single
# platform wheel works on every Python 3 — tag it py3/none (not cp3x) so one wheel covers all.
try:
    from wheel.bdist_wheel import bdist_wheel as _bdist_wheel

    class bdist_wheel(_bdist_wheel):
        def finalize_options(self):
            super().finalize_options()
            self.root_is_pure = False          # platform-specific (bundles a compiled library)

        def get_tag(self):
            _py, _abi, plat = super().get_tag()
            return "py3", "none", plat         # any Python 3, no CPython ABI dependency

    _cmd["bdist_wheel"] = bdist_wheel
except ImportError:
    pass

setup(distclass=BinaryDistribution, cmdclass=_cmd)
