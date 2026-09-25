import re

with open('app.js', 'r', encoding='utf-8') as f:
    app_content = f.read()

app_content = app_content.replace("{ name: 'ring', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },", "{ name: 'quant', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },")
app_content = app_content.replace("const ringVal = parameters.ring ? parameters.ring[0] : 0.0;", "const quantVal = parameters.quant ? parameters.quant[0] : 0.0;")

# Remove ring phase outside loop in app.js if it exists, wait, we didn't add it outside loop in app.js!
# Let's check app.js for ringVal usage
