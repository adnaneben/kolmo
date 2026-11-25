// Compiles the kolmo native core (src/kolmo.c, src/arch/<isa>/*.S, Kolmo's own coders and BWT pipeline; no third-party
// codec library) into a static library and links it, so the published crate is self-contained: it needs a C compiler,
// but no `make`, no system library and no prebuilt library. The C sources live in `native/` inside the crate,
// refreshed from the repo root by `sync-native.sh`, with a fallback to the repo tree for in-place contributor builds.
//
// The compiler is driven by the `cc` crate (build-time only), so the build follows the *target*, not the host:
// it passes the target triple (`--target`, or `-arch` on Apple), honours CC/CFLAGS/AR and their `_<target>`,
// `TARGET_` and `HOST_` forms, and runs as many compilers as cargo's jobserver allows. Cross builds
// (`cargo build --target x86_64-apple-darwin` on Apple silicon, aarch64 Linux from x86 CI, cross, cargo-zigbuild)
// compile the target's sources with the target's compiler.
use std::{env, fs, path::{Path, PathBuf}};

fn list(dir: &Path, ext: &str, out: &mut Vec<PathBuf>) {
    if let Ok(rd) = fs::read_dir(dir) {
        for e in rd.flatten() {
            let p = e.path();
            if p.is_file() && p.extension().map_or(false, |x| x == ext) {
                out.push(p);
            }
        }
    }
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // Prefer the bundled copy (present in the published crate); fall back to the repo tree.
    let bundled = manifest.join("native");
    let base = if bundled.join("src/kolmo.c").exists() {
        bundled
    } else {
        manifest.parent().unwrap().parent().unwrap().to_path_buf()
    };
    let src = base.join("src");
    // The target's architecture (cargo sets CARGO_CFG_* for the target, not for the host running this script).
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let is_x86_64 = arch == "x86_64";

    let mut b = cc::Build::new();
    // -O3 whatever the cargo profile (as the Makefile, the CLI and the Python wheels). Every C file gets
    // -ffp-contract=off: some encoder choices are made with float costs, and FMA contraction would change the frame bytes.
    b.include(&src).define("KOLMO_NO_GPU", None).opt_level(3).pic(true).warnings(false).flag("-ffp-contract=off");
    // KOLMO_NO_ASM=1: no assembly kernels (src/arch/<isa>/*.S), the C references only.
    let no_asm = env::var("KOLMO_NO_ASM").map_or(false, |v| v == "1");
    if no_asm { b.define("KOLMO_NO_ASM", None); }
    else if env::var("KOLMO_GEAR_ASM").as_deref() == Ok("1") { b.define("KOLMO_GEAR_ASM", None); }
    // Portable by default: the target's baseline ISA, like the release binaries and the Python wheels. On x86-64 the
    // AVX2/SSE4.2 paths are chosen at run time by CPUID, so baseline builds lose little. KOLMO_NATIVE=1 builds for
    // the host CPU instead (the library then runs only on CPUs like the build machine; not for cross builds).
    if env::var("KOLMO_NATIVE").map_or(false, |v| v == "1") {
        b.flag(if is_x86_64 { "-march=native" } else { "-mcpu=native" });
    }

    let mut files = vec![src.join("kolmo.c")];
    // the block-sorting pipeline, also compiled on its own (LZP, QLFC, the libsais subset, the BWT stream formats)
    for f in ["algo/lzp.c", "algo/qlfc.c", "algo/par.c", "codecs/bwt_stream.c"] { files.push(src.join(f)); }
    // Kolmo's own coders (codecs 24, 27-29, 32) and kolmo_khash64, also compiled on their own
    for f in ["algo/khuf.c", "algo/krans.c", "algo/klz.c", "algo/klzma.c", "algo/khash.c"] { files.push(src.join(f)); }
    list(&src.join("algo/sais"), "c", &mut files);
    // Kolmo's own assembly kernels for the target ISA.
    let isa = if is_x86_64 { Some("x86_64") } else if arch == "aarch64" { Some("aarch64") } else { None };
    if let (Some(isa), false) = (isa, no_asm) {
        list(&src.join("arch").join(isa), "S", &mut files);
    }
    // Dispatched kernels (src/arch/cpu.c): C references always, x86-64 versions unless KOLMO_NO_ASM (Makefile: KSRC).
    for f in ["arch/cpu.c", "algo/predict.c", "algo/planes.c", "algo/mixer.c", "algo/video.c"] { files.push(src.join(f)); }
    if is_x86_64 && !no_asm {
        for f in ["med8_sse2.c", "planes_sse2.c", "mixer_avx2.c", "video_sse2.c", "img16_sse2.c", "med8f_avx2.c"] {
            files.push(src.join("arch/x86_64").join(f));
        }
    }
    files.sort();
    b.files(&files).compile("kolmo_native");   // also prints the link lines for the static library

    // pthread/m (no C++ runtime: libbsc was replaced by Kolmo's C code).
    if env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("windows") {
        println!("cargo:rustc-link-lib=pthread");
        println!("cargo:rustc-link-lib=m");
    }
    println!("cargo:rerun-if-changed={}", src.display());   // the whole tree: src/kolmo.c includes its parts
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=KOLMO_NATIVE");
    println!("cargo:rerun-if-env-changed=KOLMO_NO_ASM");
    println!("cargo:rerun-if-env-changed=KOLMO_GEAR_ASM");
}
