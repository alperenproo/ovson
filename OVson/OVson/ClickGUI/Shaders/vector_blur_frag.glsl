R"glsl(
#version 120
varying vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2 u_velocity;

void main() {
    vec2 texCoord = v_texCoord;
    vec4 color = texture2D(u_texture, texCoord);
    
    // Sample along the velocity vector
    const int samples = 12;
    vec2 offset = u_velocity / float(samples);
    
    for(int i = 1; i < samples; ++i) {
        texCoord += offset;
        color += texture2D(u_texture, texCoord);
    }
    
    gl_FragColor = color / float(samples);
}
)glsl"
