use std::{env, fs};
fn main() {
    let args: Vec<_> = env::args_os().collect();
    let input = fs::read(&args[1]).unwrap();
    let frame = kolmo::compress_limited(&input, kolmo::Level::ULTRA, 1, 256 << 20).unwrap();
    assert_eq!(kolmo::decompress(&frame, 1).unwrap(), input);
    fs::write(&args[2], frame).unwrap();
}
