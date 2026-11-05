// Compiles the kolmo native core (czn.c + vendored zstd, lz4, brotli, libbsc, xxhash) into a
// static library and links it, so the published crate is self-contained: it needs a C/C++
// compiler and (optionally) liblzma, but no `make`, no prebuilt library, and no external Rust
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

// Probe for liblzma: compile a tiny `#include <lzma.h>` against each candidate include dir
// (default search path first, then Homebrew/MacPorts prefixes). Returns the include and matching
// lib dir to use (both may be None when it is on the default path), or None if liblzma is absent.
fn find_lzma(cc: &str, out: &Path) -> Option<(Option<String>, Option<String>)> {
    let probe = out.join("lzma_probe.c");
    let _ = fs::write(&probe, "#include <lzma.h>\nint main(void){return 0;}\n");
    let mut prefixes: Vec<String> = Vec::new();
    for args in [&["--prefix", "xz"][..], &["--prefix"][..]] {
        if let Ok(o) = Command::new("brew").args(args).output() {
            if o.status.success() {
                if let Ok(s) = String::from_utf8(o.stdout) {
                    let p = s.trim().to_string();
                    if !p.is_empty() { prefixes.push(p); }
                }
            }
        }
    }
    prefixes.extend(["/opt/homebrew".into(), "/usr/local".into(), "/opt/local".into()]);
    // First try the default include path (no -I): typical on Linux with liblzma-dev.
    let mut candidates: Vec<Option<String>> = vec![None];
    candidates.extend(prefixes.into_iter().map(Some));
    for inc in candidates {
        let mut cmd = Command::new(cc);
        cmd.arg("-fsyntax-only");
        if let Some(dir) = &inc { cmd.arg(format!("-I{dir}/include")); }
        cmd.arg(&probe);
        if cmd.status().map_or(false, |s| s.success()) {
            return Some(match inc {
                Some(d) => (Some(format!("{d}/include")), Some(format!("{d}/lib"))),
                None => (None, None),
            });
        }
    }
    None
}

fn main() {
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    // Prefer the bundled copy (present in the published crate); fall back to the repo tree.
    let bundled = manifest.join("native");
    let base = if bundled.join("src/czn.c").exists() {
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

    let no_lzma = env::var("CARGO_FEATURE_NO_LZMA").is_ok();
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    let is_x86_64 = arch == "x86_64";
    let cc = env::var("CC").unwrap_or_else(|_| "cc".into());
    let cxx = env::var("CXX").unwrap_or_else(|_| "c++".into());

    let includes = [
        src.clone(), zstd.clone(), lz4.clone(), bsc.clone(), brotli.join("include"),
    ];
    let inc_flags: Vec<String> = includes.iter().map(|p| format!("-I{}", p.display())).collect();
    let mut defs = vec![
        "-DXXH_NAMESPACE=ZSTD_".to_string(),
        "-DZSTD_MULTITHREAD".to_string(),
        "-DZSTD_LEGACY_SUPPORT=0".to_string(),
    ];
    if !is_x86_64 { defs.push("-DZSTD_DISABLE_ASM".into()); }
    // liblzma powers the max/ultra LZMA path. Use it when present; otherwise build without it
    // (those tiers fall back to BWT/context-mixing) so the crate still builds with no system deps.
    let lzma = if no_lzma { None } else { find_lzma(&cc, &out) };
    let use_lzma = lzma.is_some();
    if !use_lzma {
        defs.push("-DCZN_NO_LZMA".into());
        if !no_lzma {
            println!("cargo:warning=kolmo: liblzma not found; building without it (max/ultra use BWT/CM). Install xz (brew install xz / apt install liblzma-dev) for the LZMA path.");
        }
    } else if let Some((Some(inc), _)) = &lzma {
        defs.push(format!("-I{inc}"));
    }
    let arch_flag = if is_x86_64 { "-march=native" } else { "-mcpu=native" };

    // Gather compile jobs: (compiler, source, extra_flags).
    let mut jobs: Vec<(String, PathBuf, Vec<String>)> = Vec::new();
    // czn.c needs -ffp-contract=off for byte-identical output across compilers/CPUs.
    jobs.push((cc.clone(), src.join("czn.c"), vec!["-ffp-contract=off".into()]));
    let mut cfiles = Vec::new();
    for sub in ["common", "compress", "decompress", "dictBuilder"] { list(&zstd.join(sub), "c", false, &mut cfiles); }
    for sub in ["common", "dec", "enc"] { list(&brotli.join(sub), "c", false, &mut cfiles); }
    cfiles.push(lz4.join("lz4.c"));
    cfiles.push(lz4.join("lz4hc.c"));
    cfiles.push(vendor.join("xxhash/czn_xxh3.c"));
    cfiles.push(bsc.join("libbsc/bwt/libsais/libsais.c"));
    cfiles.sort();
    for f in cfiles { jobs.push((cc.clone(), f, vec![])); }
    if is_x86_64 { jobs.push((cc.clone(), zstd.join("decompress/huf_decompress_amd64.S"), vec![])); }
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
            (out.clone(), inc_flags.clone(), defs.clone(), arch_flag.to_string(), bsc.clone());
        handles.push(thread::spawn(move || loop {
            let job = { jobs.lock().unwrap().pop() };
            let Some((idx, (compiler, file, extra))) = job else { break };
            let obj = out.join(format!("k{idx}.o"));
            let mut cmd = Command::new(&compiler);
            cmd.arg("-O3").arg(&arch_flag).arg("-fPIC").arg("-c")
                .args(&defs).args(&inc_flags).args(&extra);
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
    // C++ runtime (libbsc/brotli pull it in), plus pthread/m and optional liblzma.
    if os == "macos" || os == "ios" { println!("cargo:rustc-link-lib=c++"); }
    else { println!("cargo:rustc-link-lib=stdc++"); }
    println!("cargo:rustc-link-lib=pthread");
    println!("cargo:rustc-link-lib=m");
    if use_lzma {
        if let Some((_, Some(libdir))) = &lzma { println!("cargo:rustc-link-search=native={libdir}"); }
        println!("cargo:rustc-link-lib=lzma");
    }

    println!("cargo:rerun-if-changed={}", src.join("czn.c").display());
    println!("cargo:rerun-if-changed={}", src.join("czn.h").display());
    println!("cargo:rerun-if-changed=build.rs");
}
