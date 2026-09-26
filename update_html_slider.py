with open('index.html', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('id="slider-mod{{LANE}}_p5"', 'id="slider-mod{{LANE}}_filter_morph"')

with open('index.html', 'w', encoding='utf-8') as f:
    f.write(text)
