R"glsl(
#version 120
varying vec2 v_screenUV;
void main() {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    v_screenUV = gl_Position.xy * 0.5 + 0.5;
}
)glsl"
