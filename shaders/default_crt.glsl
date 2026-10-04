// @param float intensity 0.0 1.0 0.8
// @param float scanlineIntensity 0.0 1.0 0.3
// @param vec3 tint 0.0 1.0 1.0 1.0 1.0

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord;
    vec2 center = vec2(0.5, 0.5);
    vec2 delta = uv - center;
    float dist = length(delta);

    vec2 curvedUV = center + delta * (1.0 + 0.2 * dist * dist);

    if (curvedUV.x < 0.0 || curvedUV.x > 1.0 || curvedUV.y < 0.0 || curvedUV.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float aberration = 0.003;
    float r = texture(iChannel0, curvedUV + vec2(aberration, 0.0)).r;
    float g = texture(iChannel0, curvedUV).g;
    float b = texture(iChannel0, curvedUV - vec2(aberration, 0.0)).b;
    vec3 col = vec3(r, g, b);

    float scanline = sin(curvedUV.y * 800.0) * 0.5 + 0.5;
    col *= 1.0 - scanlineIntensity * (1.0 - scanline);

    col *= tint;
    col *= intensity;

    fragColor = vec4(col, 1.0);
}
