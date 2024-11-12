uniform float time;

void main() {
    float v = (sin(2.0 * time * gl_FragCoord.x) + 1.0)/2.0;
    gl_FragColor = vec4(0, v, v, 1);
}