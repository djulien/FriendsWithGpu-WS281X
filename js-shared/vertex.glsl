//vertex shader:
#include "preamble.glsl"

//effect generation (color selection) should be done in here, *not* in fragment shader
//runs once per vertex, which is ~ 500 - 1000x less than fragment shader

//attribute vec3 aVertexPosition;
//attribute vec4 aVertexColor;
//attribute vec2 aTextureCoord;
//varying vec2 vTextureCoord;
attribute vec3 vXYZ; //3D view (x, y, z); for 3D viewer
attribute vec3 hUNM; //hw (univ#, node#, model#); use this for "whole house" effects
attribute vec4 mXYWH; //model (x, y, w, h); use this for model-specific effects

uniform mat4 uModelView;
uniform mat4 uProjection;
uniform float elapsed, duration; //progress bar
//uniform float outmode;
uniform sampler2D uSampler; //texture (individual pixel colors) from caller
uniform bool WS281X_FMT; //allow turn on/off at run-time for debug purposes

varying vec4 vColor; //vertex color data for fragment shader (final formatting)
varying vec4 vColor24; //pivoted color data for fragment shader (final formatting)
//varying vec3 vecpos;
//varying float thing;

//varying vec2 txcoord;
vec4 gpufx(vec3 selector); //fwd ref
//tranparent value allowing GPU fx to show thru:
//CAUTION: GLSL cpp bug is expanding the "x" in "gpufx", so use different param name
#define SAMPLE_OR_FX(var, ex, y) \
    var = texture2D(uSampler, vec2(ex / MAX_UNIV, y / UNIV_MAX)); \
    var = IIF(var.a == GPUFX.a, gpufx(var.rgb), var)
#define SAMPLE_OR_FX_BIT(var, x, y, bit) \
    SAMPLE_OR_FX(var, x, y); \
    var = IIF(IFBIT(var, bit), MASK(x), ZERO)


void main(void)
{
    gl_Position = uProjection * (uModelView * vec4(vXYZ, 1.0));
//ortho projection maps as-is to screen coordinates:
    float x = gl_Position.x; //[0..24)
    float y = gl_Position.y; //[0..1152)
//#ifdef WS281X_FMT //format as WS281X requires pivot of 24 adjacent bits
    if (WS281X_FMT) //format as WS281X requires pivot of 24 adjacent bits
    {
//    float xmask = pow(2.0, x);
//get one bit from other pixels on this row:
//    vColor = IIF(vColor.a == 0.0), gpufx(vColor.rgb), vColor);
        vec4 SAMPLE_OR_FX_BIT(nabe0, 0.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe1, 1.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe2, 2.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe3, 3.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe4, 4.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe5, 5.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe6, 6.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe7, 7.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe8, 8.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe9, 9.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe10, 10.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe11, 11.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe12, 12.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe13, 13.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe14, 14.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe15, 15.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe16, 16.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe17, 17.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe18, 18.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe19, 19.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe20, 20.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe21, 21.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe22, 22.0, y, x);
        vec4 SAMPLE_OR_FX_BIT(nabe23, 23.0, y, x);
//pivot 24-bit value onto 24 parallel GPIO pins:
        vColor24 = BLACK; //ZERO;
        vColor24 += nabe0 + nabe1 + nabe2 + nabe3;
        vColor24 += nabe4 + nabe5 + nabe6 + nabe7;
        vColor24 += nabe8 + nabe9 + nabe10 + nabe11;
        vColor24 += nabe12 + nabe13 + nabe14 + nabe15;
        vColor24 += nabe16 + nabe17 + nabe18 + nabe19;
        vColor24 += nabe20 + nabe21 + nabe22 + nabe23;
        vColor24.a = BLACK.a;
    }
//#else //just get one pixel
//    else //just get one pixel
    {
        float hw_univ = hUNM.s;
        float hw_node = hUNM.t;

        SAMPLE_OR_FX(vColor, hw_univ, hw_node);
    }
//#endif //def WS281X_FMT
   gl_PointSize = PTSIZE; //max(NODEBIT_WIDTH, NODE_HEIGHT); //PTSIZE;
}


#ifndef CUSTOM_GPUFX
//dummy effects generator:
//placeholder for caller-supplied gpufx
vec4 gpufx(vec3 selectfx)
{
    return MAGENTA;
}
#endif //CUSTOM_GPUFX


#if 0
//example effects generator:
function one_by_one(selector)
{
//hw (univ#, node#, model#):
//these are ints, but used in floating arithmetic so just leave them as floats
    float hw_univ = hUNM.s;
    float hw_node = hUNM.t;
    float hw_model = hUNM.p;
//model (x, y, w, h):
    int model_x = int(mXYWH.x);
    int model_y = int(mXYWH.y);
    int model_w = int(mXYWH.w);
    int model_h = int(mXYWH.z);

    vec4 color = vec4(hsv2rgb(vec3(hw_model / MAX_UNIV, 1.0, 1.0)), 1.0); //choose different color for each column
#define round(thing)  thing  //RPi
//       float node = round(elapsed / duration * NUMW * NUMH); //not floor
    float node_inx = floor(elapsed / duration * (NUM_UNIV * UNIV_LEN - 1.0)); //TODO: round?
    float nodex = floor(node_inx / UNIV_LEN), nodey = mod(node_inx, UNIV_LEN);
    if (GT(hw_univ, nodex) || (EQ(hw_univ, nodex) && GT(hw_node, nodey))) color = BLACK;
//color = vec4(elapsed / duration, elapsed / duration, 1.0, 1.0); //CYAN;
    return color;
#endif //0


#if 0
//no switch stmt in GLSL 1.2, so use IIF to avoid branching:
//    int color = int(mod(hw_model, 7.0));
//    vColor = BLACK;
//    vColor = IIF(color == 0, RED, vColor);
//    vColor = IIF(color == 1, GREEN, vColor);
//    vColor = IIF(color == 2, BLUE, vColor);
//    vColor = IIF(color == 3, YELLOW, vColor);
//    vColor = IIF(color == 4, CYAN, vColor);
//    vColor = IIF(color == 5, MAGENTA, vColor);
//    vColor = IIF(color == 6, WHITE, vColor);
//    if (LT(elapsed, 0.0) || GE(elapsed, duration)) //get color from texture
//    {
//        vColor = texture2D(uSampler, vec2((hw_univ + 0.1) / NUMW, hw_node / NUMH));
//first check if CPU wants to set color:
    vColor = texture2D(uSampler, vec2(hw_univ / MAX_UNIV, hw_node / UNIV_MAX)); //[0..1]
//        txcoord = vec2(hw_univ / (NUM_UNIV - 1.0), hw_node / (UNIV_LEN - 1.0)); //[0..1]
//no        vColor = texture2D(uSampler, vec2(hw_univ, hw_node)); //[0..size]
//        vColor = texture2D(uSampler, vec2(0.0, 0.0)); //[0..1]
//        vColor.a = 1.0;
//        vColor.r = 1.0;
//        vColor = total; // / 24.0;
//    }
//if not, then apply GPU fx:
    vColor = IIF(vColor.a == 0.0), gpufx(vColor.rgb), vColor);
//    else //use effect logic to generate color
//    {
        vColor = vec4(hsv2rgb(vec3(hw_model / NUM_UNIV, 1.0, 1.0)), 1.0); //choose different color for each model
#define round(thing)  thing  //RPi
//       float node = round(elapsed / duration * NUMW * NUMH); //not floor
       float node_inx = round(elapsed / duration * NUM_UNIV * UNIV_LEN); //TODO: round?
       float nodex = floor(node_inx / UNIV_LEN), nodey = mod(node_inx, UNIV_LEN);
       if (GT(hw_univ, nodex) || (EQ(hw_univ, nodex) && GT(hw_node, nodey))) vColor = BLACK;
//    }
//    if (hUNM.y > nodey) vColor = BLACK;
//    vColor = vec4(vxyz, 1.0);
//    vColor = vec4(0.0, 0.5, 0.5, 1.0);
//    vColor = vec4(vxyz, 1.0);
//    vTextureCoord = aTextureCoord;
//    vecpos = aVertexPosition;
//    vecpos = vec3(vxyz);
//    if (outmode == 0.0) gl_PointSize = min(NODEBIT_WIDTH, NODE_HEIGHT);
//    else gl_PointSize = NODEBIT_WIDTH; //TODO: screen_width / 23.25
#endif //0

//eof
