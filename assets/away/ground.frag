// A dark, rough floor fading into black, with a height-aware soft contact shadow.
// One small draw surface; no shadow map, additional light or embedded backdrop mesh.
VARYING vec2 floorUv;
float groundHash(vec2 p) {
    return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);
}
float groundNoise(vec2 p) {
    vec2 a=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(groundHash(a),groundHash(a+vec2(1,0)),f.x),
               mix(groundHash(a+vec2(0,1)),groundHash(a+vec2(1,1)),f.x),f.y);
}
void MAIN() {
    vec2 p=vec2(floorUv.x-.5,.5-floorUv.y)*5.6;
    vec2 center=vec2(petOffset.x,petOffset.z);
    float height=max(0.0,petOffset.y);
    float spread=1.0+.85*min(height,.7);
    vec2 shadow=(p-center-vec2(.08,-.12)*height)/vec2(.69*spread,.53*spread);
    float shade=exp(-dot(shadow,shadow)*1.55)*(.82-.32*min(height,.7));
    float grain=.78+.32*groundNoise(p*100.0)+.12*groundNoise(p*370.0);
    float footprint=length((p-center)/vec2(1.0,.87));
    float ring=exp(-pow((footprint-.72)/.30,2.0));
    vec2 lightPos=(p-center-vec2(-.32,-.16))/vec2(1.00,.79);
    float pool=exp(-dot(lightPos,lightPos)*1.65);
    float side=.70-.23*clamp(p.x-center.x,-1.0,1.0);
    vec3 led=signalWarmth<-.5 ? vec3(0.0) : mix(vec3(.014,.36,.42),vec3(.62,.055,.008),clamp(signalWarmth,0.0,1.0));
    float power=.5+.7*clamp(signalPower,0.0,1.0);
    vec3 base=vec3(.0045,.0060,.0070)*grain*(1.0-.82*shade);
    vec3 spill=led*(.24*ring+.10*pool)*side*power*grain*(1.0-.76*shade);
    float sheen=pow(groundNoise(p*180.0),3.0)*.004*pool*(1.0-.75*shade);
    vec3 result=base+spill+vec3(sheen);
    float fade=1.0-smoothstep(1.35,2.75,length(p));
    FRAGCOLOR=vec4(pow(clamp(result,vec3(0.0),vec3(1.0)),vec3(1.0/2.2)),fade);
}
