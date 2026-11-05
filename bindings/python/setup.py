"""Builds the kolmo native library from bundled C sources and ships it inside the wheel.

The package carries its own copy of the native core under ``native/`` (src + vendored zstd, lz4,
brotli, libbsc, xxhash), refreshed from the repo root by ``sync-native.sh``. ``setup.py`` compiles
it into ``libkolmo`` with the system C/C++ compiler — no ``make``, no system codec libraries
(beyond optional liblzma) — so both the sdist and per-platform wheels build anywhere. The compiled
library sits next to ``kolmo.py`` and is loaded by ctypes, so the installed package needs no
toolchain at run time. Portable flags (no ``-march=native``) are used so a wheel built in CI runs
on any CPU of that architecture; czn.c selects SIMD paths at compile time with scalar fallbacks.

Set ``KOLMO_NO_LZMA=1`` to build without liblzma (the max/ultra tiers then use the other coders).
"""
import glob, os, platform, subprocess, sys, sysconfig
from setuptools import setup
from setuptools.command.build_py import build_py
from setuptools.dist import Distribution

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
# Prefer the bundled copy (present in the sdist); fall back to the repo tree for in-repo builds.
NATIVE = HERE if os.path.exists(os.path.join(HERE, "native", "src", "czn.c")) else ROOT
SRC = os.path.join(NATIVE, "native", "src") if NATIVE == HERE else os.path.join(ROOT, "src")
VENDOR = os.path.join(NATIVE, "native", "vendor") if NATIVE == HERE else os.path.join(ROOT, "vendor")

IS_MAC = sys.platform == "darwin"
LIBNAME = "libkolmo.dylib" if IS_MAC else "libkolmo.so"
NO_LZMA = os.environ.get("KOLMO_NO_LZMA") == "1"


def _sources():
    z, b, l4 = (os.path.join(VENDOR, d) for d in ("zstd", "brotli", "lz4"))
    bsc = os.path.join(VENDOR, "libbsc")
    c = [os.path.join(SRC, "czn.c")]
    for sub in ("common", "compress", "decompress", "dictBuilder"):
        c += sorted(glob.glob(os.path.join(z, sub, "*.c")))
    for sub in ("common", "dec", "enc"):
        c += sorted(glob.glob(os.path.join(b, sub, "*.c")))
    c += [os.path.join(l4, "lz4.c"), os.path.join(l4, "lz4hc.c")]
    c += [os.path.join(VENDOR, "xxhash", "czn_xxh3.c")]
    c += [os.path.join(bsc, "libbsc", "bwt", "libsais", "libsais.c")]
    cpp = sorted(glob.glob(os.path.join(bsc, "libbsc", "**", "*.cpp"), recursive=True))
    return c, cpp, z, b, l4, bsc


def _find_lzma(cc, workdir):
    """Probe for liblzma. Returns (include_dir_or_None, lib_dir_or_None) if found, else None.
    A None include/lib means it is on the compiler's default search path."""
    probe = os.path.join(workdir, "lzma_probe.c")
    with open(probe, "w") as f:
        f.write("#include <lzma.h>\nint main(void){return 0;}\n")
    prefixes = []
    for args in (["--prefix", "xz"], ["--prefix"]):
        try:
            p = subprocess.check_output(["brew"] + args, stderr=subprocess.DEVNULL).decode().strip()
            if p:
                prefixes.append(p)
        except Exception:
            pass
    prefixes += ["/opt/homebrew", "/usr/local", "/opt/local"]
    for inc in [None] + prefixes:   # default path first (typical on Linux with liblzma-dev)
        cmd = [cc, "-fsyntax-only"]
        if inc:
            cmd.append("-I" + os.path.join(inc, "include"))
        cmd.append(probe)
        if subprocess.call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
            return (None, None) if inc is None else (os.path.join(inc, "include"), os.path.join(inc, "lib"))
    return None


def build_library(dest_dir):
    """Compile libkolmo into dest_dir. Returns the full path to the built library."""
    c_src, cpp_src, z, b, l4, bsc = _sources()
    arch = platform.machine().lower()
    is_x86_64 = arch in ("x86_64", "amd64")
    asm = os.path.join(z, "decompress", "huf_decompress_amd64.S")

    cc = os.environ.get("CC", "cc")
    cxx = os.environ.get("CXX", "c++")
    workdir = os.path.join(dest_dir, "_kolmo_build")
    os.makedirs(workdir, exist_ok=True)

    includes = ["-I" + p for p in (SRC, z, l4, bsc, os.path.join(b, "include"))]
    defs = ["-DXXH_NAMESPACE=ZSTD_", "-DZSTD_MULTITHREAD", "-DZSTD_LEGACY_SUPPORT=0"]
    if not is_x86_64:
        defs.append("-DZSTD_DISABLE_ASM")

    # liblzma powers the max/ultra LZMA path. Detect it (on macOS it lives under the Homebrew/
    # MacPorts prefix, not the default include path); build without it if absent so the package
    # still installs with no system deps (those tiers then fall back to BWT/context-mixing).
    found = None if NO_LZMA else _find_lzma(cc, workdir)
    use_lzma = found is not None
    lzma_inc, lzma_libdir = found if use_lzma else (None, None)
    if not use_lzma:
        defs.append("-DCZN_NO_LZMA")
        if not NO_LZMA:
            sys.stderr.write("kolmo: liblzma not found; building without it (max/ultra use BWT/CM). "
                             "Install xz (brew install xz / apt install liblzma-dev) for the LZMA path.\n")
    elif lzma_inc:
        includes.append("-I" + lzma_inc)

    objs = []

    def compile_one(compiler, f, extra):
        obj = os.path.join(workdir, "o%d.o" % len(objs))
        cmd = [compiler, "-O3", "-fPIC", "-c"] + defs + includes + extra + [f, "-o", obj]
        subprocess.check_call(cmd)
        objs.append(obj)

    compile_one(cc, c_src[0], ["-ffp-contract=off"])  # czn.c: deterministic across compilers
    for f in c_src[1:]:
        compile_one(cc, f, [])
    if is_x86_64 and os.path.exists(asm):
        compile_one(cc, asm, [])
    for f in cpp_src:
        compile_one(cxx, f, ["-I" + bsc])

    out = os.path.join(dest_dir, LIBNAME)
    link = [cxx, "-shared"] + objs + ["-o", out, "-lpthread", "-lm"]
    if IS_MAC:
        link += ["-install_name", "@loader_path/" + LIBNAME]
    if use_lzma:
        # On macOS, prefer the static liblzma.a: linking it into the dylib leaves no external
        # liblzma reference for delocate to bundle (which otherwise fails to repair the wheel),
        # while keeping the full LZMA path. On Linux, -llzma + auditwheel bundles it fine.
        static_a = os.path.join(lzma_libdir, "liblzma.a") if lzma_libdir else None
        if IS_MAC and static_a and os.path.exists(static_a):
            link.append(static_a)
        else:
            if lzma_libdir:
                link.append("-L" + lzma_libdir)
            link.append("-llzma")
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
