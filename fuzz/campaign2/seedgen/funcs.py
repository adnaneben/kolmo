import json, sys, re
d = json.load(sys.stdin)
W = re.compile(r'^(tbl_decode|tbl2_decode|tbl2_stream|num_decode|e8e9_inv|lines_decode|img16_decode|img16_inv|rec_decode|rec_field_dec|planes_get|pfor_get|kg_fields_inv|kg_dec_field|kg_dec_done|kg_decimal_inv|kg_put_num|fi_inv_\w+|skel_parse|log_decode|tpq_inv_wide|word_decode\w*|kg_int_inv)$')
for f in d['data'][0]['functions']:
    n = f['name'].split(':')[-1]
    if f['count'] > 0 and W.match(n): print(n)
