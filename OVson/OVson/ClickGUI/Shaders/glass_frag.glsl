R"glsl(
#version 120
uniform vec2 u_resolution;
uniform vec2 u_rectPos;
uniform vec2 u_rectSize;
uniform float u_radius;
uniform float u_time;
uniform float u_alpha;
uniform vec4 u_accentColor;
uniform vec2 u_mouse;
uniform int u_wiggleEnabled;
uniform int u_glowEnabled;
uniform float u_refractStrength;
uniform float u_edgeWidth;
uniform float u_darkness;
uniform sampler2D u_screenTexture;
uniform sampler2D u_blurredTexture;
varying vec2 v_screenUV;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float Glow(vec2 uv) {
    return sin(atan(uv.y * 2.0 - 1.0, uv.x * 2.0 - 1.0) - 0.5);
}

void main() {
    vec2 pixelPos = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y);
    
    vec2 uv = (pixelPos - u_rectPos) / u_rectSize;
    
    vec2 p = (uv - 0.5) * 2.0;
    
    vec2 center = u_rectPos + u_rectSize * 0.5;
    vec2 localP = pixelPos - center;
    vec2 halfSize = u_rectSize * 0.5;
    
    float d = sdRoundBox(localP, halfSize, u_radius);
    
    if (d > 1.0) {
        discard;
    }
    
    float edgeAlpha = (1.0 - smoothstep(-1.0, 1.0, d)) * u_alpha;
    
    float insideDist = max(0.0, -d); 
    
    float maxDist = min(halfSize.x, halfSize.y);
    float normDist = insideDist / (maxDist + 0.001);
    
    float edgeW = max(0.01, u_edgeWidth);
    float lensScale = smoothstep(0.0, edgeW, normDist);
    lensScale = pow(lensScale, 0.35);
    float minScale = 1.0 - u_refractStrength;
    lensScale = lensScale * u_refractStrength + minScale;
    
    vec2 warpedP = p * lensScale;
    
    if (u_wiggleEnabled == 1) {
        float wave1 = sin(pixelPos.x * 0.03 + u_time * 1.5) * cos(pixelPos.y * 0.025 + u_time) * 0.02;
        float wave2 = cos(pixelPos.y * 0.035 - u_time * 1.2) * sin(pixelPos.x * 0.02 + u_time * 0.8) * 0.015;
        warpedP += vec2(wave1, wave2);
    }
    
    vec2 rectCenterBL = vec2(
        u_rectPos.x + u_rectSize.x * 0.5,
        u_resolution.y - (u_rectPos.y + u_rectSize.y * 0.5)
    );
    
    vec2 rectCenterUV = rectCenterBL / u_resolution;
    
    vec2 halfUV = (u_rectSize * 0.5) / u_resolution;
    
    vec2 sampleUV = rectCenterUV + vec2(warpedP.x * halfUV.x, -warpedP.y * halfUV.y);
    
    float bevelBase = 15.0 + u_edgeWidth * 45.0;
    float maxBevelW = min(bevelBase, maxDist * 0.75);
    if (insideDist < maxBevelW) {
        vec2 eps = vec2(1.0, 0.0);
        float d1x = sdRoundBox(localP + eps.xy, halfSize, u_radius);
        float d2x = sdRoundBox(localP - eps.xy, halfSize, u_radius);
        float d1y = sdRoundBox(localP + eps.yx, halfSize, u_radius);
        float d2y = sdRoundBox(localP - eps.yx, halfSize, u_radius);
        vec2 normal = vec2(d1x - d2x, d1y - d2y);
        if (length(normal) > 0.001) {
            normal = normalize(normal);
            float bevelWarp = smoothstep(maxBevelW, 0.0, insideDist);
            float bevelDisp = 12.0 + u_edgeWidth * 23.0;
            sampleUV += normal * bevelWarp * (bevelDisp / u_resolution) * u_refractStrength;
        }
    }

    sampleUV = clamp(sampleUV, vec2(0.002), vec2(0.998));
    
    vec3 color = texture2D(u_screenTexture, sampleUV).rgb;
    
    float noiseVal = rand(gl_FragCoord.xy * 1e-3) - 0.5;
    color += vec3(noiseVal) * 0.1;
    
    if (u_glowEnabled == 1) {
        float glowWeight = 0.3;
        float glowEdge0 = 0.06;
        float glowEdge1 = 0.0;
        
        float glowVal = Glow(uv) * glowWeight * smoothstep(glowEdge0, glowEdge1, normDist) + 1.0;
        color *= glowVal;
    }
    
    color = mix(color, vec3(0.05, 0.05, 0.09), u_darkness);

    color = mix(color, u_accentColor.rgb, u_accentColor.a * 0.85);

    gl_FragColor = vec4(color, edgeAlpha);
}
)glsl"
