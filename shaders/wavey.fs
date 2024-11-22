uniform float time;
uniform vec2 resolution;
uniform sampler2D audioTexture;

float sample_between(float coord, float lower, float upper){
    return (upper - lower) * coord + lower;
}

void main() {
    float bass_intensity = texture2D(audioTexture, vec2(sample_between(gl_FragCoord.x / resolution.x, 0.75, 1.0), 0.125)).r;
    float treble_intensity = sin( (-0.4 * time + gl_FragCoord.x / resolution.x) * 60.0 ) * texture2D(audioTexture, vec2(0.99, 0.625)).r;
    gl_FragColor = vec4( bass_intensity, treble_intensity, treble_intensity, 1);
}