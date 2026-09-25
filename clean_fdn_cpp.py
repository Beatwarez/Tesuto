import re

with open('Source/PluginProcessor.h', 'r') as f:
    content = f.read()

# Remove setGlobalSendAccum
content = re.sub(
    r"void setGlobalSendAccum\(.*?\}\n",
    "",
    content,
    flags=re.DOTALL
)

# Remove globalSendAccum definition
content = re.sub(
    r"float\* globalSendAccum\[8\] = \{nullptr\};",
    "",
    content
)

with open('Source/PluginProcessor.h', 'w') as f:
    f.write(content)


with open('Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

# Remove setGlobalSendAccum call
content = re.sub(
    r"voice->setGlobalSendAccum\(\s*sendBuffers\.getWritePointer\(0\),.*?sendBuffers\.getWritePointer\(7\)\s*\);",
    "",
    content,
    flags=re.DOTALL
)

# Remove localCloudVal
content = content.replace("float localCloudVal = 0.0f;\n", "")

# Remove dryMix
content = re.sub(
    r"// Blend dry signal output\s*float dryMix = 1\.0f - localCloudVal \* 0\.3f;\s*sampleL \+= dryVal \* dryMix \* pL_block\[p\];\s*sampleR \+= dryVal \* dryMix \* pR_block\[p\];",
    "sampleL += dryVal * pL_block[p];\n        sampleR += dryVal * pR_block[p];",
    content
)

# Remove p_send_gain logic
content = re.sub(
    r"// Send logic\s*p_send_gain\[p\] = 0\.0f;\s*",
    "",
    content
)

# Remove send accumulation
content = re.sub(
    r"int route = 7 - \(p / 32\).*?\}\s*",
    "",
    content,
    flags=re.DOTALL
)


with open('Source/PluginProcessor.cpp', 'w') as f:
    f.write(content)

