import re

js_code = """
                    // CLONE Logic (Amplitude Masking)
                    if (cloneAmountVal > 0.001) {
                        let stateFloat = cloneVal * 14.0;
                        let stateIndex = Math.floor(stateFloat);
                        let morph = stateFloat - stateIndex;

                        const getCloneMask = (state, p) => {
                            switch(state) {
                                case 0: return (p % 2 === 0) ? 1.5 : 0.5; // Odd/Even Alternation
                                case 1: return (p % 3 === 0) ? 1.5 : 0.5; // Triplets Focus
                                case 2: return ((p+1) & p) === 0 ? 1.8 : 0.2; // Octave Isolation (powers of 2)
                                case 3: return 0.5 + 0.5 * Math.sin(p * 0.1); // Gentle Comb Filter
                                case 4: return 0.5 + 0.5 * Math.sin(p * 0.5); // Aggressive Comb Filter
                                case 5: return (p % 16) / 15.0; // Fractal Clones
                                case 6: { // Prime Number Mask
                                    const primes = [2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97];
                                    return primes.includes(p+1) ? 1.5 : 0.2;
                                }
                                case 7: return (p % 10) / 9.0; // Sawtooth Ripple
                                case 8: return Math.floor(p / 10) % 2 === 0 ? 0.2 : 1.5; // Spectral Gapping
                                case 9: return (p < 30) ? 0.4 : 1.5; // High-Frequency Mirror
                                case 10: return (p === 0 || p === 1) ? 0.1 : 1.2; // Sub-Harmonic Ghosting
                                case 11: return Math.floor(p / 4) % 2 === 0 ? 1.5 : 0.2; // Spectral Checkerboard
                                case 12: { // Fibonacci Masking
                                    const fibs = [1,2,3,5,8,13,21,34,55,89,144,233,377];
                                    return fibs.includes(p+1) ? 1.8 : 0.1;
                                }
                                case 13: return ((p * 7) % 13) / 13.0; // Modulo Shredding
                                case 14: return Math.abs(Math.sin(p * 42.1337)) * 1.5; // Amplitude Entropy
                            }
                            return 1.0;
                        };

                        for (let p = 0; p < MAX_PARTIALS; ++p) {
                            if (targetAmps[p] > 0.0) {
                                let maskA = getCloneMask(stateIndex, p);
                                let maskB = getCloneMask(Math.min(14, stateIndex + 1), p);
                                let mask = maskA * (1.0 - morph) + maskB * morph;
                                targetAmps[p] *= (1.0 - cloneAmountVal) + (mask * cloneAmountVal);
                            }
                        }
                    }

                    // Precalculate phase delta and panning
"""

with open('app.js', 'r') as f:
    app_content = f.read()

# Add parameter descriptors
app_content = app_content.replace(
    "{ name: 'infectAmount', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },",
    "{ name: 'infectAmount', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },\n            { name: 'cloneAmount', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },\n            { name: 'clone', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },"
)

# Read variables
app_content = app_content.replace(
    "const infectAmount = parameters.infectAmount ? parameters.infectAmount[0] : 0.0;",
    "const infectAmount = parameters.infectAmount ? parameters.infectAmount[0] : 0.0;\n              const cloneAmountVal = parameters.cloneAmount ? parameters.cloneAmount[0] : 0.0;\n              const cloneVal = parameters.clone ? parameters.clone[0] : 0.0;"
)

# Insert logic
app_content = app_content.replace("// Precalculate phase delta and panning", js_code)

with open('app.js', 'w') as f:
    f.write(app_content)
