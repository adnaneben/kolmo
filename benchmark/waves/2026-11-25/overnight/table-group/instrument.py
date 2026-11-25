"""Add an emission-only no-group switch and optional on-wire diagnostics to an isolated source copy."""
from pathlib import Path
import shutil,subprocess,sys
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();src=out/'src'
shutil.copytree(root/'src',src,dirs_exist_ok=True)
p=src/'transforms/table.c';s=p.read_text()
mark='    /* entity groups: gid increases where the group column changes */'
assert s.count(mark)==1
s=s.replace(mark,'''    if (getenv("KOLMO_B40_NO_GROUP")) gcol = -1;
'''+mark)
mark='    PROF_T(2);\n    if (aligned && nr > 0)'
assert s.count(mark)==1
s=s.replace(mark,'''    if (getenv("KOLMO_B40_TRACE")) {
        int grouped = 0;
        for (int j=0; j<k; j++) if (C[j].type == 2) {
            int mode = C[j].mode;
            grouped += (mode == 2 || mode == 4 || mode == 5 ||
                       (mode == 3 && C[j].fid < nform && FB[C[j].fid].form != FM_RATIO)) && gcol >= 0;
        }
        fprintf(stderr, "B40 wire cols=%d rows=%lld group=%d groups=%u grouped_modes=%d formulas=%u\\n",
                k,(long long)nr,gcol,gcol >= 0 && nr ? gid[nr-1]+1 : 0,grouped,nform);
    }
'''+mark)
p.write_text(s)
subprocess.run(['cc','-O3','-mcpu=native','-ffp-contract=off','-Wno-unknown-attributes','-Wno-constant-logical-operand','-I'+str(src),src/'cli/cli.c',src/'cli/features.c',src/'kolmo.c',root/'build/libkolmoarch.a','-lm','-lpthread','-Wl,-dead_strip','-o',out/'kolmo'],check=True)
