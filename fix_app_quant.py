import re

with open('app.js', 'r', encoding='utf-8') as f:
    app_content = f.read()

old_quant = """                            if (quantVal > 0.0) {
                                let steps = 2.0 + (1.0 - quantVal) * 62.0;
                                let quantizedPhase = Math.floor(modPhase * steps) / steps;
                                modPhase = modPhase * (1.0 - quantVal) + quantizedPhase * quantVal;
                            }"""
                            
new_quant = """                            if (quantVal > 0.0) {
                                let curve = quantVal * quantVal * quantVal;
                                let steps = 2.0 + (1.0 - curve) * 62.0;
                                let quantizedPhase = (Math.floor(modPhase * steps) + 0.5) / steps;
                                modPhase = modPhase * (1.0 - quantVal) + quantizedPhase * quantVal;
                            }"""

app_content = app_content.replace(old_quant, new_quant)

with open('app.js', 'w', encoding='utf-8') as f:
    f.write(app_content)
