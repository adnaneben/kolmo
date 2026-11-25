# Development sweep (dev producer only; the holdout was not opened until the candidate was frozen)

All sizes in this file come from the harness encoder on prefixes of the dev input
`dickens4m` (Silesia dickens, first 4,194,304 bytes, SHA-256
`6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215`):

- `dev256k` is the first 262,144 bytes.
- `dev1m` is the first 1,048,576 bytes.

`bytes_cm`, `bytes_lstm` and `bytes_final` are the encoder's ideal code lengths (−Σlog2 p / 8). They are for model 4
alone, the LSTM alone, and the final mixed probability. "stream" is the actual `KLS1` file, including its 24-byte
header. Times are single runs taken while the shared host was loaded (1-minute load 23–45). Each sweep ran 4–6
encoders in parallel, so only the relative cost between rows means anything.

## Controls

| check | result |
|---|---|
| `enc0`/`dec0` (production `cm_compress_tm` with the text mask and `cm4_tune`) on dev256k | 65,104 B including the 4-byte magic; round trip OK |
| `LSTM_PASS=1` (the candidate's coding loop with p = model 4's p) | 65,112 B = the same 65,088 coded bytes as `enc0`, plus the 24-byte header |
| stock `kolmo -U -T 1` frame on dev256k | 65,152 B = the 65,100-byte model-4 stream + 52 B of container |

The harness's coding loop is therefore byte-exact with production model 4, and stock `-U` codes these inputs as one
model-4 stream.

## Final mixer learning rate (dev256k, H = 32, T = 10)

The first mixer update shift (`>> 10`) was 16× too aggressive, and the final stream was +1.3% larger than model 4
alone. The shift was changed to `>> 14` before any of the rows below.

| setting | stream B | model 4 alone (ideal B) | LSTM alone (ideal B) | final (ideal B) |
|---|---:|---:|---:|---:|
| LR 50, LRO 1000, LRM 1 | 65,087 | 65,085 | 92,140 | 65,059 |
| LR 50, LRO 1000, LRM 4 | **65,058** | 65,085 | 92,140 | **65,030** |
| LR 50, LRO 1000, LRM 16 | 65,361 | 65,085 | 92,140 | 65,333 |
| LRM 4, SEL 1 (partial byte only) | 65,080 | | | 65,053 |
| LRM 4, SEL 2 (confidence × bit position) | 65,069 | | | 65,042 |

An earlier sweep (with the `>> 10` mixer) settled the LSTM learning rates. LSTM-alone ideal bytes were:

- LR 100 / LRO 200: 129,845
- LR 100 / LRO 500: 116,258
- LR 300 / LRO 500: 159,939
- LR 50 / LRO 1000: 92,140
- LR 200 / LRO 2000: 95,461
- LR 500 / LRO 300: 155,167

## Capacity and horizon (dev1m, LRM 4, SEL 0)

| H | T | LR | LRO | stream B | vs model 4 alone (229,657 ideal) | LSTM alone (ideal B) | encode s (loaded host) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 | 10 | 50 | 1000 | 229,801 | +0.05% | 340,274 | 28.3 |
| 64 | 10 | 50 | 1000 | 229,393 | −0.13% | 315,736 | 61.0 |
| 64 | 20 | 50 | 1000 | 229,287 | −0.17% | 313,487 | 59.9 |
| 128 | 10 | 50 | 1000 | 228,955 | −0.32% | 302,105 | 168.9 |
| 128 | 10 | 20 | 500 | 228,790 | −0.39% | 300,833 | 168.4 |
| **128** | **30** | **50** | **1000** | **228,445** | **−0.54%** | 293,708 | 165.2 |

## Final-mixer inputs (dev1m, H = 32, T = 10, LR 50, LRO 1000)

| final mixer | stream B |
|---|---:|
| 3 inputs: st(model 4), st(LSTM), bias (LRM 4) | **229,801** |
| plus model 4's five internal mixer outputs, LRM 4 (`LSTM_LRM=20`) | 230,172 |
| the same with LRM 2 (`LSTM_LRM=18`) | 229,896 |
| the same with LRM 4 and SEL 2 | 230,220 |

Re-mixing model 4's internals loses, which agrees with R16's F3: a learned final mixer is +0.36%.

## Frozen candidate

The candidate is `LSTM_H=128 LSTM_T=30 LSTM_LR=50 LSTM_LRO=1000 LSTM_SEL=0 LSTM_LRM=4`, chosen because it had the
smallest dev1m stream. The trend says a larger or deeper LSTM would gain more bytes. Every doubling of H cost about
2.5× the time here, so any larger model moves further from the speed gates. No setting was tried on the holdout.
The frozen binaries and source are hashed in `frozen-SHA256SUMS`.
