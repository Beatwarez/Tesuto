import re

with open('index.html', 'r', encoding='utf-8') as f:
    html = f.read()

def replace_param(html, engine_id, old_param, new_param):
    start = html.find(f'<template id="tmpl-engine-{engine_id}">')
    if start == -1: return html
    end = html.find('</template>', start)
    block = html[start:end]
    block = block.replace(f'data-param="mod{{{{LANE}}}}_{old_param}"', f'data-param="mod{{{{LANE}}}}_{new_param}"')
    block = block.replace(f'id="knob-mod{{{{LANE}}}}_{old_param}"', f'id="knob-mod{{{{LANE}}}}_{new_param}"')
    block = block.replace(f'data-mod-param="mod{{{{LANE}}}}_{old_param}_mod"', f'data-mod-param="mod{{{{LANE}}}}_{new_param}_mod"')
    block = block.replace(f'id="val-mod{{{{LANE}}}}_{old_param}"', f'id="val-mod{{{{LANE}}}}_{new_param}"')
    return html[:start] + block + html[end:]

html = replace_param(html, 'alter', 'p1', 'alter_fm')
html = replace_param(html, 'alter', 'p2', 'alter_pinch')
html = replace_param(html, 'alter', 'p3', 'alter_desync')
html = replace_param(html, 'alter', 'p4', 'alter_quant')

html = replace_param(html, 'filter', 'p1', 'filter_cutoff')
html = replace_param(html, 'filter', 'p2', 'filter_offset')
html = replace_param(html, 'filter', 'p3', 'filter_reso')
html = replace_param(html, 'filter', 'p4', 'filter_slope')
html = replace_param(html, 'filter', 'p5', 'filter_morph')
html = replace_param(html, 'filter', 'filterA', 'filter_typeA')
html = replace_param(html, 'filter', 'filterB', 'filter_typeB')

html = replace_param(html, 'space', 'p1', 'space_width')
html = replace_param(html, 'space', 'p2', 'space_orbit')
html = replace_param(html, 'space', 'p3', 'space_smear')

html = replace_param(html, 'infect', 'p1', 'infect_drive')
html = replace_param(html, 'infect', 'p2', 'infect_sym')
html = replace_param(html, 'infect', 'p3', 'infect_clone')
html = replace_param(html, 'infect', 'p4', 'infect_cloneAmount')

html = replace_param(html, 'form', 'p1', 'form_warp')
html = replace_param(html, 'form', 'p2', 'form_fold')
html = replace_param(html, 'form', 'p3', 'form_tension')
html = replace_param(html, 'form', 'p4', 'form_shape')

html = html.replace('BUILD #0.73', 'BUILD #0.75')

with open('index.html', 'w', encoding='utf-8') as f:
    f.write(html)
