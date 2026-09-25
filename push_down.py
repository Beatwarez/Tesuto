import re

with open('index.html', 'r', encoding='utf-8') as f:
    content = f.read()

replacement = """<template id="tmpl-engine-infect">
        <div class="dsp-panel engine-infect-panel">
            <div class="filter-ui-canvas-container"></div>
            <div class="filter-params-row">"""

content = re.sub(
    r"<template id=\"tmpl-engine-infect\">\s*<div class=\"dsp-panel engine-infect-panel\">\s*<div class=\"filter-params-row\">",
    replacement,
    content,
    flags=re.DOTALL
)

with open('index.html', 'w', encoding='utf-8') as f:
    f.write(content)
