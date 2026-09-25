import re

cpp_code = """
            float clone_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float clone_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            float cloneVal = std::clamp(clone_param + macroVal * clone_mod, 0.0f, 1.0f);
            
            float cloneAmount_param = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.0f;
            float cloneAmount_mod   = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            float cloneAmountVal = std::clamp(cloneAmount_param + macroVal * cloneAmount_mod, 0.0f, 1.0f);

            if (cloneAmountVal > 0.001f) {
                float stateFloat = cloneVal * 14.0f;
                int stateIndex = (int)stateFloat;
                float morph = stateFloat - (float)stateIndex;

                auto getCloneMask = [&](int state, int p) -> float {
                    switch(state) {
                        case 0: return (p % 2 == 0) ? 1.5f : 0.5f; // Odd/Even Alternation
                        case 1: return (p % 3 == 0) ? 1.5f : 0.5f; // Triplets Focus
                        case 2: return (((p+1) & p) == 0) ? 1.8f : 0.2f; // Octave Isolation (powers of 2)
                        case 3: return 0.5f + 0.5f * std::sin((float)p * 0.1f); // Gentle Comb Filter
                        case 4: return 0.5f + 0.5f * std::sin((float)p * 0.5f); // Aggressive Comb Filter
                        case 5: return (float)(p % 16) / 15.0f; // Fractal Clones
                        case 6: { // Prime Number Mask
                            int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
                            for (int pr : primes) { if (p+1 == pr) return 1.5f; }
                            return 0.2f;
                        }
                        case 7: return (float)(p % 10) / 9.0f; // Sawtooth Ripple
                        case 8: return (p / 10) % 2 == 0 ? 0.2f : 1.5f; // Spectral Gapping
                        case 9: return (p < 30) ? 0.4f : 1.5f; // High-Frequency Mirror
                        case 10: return (p == 0 || p == 1) ? 0.1f : 1.2f; // Sub-Harmonic Ghosting
                        case 11: return (p / 4) % 2 == 0 ? 1.5f : 0.2f; // Spectral Checkerboard
                        case 12: { // Fibonacci Masking
                            int fibs[] = {1,2,3,5,8,13,21,34,55,89,144,233,377};
                            for (int f : fibs) { if (p+1 == f) return 1.8f; }
                            return 0.1f;
                        }
                        case 13: return (float)((p * 7) % 13) / 13.0f; // Modulo Shredding
                        case 14: return std::abs(std::sin((float)p * 42.1337f)) * 1.5f; // Amplitude Entropy
                    }
                    return 1.0f;
                };

                for (int p = 0; p < targetPartials; ++p) {
                    if (targetAmps[p] > 0.0f) {
                        float maskA = getCloneMask(stateIndex, p);
                        float maskB = getCloneMask(std::min(14, stateIndex + 1), p);
                        float mask = maskA * (1.0f - morph) + maskB * morph;
                        targetAmps[p] *= (1.0f - cloneAmountVal) + (mask * cloneAmountVal);
                    }
                }
            }

        } else if (engineType == 3) { // SPACE
"""

with open('Source/PluginProcessor.cpp', 'r') as f:
    cpp_content = f.read()

cpp_content = cpp_content.replace("        } else if (engineType == 3) { // SPACE", cpp_code)

with open('Source/PluginProcessor.cpp', 'w') as f:
    f.write(cpp_content)
