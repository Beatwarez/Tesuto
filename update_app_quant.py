import re

with open('app.js', 'r', encoding='utf-8') as f:
    app_content = f.read()

app_content = app_content.replace("{ name: 'ring', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },", "{ name: 'quant', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },")
app_content = app_content.replace("const ringVal = parameters.ring ? parameters.ring[0] : 0.0;", "const quantVal = parameters.quant ? parameters.quant[0] : 0.0;")

# Inject quant logic right before 'let normModPhase'
quant_logic = """                            if (quantVal > 0.0) {
                                let steps = 2.0 + (1.0 - quantVal) * 62.0;
                                let quantizedPhase = Math.floor(modPhase * steps) / steps;
                                modPhase = modPhase * (1.0 - quantVal) + quantizedPhase * quantVal;
                            }
                            // Lookup sine table with phase wrapped to [0, 1)"""
                            
app_content = app_content.replace("// Lookup sine table with phase wrapped to [0, 1)", quant_logic)

with open('app.js', 'w', encoding='utf-8') as f:
    f.write(app_content)
