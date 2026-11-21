// Compiles the kolmo native core (src/kolmo.c, src/arch/<isa>/*.S, Kolmo's own coders and BWT pipeline; no third-party
// codec library) into a static library and links it, so the published crate is self-contained:
// it needs a C compiler, but no `make`, no system library, no prebuilt library, and no external Rust
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
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let is_x86_64 = arch == "x86_64";
    let cc = env::var("CC").unwrap_or_else(|_| "cc".into());

    let inc_flags: Vec<String> = vec![format!("-I{}", src.display())];
    let mut defs = vec!["-DKOLMO_NO_GPU".to_string()];
    // KOLMO_NO_ASM=1: no assembly kernels (src/arch/<isa>/*.S), the C references only.
    let no_asm = env::var("KOLMO_NO_ASM").map_or(false, |v| v == "1");
    if no_asm { defs.push("-DKOLMO_NO_ASM".into()); }
    // Portable by default: the target's baseline ISA, like the release binaries and the Python wheels. On x86-64 the
    // AVX2/SSE4.2 paths are chosen at run time by CPUID, so baseline builds lose little. KOLMO_NATIVE=1 builds for
    // the host CPU instead (the library then runs only on CPUs like the build machine).
    let native = env::var("KOLMO_NATIVE").map_or(false, |v| v == "1");
    let arch_flag: Option<&str> = if !native { None } else if is_x86_64 { Some("-march=native") } else { Some("-mcpu=native") };

    // Gather compile jobs: (compiler, source, extra_flags). Every C file gets -ffp-contract=off (added below):
    // some encoder choices are made with float costs, and FMA contraction would change the frame bytes.
    let mut jobs: Vec<(String, PathBuf, Vec<String>)> = Vec::new();
    jobs.push((cc.clone(), src.join("kolmo.c"), vec![]));
    let mut cfiles = Vec::new();
    // the block-sorting pipeline, also compiled on its own (LZP, QLFC, the libsais subset, the BWT stream formats)
    for f in ["algo/lzp.c", "algo/qlfc.c", "algo/par.c", "codecs/bwt_stream.c"] { cfiles.push(src.join(f)); }
    // Kolmo's own coders (codecs 24, 27-29, 32) and kolmo_khash64, also compiled on their own
    for f in ["algo/khuf.c", "algo/krans.c", "algo/klz.c", "algo/klzma.c", "algo/khash.c"] { cfiles.push(src.join(f)); }
    list(&src.join("algo/sais"), "c", false, &mut cfiles);
    cfiles.sort();
    for f in cfiles { jobs.push((cc.clone(), f, vec![])); }
    // Kolmo's own assembly kernels for the target ISA.
    let isa = if is_x86_64 { Some("x86_64") } else if arch == "aarch64" { Some("aarch64") } else { None };
    if let (Some(isa), false) = (isa, no_asm) {
        let mut sfiles = Vec::new();
        list(&src.join("arch").join(isa), "S", false, &mut sfiles);
        sfiles.sort();
        for f in sfiles { jobs.push((cc.clone(), f, vec![])); }
    }
    // Dispatched kernels (src/arch/cpu.c): C references always, x86-64 versions unless KOLMO_NO_ASM (Makefile: KSRC).
    for f in ["arch/cpu.c", "algo/predict.c", "algo/planes.c", "algo/mixer.c", "algo/video.c"] { jobs.push((cc.clone(), src.join(f), vec![])); }
    if is_x86_64 && !no_asm {
        for f in ["med8_sse2.c", "planes_sse2.c", "mixer_avx2.c", "video_sse2.c", "img16_sse2.c", "med8f_avx2.c"] { jobs.push((cc.clone(), src.join("arch/x86_64").join(f), vec![])); }
    }

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
        let (out, inc_flags, defs, arch_flag) =
            (out.clone(), inc_flags.clone(), defs.clone(), arch_flag.map(str::to_string));
        handles.push(thread::spawn(move || loop {
            let job = { jobs.lock().unwrap().pop() };
            let Some((idx, (compiler, file, extra))) = job else { break };
            let obj = out.join(format!("k{idx}.o"));
            let mut cmd = Command::new(&compiler);
            cmd.arg("-O3").args(arch_flag.iter()).arg("-fPIC").arg("-c");
            if file.extension().map_or(true, |e| e != "S") { cmd.arg("-ffp-contract=off"); }
            cmd.args(&defs).args(&inc_flags).args(&extra);
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
    // pthread/m (no C++ runtime: libbsc was replaced by Kolmo's C code).
    println!("cargo:rustc-link-lib=pthread");
    println!("cargo:rustc-link-lib=m");

    println!("cargo:rerun-if-changed={}", src.display());   // the whole tree: src/kolmo.c includes its parts
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=KOLMO_NATIVE");
    println!("cargo:rerun-if-env-changed=KOLMO_NO_ASM");
}
