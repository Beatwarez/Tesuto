import re

with open('Source/PluginProcessor.cpp', 'r', encoding='utf-8') as f:
    cpp_content = f.read()

# Update processBlock usage
def replace_mod(text, engine_id):
    start_idx = text.find(f"if (engineType == {engine_id})")
    if start_idx == -1: return text
    end_idx = text.find("} else if", start_idx)
    if end_idx == -1: end_idx = text.find("// --- 3. Envelopes", start_idx)
    if end_idx == -1: return text
    
    block = text[start_idx:end_idx]
    block = re.sub(r"processor->mod_p\[laneIdx\]\[(\d+)\]", f"processor->mod_engine_p[laneIdx][{engine_id}][\\g<1>]", block)
    block = re.sub(r"processor->mod_pMod\[laneIdx\]\[(\d+)\]", f"processor->mod_engine_pMod[laneIdx][{engine_id}][\\g<1>]", block)
    return text[:start_idx] + block + text[end_idx:]

cpp_content = replace_mod(cpp_content, 2) # FILTER
cpp_content = replace_mod(cpp_content, 8) # FORM
cpp_content = replace_mod(cpp_content, 5) # ALTER
cpp_content = replace_mod(cpp_content, 7) # INFECT
cpp_content = replace_mod(cpp_content, 3) # SPACE

with open('Source/PluginProcessor.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_content)
