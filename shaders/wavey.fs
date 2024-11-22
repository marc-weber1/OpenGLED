uniform float time;
uniform vec2 resolution;
uniform sampler2D audioTexture;

void main() {
    float bass_intensity = texture2D(audioTexture, vec2(gl_FragCoord.x / resolution.x, 0.1)).r;
    float treble_intensity = sin( (-1.0 * time + gl_FragCoord.x / resolution.x) * 60.0 ) * texture2D(audioTexture, vec2(0.99, 0.3)).r;
    gl_FragColor = vec4( bass_intensity, treble_intensity, treble_intensity, 1);
}