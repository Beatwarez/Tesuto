import re

with open('Source/PluginProcessor.cpp', 'r', encoding='utf-8') as f:
    cpp_content = f.read()

# 1. Update QUANT logic
old_quant1 = """        if (quantVal > 0.0f) {
            float steps = 2.0f + (1.0f - quantVal) * 62.0f;
            float quantizedPhase = std::floor(modPhaseUnsync * steps) / steps;
            modPhaseUnsync = modPhaseUnsync * (1.0f - quantVal) + quantizedPhase * quantVal;
        }"""
new_quant1 = """        if (quantVal > 0.0f) {
            float curve = quantVal * quantVal * quantVal;
            float steps = 2.0f + (1.0f - curve) * 62.0f;
            float quantizedPhase = (std::floor(modPhaseUnsync * steps) + 0.5f) / steps;
            modPhaseUnsync = modPhaseUnsync * (1.0f - quantVal) + quantizedPhase * quantVal;
        }"""
cpp_content = cpp_content.replace(old_quant1, new_quant1)

old_quant2 = """              if (quantVal > 0.0f) {
                  float steps = 2.0f + (1.0f - quantVal) * 62.0f;
                  float quantizedPhase = std::floor(modPhaseSync * steps) / steps;
                  modPhaseSync = modPhaseSync * (1.0f - quantVal) + quantizedPhase * quantVal;
              }"""
new_quant2 = """              if (quantVal > 0.0f) {
                  float curve = quantVal * quantVal * quantVal;
                  float steps = 2.0f + (1.0f - curve) * 62.0f;
                  float quantizedPhase = (std::floor(modPhaseSync * steps) + 0.5f) / steps;
                  modPhaseSync = modPhaseSync * (1.0f - quantVal) + quantizedPhase * quantVal;
              }"""
cpp_content = cpp_content.replace(old_quant2, new_quant2)


# 2. Update APVTS layout
old_layout_start = "      // Modulators 2-8\n      for (int m = 2; m <= 8; ++m) {"
old_layout_end = "      // Lane Enable Toggles"

new_layout = """      // Modulators 2-8
      for (int m = 2; m <= 8; ++m) {
          juce::String mStr = juce::String(m);
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_engine", 1), "mod" + mStr + "_engine", 0.0f, 10.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_macro", 1), "mod" + mStr + "_macro", 0.0f, 1.0f, 0.0f));
          
          // FILTER
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_cutoff", 1), "mod" + mStr + "_filter_cutoff", 0.0f, 1.0f, 0.5f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_cutoff_mod", 1), "mod" + mStr + "_filter_cutoff_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_offset", 1), "mod" + mStr + "_filter_offset", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_offset_mod", 1), "mod" + mStr + "_filter_offset_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_reso", 1), "mod" + mStr + "_filter_reso", 0.0f, 1.0f, 0.2f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_reso_mod", 1), "mod" + mStr + "_filter_reso_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_slope", 1), "mod" + mStr + "_filter_slope", 0.0f, 1.0f, 0.5f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_slope_mod", 1), "mod" + mStr + "_filter_slope_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_morph", 1), "mod" + mStr + "_filter_morph", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_morph_mod", 1), "mod" + mStr + "_filter_morph_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("mod" + mStr + "_filter_typeA", 1), "mod" + mStr + "_filter_typeA", 0, 5, 0));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_typeA_mod", 1), "mod" + mStr + "_filter_typeA_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("mod" + mStr + "_filter_typeB", 1), "mod" + mStr + "_filter_typeB", 0, 5, 0));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filter_typeB_mod", 1), "mod" + mStr + "_filter_typeB_mod", -1.0f, 1.0f, 0.0f));
          
          // SPACE
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_width", 1), "mod" + mStr + "_space_width", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_width_mod", 1), "mod" + mStr + "_space_width_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_orbit", 1), "mod" + mStr + "_space_orbit", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_orbit_mod", 1), "mod" + mStr + "_space_orbit_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_smear", 1), "mod" + mStr + "_space_smear", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_space_smear_mod", 1), "mod" + mStr + "_space_smear_mod", -1.0f, 1.0f, 0.0f));
          
          // ALTER
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_fm", 1), "mod" + mStr + "_alter_fm", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_fm_mod", 1), "mod" + mStr + "_alter_fm_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_pinch", 1), "mod" + mStr + "_alter_pinch", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_pinch_mod", 1), "mod" + mStr + "_alter_pinch_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_desync", 1), "mod" + mStr + "_alter_desync", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_desync_mod", 1), "mod" + mStr + "_alter_desync_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_quant", 1), "mod" + mStr + "_alter_quant", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_alter_quant_mod", 1), "mod" + mStr + "_alter_quant_mod", -1.0f, 1.0f, 0.0f));
          
          // INFECT
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_drive", 1), "mod" + mStr + "_infect_drive", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_drive_mod", 1), "mod" + mStr + "_infect_drive_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_sym", 1), "mod" + mStr + "_infect_sym", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_sym_mod", 1), "mod" + mStr + "_infect_sym_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_clone", 1), "mod" + mStr + "_infect_clone", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_clone_mod", 1), "mod" + mStr + "_infect_clone_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_cloneAmount", 1), "mod" + mStr + "_infect_cloneAmount", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_infect_cloneAmount_mod", 1), "mod" + mStr + "_infect_cloneAmount_mod", -1.0f, 1.0f, 0.0f));
          
          // FORM
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_warp", 1), "mod" + mStr + "_form_warp", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_warp_mod", 1), "mod" + mStr + "_form_warp_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_fold", 1), "mod" + mStr + "_form_fold", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_fold_mod", 1), "mod" + mStr + "_form_fold_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_tension", 1), "mod" + mStr + "_form_tension", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_tension_mod", 1), "mod" + mStr + "_form_tension_mod", -1.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_shape", 1), "mod" + mStr + "_form_shape", 0.0f, 1.0f, 0.0f));
          layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_form_shape_mod", 1), "mod" + mStr + "_form_shape_mod", -1.0f, 1.0f, 0.0f));
      }
      
"""
idx1 = cpp_content.find(old_layout_start)
idx2 = cpp_content.find(old_layout_end)
cpp_content = cpp_content[:idx1] + new_layout + cpp_content[idx2:]


# 3. Update Constructor Pointers
old_constructor_start = "      for (int m = 2; m <= 8; ++m) {"
# Search for it AFTER the createParameterLayout function
constructor_idx = cpp_content.find("KronosAudioProcessor::KronosAudioProcessor")
old_ptr_idx = cpp_content.find(old_constructor_start, constructor_idx)
old_ptr_end_idx = cpp_content.find("  // ==========================================================================", old_ptr_idx)

new_ptr = """      for (int m = 2; m <= 8; ++m) {
          juce::String mStr = juce::String(m);
          mod_engine[m-2] = apvts.getRawParameterValue("mod" + mStr + "_engine");
          mod_macro[m-2] = apvts.getRawParameterValue("mod" + mStr + "_macro");
          
          mod_engine_p[m-2][2][0] = apvts.getRawParameterValue("mod" + mStr + "_filter_cutoff");
          mod_engine_pMod[m-2][2][0] = apvts.getRawParameterValue("mod" + mStr + "_filter_cutoff_mod");
          mod_engine_p[m-2][2][1] = apvts.getRawParameterValue("mod" + mStr + "_filter_offset");
          mod_engine_pMod[m-2][2][1] = apvts.getRawParameterValue("mod" + mStr + "_filter_offset_mod");
          mod_engine_p[m-2][2][2] = apvts.getRawParameterValue("mod" + mStr + "_filter_reso");
          mod_engine_pMod[m-2][2][2] = apvts.getRawParameterValue("mod" + mStr + "_filter_reso_mod");
          mod_engine_p[m-2][2][3] = apvts.getRawParameterValue("mod" + mStr + "_filter_slope");
          mod_engine_pMod[m-2][2][3] = apvts.getRawParameterValue("mod" + mStr + "_filter_slope_mod");
          mod_engine_p[m-2][2][4] = apvts.getRawParameterValue("mod" + mStr + "_filter_morph");
          mod_engine_pMod[m-2][2][4] = apvts.getRawParameterValue("mod" + mStr + "_filter_morph_mod");
          mod_engine_p[m-2][2][5] = apvts.getRawParameterValue("mod" + mStr + "_filter_typeA");
          mod_engine_pMod[m-2][2][5] = apvts.getRawParameterValue("mod" + mStr + "_filter_typeA_mod");
          mod_engine_p[m-2][2][6] = apvts.getRawParameterValue("mod" + mStr + "_filter_typeB");
          mod_engine_pMod[m-2][2][6] = apvts.getRawParameterValue("mod" + mStr + "_filter_typeB_mod");
          
          mod_engine_p[m-2][3][0] = apvts.getRawParameterValue("mod" + mStr + "_space_width");
          mod_engine_pMod[m-2][3][0] = apvts.getRawParameterValue("mod" + mStr + "_space_width_mod");
          mod_engine_p[m-2][3][1] = apvts.getRawParameterValue("mod" + mStr + "_space_orbit");
          mod_engine_pMod[m-2][3][1] = apvts.getRawParameterValue("mod" + mStr + "_space_orbit_mod");
          mod_engine_p[m-2][3][2] = apvts.getRawParameterValue("mod" + mStr + "_space_smear");
          mod_engine_pMod[m-2][3][2] = apvts.getRawParameterValue("mod" + mStr + "_space_smear_mod");
          
          mod_engine_p[m-2][5][0] = apvts.getRawParameterValue("mod" + mStr + "_alter_fm");
          mod_engine_pMod[m-2][5][0] = apvts.getRawParameterValue("mod" + mStr + "_alter_fm_mod");
          mod_engine_p[m-2][5][1] = apvts.getRawParameterValue("mod" + mStr + "_alter_pinch");
          mod_engine_pMod[m-2][5][1] = apvts.getRawParameterValue("mod" + mStr + "_alter_pinch_mod");
          mod_engine_p[m-2][5][2] = apvts.getRawParameterValue("mod" + mStr + "_alter_desync");
          mod_engine_pMod[m-2][5][2] = apvts.getRawParameterValue("mod" + mStr + "_alter_desync_mod");
          mod_engine_p[m-2][5][3] = apvts.getRawParameterValue("mod" + mStr + "_alter_quant");
          mod_engine_pMod[m-2][5][3] = apvts.getRawParameterValue("mod" + mStr + "_alter_quant_mod");
          
          mod_engine_p[m-2][7][0] = apvts.getRawParameterValue("mod" + mStr + "_infect_drive");
          mod_engine_pMod[m-2][7][0] = apvts.getRawParameterValue("mod" + mStr + "_infect_drive_mod");
          mod_engine_p[m-2][7][1] = apvts.getRawParameterValue("mod" + mStr + "_infect_sym");
          mod_engine_pMod[m-2][7][1] = apvts.getRawParameterValue("mod" + mStr + "_infect_sym_mod");
          mod_engine_p[m-2][7][2] = apvts.getRawParameterValue("mod" + mStr + "_infect_clone");
          mod_engine_pMod[m-2][7][2] = apvts.getRawParameterValue("mod" + mStr + "_infect_clone_mod");
          mod_engine_p[m-2][7][3] = apvts.getRawParameterValue("mod" + mStr + "_infect_cloneAmount");
          mod_engine_pMod[m-2][7][3] = apvts.getRawParameterValue("mod" + mStr + "_infect_cloneAmount_mod");
          
          mod_engine_p[m-2][8][0] = apvts.getRawParameterValue("mod" + mStr + "_form_warp");
          mod_engine_pMod[m-2][8][0] = apvts.getRawParameterValue("mod" + mStr + "_form_warp_mod");
          mod_engine_p[m-2][8][1] = apvts.getRawParameterValue("mod" + mStr + "_form_fold");
          mod_engine_pMod[m-2][8][1] = apvts.getRawParameterValue("mod" + mStr + "_form_fold_mod");
          mod_engine_p[m-2][8][2] = apvts.getRawParameterValue("mod" + mStr + "_form_tension");
          mod_engine_pMod[m-2][8][2] = apvts.getRawParameterValue("mod" + mStr + "_form_tension_mod");
          mod_engine_p[m-2][8][3] = apvts.getRawParameterValue("mod" + mStr + "_form_shape");
          mod_engine_pMod[m-2][8][3] = apvts.getRawParameterValue("mod" + mStr + "_form_shape_mod");
      }
"""
cpp_content = cpp_content[:old_ptr_idx] + new_ptr + cpp_content[old_ptr_end_idx:]


# 4. Update processBlock usage
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
