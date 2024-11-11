uniform float time;

void main() {
    gl_FragColor = vec4((sin(2.0 * time * gl_FragCoord.x) + 1.0)/2.0, 0, 0, 1);
}