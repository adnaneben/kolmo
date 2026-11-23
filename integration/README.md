# Integrations

| directory | what | status |
|---|---|---|
| [hdf5/](hdf5/) | HDF5 filter plugin (C, h5py, HDF5 tools) | works; filter ID 480 is a testing placeholder until The HDF Group registers one |
| [blosc2/](blosc2/) | c-blosc2 codec plugin (linked in, or loaded by Blosc2) | works; codec ID 244 is a user-range placeholder until the Blosc team registers one |
| [common/](common/) | shared benchmark inputs, and the helper that links libkolmo's objects into a plugin | |
| [rc1/](rc1/) | feature status of the RC1 integration branch (historical) | |

Each plugin directory has a README with build, use, tests and a single-thread benchmark against the filters people use
today. The registration drafts for the owner are `hdf5/FILTER-ID-REQUEST.md`, `hdf5/HDF5PLUGIN-CONTRIBUTION.md` and
`blosc2/REGISTRY-SUBMISSION.md`.
