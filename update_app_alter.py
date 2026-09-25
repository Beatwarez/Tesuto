import re

with open('app.js', 'r', encoding='utf-8') as f:
    app_content = f.read()

# 1. Add parameter descriptors
app_content = app_content.replace(
    "{ name: 'alter', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },",
    "{ name: 'alter', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },\n            { name: 'pinch', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },\n            { name: 'ring', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },"
)

# 2. Extract parameters
app_content = app_content.replace(
    "const alterVal = parameters.alter ? parameters.alter[0] : 0.0;",
    "const alterVal = parameters.alter ? parameters.alter[0] : 0.0;\n              const pinchVal = parameters.pinch ? parameters.pinch[0] : 0.0;\n              const ringVal = parameters.ring ? parameters.ring[0] : 0.0;"
)

# 3. Inject PINCH and RING in the audio loop
# Pinch modifies modPhase right before sine lookup
pinch_logic = """                          if (pinchVal > 0.0) {
                              modPhase += Math.sin(modPhase * 2.0 * Math.PI) * pinchVal * 0.3;
                          }
                          // Lookup sine table"""
app_content = app_content.replace("// Lookup sine table", pinch_logic)

# Ring modifies val right after sine lookup and deSync
ring_logic = """                            if (ringVal > 0.0) {
                                let ringPhase = voice.phases[0] * 1.5;
                                let ringSine = Math.sin(ringPhase * 2.0 * Math.PI);
                                val = val * (1.0 - ringVal) + (val * ringSine) * ringVal;
                            }
                            
                            val *= voice.smoothedAmps[p];"""
app_content = app_content.replace("val *= voice.smoothedAmps[p];", ring_logic)

with open('app.js', 'w', encoding='utf-8') as f:
    f.write(app_content)

