#!/bin/bash
# final.sh S OUT.jsonl: the end-to-end comparison of RESULTS.md (base, base2 = A/A copy, final, final at the SSE2 level)
S=$1; OUT=$2; B="base=$S/bin/kolmo-base base2=$S/bin/kolmo-base2 final=$S/m_generic/build/kolmo final-sse2=$S/m_generic/build/kolmo@KOLMO_CPU=sse2"
cd $S
C1=(); C4=(); for f in data/*; do for m in "-m speed" "-m balanced"; do C1+=("$f:$m:1"); C4+=("$f:$m:4"); done; done
AB_JSON=$OUT python3 -I tools/ab.py 9 $S/we "${C1[@]}" "${C4[@]}" -- $B
C=(); for f in d4/*; do for m in "-m eco" "-m hc" "-m ratio"; do C+=("$f:$m:1"); done; done
AB_JSON=$OUT python3 -I tools/ab.py 3 $S/we "${C[@]}" -- $B
C=(); for f in d05/*; do C+=("$f:-x:1"); done
AB_JSON=$OUT python3 -I tools/ab.py 3 $S/we "${C[@]}" -- $B
echo FINAL DONE
