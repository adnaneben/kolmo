//! Rust binding for **kolmo**, a structure-aware lossless compressor.
//!
//! Thin, safe wrappers over the native core (`kolmo.h`). The whole file is compressed or
//! decompressed in one call; [`read_range`] decodes only the blocks overlapping a byte range.
//!
//! ```no_run
//! let blob = kolmo::compress(b"country,year\nA,2020\n", kolmo::Level::COMPACT, 0).unwrap();
//! let back = kolmo::decompress(&blob, 0).unwrap();
//! ```

use std::os::raw::{c_int, c_char};

/// A compression level (docs/LEVELS.md): one scale from 1 (fastest) to 9 (max), and [`Level::ULTRA`] outside it.
/// The library's table turns a level into settings, so a level gives the same bytes as `kolmo -N` on the command
/// line and as the Python binding. Every coder is Kolmo's own; blobs are container version 12, and blobs of earlier
/// Kolmo versions fail with code -13 (`KOLMO_ERR_LEGACY`).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct Level(i32);
impl Level {
    /// Level 1: the fastest Kolmo compresses.
    pub const FASTEST: Level = Level(1);
    /// Level 3, the default: zstd -3 class speed, smaller output.
    pub const LIGHT: Level = Level(3);
    /// Level 5: markedly smaller than light at about half its compress speed.
    pub const BALANCED: Level = Level(5);
    /// Level 7: small output, still fast to decompress.
    pub const COMPACT: Level = Level(7);
    /// Level 9: the smallest output that still decompresses at practical speed.
    pub const MAX: Level = Level(9);
    /// The extreme tier, outside the scale: decompresses about as slowly as it compresses, ~1 GB+ of memory.
    pub const ULTRA: Level = Level(10);
    /// A level on the scale, 1..=9; `None` otherwise (use [`Level::ULTRA`] for the extreme tier).
    pub fn new(n: u32) -> Option<Level> {
        if (1..=9).contains(&n) { Some(Level(n as i32)) } else { None }
    }
    /// 1..=9, or 10 for ultra.
    pub fn get(self) -> u32 { self.0 as u32 }
}
impl Default for Level {
    fn default() -> Level { Level::LIGHT }
}
impl std::str::FromStr for Level {
    type Err = String;
    /// "1".."9", "ultra", or a name: "fastest", "light", "balanced", "compact", "max".
    fn from_str(s: &str) -> Result<Level, String> {
        match s {
            "fastest" => Ok(Level::FASTEST), "light" => Ok(Level::LIGHT), "balanced" => Ok(Level::BALANCED),
            "compact" => Ok(Level::COMPACT), "max" => Ok(Level::MAX), "ultra" => Ok(Level::ULTRA),
            _ => s.parse::<u32>().ok().and_then(Level::new)
                .ok_or_else(|| format!("level must be 1..9, ultra, fastest, light, balanced, compact or max, not {:?}", s)),
        }
    }
}

#[repr(C)]
struct Params {
    mode: c_int,
    level: c_int,
    threads: c_int,
    block: u32,
    transforms: c_int,
    mem_limit: u64, // 0 = auto (docs/MEMORY-MODEL.md); mirrors kolmo_params
}

#[repr(C)]
struct Limits {
    max_output_bytes: u64,
    max_block_bytes: u32,
    max_blocks: u32,
    max_memory: u64, // 0 = auto; mirrors kolmo_decode_limits
}

extern "C" {
    fn kolmo_bound(n: usize, p: *const Params) -> usize;
    fn kolmo_params_for_level(level: c_int, p: *mut Params) -> c_int;
    fn kolmo_compress(src: *const u8, n: usize, dst: *mut u8, cap: usize, p: *const Params) -> i64;
    fn kolmo_original_size(src: *const u8, n: usize) -> i64;
    fn kolmo_check_frame(src: *const u8, n: usize, limits: *const Limits) -> c_int;
    fn kolmo_decompress_limited(src: *const u8, n: usize, dst: *mut u8, cap: usize, threads: c_int, limits: *const Limits) -> i64;
    fn kolmo_read_range(src: *const u8, n: usize, off: u64, len: usize, dst: *mut u8, threads: c_int) -> i64;
    fn kolmo_strerror(code: i64) -> *const c_char;
}

/// An error returned by the native core (corrupt input, checksum mismatch, allocation, …).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    pub code: i64,
    pub message: String,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "kolmo: {} (code {})", self.message, self.code)
    }
}
impl std::error::Error for Error {}

fn err(code: i64) -> Error {
    let msg = unsafe {
        let p = kolmo_strerror(code);
        if p.is_null() { String::from("unknown error") }
        else { std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned() }
    };
    Error { code, message: msg }
}

/// Compress `data` at `level`. `threads` = 0 uses all cores (never changes the output).
pub fn compress(data: &[u8], level: Level, threads: i32) -> Result<Vec<u8>, Error> {
    compress_limited(data, level, threads, 0)
}

/// As [`compress`], with a memory limit in bytes (0 = auto, `u64::MAX` = none), as `kolmo --memlimit`: an explicit
/// limit also sizes the blocks of levels 8, 9 and ultra so that the output decodes within it. Below the level's floor
/// the call fails with code -12 (`KOLMO_ERR_MEMORY`).
pub fn compress_limited(data: &[u8], level: Level, threads: i32, mem_limit: u64) -> Result<Vec<u8>, Error> {
    let mut p = Params { mode: 0, level: 0, threads: threads as c_int, block: 0, transforms: 0, mem_limit };
    let r = unsafe { kolmo_params_for_level(level.0 as c_int, &mut p) };
    if r < 0 { return Err(err(r as i64)); }
    let cap = unsafe { kolmo_bound(data.len(), &p) };
    let mut out = vec![0u8; cap.max(1)];
    let n = unsafe { kolmo_compress(data.as_ptr(), data.len(), out.as_mut_ptr(), out.len(), &p) };
    if n < 0 { return Err(err(n)); }
    out.truncate(n as usize);
    Ok(out)
}

/// Original (decompressed) length recorded in a compressed blob's header (not checked: see [`decompress`]).
pub fn original_size(blob: &[u8]) -> Result<usize, Error> {
    let n = unsafe { kolmo_original_size(blob.as_ptr(), blob.len()) };
    if n < 0 { Err(err(n)) } else { Ok(n as usize) }
}

/// Header, index checksum, block sizes against the claimed size, and the memory the frame declares, before anything
/// is allocated from the header.
fn check_frame(blob: &[u8], mem_limit: u64) -> Result<(), Error> {
    let lim = Limits { max_output_bytes: 0, max_block_bytes: 0, max_blocks: 0, max_memory: mem_limit };
    let r = unsafe { kolmo_check_frame(blob.as_ptr(), blob.len(), &lim) };
    if r < 0 { Err(err(r as i64)) } else { Ok(()) }
}

/// Decompress a whole blob. `threads` = 0 uses all cores. The frame is checked before the output is allocated, so a
/// damaged or hostile blob is an error, not a huge allocation.
pub fn decompress(blob: &[u8], threads: i32) -> Result<Vec<u8>, Error> {
    decompress_limited(blob, threads, 0)
}

/// As [`decompress`], with a memory limit in bytes (0 = auto, `u64::MAX` = none): a frame that needs more fails with
/// code -12 (`KOLMO_ERR_MEMORY`) before anything is allocated.
pub fn decompress_limited(blob: &[u8], threads: i32, mem_limit: u64) -> Result<Vec<u8>, Error> {
    let o = original_size(blob)?;
    check_frame(blob, mem_limit)?;
    let mut out = vec![0u8; o.max(1)];
    let lim = Limits { max_output_bytes: 0, max_block_bytes: 0, max_blocks: 0, max_memory: mem_limit };
    let got = unsafe { kolmo_decompress_limited(blob.as_ptr(), blob.len(), out.as_mut_ptr(), o, threads as c_int, &lim) };
    if got < 0 { return Err(err(got)); }
    out.truncate(got as usize);
    Ok(out)
}

/// Return bytes `[off, off + len)` of the original (clamped to its end), decoding only the overlapping blocks.
pub fn read_range(blob: &[u8], off: u64, len: usize, threads: i32) -> Result<Vec<u8>, Error> {
    let total = original_size(blob)? as u64;
    check_frame(blob, 0)?;
    if off > total { return Err(err(-1)); }
    let len = (len as u64).min(total - off) as usize;
    let mut out = vec![0u8; len.max(1)];
    let got = unsafe { kolmo_read_range(blob.as_ptr(), blob.len(), off, len, out.as_mut_ptr(), threads as c_int) };
    if got < 0 { return Err(err(got)); }
    out.truncate(got as usize);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> Vec<u8> {
        let mut v = Vec::from(&b"country,year,value\n"[..]);
        for i in 0..40_000u32 {
            v.extend_from_slice(format!("Utopia,{},{}.5\n", 2000 + i % 20, i).as_bytes());
        }
        v
    }

    fn all_levels() -> Vec<Level> {
        let mut v: Vec<Level> = (1..=9).map(|n| Level::new(n).unwrap()).collect();
        v.push(Level::ULTRA);
        v
    }

    #[test]
    fn round_trip_all_levels() {
        let data = sample();
        for level in all_levels() {
            let blob = compress(&data, level, 1).expect("compress");
            assert_eq!(original_size(&blob).unwrap(), data.len(), "{:?} size", level);
            assert_eq!(decompress(&blob, 1).expect("decompress"), data, "{:?} round trip", level);
            assert_eq!(compress(&data, level, 4).unwrap(), blob, "{:?}: threads change the output", level);
        }
    }

    #[test]
    fn level_names_and_bounds() {
        assert_eq!(Level::default(), Level::LIGHT);
        assert_eq!(Level::new(0), None);
        assert_eq!(Level::new(10), None);
        assert_eq!(Level::new(1), Some(Level::FASTEST));
        assert_eq!(Level::new(9), Some(Level::MAX));
        assert_eq!("compact".parse::<Level>().unwrap(), Level::COMPACT);
        assert_eq!("ultra".parse::<Level>().unwrap(), Level::ULTRA);
        assert_eq!("5".parse::<Level>().unwrap(), Level::BALANCED);
        assert!("speed".parse::<Level>().is_err() && "0".parse::<Level>().is_err());
        let data = sample();
        assert_eq!(compress(&data, Level::default(), 1).unwrap(), compress(&data, Level::new(3).unwrap(), 1).unwrap());
    }

    #[test]
    fn range_read() {
        let data = sample();
        let blob = compress(&data, Level::LIGHT, 1).unwrap();
        let mid = data.len() / 2;
        assert_eq!(read_range(&blob, mid as u64, 64, 1).unwrap(), &data[mid..mid + 64]);
        assert_eq!(read_range(&blob, mid as u64, usize::MAX, 1).unwrap(), &data[mid..]);
    }

    #[test]
    fn empty_and_tiny() {
        for d in [&b""[..], &b"x"[..], &b"hello world"[..]] {
            for level in all_levels() {
                let blob = compress(d, level, 1).unwrap();
                assert_eq!(decompress(&blob, 1).unwrap(), d);
            }
        }
    }

    #[test]
    fn memory_limit() {
        let data = sample();
        let blob = compress_limited(&data, Level::MAX, 1, 4 << 30).unwrap();
        assert_eq!(decompress_limited(&blob, 1, u64::MAX).unwrap(), data);
        assert_eq!(compress_limited(&data, Level::MAX, 1, 1 << 20).unwrap_err().code, -12);
    }

    #[test]
    fn hostile_header_refused_before_allocation() {
        // 32 bytes claiming a 40 GiB original: checked (index CRC, sizes) before any output is allocated
        let mut h = vec![0u8; 32];
        h[..5].copy_from_slice(b"CZN1\x0c");
        h[12..20].copy_from_slice(&(40u64 << 30).to_le_bytes());
        assert!(decompress(&h, 1).is_err());
        assert!(read_range(&h, 0, 1 << 30, 1).is_err());
    }

    #[test]
    fn legacy_version_refused() {
        // Every blob is container version 12; the same blob relabelled as version 11 is refused as legacy (-13).
        let data = sample();
        let mut blob = compress(&data, Level::LIGHT, 1).unwrap();
        assert_eq!(&blob[..5], b"CZN1\x0c");
        blob[4] = 11;
        let e = decompress(&blob, 1).unwrap_err();
        assert_eq!(e.code, -13);
        assert!(e.message.contains("written by an earlier Kolmo"), "{}", e.message);
    }
}
