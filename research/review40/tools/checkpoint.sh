#!/bin/bash
# Copy the campaign records (reviews, claims, results, patch series, verifications) onto the integration branch and push.
set -e
C=/home/user/kolmo-review40-coord; W=/home/user/kolmo-review40-integration; D=$W/research/review40
mkdir -p $D
# Work in progress: export every reviewer branch's commits so far, so they survive a lost container.
for w in /home/user/kolmo-review40-wt/R*/; do r=$(basename $w); n=$(git -C $w rev-list --count 5071f388..HEAD 2>/dev/null || echo 0)
  if [ "$n" -gt 0 ]; then mkdir -p $C/patches-wip/$r.new && git -C $w format-patch -q 5071f388..HEAD -o $C/patches-wip/$r.new && mkdir -p $C/patches-wip/$r && cp -f $C/patches-wip/$r.new/* $C/patches-wip/$r/ && mv $C/patches-wip/$r.new $C/patches-wip/.$r.last.$$ ; fi; done
which rsync >/dev/null 2>&1 && rsync -a --max-size=5m --exclude='.*.lock' --exclude='.R*.last.*' --exclude='*.o' $C/ $D/ || cp -r $C/. $D/
cd $W && git add research/review40 && if git diff --cached --quiet; then echo "nothing new"; else
git commit -q -m "review40: checkpoint of reviews, claims, results, patches and verifications

Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01FUEN4aRWhUw7yL7cZMqTmZ" && for i in 1 2 3 4; do git push -q origin claude/review40-integration-20261123 && break; sleep $((2**i)); done; git log --oneline -1; fi
echo "claims=$(wc -l < $C/claims.jsonl 2>/dev/null || echo 0) reviews=$(ls $C/reviews | wc -l) results=$(ls $C/results/*.md 2>/dev/null | wc -l) verify=$(ls $C/verify/*.md 2>/dev/null | wc -l) patchdirs=$(ls $C/patches | wc -l)"
