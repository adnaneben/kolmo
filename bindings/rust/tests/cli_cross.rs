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
