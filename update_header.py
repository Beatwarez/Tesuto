import re

with open('Source/PluginProcessor.h', 'r', encoding='utf-8') as f:
    h_content = f.read()

old_h = """  std::atomic<float>* mod_p[7][8] = { {nullptr} };
  std::atomic<float>* mod_pMod[7][8] = { {nullptr} };"""
  
new_h = """  std::atomic<float>* mod_engine_p[7][11][8] = { { {nullptr} } };
  std::atomic<float>* mod_engine_pMod[7][11][8] = { { {nullptr} } };"""
  
h_content = h_content.replace(old_h, new_h)

with open('Source/PluginProcessor.h', 'w', encoding='utf-8') as f:
    f.write(h_content)
