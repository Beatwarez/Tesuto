import re

with open('app.js', 'r') as f:
    content = f.read()

# 1. Remove the old 1.5 Draw INFECT background smokey light pulsation
content = re.sub(
    r"// 1\.5 Draw INFECT background smokey light pulsation \(Concept 2 - Revised\).*?this\.ctx\.fill\(\);\s*\}",
    "",
    content,
    flags=re.DOTALL
)

# 2. Add jitter to the geometric grid background (2.)
grid_replacement = """// 2. Draw geometric grid background
        const gridCount = 6;
        this.ctx.lineWidth = 1.0;
        for (let i = 1; i <= gridCount; i++) {
            const rad = maxRadius * (i / gridCount) * (1.0 - visual3 * 0.15);
            this.ctx.strokeStyle = gba(255, 255, 255, );
            this.ctx.beginPath();
            for (let angle = 0; angle <= Math.PI * 2; angle += 0.05) {
                const warp = Math.sin(angle * 5 + Date.now() * 0.0008) * visual1 * 14 * (i / gridCount);
                
                // INFECT (visual6) Spiky Grid Jitter
                let spike = 0.0;
                if (visual6 > 0.001) {
                    const hash = Math.sin(angle * 123.456 + i * 87.65 + Date.now() * 0.005);
                    spike = hash * 40.0 * visual6 * (i / gridCount);
                }
                
                const x = centerX + (rad + warp + spike) * Math.cos(angle);
                const y = centerY + (rad + warp + spike) * Math.sin(angle);
                if (angle === 0) this.ctx.moveTo(x, y);
                else this.ctx.lineTo(x, y);
            }
            this.ctx.closePath();
            this.ctx.stroke();
        }"""

content = re.sub(
    r"// 2\. Draw geometric grid background.*?this\.ctx\.stroke\(\);\s*\}",
    grid_replacement,
    content,
    flags=re.DOTALL
)

# 3. Add Spiky Web to the Interconnecting elastic mesh strings (4.)
mesh_replacement = """// Interconnecting elastic mesh strings (clean, no trails)
            if (i > 0) {
                this.ctx.strokeStyle = hsla(, %, %, );
                this.ctx.beginPath();
                this.ctx.moveTo(prevX, prevY);
                
                if (visual6 > 0.001) {
                    // INFECT (visual6) Spiky Web Jitter
                    const hash = Math.sin(i * 1234.5 + Date.now() * 0.005);
                    const midX = (prevX + x) * 0.5 + Math.cos(hash * Math.PI) * 50.0 * visual6;
                    const midY = (prevY + y) * 0.5 + Math.sin(hash * Math.PI) * 50.0 * visual6;
                    this.ctx.lineTo(midX, midY);
                }
                
                this.ctx.lineTo(x, y);
                this.ctx.stroke();
            }"""

content = re.sub(
    r"// Interconnecting elastic mesh strings \(clean, no trails\).*?this\.ctx\.stroke\(\);\s*\}",
    mesh_replacement,
    content,
    flags=re.DOTALL
)


with open('app.js', 'w') as f:
    f.write(content)
