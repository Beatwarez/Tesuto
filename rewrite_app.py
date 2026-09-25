import re

with open('app.js', 'r') as f:
    content = f.read()

content = re.sub(
    r"\{ name: 'cloud', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 \},",
    "{ name: 'infectAmount', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },\n            { name: 'infect', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },",
    content
)

content = re.sub(
    r"// FDN Reverb Initialization.*?this\.sendBuffers = Array\.from\(\{ length: 8 \}, \(\) => new Float32Array\(128\)\);",
    "",
    content,
    flags=re.DOTALL
)

content = re.sub(
    r"// Clear FDN send buffers for this block\s*for \(let i = 0; i < 8; i\+\+\) \{\s*this\.sendBuffers\[i\]\.fill\(0\.0\);\s*\}",
    "",
    content
)

content = content.replace("// Clear output if no active voices, but still run FDN tail decay!", "// Clear output if no active voices")

content = content.replace(
    "const cloudVal = parameters.cloud ? parameters.cloud[0] : 0.0;",
    "const infectAmount = parameters.infectAmount ? parameters.infectAmount[0] : 0.0;\n            const infectVal = parameters.infect ? parameters.infect[0] : 0.0;"
)

content = re.sub(
    r"// Send amount based on sweep Gaussian.*?voice\.p_send_gain\[p\] = cloudVal \* sendAmp \* 0\.2;",
    "",
    content,
    flags=re.DOTALL
)

content = re.sub(
    r"// FDN send routing based on harmonic index p.*?this\.sendBuffers\[route\]\[i\] \+= dryVal \* voice\.p_send_gain\[p\];",
    "",
    content,
    flags=re.DOTALL
)

content = re.sub(
    r"// Blend dry signals\s*const dryMix = 1\.0 - cloudVal \* 0\.3;\s*sumL \+= dryVal \* dryMix \* pL_block\[p\];\s*sumR \+= dryVal \* dryMix \* pR_block\[p\];",
    "sumL += dryVal * pL_block[p];\n                        sumR += dryVal * pR_block[p];",
    content
)

content = re.sub(
    r"// 2\. Process Global FDN Reverb sample-by-sample.*?leftChannel\[i\] \+= wetL;\s*rightChannel\[i\] \+= wetR;\s*\}",
    "",
    content,
    flags=re.DOTALL
)

content = content.replace(
    "['alter', 'size', 'sweep', 'cloud', 'pitch']",
    "['alter', 'size', 'sweep', 'infect', 'pitch']"
)

content = content.replace(
    "cloud: 0.30,",
    "infect: 0.0,\n            infectAmount: 0.0,"
)

content = content.replace(
    "{ 0: 'empty', 1: 'source', 2: 'filter', 3: 'space', 4: 'pitch', 5: 'alter', 6: 'cloud', 7: 'infect', 8: 'form' }",
    "{ 0: 'empty', 1: 'source', 2: 'filter', 3: 'space', 4: 'pitch', 5: 'alter', 6: 'infect', 7: 'infect', 8: 'form' }"
)

content = content.replace(
    "{ 'empty': 0, 'source': 1, 'filter': 2, 'space': 3, 'pitch': 4, 'alter': 5, 'cloud': 6, 'infect': 7, 'form': 8 }",
    "{ 'empty': 0, 'source': 1, 'filter': 2, 'space': 3, 'pitch': 4, 'alter': 5, 'infect': 7, 'form': 8 }"
)

content = content.replace("let visual6 = 0.0; // Cloud", "let visual6 = 0.0; // Infect")
content = content.replace("else if (engine === 'cloud') visual6 = Math.max(visual6, macroVal);", "else if (engine === 'infect') visual6 = Math.max(visual6, macroVal);")
content = content.replace("else if (engine === 'infect') visual7 = Math.max(visual7, macroVal);", "else if (engine === 'desync') visual7 = Math.max(visual7, macroVal);")
content = content.replace("// 1.5 Draw CLOUD background smokey light pulsation", "// 1.5 Draw INFECT background smokey light pulsation")

with open('app.js', 'w') as f:
    f.write(content)
