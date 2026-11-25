# Compressed-input memory on files and pipes

On #104 at `ac10ce80048ab0664074f7ba031634055f4d2225`, public stream decoding of the complete Silesia ZIP in a single CZN1 frame held **68,130,757 bytes** of compressed input. The seekable file API held **4,195,424 bytes** at four requested threads, including its 1,104-byte index. Encoding this same ZIP as CZNS with 2 MiB segments reduced the public streaming decoder’s compressed-input buffer to **2,097,224 bytes**. Every decode reproduced the complete input SHA-256.

## Measured allocations

The two inputs were the complete 68,182,744-byte Silesia ZIP at preset 1 and the complete 10,134,321-byte Miami GHCN CSV at preset 3. All encodes used 1 MiB blocks and one thread. A single CZN1 and a CZNS file with 2 MiB segments were produced for each source. The segmented files contained 33 and five independent frames, respectively. All four compressed files matched byte for byte between the instrumented and stock builds. [Source and frame hashes](manifest.json)

| Input and decode entry point, T4 | Compressed input owned by decoder | Separate stream output window | Caller-mapped input | Median process peak RSS |
| --- | ---: | ---: | ---: | ---: |
| Silesia CZN1, stream API / pipe | 68,130,757 B | 67,108,864 B | 0 B | 130.08 MiB |
| Silesia CZN1, seekable file API | 4,195,424 B | 0 B | 0 B | 10.14 MiB |
| Silesia CZN1, mapped input to fd | 0 B | 0 B | 68,130,757 B | 71.03 MiB |
| Silesia CZNS, stream API / pipe | 2,097,224 B | 2,097,152 B | 0 B | 6.06 MiB |
| GHCN CZN1, stream API / pipe | 577,889 B | 10,134,321 B | 0 B | 18.45 MiB |
| GHCN CZN1, seekable file API | 328,168 B | 0 B | 0 B | 12.69 MiB |
| GHCN CZN1, mapped input to fd | 0 B | 0 B | 577,889 B | 12.47 MiB |
| GHCN CZNS, stream API / pipe | 149,246 B | 2,097,152 B | 0 B | 8.62 MiB |

The stream API used the same input capacity with a pipe and a regular descriptor. The seekable file API used a header/index allocation plus the overlapping per-worker compressed block buffers. At T1 those totals fell to 1,049,684 bytes on Silesia and 82,186 bytes on GHCN. The mapped entry point was `kolmo_decompress_to_fd` with the entire frame supplied by the caller. Zero library-owned compressed input there still entails a full virtual mapping and resident pages on access. Both fd paths used per-thread output/transform scratch. For Silesia at T4, that scratch retained 8,978,944 bytes after either fd path, 4,784,384 bytes after the one-frame stream path and 2,392,192 bytes after segmented stream decoding. [All cells](score.csv) · [Individual API observations](raw.csv)

## Normal CLI behavior and explicit memory limits

The unmodified CLI decoded the identical serialized frames at T4 to an output file. On Silesia CZN1 its median peak RSS was **134.19 MiB** for a pipe and **10.23 MiB** for a regular file. On the segmented Silesia CZNS the two figures were **6.25 and 6.22 MiB**. The CLI buffers a piped one-shot input, uses the seekable file API for these large regular one-shot files and recognizes CZNS on either descriptor. These are process measurements from the stock executable. Its stdin staging allocations are outside the public-library counters in the preceding table. [Stock CLI scores](cli-score.csv) · [CLI observations](cli.csv)

In 12 additional valid-file T4 tests of `kolmo_stream_decompress_limited`, both regular and pipe input retained the same 68,130,757-byte Silesia CZN1 compressed buffer at explicit decode-memory limits of 32 and 64 MiB. The output window changed to 20,381,696 and 53,936,128 bytes, respectively. Median process peak RSS was 86.08 and 118.08 MiB on either descriptor. The current limit governs the frame decode working set and its output window. Incoming compressed-member storage remains a separate budget concern. [Explicit-limit observations](limited.csv)

Across the 72 instrumented API, 24 stock CLI and 12 explicit-limit decodes, all 108 full SHA-256 comparisons passed. Each cell used three fresh decoder processes on shared Apple M5 hardware. Compressed-input counters cover requested heap capacity for the stream member or simultaneous fd payload buffers plus the held index. They exclude transient allocator internals during realloc, fixed stack headers, parsed index arrays, codec allocations, caller mappings, kernel pipe pages and destination-file caching. The stream output window, post-call retained TLS capacity, and complete process high-water RSS are recorded separately. RSS is physical residency and cannot be obtained by adding logical capacities. These paths used no application-managed spill file.

For senders controlling framing, the measured segmented format bounds each member while preserving sequential decoding of the full stream. For regular one-shot files, the fd API can read compressed blocks by position. A separate pipe-input budget or spill policy for existing large CZN1 streams would require an API decision, including default compatibility, disk accounting and multi-member handling. This experiment establishes a memory cost on valid data. It leaves the production source unchanged.

## Reproduction

Build the pinned default tree on macOS arm64. Run `replay.py --source CHECKOUT --csv USW00012839.csv --zip silesia-silesia.zip --out FRESH_DIRECTORY` with Python 3, then run `cli.py --binary CHECKOUT/build/kolmo` with the same input/output arguments. Run `limited.py --source CHECKOUT --zip silesia-silesia.zip --out FRESH_DIRECTORY` using the same output directory. The scripts validate decoded SHA-256s, stage measurement-only counters in a temporary copy of the source, and discard restored files. `score.py` summarizes the raw files in its directory.
