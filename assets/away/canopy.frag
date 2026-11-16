// A single transparent draw over the opaque face. The reflected panorama is
// fixed in studio space so its cards travel over the lens as the pet turns.
void MAIN()
{
    vec3 n = normalize(NORMAL);
    vec3 v = normalize(VIEW_VECTOR);
    float nv = clamp(dot(n, v), 0.0, 1.0);
    vec3 r = reflect(-v, n);
    vec2 envUV = vec2(0.5 + atan(r.x, r.z) / 6.2831853,
                      0.5 + asin(clamp(r.y, -1.0, 1.0)) / 3.14159265);
    vec3 light = texture(reflectionMap, envUV).rgb;
    float fresnel = 0.046 + 0.954 * pow(1.0 - nv, 3.9);
    float rim = smoothstep(0.62, 0.995, UV0.x);
    // RuntimeLoader changes glTF's top-origin V to Qt's bottom-origin V.
    float lower = smoothstep(0.53, 0.82, 1.0 - UV0.y);
    vec3 reflection = 1.0 - exp(-light * (0.28 + 0.76 * fresnel));
    // The bottom of the optic picks up the cyan light guide under its lip.
    float spill = lower * smoothstep(0.44, 0.995, UV0.x);
    vec3 reflectedLED=mix(vec3(.002,.13,.14),vec3(.32,.032,.003),clamp(signalWarmth,0.0,1.0));
    if (signalWarmth<-.5) reflectedLED=vec3(.0025,.003,.0035);
    reflection += (1.0-reflection)*reflectedLED*spill*(.35+1.9*signalPower);
    float peak = max(reflection.r, max(reflection.g, reflection.b));
    vec3 glass = vec3(0.055, 0.175, 0.188) + fresnel * vec3(0.17, 0.37, 0.39);
    glass += rim*lower*reflectedLED*(.5+1.7*signalPower);
    if (signalWarmth<-.5) glass=vec3(dot(glass,vec3(.2126,.7152,.0722)))*.45;
    float cover = 0.038 + 0.61 * fresnel + rim * (0.055 + 0.075 * lower);
    float alpha = peak + (1.0 - peak) * cover;
    BASE_COLOR = vec4(0.0, 0.0, 0.0, alpha);
    METALNESS = 0.0;
    SPECULAR_AMOUNT = 0.0;
    EMISSIVE_COLOR = (reflection + (1.0 - peak) * cover * glass) / max(alpha, 0.001);
}
