# Experimental codec provenance

KRC1 and KBC1 are separate codec implementations within this project and do not invoke third-party compression backends. That does not mean their mathematical building blocks were invented here.

KRC1 uses established modular prediction, zigzag mapping, bit packing, sparse exceptions and CRC32C. KBC1 uses established integer arithmetic coding, adaptive binary counts and byte contexts; the arithmetic interval implementation was written for this experiment rather than adapted from a competitor codec or a published reference program. Relevant background is Witten, Neal and Cleary, [Arithmetic Coding for Data Compression](https://doi.org/10.1145/214762.214771).

The two-multiply integer hash construction and constants in KBC1's `mixhash` are the known `lowbias32` function documented by Chris Wellons's [Hash Function Prospector](https://github.com/skeeto/hash-prospector). That project publishes the function under the [Unlicense](https://github.com/skeeto/hash-prospector/blob/master/UNLICENSE). Credit this primitive rather than presenting it as an original hash design. It is a context-table index permutation, not a compression backend. The source, format and compression results do not establish algorithmic novelty.

Normal Kolmo remains a hybrid of its structure transforms and vendored codecs, with separate attribution in the root `NOTICE`. Its existing context mixer has a different provenance/history from the new KBC1 implementation. Benchmarks of normal Kolmo must not be presented as measurements of an entirely independent new entropy codec.

No competing codec source or trained model was imported into the new byte-context codec. Public benchmark inputs are used for evaluation, not embedded dictionaries or expected-output lookups. Dataset sources and frozen hashes are recorded under `benchmark/next` and `benchmark/ratio`. Refer to those manifests for corpus attribution.
