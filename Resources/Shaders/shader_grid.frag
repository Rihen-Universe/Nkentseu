#version 450

in vec4 worldPosition;

in vec3 gridColor;

void main() {
    vec3 fragWorldPos = worldPosition.xyz / worldPosition.w;
    float cellSize = 0.1;
    vec2 repeatGrid = vec2(10.0, 10.0);
    vec2 gridPos = fract(fragWorldPos.xz / cellSize) - 0.5;
    vec2 grid = step(abs(gridPos * repeatGrid), vec2(0.005));
    vec3 finalColor = mix(vec3(1.0), gridColor, grid.x * grid.y);
    gl_FragColor = vec4(finalColor, 1.0);
}
