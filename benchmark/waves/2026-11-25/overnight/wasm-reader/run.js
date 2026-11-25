// Only fixed, independently pinned one-shot fixtures are supplied by verify.py.
const usingNode = typeof process !== "undefined" && !!process.versions?.node;
const [wasmFile, caseFile, corpus, out] = usingNode ? process.argv.slice(2) : globalThis.arguments;
const fs = usingNode ? require("node:fs") : null;
const bytes = f => usingNode ? fs.readFileSync(f) : readFile(f, "binary");
const text = f => usingNode ? fs.readFileSync(f, "utf8") : readFile(f);
const save = (f, data) => usingNode ? fs.writeFileSync(f, data) : writeFile(f, data);
const cases = JSON.parse(text(caseFile));
const log = o => (usingNode ? console.log : print)(JSON.stringify(o));
const mod = new WebAssembly.Module(bytes(wasmFile));
const calls = {};
const wasifns = {};
let mem;
for (const {module:ns, name, kind} of WebAssembly.Module.imports(mod)) {
    if (ns !== "wasi_snapshot_preview1" || kind !== "function") throw Error(ns + "." + name);
    wasifns[name] = (...args) => {
        calls[name] = (calls[name] || 0) + 1;
        const view = new DataView(mem.buffer);
        if (name === "environ_sizes_get") {
            view.setUint32(args[0], 0, true);
            view.setUint32(args[1], 0, true);
            return 0;
        }
        if (name === "environ_get" || name === "sched_yield") return 0;
        if (name === "clock_time_get") {
            view.setBigUint64(args[2], BigInt(Math.floor(performance.now()*1000000)), true);
            return 0;
        }
        throw Error("unexpected WASI call " + name); // no host file descriptors, polling or exit
    };
}
for (const [k, c] of cases.entries()) {
    const e = new WebAssembly.Instance(mod, {wasi_snapshot_preview1:wasifns}).exports;
    mem = e.memory;
    const initial = mem.buffer.byteLength;
    e._initialize();
    const frame = bytes(c.frame_path ? corpus+"/"+c.frame_path : c.path);
    const src = e.malloc(frame.length || 1);
    const dst = e.malloc(c.original_bytes || 1);
    if (!src || !dst) throw Error("Wasm allocation failed for " + c.frame_path);
    new Uint8Array(mem.buffer).set(frame,src);
    const before = mem.buffer.byteLength;
    const n = Number(e.restore_frame(src,frame.length,dst,c.original_bytes));
    if (n === c.original_bytes) save(out+"/"+k+".bin", new Uint8Array(mem.buffer,dst,n));
    e.free(dst);
    e.free(src);
    log({case:k,decoded:n,initial_bytes:initial,before_decode_bytes:before,after_decode_bytes:mem.buffer.byteLength});
    mem = null;
    if (typeof gc === "function") gc();
}
log({wasm_imports:WebAssembly.Module.imports(mod),wasm_exports:WebAssembly.Module.exports(mod),calls});
