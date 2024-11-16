uniform float time;
uniform vec2 resolution;
uniform sampler2D audioTexture;

void main() {
    float bass_intensity = texture2D(audioTexture, vec2(gl_FragCoord.x / resolution.x, 0)).x;
    gl_FragColor = vec4( bass_intensity, 0, 0, 1);
}