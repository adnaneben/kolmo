// Compiles the kolmo native core (src/kolmo.c, src/arch/xxh3.c and src/arch/<isa>/*.S + vendored zstd, lz4, brotli, libbsc, xxhash and
// the liblzma subset) into a static library and links it, so the published crate is self-contained: it needs a C/C++
// compiler, but no `make`, no system library, no prebuilt library, and no external Rust
// crates. The C sources live in `native/` inside the crate — refreshed from the repo root by
// `sync-native.sh` — with a fallback to the repo tree for in-place contributor builds.
use std::{env, fs, path::{Path, PathBuf}, process::Command, sync::{Arc, Mutex}, thread};

fn list(dir: &Path, ext: &str, recurse: bool, out: &mut Vec<PathBuf>) {
    if let Ok(rd) = fs::read_dir(dir) {
        for e in rd.flatten() {
            let p = e.path();
            if p.is_dir() {
                if recurse { list(&p, ext, true, out); }
            } else if p.extension().map_or(false, |x| x == ext) {
                out.push(p);
            }
        }
    }
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    // Prefer the bundled copy (present in the published crate); fall back to the repo tree.
    let bundled = manifest.join("native");
    let base = if bundled.join("src/kolmo.c").exists() {
        bundled
    } else {
        manifest.parent().unwrap().parent().unwrap().to_path_buf()
    };
    let src = base.join("src");
    let vendor = base.join("vendor");
    let zstd = vendor.join("zstd");
    let brotli = vendor.join("brotli");
    let lz4 = vendor.join("lz4");
    let bsc = vendor.join("libbsc");
    let xz = vendor.join("xz");

    let no_lzma = env::var("CARGO_FEATURE_NO_LZMA").is_ok();
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let is_x86_64 = arch == "x86_64";
    let cc = env::var("CC").unwrap_or_else(|_| "cc".into());
    let cxx = env::var("CXX").unwrap_or_else(|_| "c++".into());

    let includes = [
        src.clone(), zstd.clone(), lz4.clone(), bsc.clone(), brotli.join("include"), xz.clone(),
    ];
    let inc_flags: Vec<String> = includes.iter().map(|p| format!("-I{}", p.display())).collect();
    let mut defs = vec![
        "-DXXH_NAMESPACE=ZSTD_".to_string(),
        "-DZSTD_MULTITHREAD".to_string(),
        "-DZSTD_LEGACY_SUPPORT=0".to_string(),
        "-DKOLMO_NO_GPU".to_string(),
    ];
    // KOLMO_NO_ASM=1: no assembly kernels (src/arch/<isa>/*.S), the C references only.
    let no_asm = env::var("KOLMO_NO_ASM").map_or(false, |v| v == "1");
    if no_asm { defs.push("-DKOLMO_NO_ASM".into()); }
    if !is_x86_64 { defs.push("-DZSTD_DISABLE_ASM".into()); }
    // liblzma: the vendored subset (vendor/xz, raw LZMA2 only), compiled with its own config and symbol namespace.
    if no_lzma { defs.push("-DKOLMO_NO_LZMA".into()); }
    let mut xz_flags = vec!["-DHAVE_CONFIG_H".to_string(), "-include".into(), xz.join("kolmo_lzma_namespace.h").display().to_string()];
    for d in ["api", "common", "check", "lz", "lzma", "rangecoder", "simple", "delta"] {
        xz_flags.push(format!("-I{}", xz.join("liblzma").join(d).display()));
    }
    xz_flags.push(format!("-I{}", xz.join("common").display()));
    // Portable by default: the target's baseline ISA, like the release binaries and the Python wheels. On x86-64 the
    // AVX2/SSE4.2 paths are chosen at run time by CPUID, so baseline builds lose little. KOLMO_NATIVE=1 builds for
    // the host CPU instead (the library then runs only on CPUs like the build machine).
    let native = env::var("KOLMO_NATIVE").map_or(false, |v| v == "1");
    let arch_flag: Option<&str> = if !native { None } else if is_x86_64 { Some("-march=native") } else { Some("-mcpu=native") };

    // Gather compile jobs: (compiler, source, extra_flags). Every C and C++ file gets -ffp-contract=off (added below):
    // kolmo.c and Brotli's encoder decide with floats, and FMA contraction would change the frame bytes.
    let mut jobs: Vec<(String, PathBuf, Vec<String>)> = Vec::new();
    jobs.push((cc.clone(), src.join("kolmo.c"), vec![]));
    let mut cfiles = Vec::new();
    for sub in ["common", "compress", "decompress", "dictBuilder"] { list(&zstd.join(sub), "c", false, &mut cfiles); }
    for sub in ["common", "dec", "enc"] { list(&brotli.join(sub), "c", false, &mut cfiles); }
    cfiles.push(lz4.join("lz4.c"));
    cfiles.push(lz4.join("lz4hc.c"));
    cfiles.push(src.join("arch/xxh3.c"));          // compiled apart from src/kolmo.c (docs/LAYOUT.md)
    cfiles.push(bsc.join("libbsc/bwt/libsais/libsais.c"));
    cfiles.sort();
    for f in cfiles { jobs.push((cc.clone(), f, vec![])); }
    if !no_lzma {
        let mut xfiles = Vec::new();
        list(&xz, "c", true, &mut xfiles);
        xfiles.sort();
        for f in xfiles { jobs.push((cc.clone(), f, xz_flags.clone())); }
    }
    if is_x86_64 { jobs.push((cc.clone(), zstd.join("decompress/huf_decompress_amd64.S"), vec![])); }
    // Kolmo's own assembly kernels for the target ISA.
    let isa = if is_x86_64 { Some("x86_64") } else if arch == "aarch64" { Some("aarch64") } else { None };
    if let (Some(isa), false) = (isa, no_asm) {
        let mut sfiles = Vec::new();
        list(&src.join("arch").join(isa), "S", false, &mut sfiles);
        sfiles.sort();
        for f in sfiles { jobs.push((cc.clone(), f, vec![])); }
    }
    // libbsc C++ sources.
    let mut cpps = Vec::new();
    list(&bsc.join("libbsc"), "cpp", true, &mut cpps);
    cpps.sort();
    for f in cpps { jobs.push((cxx.clone(), f, vec![])); }

    // Compile in parallel; collect object paths.
    let nproc = thread::available_parallelism().map(|n| n.get()).unwrap_or(4);
    let jobs = Arc::new(Mutex::new(jobs.into_iter().enumerate().collect::<Vec<_>>()));
    let objs = Arc::new(Mutex::new(Vec::<PathBuf>::new()));
    let failed = Arc::new(Mutex::new(Vec::<String>::new()));
    let mut handles = Vec::new();
    for _ in 0..nproc {
        let jobs = Arc::clone(&jobs);
        let objs = Arc::clone(&objs);
        let failed = Arc::clone(&failed);
        let (out, inc_flags, defs, arch_flag, bsc) =
            (out.clone(), inc_flags.clone(), defs.clone(), arch_flag.map(str::to_string), bsc.clone());
        handles.push(thread::spawn(move || loop {
            let job = { jobs.lock().unwrap().pop() };
            let Some((idx, (compiler, file, extra))) = job else { break };
            let obj = out.join(format!("k{idx}.o"));
            let mut cmd = Command::new(&compiler);
            cmd.arg("-O3").args(arch_flag.iter()).arg("-fPIC").arg("-c");
            if file.extension().map_or(true, |e| e != "S") { cmd.arg("-ffp-contract=off"); }
            cmd.args(&defs).args(&inc_flags).args(&extra);
            if file.extension().map_or(false, |e| e == "cpp") { cmd.arg(format!("-I{}", bsc.display())); }
            cmd.arg(&file).arg("-o").arg(&obj);
            match cmd.status() {
                Ok(s) if s.success() => objs.lock().unwrap().push(obj),
                _ => failed.lock().unwrap().push(file.display().to_string()),
            }
        }));
    }
    for h in handles { h.join().unwrap(); }
    let failed = Arc::try_unwrap(failed).unwrap().into_inner().unwrap();
    if !failed.is_empty() { panic!("kolmo: failed to compile: {}", failed.join(", ")); }

    // Archive into a static lib.
    let objs = Arc::try_unwrap(objs).unwrap().into_inner().unwrap();
    let lib = out.join("libkolmo_native.a");
    let _ = fs::remove_file(&lib);
    let ar = env::var("AR").unwrap_or_else(|_| "ar".into());
    let st = Command::new(&ar).arg("crs").arg(&lib).args(&objs).status().expect("ar");
    assert!(st.success(), "kolmo: ar failed");

    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=kolmo_native");
    // C++ runtime (libbsc pulls it in), plus pthread/m.
    if os == "macos" || os == "ios" { println!("cargo:rustc-link-lib=c++"); }
    else { println!("cargo:rustc-link-lib=stdc++"); }
    println!("cargo:rustc-link-lib=pthread");
    println!("cargo:rustc-link-lib=m");

    println!("cargo:rerun-if-changed={}", src.display());   // the whole tree: src/kolmo.c includes its parts
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=KOLMO_NATIVE");
    println!("cargo:rerun-if-env-changed=KOLMO_NO_ASM");
}
