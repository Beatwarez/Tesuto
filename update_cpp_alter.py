import re

with open('Source/PluginProcessor.cpp', 'r', encoding='utf-8') as f:
    cpp_content = f.read()

# 1. Add variable declarations inside Dynamic Serial Router
alter_vars = """float deSyncVal = 0.0f;
    float alterVal = 0.0f;
    float pinchVal = 0.0f;
    float ringVal = 0.0f;"""
cpp_content = cpp_content.replace(
    "float deSyncVal = 0.0f;\n    float alterVal = 0.0f;",
    alter_vars
)

# 2. Extract PINCH and RING in the ALTER block
alter_extract = """            alterVal = std::clamp(fm_param + macroVal * fm_mod, 0.0f, 1.0f);
            
            float pinch_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float pinch_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            pinchVal = std::clamp(pinch_param + macroVal * pinch_mod, 0.0f, 1.0f);
            
            float desync_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float desync_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            deSyncVal = std::clamp(desync_param + macroVal * desync_mod, 0.0f, 1.0f);

            float ring_param = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.0f;
            float ring_mod   = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            ringVal = std::clamp(ring_param + macroVal * ring_mod, 0.0f, 1.0f);"""
            
cpp_content = cpp_content.replace(
    """            alterVal = std::clamp(fm_param + macroVal * fm_mod, 0.0f, 1.0f);
            
            float desync_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float desync_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            deSyncVal = std::clamp(desync_param + macroVal * desync_mod, 0.0f, 1.0f);""",
    alter_extract
)

# 3. Inject PINCH in audio loop
pinch_logic_cpp = """              if (pinchVal > 0.0f) {
                  modPhase += std::sin(modPhase * 6.2831853f) * pinchVal * 0.3f;
              }
              // Wrap phase to [0, 1)"""
cpp_content = cpp_content.replace("// Wrap phase to [0, 1)", pinch_logic_cpp)

# 4. Inject RING in audio loop
ring_logic_cpp = """                val = valUnsync * (1.0f - syncMix) + valSync * syncMix;
              }
              
              if (ringVal > 0.0f) {
                  float ringPhase = phases[0] * 1.5f;
                  float ringSine = std::sin(ringPhase * 6.2831853f);
                  val = val * (1.0f - ringVal) + (val * ringSine) * ringVal;
              }

              val *= targetAmps[p];"""
cpp_content = cpp_content.replace("""                val = valUnsync * (1.0f - syncMix) + valSync * syncMix;
              }

              val *= targetAmps[p];""", ring_logic_cpp)

with open('Source/PluginProcessor.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_content)
