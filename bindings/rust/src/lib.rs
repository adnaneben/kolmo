//! Rust binding for **kolmo**, a structure-aware lossless compressor.
//!
//! Thin, safe wrappers over the native core (`kolmo.h`). The whole file is compressed or
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
impl Mode {
    /// Fastest: for data that is read or sent right away.
    pub const SPEED: Mode = Mode::Rt;
    /// Small and fast; the usual choice.
    pub const BALANCED: Mode = Mode::Fast;
    /// Smallest at practical speed.
    pub const RATIO: Mode = Mode::Max;
}


#[repr(C)]
struct Params {
    mode: c_int,
    level: c_int,
    threads: c_int,
    block: u32,
    transforms: c_int,
    mem_limit: u64, // 0 = auto (docs/MEMORY-MODEL.md); mirrors kolmo_params
    jobs_shared: c_int, // 0 = process setting, < 0 = out of the machine-wide pool (docs/CONCURRENCY.md)
}

#[repr(C)]
struct JobsProc { pid: i32, tokens: i32, jobs: i32, reserved: u64 }
#[repr(C)]
struct JobsInfo {
    enabled: c_int, mode: c_int, tokens: c_int, busy: c_int, held_here: c_int, processes: c_int,
    reserved: u64, limit: u64, dir: [c_char; 512], state: [c_char; 128],
}

extern "C" {
    fn kolmo_bound(n: usize, p: *const Params) -> usize;
    fn kolmo_compress(src: *const u8, n: usize, dst: *mut u8, cap: usize, p: *const Params) -> i64;
    fn kolmo_original_size(src: *const u8, n: usize) -> i64;
    fn kolmo_decompress(src: *const u8, n: usize, dst: *mut u8, cap: usize, threads: c_int) -> i64;
    fn kolmo_read_range(src: *const u8, n: usize, off: u64, len: usize, dst: *mut u8, threads: c_int) -> i64;
    fn kolmo_strerror(code: i64) -> *const c_char;
    fn kolmo_train_dict(samples: *const u8, sizes: *const usize, nsamples: usize, dict: *mut u8, dict_cap: usize) -> i64;
    fn kolmo_compress_dict(src: *const u8, n: usize, dst: *mut u8, cap: usize, p: *const Params, dict: *const u8, dictlen: usize) -> i64;
    fn kolmo_decompress_dict(src: *const u8, n: usize, dst: *mut u8, cap: usize, threads: c_int, dict: *const u8, dictlen: usize) -> i64;
    fn kolmo_jobs_shared(mode: c_int) -> c_int;
    fn kolmo_jobs_status(info: *mut JobsInfo, procs: *mut JobsProc, max: c_int) -> c_int;
}

/// How this process takes part in the machine-wide pool (docs/CONCURRENCY.md): worker tokens (default: the physical
/// cores) and memory shared by the Kolmo processes of the user, so concurrent instances divide the machine instead of
/// oversubscribing it. Never changes output.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum JobsShared { Off = 0, User = 1, System = 2, Environment = -1 }

/// Sets the pool mode of this process; fails while calls hold tokens.
pub fn jobs_shared(mode: JobsShared) -> Result<(), Error> {
    if unsafe { kolmo_jobs_shared(mode as c_int) } == 0 { Ok(()) }
    else { Err(Error { code: -1, message: String::from("cannot change the pool while calls hold tokens") }) }
}

/// Machine-wide use of the pool, as `kolmo --jobs-status` prints it.
#[derive(Clone, Debug)]
pub struct JobsStatus {
    pub enabled: bool, pub tokens: i32, pub busy: i32, pub held_here: i32,
    /// other live Kolmo processes: (pid, tokens, block jobs, memory reserved)
    pub processes: Vec<(i32, i32, i32, u64)>,
    pub reserved: u64, pub limit: u64, pub dir: String, pub state: String,
}

pub fn jobs_status() -> JobsStatus {
    let mut info: JobsInfo = unsafe { std::mem::zeroed() };
    let mut procs: Vec<JobsProc> = (0..256).map(|_| JobsProc { pid: 0, tokens: 0, jobs: 0, reserved: 0 }).collect();
    let n = unsafe { kolmo_jobs_status(&mut info, procs.as_mut_ptr(), 256) };
    let s = |b: &[c_char]| unsafe { std::ffi::CStr::from_ptr(b.as_ptr()).to_string_lossy().into_owned() };
    JobsStatus {
        enabled: info.enabled != 0, tokens: info.tokens, busy: info.busy, held_here: info.held_here,
        processes: procs.iter().take(n.max(0) as usize).map(|p| (p.pid, p.tokens, p.jobs, p.reserved)).collect(),
        reserved: info.reserved, limit: info.limit, dir: s(&info.dir), state: s(&info.state),
    }
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

/// Compress `data` in `mode`. `threads` = 0 uses all cores.
pub fn compress(data: &[u8], mode: Mode, threads: i32) -> Result<Vec<u8>, Error> {
    let p = Params { mode: mode as c_int, level: 0, threads: threads as c_int, block: 0, transforms: 1, mem_limit: 0, jobs_shared: 0 };
    let cap = unsafe { kolmo_bound(data.len(), &p) };
    let mut out = vec![0u8; cap.max(1)];
    let n = unsafe { kolmo_compress(data.as_ptr(), data.len(), out.as_mut_ptr(), out.len(), &p) };
    if n < 0 { return Err(err(n)); }
    out.truncate(n as usize);
    Ok(out)
}

/// Original (decompressed) length recorded in a compressed blob's header.
pub fn original_size(blob: &[u8]) -> Result<usize, Error> {
    let n = unsafe { kolmo_original_size(blob.as_ptr(), blob.len()) };
    if n < 0 { Err(err(n)) } else { Ok(n as usize) }
}

/// Decompress a whole blob. `threads` = 0 uses all cores.
pub fn decompress(blob: &[u8], threads: i32) -> Result<Vec<u8>, Error> {
    let o = original_size(blob)?;
    let mut out = vec![0u8; o.max(1)];
    let got = unsafe { kolmo_decompress(blob.as_ptr(), blob.len(), out.as_mut_ptr(), out.len(), threads as c_int) };
    if got < 0 { return Err(err(got)); }
    out.truncate(got as usize);
    Ok(out)
}

/// Return bytes `[off, off + len)` of the original, decoding only the overlapping blocks.
pub fn read_range(blob: &[u8], off: u64, len: usize, threads: i32) -> Result<Vec<u8>, Error> {
    let mut out = vec![0u8; len.max(1)];
    let got = unsafe { kolmo_read_range(blob.as_ptr(), blob.len(), off, len, out.as_mut_ptr(), threads as c_int) };
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
    let n = unsafe { kolmo_train_dict(blob.as_ptr(), sizes.as_ptr(), sizes.len(), dict.as_mut_ptr(), dict.len()) };
    if n < 0 { return Err(err(n)); }
    dict.truncate(n as usize);
    Ok(dict)
}

/// Compress `data` against a trained dictionary (see [`train_dict`]). The same dictionary must be
/// supplied to [`decompress_dict`]; a blob written with a dictionary refuses to decode without it.
pub fn compress_dict(data: &[u8], mode: Mode, threads: i32, dict: &[u8]) -> Result<Vec<u8>, Error> {
    let p = Params { mode: mode as c_int, level: 0, threads: threads as c_int, block: 0, transforms: 1, mem_limit: 0, jobs_shared: 0 };
    let cap = unsafe { kolmo_bound(data.len(), &p) };
    let mut out = vec![0u8; cap.max(1)];
    let n = unsafe { kolmo_compress_dict(data.as_ptr(), data.len(), out.as_mut_ptr(), out.len(), &p, dict.as_ptr(), dict.len()) };
    if n < 0 { return Err(err(n)); }
    out.truncate(n as usize);
    Ok(out)
}

/// Decompress a blob written by [`compress_dict`], supplying the same dictionary.
pub fn decompress_dict(blob: &[u8], threads: i32, dict: &[u8]) -> Result<Vec<u8>, Error> {
    let o = original_size(blob)?;
    let mut out = vec![0u8; o.max(1)];
    let got = unsafe { kolmo_decompress_dict(blob.as_ptr(), blob.len(), out.as_mut_ptr(), out.len(), threads as c_int, dict.as_ptr(), dict.len()) };
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
    fn jobs_pool_same_bytes() {
        let d = sample();
        let st = jobs_status();
        if st.enabled { assert!(st.tokens >= 1); }
        let a = compress(&d, Mode::BALANCED, 4).unwrap();
        // the other tests run at the same time and may hold tokens: wait until the pool can change
        while jobs_shared(JobsShared::Off).is_err() { std::thread::sleep(std::time::Duration::from_millis(1)); }
        let b = compress(&d, Mode::BALANCED, 4).unwrap();
        while jobs_shared(JobsShared::Environment).is_err() { std::thread::sleep(std::time::Duration::from_millis(1)); }
        assert_eq!(a, b);
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
