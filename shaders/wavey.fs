uniform float time;
uniform vec2 resolution;
uniform sampler2D audioTexture;

void main() {
    float treble_intensity = texture2D(audioTexture, vec2(gl_FragCoord.x / resolution.x, 0.75)).r;
    gl_FragColor = vec4( treble_intensity, 0, 0, 1);
}