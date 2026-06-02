R"glsl(
#version 120
varying vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2 u_direction;

void main() {
    vec4 color = vec4(0.0);
    color += texture2D(u_texture, v_texCoord) * 0.2270270270;
    color += texture2D(u_texture, v_texCoord + u_direction * 1.0) * 0.1945945946;
    color += texture2D(u_texture, v_texCoord - u_direction * 1.0) * 0.1945945946;
    color += texture2D(u_texture, v_texCoord + u_direction * 2.0) * 0.1216216216;
    color += texture2D(u_texture, v_texCoord - u_direction * 2.0) * 0.1216216216;
    color += texture2D(u_texture, v_texCoord + u_direction * 3.0) * 0.0540540541;
    color += texture2D(u_texture, v_texCoord - u_direction * 3.0) * 0.0540540541;
    color += texture2D(u_texture, v_texCoord + u_direction * 4.0) * 0.0162162162;
    color += texture2D(u_texture, v_texCoord - u_direction * 4.0) * 0.0162162162;
    gl_FragColor = color;
}
)glsl"
