import re

cpp_logic = """
            float infectVal = std::clamp(drive_param + macroVal * drive_mod, 0.0f, 1.0f);
            
            float sym_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float sym_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            float amountVal = std::clamp(sym_param + macroVal * sym_mod, 0.0f, 1.0f);

            if (amountVal > 0.001f) {
                float old_freqs[512];
                for (int p = 0; p < 512; ++p) old_freqs[p] = freqs[p];
                
                float stateFloat = infectVal * 14.0f;
                int stateIndex = (int)stateFloat;
                float morph = stateFloat - (float)stateIndex;

                auto getTargetFreq = [&](int state, int p) -> float {
                    if (p >= targetPartials) return old_freqs[p];
                    
                    int target_p = p;
                    switch(state) {
                        case 0: target_p = (p % 2 == 1) ? p - 1 : p; break;
                        case 1: target_p = p - (p % 3); break;
                        case 2: return old_freqs[0] + (old_freqs[p] - old_freqs[0]) * 0.1f;
                        case 3: {
                            int oct = 0; while((1 << (oct+1)) - 1 <= p) oct++;
                            target_p = (1 << oct) - 1; 
                            break;
                        }
                        case 4: return old_freqs[p] + ((p % 2 == 1) ? old_freqs[0] * 0.5f : 0.0f);
                        case 5: {
                            int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
                            int h = p + 1;
                            int best = 2; int min_diff = 9999;
                            for (int pr : primes) { if (std::abs(h - pr) < min_diff) { min_diff = std::abs(h - pr); best = pr; } }
                            target_p = best - 1; 
                            break;
                        }
                        case 6: return (p < 15) ? old_freqs[0] : old_freqs[p] + old_freqs[0] * 32.0f;
                        case 7: return old_freqs[p] + std::sin(p * 0.5f) * old_freqs[0] * 2.0f;
                        case 8: return old_freqs[0] * (p + 1) * 1.61803398f;
                        case 9: target_p = std::round(p / 16.0f) * 16.0f; break;
                        case 10: target_p = 31 - std::abs(31 - p); break;
                        case 11: target_p = 6; break;
                        case 12: return old_freqs[0] + std::fmod(old_freqs[p] * 3.7f, old_freqs[0] * 16.0f);
                        case 13: target_p = 511 - p; break;
                        case 14: return old_freqs[p] + std::sin(p * 12.9898f) * old_freqs[p] * 0.5f;
                    }
                    if (target_p < 0) target_p = 0;
                    if (target_p > 511) target_p = 511;
                    return old_freqs[target_p];
                };

                for (int p = 0; p < targetPartials; ++p) {
                    if (targetAmps[p] > 0.0f) {
                        float freqA = getTargetFreq(stateIndex, p);
                        float freqB = getTargetFreq(std::min(14, stateIndex + 1), p);
                        float interpFreq = freqA * (1.0f - morph) + freqB * morph;
                        freqs[p] = old_freqs[p] * (1.0f - amountVal) + interpFreq * amountVal;
                        if (freqs[p] < 0.0f) freqs[p] = std::abs(freqs[p]);
                    }
                }
            }
"""

with open('Source/PluginProcessor.cpp', 'r') as f:
    content = f.read()

content = re.sub(
    r"float driveVal = std::clamp\(drive_param \+ macroVal \* drive_mod, 0\.0f, 1\.0f\);\s*float sym_param.*?// TODO: Implement INFECT logic here later",
    cpp_logic,
    content,
    flags=re.DOTALL
)

with open('Source/PluginProcessor.cpp', 'w') as f:
    f.write(content)

js_logic = """
                    // INFECT Logic
                    if (infectAmount > 0.001) {
                        const old_freqs = new Float32Array(freqs);
                        let stateFloat = infectVal * 14.0;
                        let stateIndex = Math.floor(stateFloat);
                        let morph = stateFloat - stateIndex;

                        const getTargetFreq = (state, p) => {
                            let target_p = p;
                            switch(state) {
                                case 0: target_p = (p % 2 === 1) ? p - 1 : p; break;
                                case 1: target_p = p - (p % 3); break;
                                case 2: return old_freqs[0] + (old_freqs[p] - old_freqs[0]) * 0.1;
                                case 3: {
                                    let oct = 0; while((1 << (oct+1)) - 1 <= p) oct++;
                                    target_p = (1 << oct) - 1; 
                                    break;
                                }
                                case 4: return old_freqs[p] + ((p % 2 === 1) ? old_freqs[0] * 0.5 : 0.0);
                                case 5: {
                                    const primes = [2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97];
                                    let h = p + 1;
                                    let best = 2; let min_diff = 9999;
                                    for (let i=0; i<primes.length; i++) {
                                        if (Math.abs(h - primes[i]) < min_diff) { min_diff = Math.abs(h - primes[i]); best = primes[i]; }
                                    }
                                    target_p = best - 1; 
                                    break;
                                }
                                case 6: return (p < 15) ? old_freqs[0] : old_freqs[p] + old_freqs[0] * 32.0;
                                case 7: return old_freqs[p] + Math.sin(p * 0.5) * old_freqs[0] * 2.0;
                                case 8: return old_freqs[0] * (p + 1) * 1.61803398;
                                case 9: target_p = Math.round(p / 16.0) * 16; break;
                                case 10: target_p = 31 - Math.abs(31 - p); break;
                                case 11: target_p = 6; break;
                                case 12: return old_freqs[0] + ((old_freqs[p] * 3.7) % (old_freqs[0] * 16.0));
                                case 13: target_p = 511 - p; break;
                                case 14: return old_freqs[p] + Math.sin(p * 12.9898) * old_freqs[p] * 0.5;
                            }
                            if (target_p < 0) target_p = 0;
                            if (target_p > 511) target_p = 511;
                            return old_freqs[target_p];
                        };

                        for (let p = 0; p < MAX_PARTIALS; ++p) {
                            if (targetAmps[p] > 0.0) {
                                let freqA = getTargetFreq(stateIndex, p);
                                let freqB = getTargetFreq(Math.min(14, stateIndex + 1), p);
                                let interpFreq = freqA * (1.0 - morph) + freqB * morph;
                                freqs[p] = old_freqs[p] * (1.0 - infectAmount) + interpFreq * infectAmount;
                                if (freqs[p] < 0.0) freqs[p] = Math.abs(freqs[p]);
                            }
                        }
                    }

                    // Precalculate phase delta and panning
"""

with open('app.js', 'r') as f:
    app_content = f.read()

app_content = app_content.replace("// Precalculate phase delta and panning", js_logic)

with open('app.js', 'w') as f:
    f.write(app_content)
