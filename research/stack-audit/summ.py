import json, sys
# usage: python3 -I summ.py file.jsonl...   per-file lines and per-class totals
cls = {}
tot = [0, 0, 0, 0]
for fn in sys.argv[1:]:
    for l in open(fn):
        r = json.loads(l); f = r['file'].split('/')[-1]
        d = 100 * (r['stack_best'] / r['kolmo'] - 1)
        w = ''
        if 'kfam_whole' in r: w = ' whole=%d(%+.2f%%) whole_enc=%.1fs dec=%.1fs' % (r['kfam_whole'], 100 * (r['kfam_whole'] / r['kolmo'] - 1), r['kfam_whole_enc_s'], r['kfam_whole_dec_s'])
        wall = [v for k, v in r.items() if k.startswith('wall_all_variants_')]
        print('%-28s %-5s kolmo=%d best=%d (%+.2f%%) always=%d kfam1t=%.1fs wallT=%.1fs kolmo_enc=%.1fs dec=%.2fs ok=%d%s %s' % (
            f, r['mode'], r['kolmo'], r['stack_best'], d, r['stack_kfam_always'], r['kfam_v0_s_1t'], wall[0] if wall else 0,
            r['kolmo_enc_s'], r['kolmo_dec_s'], r['verified'], w, r.get('int_planes', '')))
        tot[0] += r['orig']; tot[1] += r['kolmo']; tot[2] += r['stack_best']; tot[3] += r['kfam_v0_s_1t']
        for c, v in r['classes'].items():
            a = cls.setdefault(c, [0, 0, 0, 0, 0, 0])
            a[0] += v[0]; a[1] += v[1]; a[2] += v[2]; a[3] += v[3]
            if v[3] < v[2]: a[4] += 1; a[5] += v[2] - v[3]
print('TOTAL orig=%d kolmo=%d stack_best=%d (%+.3f%%) kfam_1t=%.0fs' % (tot[0], tot[1], tot[2], 100 * (tot[2] / tot[1] - 1), tot[3]))
for c, a in sorted(cls.items()):
    print('  %-14s streams=%d raw=%d kolmo=%d kfam=%d (%+.2f%%) files_where_kfam_smaller=%d saved=%d' % (c, a[0], a[1], a[2], a[3], 100 * (a[3] / a[2] - 1) if a[2] else 0, a[4], a[5]))
