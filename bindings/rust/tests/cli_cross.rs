//! The same bytes as the command line: `kolmo -N -c FILE` equals `kolmo::compress(FILE, Level N)` at every level.
//! Uses ../../build/kolmo (or $KOLMO_CLI); skipped when the CLI is not built.
use std::path::PathBuf;
use std::process::Command;

#[test]
fn cli_and_rust_give_the_same_bytes() {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let cli = std::env::var_os("KOLMO_CLI").map(PathBuf::from).unwrap_or_else(|| manifest.join("../../build/kolmo"));
    if !cli.exists() {
        eprintln!("{} not built: cross-check skipped", cli.display());
        return;
    }
    let mut data = Vec::from(&b"country,year,co2\n"[..]);
    for i in 0..40_000u32 {
        data.extend_from_slice(format!("Utopia,{},{}.5\n", 2000 + i % 20, i).as_bytes());
    }
    let dir = std::env::temp_dir().join(format!("kolmo-cli-cross-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let src = dir.join("in.csv");
    std::fs::write(&src, &data).unwrap();
    let mut levels: Vec<(String, kolmo::Level)> = (1..=9).map(|n| (format!("-{}", n), kolmo::Level::new(n).unwrap())).collect();
    levels.push(("-U".into(), kolmo::Level::ULTRA));
    for (flag, level) in levels {
        let out = Command::new(&cli).arg(&flag).arg("-c").arg(&src).output().expect("run kolmo");
        assert!(out.status.success(), "kolmo {} failed", flag);
        assert_eq!(out.stdout, kolmo::compress(&data, level, 0).unwrap(), "CLI {} and Rust {:?} differ", flag, level);
    }
    std::fs::remove_dir_all(&dir).ok();
}

/// Files the command line writes decode here (bug 11 of the review40 synthesis): `--stream` (CZNS, also the CLI's
/// output for pipes over 256 MiB), concatenated outputs, and a stream followed by a frame.
#[test]
fn cli_stream_and_concatenated_files() {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let cli = std::env::var_os("KOLMO_CLI").map(PathBuf::from).unwrap_or_else(|| manifest.join("../../build/kolmo"));
    if !cli.exists() {
        eprintln!("{} not built: CLI stream files skipped", cli.display());
        return;
    }
    let mut data = Vec::new();
    for i in 0..200_000u32 {
        data.extend_from_slice(format!("Utopia,{},{}.5,{}\n", 2000 + i % 20, i, i.wrapping_mul(2_654_435_761) % 1000).as_bytes());
    }
    data.truncate(3 << 20);                    // three 1 MiB segments
    let dir = std::env::temp_dir().join(format!("kolmo-cli-stream-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let src = dir.join("in.csv");
    std::fs::write(&src, &data).unwrap();
    let run = |args: &[&str]| {
        let out = Command::new(&cli).args(args).arg(&src).output().expect("run kolmo");
        assert!(out.status.success(), "kolmo {:?} failed", args);
        out.stdout
    };
    let st = run(&["-3", "-T", "1", "--stream=1", "-c"]);
    let fr = run(&["-1", "-c"]);
    assert_eq!(&st[..4], b"CZNS");
    let twice = [&data[..], &data[..]].concat();
    for (name, blob, want) in [("--stream", st.clone(), &data), ("frame + frame", [&fr[..], &fr[..]].concat(), &twice),
                               ("--stream + frame", [&st[..], &fr[..]].concat(), &twice)] {
        assert_eq!(&kolmo::decompress(&blob, 2).unwrap(), want, "{}", name);
        assert_eq!(kolmo::original_size(&blob).unwrap(), want.len(), "{}", name);
        let off = (1usize << 20) - 7;          // across the first segment boundary
        assert_eq!(kolmo::read_range(&blob, off as u64, 100, 1).unwrap(), &want[off..off + 100], "{}", name);
    }
    std::fs::remove_dir_all(&dir).ok();
}
