# P2-E18: export the GHCN development inputs for local modelling (no measurement)

OpenZL's typed profile keeps GHCN daily weather in every archive comparison so far: P2-E10, P2-E14 and P2-E17
(7/9). The GHCN source (NOAA) cannot be reached from the local research container, but GitHub runners can reach it.

This job rebuilds the P2-E10 cohort with the unchanged registered scripts, then prints the nine already-observed
`E02E__ghcn_*` NPY inputs (public NOAA GHCN-Daily data) into the job log, with their SHA-256. Locally, those hashes
must match P2-E14's prepared manifest before use. The job does no compression measurement.

Local work on these inputs is model development on **observed** data. Any rule found this way needs a preregistered
validation on fresh stations or a fresh period before it is claimed.

Budget: timeout 15 minutes, expected about 4 runner-minutes.
