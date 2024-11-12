uniform float time;

void main() {
    float v = (sin(time * gl_FragCoord.x / 3.0) + 1.0)/2.0;
    gl_FragColor = vec4(v, 0, 0, 1);
}