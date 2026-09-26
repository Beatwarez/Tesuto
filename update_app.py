with open('app.js', 'r', encoding='utf-8') as f:
    text = f.read()

# Update base values
text = text.replace(r'_p1', r'_filter_cutoff')
text = text.replace(r'_p2', r'_filter_offset')
text = text.replace(r'_p3', r'_filter_reso')
text = text.replace(r'_p4', r'_filter_slope')
text = text.replace(r'_p5', r'_filter_morph')

text = text.replace(r'_p1_mod', r'_filter_cutoff_mod')
text = text.replace(r'_p2_mod', r'_filter_offset_mod')
text = text.replace(r'_p3_mod', r'_filter_reso_mod')
text = text.replace(r'_p4_mod', r'_filter_slope_mod')
text = text.replace(r'_p5_mod', r'_filter_morph_mod')

text = text.replace(r'_filterA', r'_filter_typeA')
text = text.replace(r'_filterB', r'_filter_typeB')

with open('app.js', 'w', encoding='utf-8') as f:
    f.write(text)
