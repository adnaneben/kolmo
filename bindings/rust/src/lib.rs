//! Rust binding for **kolmo**, a structure-aware lossless compressor.
//!
//! Thin, safe wrappers over the native core (`czn.h`). The whole file is compressed or
//! decompressed in one call; [`read_range`] decodes only the blocks overlapping a byte range.
//!
//! ```no_run
//! let blob = kolmo::compress(b"country,year\nA,2020\n", kolmo::Mode::Hc, 0).unwrap();
//! let back = kolmo::decompress(&blob, 0).unwrap();
//! ```

use std::os::raw::{c_int, c_char};

/// Compression mode. `Rt` is LZ4-speed; `Ultra` is the context-mixing archive tier.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Rt = 0,
    Fast = 1,
    Eco = 2,
    Hc = 3,
    Max = 4,
    Ultra = 5,
}

#[repr(C)]
struct Params {
    mode: c_int,
    level: c_int,
    threads: c_int,
    block: u32,
    transforms: c_int,
}

extern "C" {
    fn czn_bound(n: usize, p: *const Params) -> usize;
    fn czn_compress(src: *const u8, n: usize, dst: *mut u8, cap: usize, p: *const Params) -> i64;
    fn czn_original_size(src: *const u8, n: usize) -> i64;
    fn czn_decompress(src: *const u8, n: usize, dst: *mut u8, cap: usize, threads: c_int) -> i64;
    fn czn_read_range(src: *const u8, n: usize, off: u64, len: usize, dst: *mut u8, threads: c_int) -> i64;
    fn czn_strerror(code: i64) -> *const c_char;
    fn czn_train_dict(samples: *const u8, sizes: *const usize, nsamples: usize, dict: *mut u8, dict_cap: usize) -> i64;
    fn czn_compress_dict(src: *const u8, n: usize, dst: *mut u8, cap: usize, p: *const Params, dict: *const u8, dictlen: usize) -> i64;
    fn czn_decompress_dict(src: *const u8, n: usize, dst: *mut u8, cap: usize, threads: c_int, dict: *const u8, dictlen: usize) -> i64;
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
        let p = czn_strerror(code);
        if p.is_null() { String::from("unknown error") }
        else { std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned() }
    };
    Error { code, message: msg }
}

/// Compress `data` in `mode`. `threads` = 0 uses all cores.
pub fn compress(data: &[u8], mode: Mode, threads: i32) -> Result<Vec<u8>, Error> {
    let p = Params { mode: mode as c_int, level: 0, threads: threads as c_int, block: 0, transforms: 1 };
    let cap = unsafe { czn_bound(data.len(), &p) };
    let mut out = vec![0u8; cap.max(1)];
    let n = unsafe { czn_compress(data.as_ptr(), data.len(), out.as_mut_ptr(), out.len(), &p) };
    if n < 0 { return Err(err(n)); }
    out.truncate(n as usize);
    Ok(out)
}

/// Original (decompressed) length recorded in a compressed blob's header.
pub fn original_size(blob: &[u8]) -> Result<usize, Error> {
    let n = unsafe { czn_original_size(blob.as_ptr(), blob.len()) };
    if n < 0 { Err(err(n)) } else { Ok(n as usize) }
}

/// Decompress a whole blob. `threads` = 0 uses all cores.
pub fn decompress(blob: &[u8], threads: i32) -> Result<Vec<u8>, Error> {
    let o = original_size(blob)?;
    let mut out = vec![0u8; o.max(1)];
    let got = unsafe { czn_decompress(blob.as_ptr(), blob.len(), out.as_mut_ptr(), out.len(), threads as c_int) };
    if got < 0 { return Err(err(got)); }
    out.truncate(got as usize);
    Ok(out)
}

/// Return bytes `[off, off + len)` of the original, decoding only the overlapping blocks.
pub fn read_range(blob: &[u8], off: u64, len: usize, threads: i32) -> Result<Vec<u8>, Error> {
    let mut out = vec![0u8; len.max(1)];
    let got = unsafe { czn_read_range(blob.as_ptr(), blob.len(), off, len, out.as_mut_ptr(), threads as c_int) };
    if got < 0 { return Err(err(got)); }
    out.truncate(got as usize);
    Ok(out)
}

/// Train a dictionary from a set of small, similar samples (records, log lines, JSON blobs).
/// `dict_cap` bounds the dictionary size (~112 KiB is typical). Use the result with
/// [`compress_dict`] / [`decompress_dict`].
pub fn train_dict(samples: &[&[u8]], dict_cap: usize) -> Result<Vec<u8>, Error> {
    let mut blob = Vec::new();
    let sizes: Vec<usize> = samples.iter().map(|s| { blob.extend_from_slice(s); s.len() }).collect();
    let mut dict = vec![0u8; dict_cap.max(256)];
    let n = unsafe { czn_train_dict(blob.as_ptr(), sizes.as_ptr(), sizes.len(), dict.as_mut_ptr(), dict.len()) };
    if n < 0 { return Err(err(n)); }
    dict.truncate(n as usize);
    Ok(dict)
}

/// Compress `data` against a trained dictionary (see [`train_dict`]). The same dictionary must be
/// supplied to [`decompress_dict`]; a blob written with a dictionary refuses to decode without it.
pub fn compress_dict(data: &[u8], mode: Mode, threads: i32, dict: &[u8]) -> Result<Vec<u8>, Error> {
    let p = Params { mode: mode as c_int, level: 0, threads: threads as c_int, block: 0, transforms: 1 };
    let cap = unsafe { czn_bound(data.len(), &p) };
    let mut out = vec![0u8; cap.max(1)];
    let n = unsafe { czn_compress_dict(data.as_ptr(), data.len(), out.as_mut_ptr(), out.len(), &p, dict.as_ptr(), dict.len()) };
    if n < 0 { return Err(err(n)); }
    out.truncate(n as usize);
    Ok(out)
}

/// Decompress a blob written by [`compress_dict`], supplying the same dictionary.
pub fn decompress_dict(blob: &[u8], threads: i32, dict: &[u8]) -> Result<Vec<u8>, Error> {
    let o = original_size(blob)?;
    let mut out = vec![0u8; o.max(1)];
    let got = unsafe { czn_decompress_dict(blob.as_ptr(), blob.len(), out.as_mut_ptr(), out.len(), threads as c_int, dict.as_ptr(), dict.len()) };
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

    #[test]
    fn round_trip_all_modes() {
        let data = sample();
        for mode in [Mode::Rt, Mode::Fast, Mode::Eco, Mode::Hc, Mode::Max, Mode::Ultra] {
            let blob = compress(&data, mode, 1).expect("compress");
            assert_eq!(original_size(&blob).unwrap(), data.len(), "{:?} size", mode);
            let back = decompress(&blob, 1).expect("decompress");
            assert_eq!(back, data, "{:?} round trip", mode);
        }
    }

    #[test]
    fn range_read() {
        let data = sample();
        let blob = compress(&data, Mode::Fast, 1).unwrap();
        let mid = data.len() / 2;
        let got = read_range(&blob, mid as u64, 64, 1).unwrap();
        assert_eq!(got, &data[mid..mid + 64]);
    }

    #[test]
    fn empty_and_tiny() {
        for d in [&b""[..], &b"x"[..], &b"hello world"[..]] {
            let blob = compress(d, Mode::Fast, 1).unwrap();
            assert_eq!(decompress(&blob, 1).unwrap(), d);
        }
    }

    #[test]
    fn dictionary_round_trip() {
        // Many small, similar records: train on some, test on others.
        let recs: Vec<Vec<u8>> = (0..600u32)
            .map(|i| format!("id,{},status,OK,region,eu-west-{},ts,2024-01-{:02}T00:00:00Z\n", i, i % 4, 1 + i % 28).into_bytes())
            .collect();
        let samples: Vec<&[u8]> = recs[..400].iter().map(|v| v.as_slice()).collect();
        let dict = train_dict(&samples, 64 * 1024).expect("train");
        assert!(!dict.is_empty());
        for r in &recs[400..] {
            let blob = compress_dict(r, Mode::Fast, 1, &dict).unwrap();
            assert_eq!(decompress_dict(&blob, 1, &dict).unwrap(), *r);
            // A dictionary blob must not decode without the dictionary.
            assert!(decompress(&blob, 1).is_err());
        }
    }
}
