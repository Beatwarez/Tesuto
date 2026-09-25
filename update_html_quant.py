import re

with open('index.html', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace(
    "<span class=\"knob-name-a\">RING</span>",
    "<span class=\"knob-name-a\">QUANT</span>"
)

with open('index.html', 'w', encoding='utf-8') as f:
    f.write(content)

