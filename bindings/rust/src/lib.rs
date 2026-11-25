//! Rust binding for **kolmo**, a structure-aware lossless compressor.
//!
//! Thin, safe wrappers over the native core (`kolmo.h`). The whole file is compressed or
//! decompressed in one call; [`read_range`] decodes only the blocks overlapping a byte range.
//!
//! The decoding functions read whatever the command line writes: one frame (`CZN1`), several frames one after
//! another, and `kolmo --stream` output (`CZNS`, also what the CLI writes for pipes over 256 MiB).
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
    fn kolmo_frame_size(src: *const u8, n: usize) -> i64;
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
    let cap = unsafe { kolmo_bound(data.len(), &p) }.max(1);
    let mut out = out_buf(cap as u64)?;
    let n = unsafe { kolmo_compress(data.as_ptr(), data.len(), out.as_mut_ptr(), cap, &p) };
    if n < 0 { return Err(err(n)); }
    let written = usize::try_from(n).map_err(|_| err(ERR_CORRUPT))?;
    if written > cap { return Err(err(ERR_CORRUPT)); }
    // SAFETY: compression initialized the reported bytes within the reserved buffer.
    unsafe { out.set_len(written) };
    Ok(out)
}

const ERR_CORRUPT: i64 = -1;
const ERR_ALLOC: i64 = -4;
const ERR_LIMIT: i64 = -7;
const ERR_RANGE: i64 = -16;

/// The CZN1 frames of `blob`, in order, as (offset, length): one frame, frames one after another (as
/// `cat a.kz b.kz`), and CZNS streams (`kolmo --stream`, and the CLI's output for pipes over 256 MiB: `"CZNS" 1 0 0 0
/// segment_bytes u32`, then `length u64 | frame`, ending with a zero length; FORMAT.md section 10), in any order. Only
/// headers, indexes and stream lengths are read; anything else (trailing bytes, a truncated frame, a stream with no
/// terminator) is code -1. Payloads are checked later, frame by frame, before anything is allocated.
fn frames(blob: &[u8]) -> Result<Vec<(usize, usize)>, Error> {
    let n = blob.len();
    if n == 0 { return Err(err(ERR_CORRUPT)); }
    let mut out = Vec::new();
    let mut off = 0usize;
    while off < n {
        let rest = &blob[off..];
        if rest.starts_with(b"CZNS") {
            if rest.len() < 12 || rest[4] != 1 { return Err(err(ERR_CORRUPT)); }
            let mut p = off + 12;
            loop {
                if n - p < 8 { return Err(err(ERR_CORRUPT)); }          // no terminator
                let len = u64::from_le_bytes(blob[p..p + 8].try_into().unwrap());
                p += 8;
                if len == 0 { break; }
                if len > (n - p) as u64 { return Err(err(ERR_CORRUPT)); }  // truncated
                let len = len as usize;
                let e = unsafe { kolmo_frame_size(blob.as_ptr().add(p), len) };  // exactly one frame per segment
                if e < 0 { return Err(err(e)); }
                if e as u64 != len as u64 { return Err(err(ERR_CORRUPT)); }
                out.push((p, len));
                p += len;
            }
            off = p;
        } else if rest.starts_with(b"CZN1") {
            let e = unsafe { kolmo_frame_size(rest.as_ptr(), rest.len()) };
            if e < 0 { return Err(err(e)); }
            if e as u64 > rest.len() as u64 || e == 0 { return Err(err(ERR_CORRUPT)); }
            out.push((off, e as usize));
            off += e as usize;
        } else {
            return Err(err(ERR_CORRUPT));
        }
    }
    Ok(out)
}

/// The claimed decoded size of each frame (header and index checked).
fn sizes(blob: &[u8], frames: &[(usize, usize)]) -> Result<Vec<u64>, Error> {
    frames.iter().map(|&(o, l)| {
        let r = unsafe { kolmo_original_size(blob.as_ptr().add(o), l) };
        if r < 0 { Err(err(r)) } else { Ok(r as u64) }
    }).collect()
}

/// An empty Vec with room for `n` bytes, or code -4 (`KOLMO_ERR_ALLOC`) when the system refuses: never an abort.
/// Nothing is written to it here, so the pages are committed only as native code writes them: a frame that claims
/// gigabytes and fails at its first block costs a few pages, not the claim.
fn out_buf(n: u64) -> Result<Vec<u8>, Error> {
    let n = usize::try_from(n).map_err(|_| err(ERR_ALLOC))?;
    let mut v = Vec::new();
    v.try_reserve_exact(n.max(1)).map_err(|_| err(ERR_ALLOC))?;
    Ok(v)
}

/// Original (decompressed) length recorded in a compressed blob's headers (not checked: see [`decompress`]): the
/// frame's claim, or the sum of the claims of concatenated frames and CZNS streams. A single frame needs only its
/// header and index.
pub fn original_size(blob: &[u8]) -> Result<usize, Error> {
    if blob.starts_with(b"CZN1") {
        let e = unsafe { kolmo_frame_size(blob.as_ptr(), blob.len()) };
        if e < 0 || e as u64 >= blob.len() as u64 {     // one frame (or its first part): its header answers
            let n = unsafe { kolmo_original_size(blob.as_ptr(), blob.len()) };
            return if n < 0 { Err(err(n)) } else { usize::try_from(n).map_err(|_| err(ERR_ALLOC)) };
        }
    }
    let f = frames(blob)?;
    let t = sizes(blob, &f)?.iter().try_fold(0u64, |a, &s| a.checked_add(s)).ok_or_else(|| err(ERR_CORRUPT))?;
    usize::try_from(t).map_err(|_| err(ERR_ALLOC))
}

/// Header, index checksum, block sizes against the claimed size, and the memory the frame declares, before anything
/// is allocated from the header.
fn check_frame(frame: &[u8], lim: &Limits) -> Result<(), Error> {
    let r = unsafe { kolmo_check_frame(frame.as_ptr(), frame.len(), lim) };
    if r < 0 { Err(err(r as i64)) } else { Ok(()) }
}

/// Decompress a whole blob: one frame, frames one after another, or a CZNS stream. `threads` = 0 uses all cores.
/// Every frame is checked before the output is reserved, the output's pages are committed only as they are decoded,
/// and an allocation the system refuses is an error (code -4), so a damaged or hostile blob is an error, not a huge
/// allocation or an abort.
pub fn decompress(blob: &[u8], threads: i32) -> Result<Vec<u8>, Error> {
    decompress_limited(blob, threads, 0, None)
}

/// As [`decompress`], with a memory limit in bytes (0 = auto, `u64::MAX` = none): a frame that needs more fails with
/// code -12 (`KOLMO_ERR_MEMORY`) before anything is allocated. `max_output` bounds the decoded size of the whole blob:
/// a valid frame can claim well over 100,000 times its own size (long runs), so callers decoding untrusted input
/// should pass `Some(bytes)`; a blob that claims more fails with code -7 (`KOLMO_ERR_LIMIT`) before anything is
/// allocated. `None` accepts any size the frames validly claim.
pub fn decompress_limited(blob: &[u8], threads: i32, mem_limit: u64, max_output: Option<u64>) -> Result<Vec<u8>, Error> {
    let f = frames(blob)?;
    // max_output_bytes 0 means no limit in the library; the bound on the whole blob is applied below
    let lim = Limits { max_output_bytes: max_output.unwrap_or(0), max_block_bytes: 0, max_blocks: 0, max_memory: mem_limit };
    for &(o, l) in &f { check_frame(&blob[o..o + l], &lim)?; }
    let sz = sizes(blob, &f)?;
    let total = sz.iter().try_fold(0u64, |a, &s| a.checked_add(s)).ok_or_else(|| err(ERR_CORRUPT))?;
    if let Some(m) = max_output { if total > m { return Err(err(ERR_LIMIT)); } }
    let mut out = out_buf(total)?;
    let mut got = 0usize;
    for (&(o, l), &s) in f.iter().zip(&sz) {
        // s <= total - got: the sizes add up to total, and each frame wrote exactly its size so far
        let r = unsafe {
            kolmo_decompress_limited(blob.as_ptr().add(o), l, out.as_mut_ptr().add(got), s as usize, threads as c_int, &lim)
        };
        if r < 0 { return Err(err(r)); }
        if r as u64 != s { return Err(err(ERR_CORRUPT)); }
        got += s as usize;
    }
    // SAFETY: the decoder initialised out[..got] (each call wrote exactly its frame's size, checked above)
    unsafe { out.set_len(got) };
    Ok(out)
}

/// Return bytes `[off, off + len)` of the original (clamped to its end), decoding only the overlapping blocks. An
/// offset past the end is code -16 (`KOLMO_ERR_RANGE`).
pub fn read_range(blob: &[u8], off: u64, len: usize, threads: i32) -> Result<Vec<u8>, Error> {
    let f = frames(blob)?;
    let lim = Limits { max_output_bytes: 0, max_block_bytes: 0, max_blocks: 0, max_memory: 0 };
    for &(o, l) in &f { check_frame(&blob[o..o + l], &lim)?; }   // before allocating anything from the headers
    let sz = sizes(blob, &f)?;
    let total = sz.iter().try_fold(0u64, |a, &s| a.checked_add(s)).ok_or_else(|| err(ERR_CORRUPT))?;
    if off > total { return Err(err(ERR_RANGE)); }
    let end = off + (len as u64).min(total - off);
    let mut out = out_buf(end - off)?;
    let (mut got, mut start) = (0usize, 0u64);
    for (&(o, l), &s) in f.iter().zip(&sz) {
        let (lo, hi) = (off.max(start), end.min(start + s));
        if lo < hi {
            let want = (hi - lo) as usize;
            let r = unsafe {
                kolmo_read_range(blob.as_ptr().add(o), l, lo - start, want, out.as_mut_ptr().add(got), threads as c_int)
            };
            if r < 0 { return Err(err(r)); }
            if r as u64 != hi - lo { return Err(err(ERR_CORRUPT)); }
            got += want;
        }
        start += s;
        if start >= end { break; }
    }
    // SAFETY: the decoder initialised out[..got] (each call wrote exactly what it was asked for, checked above)
    unsafe { out.set_len(got) };
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
        assert_eq!(decompress_limited(&blob, 1, u64::MAX, None).unwrap(), data);
        assert_eq!(decompress_limited(&blob, 1, 0, Some(data.len() as u64)).unwrap(), data);
        assert_eq!(decompress_limited(&blob, 1, 0, Some(data.len() as u64 - 1)).unwrap_err().code, -7);
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

    extern "C" { fn kolmo_crc32c(crc: u32, buf: *const u8, len: usize) -> u32; }

    /// A CZNS stream (FORMAT.md section 10) wrapping the given frames.
    fn czns(frames: &[&[u8]]) -> Vec<u8> {
        let mut v = Vec::from(&b"CZNS\x01\x00\x00\x00"[..]);
        v.extend_from_slice(&(1u32 << 20).to_le_bytes());
        for f in frames { v.extend_from_slice(&(f.len() as u64).to_le_bytes()); v.extend_from_slice(f); }
        v.extend_from_slice(&[0u8; 8]);
        v
    }

    /// A frame whose header and index are valid (CRC included) and whose nb blocks each claim `raw` bytes from a
    /// 5-byte payload, as review R23's forge.c: the claim is consistent, the payloads are garbage.
    fn forge(nb: u32, raw: u32, kmem: u8) -> Vec<u8> {
        let mut h = vec![0u8; 32];
        h[..4].copy_from_slice(b"CZN1"); h[4] = 12; h[5] = 1; h[11] = kmem;
        h[12..20].copy_from_slice(&(raw as u64 * nb as u64).to_le_bytes());
        h[20..24].copy_from_slice(&nb.to_le_bytes());
        h[24..28].copy_from_slice(&raw.to_le_bytes());
        let mut idx = Vec::new();
        for _ in 0..nb {
            idx.extend_from_slice(&raw.to_le_bytes()); idx.extend_from_slice(&5u32.to_le_bytes());
            idx.extend_from_slice(&[0, 0, 0, 0, 0x20, 0, 0, 0]);
        }
        let crc = unsafe { kolmo_crc32c(kolmo_crc32c(0, h.as_ptr(), 28), idx.as_ptr(), idx.len()) };
        h[28..32].copy_from_slice(&crc.to_le_bytes());
        h.extend_from_slice(&idx);
        for i in 0..nb { h.extend_from_slice(&[0, 0, 0, 0, (i as u8).wrapping_mul(131).wrapping_add(7)]); }
        h
    }

    #[test]
    fn forged_frames_are_errors_not_aborts() {
        // Bug 5 of the review40 synthesis: a frame claiming 2^48 bytes aborted the process in vec![0u8; claimed].
        for (nb, raw, kmem) in [(2u32, 1u32 << 30, 0x59u8), (16, u32::MAX, 0x69), (65536, u32::MAX, 0x69)] {
            let f = forge(nb, raw, kmem);
            assert_eq!(original_size(&f).unwrap() as u64, nb as u64 * raw as u64);
            assert!(decompress(&f, 1).is_err(), "{}x{} accepted", nb, raw);
            assert!(read_range(&f, 0, usize::MAX, 1).is_err(), "{}x{} accepted by read_range", nb, raw);
        }
    }

    #[test]
    fn concatenated_frames_and_streams() {
        // Bug 11 of the review40 synthesis: CZNS (kolmo --stream, pipes over 256 MiB) and concatenated frames.
        let data = sample();
        let (a, b) = data.split_at(300_000);
        let fa = compress(a, Level::FASTEST, 1).unwrap();
        let fb = compress(b, Level::BALANCED, 1).unwrap();
        let cat = |x: &[&[u8]]| x.concat();
        let cases: Vec<(&str, Vec<u8>, Vec<u8>)> = vec![
            ("two frames", cat(&[&fa, &fb]), data.clone()),
            ("stream", czns(&[&fa, &fb]), data.clone()),
            ("stream then frame", cat(&[&czns(&[&fa]), &fb]), data.clone()),
            ("two streams", cat(&[&czns(&[&fa]), &czns(&[&fb])]), data.clone()),
            ("empty stream", czns(&[]), Vec::new()),
            ("empty stream then frame", cat(&[&czns(&[]), &fa]), a.to_vec()),
        ];
        for (name, blob, want) in &cases {
            assert_eq!(&decompress(&blob, 2).unwrap(), want, "{}", name);
            assert_eq!(original_size(&blob).unwrap(), want.len(), "{}", name);
            for (off, n) in [(0usize, 10usize), (299_990, 20), (want.len().saturating_sub(5), 100), (12_345, 400_000)] {
                if want.is_empty() { break; }
                let end = (off + n).min(want.len());
                assert_eq!(read_range(&blob, off as u64, n, 1).unwrap(), &want[off..end], "{} read_range({}, {})", name, off, n);
            }
            assert_eq!(&decompress_limited(&blob, 1, 0, Some(want.len() as u64)).unwrap(), want, "{}", name);
            if !want.is_empty() {
                assert_eq!(decompress_limited(&blob, 1, 0, Some(want.len() as u64 - 1)).unwrap_err().code, -7, "{}", name);
            }
        }
        let s = czns(&[&fa]);
        let mut v2 = s.clone(); v2[4] = 2;
        let mut past = s[..s.len() - 9].to_vec(); past.extend_from_slice(&[0u8; 8]);
        let bad: Vec<(&str, Vec<u8>)> = vec![
            ("trailing byte", cat(&[&fa, b"x"])), ("trailing zeros", cat(&[&fa, &[0u8; 8]])),
            ("truncated frame", cat(&[&fa, &fb[..fb.len() - 1]])), ("stream without terminator", s[..s.len() - 8].to_vec()),
            ("stream cut in a length", s[..s.len() - 4].to_vec()), ("stream length past the end", past),
            ("two frames in one segment", czns(&[&cat(&[&fa, &fb])])), ("stream version 2", v2),
            ("short stream header", b"CZNS\x01\x00".to_vec()), ("empty", Vec::new()),
        ];
        for (name, blob) in &bad {
            assert!(decompress(blob, 1).is_err(), "{} accepted", name);
            assert!(original_size(blob).is_err(), "{} accepted by original_size", name);
            assert!(read_range(blob, 0, 1, 1).is_err(), "{} accepted by read_range", name);
        }
        let two = cat(&[&fa, &fb]);
        assert_eq!(read_range(&two, data.len() as u64 + 1, 1, 1).unwrap_err().code, -16);
        assert_eq!(read_range(&two, data.len() as u64, 5, 1).unwrap(), b"");
    }
}
