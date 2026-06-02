R"glsl(
#version 120
varying vec2 v_texCoord;
void main() {
    v_texCoord = gl_MultiTexCoord0.xy;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)glsl"
