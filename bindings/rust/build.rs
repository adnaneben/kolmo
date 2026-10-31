// Builds the kolmo native core (static libs) with the project Makefile and links it,
// so the crate needs no prebuilt shared library at runtime.
use std::{env, path::PathBuf, process::Command};

fn main() {
    // crate dir = <repo>/bindings/rust ; project root is two levels up.
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let root = manifest.parent().unwrap().parent().unwrap().to_path_buf();
    let build_dir = root.join("build");

    let no_lzma = env::var("CARGO_FEATURE_NO_LZMA").is_ok();

    // Build the static libraries (czn core + vendored codecs).
    let mut make = Command::new("make");
    make.current_dir(&root).arg("-j").arg("build/libczn.a").arg("build/libvendor.a");
    if no_lzma {
        make.arg("NO_LZMA=1");
    }
    let status = make.status().expect("failed to run `make`; a C toolchain is required to build kolmo");
    assert!(status.success(), "`make` failed while building the kolmo native core");

    println!("cargo:rustc-link-search=native={}", build_dir.display());
    // Order matters: czn depends on symbols in vendor.
    println!("cargo:rustc-link-lib=static=czn");
    println!("cargo:rustc-link-lib=static=vendor");

    // C++ runtime (libbsc/brotli are C++), plus pthread/m, and liblzma unless disabled.
    let target = env::var("TARGET").unwrap_or_default();
    if target.contains("apple") || target.contains("darwin") {
        println!("cargo:rustc-link-lib=dylib=c++");
    } else {
        println!("cargo:rustc-link-lib=dylib=stdc++");
    }
    println!("cargo:rustc-link-lib=dylib=pthread");
    println!("cargo:rustc-link-lib=dylib=m");
    if !no_lzma {
        println!("cargo:rustc-link-lib=dylib=lzma");
    }

    println!("cargo:rerun-if-changed={}", root.join("src/czn.c").display());
    println!("cargo:rerun-if-changed={}", root.join("src/czn.h").display());
}
