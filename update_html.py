import re

html_code = """<template id="tmpl-engine-infect">
        <div class="dsp-panel engine-infect-panel">
            <div class="filter-params-row">
                <div class="knob-wrapper" data-param="mod{{LANE}}_p1">
                    <div class="custom-knob filter-knob has-mod" id="knob-mod{{LANE}}_p1" data-min="0" data-max="1" data-default="0" data-mod-param="mod{{LANE}}_p1_mod">
                        <svg class="mod-ring-svg" viewBox="0 0 100 100"><path class="mod-ring-bg" d="M 17.47 82.53 A 46 46 0 1 1 82.53 82.53" /><path class="mod-ring-arc" d="" /></svg>
                        <div class="knob-dial"><div class="knob-marker"></div></div>
                    </div>
                    <div class="knob-names">
                        <span class="knob-name-a">INFECT</span>
                    </div>
                    <span class="knob-val" id="val-mod{{LANE}}_p1">0.00</span>
                </div>
                <div class="knob-wrapper" data-param="mod{{LANE}}_p2">
                    <div class="custom-knob filter-knob has-mod" id="knob-mod{{LANE}}_p2" data-min="0" data-max="1" data-default="0" data-mod-param="mod{{LANE}}_p2_mod">
                        <svg class="mod-ring-svg" viewBox="0 0 100 100"><path class="mod-ring-bg" d="M 17.47 82.53 A 46 46 0 1 1 82.53 82.53" /><path class="mod-ring-arc" d="" /></svg>
                        <div class="knob-dial"><div class="knob-marker"></div></div>
                    </div>
                    <div class="knob-names">
                        <span class="knob-name-a">AMOUNT</span>
                    </div>
                    <span class="knob-val" id="val-mod{{LANE}}_p2">0.00</span>
                </div>
                <div class="knob-wrapper" data-param="mod{{LANE}}_p3">
                    <div class="custom-knob filter-knob has-mod" id="knob-mod{{LANE}}_p3" data-min="0" data-max="1" data-default="0" data-mod-param="mod{{LANE}}_p3_mod">
                        <svg class="mod-ring-svg" viewBox="0 0 100 100"><path class="mod-ring-bg" d="M 17.47 82.53 A 46 46 0 1 1 82.53 82.53" /><path class="mod-ring-arc" d="" /></svg>
                        <div class="knob-dial"><div class="knob-marker"></div></div>
                    </div>
                    <div class="knob-names">
                        <span class="knob-name-a">CLONE</span>
                    </div>
                    <span class="knob-val" id="val-mod{{LANE}}_p3">0.00</span>
                </div>
                <div class="knob-wrapper" data-param="mod{{LANE}}_p4">
                    <div class="custom-knob filter-knob has-mod" id="knob-mod{{LANE}}_p4" data-min="0" data-max="1" data-default="0" data-mod-param="mod{{LANE}}_p4_mod">
                        <svg class="mod-ring-svg" viewBox="0 0 100 100"><path class="mod-ring-bg" d="M 17.47 82.53 A 46 46 0 1 1 82.53 82.53" /><path class="mod-ring-arc" d="" /></svg>
                        <div class="knob-dial"><div class="knob-marker"></div></div>
                    </div>
                    <div class="knob-names">
                        <span class="knob-name-a">AMOUNT</span>
                    </div>
                    <span class="knob-val" id="val-mod{{LANE}}_p4">0.00</span>
                </div>
            </div>
        </div>
        </template>"""

with open('index.html', 'r', encoding='utf-8') as f:
    content = f.read()

content = re.sub(
    r"<template id=\"tmpl-engine-infect\">.*?</template>",
    html_code,
    content,
    flags=re.DOTALL
)

with open('index.html', 'w', encoding='utf-8') as f:
    f.write(content)

