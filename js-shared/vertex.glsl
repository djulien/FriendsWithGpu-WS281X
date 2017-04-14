//vertex shader:
#include "shared.glsl"

//#ifdef GL_ES //RPi
//precision highp float; //defaults to highp in OpenGL ES; illegal in OpenGL desktop
//#endif //def GL_ES

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
uniform float elapsed, duration; //progress bar; can also be used to drive fx animation
//uniform float outmode;
uniform sampler2D uSampler; //texture (individual pixel colors) from caller
uniform bool WS281X_FMT; //allow turn on/off at run-time for debug purposes

varying vec4 vColor; //vertex color data for fragment shader (final formatting)
varying vec4 vColor24; //pivoted color data x24 for fragment shader (final formatting)
varying float debug_val;
//varying vec3 vecpos;
//varying float thing;

//color space conversion:
//color manipulation is more natural in HSV color space, but hardware wants RGB
vec3 rgb2hsv(vec3 c);
vec3 hsv2rgb(vec3 c);


//varying vec2 txcoord;
#ifdef CUSTOM_GPUFX
 vec4 gpufx(float x, float fromcpu); //fwd ref
// #define CUSTOM(stmt)  stmt
// #define GPUFX(val)  gpufx(val) //dot(gpufx(val).rgb, TORGB24)
//#else
// #define CUSTOM(stmt) //noop
// #define GPUFX(val)  val //dot(gpufx(val).rgb, TORGB24)
#endif //def CUSTOM_GPUFX

//CAUTION: GLSL cpp bug is expanding the "x" in "gpufx", so use different param name

#if 0
const vec3 TORGB24 = vec3(255.0 * 256.0 * 256.0, 255.0 * 256.0, 255.0);
const vec3 FROMRGB24 = vec3(256.0 * 256.0 * 256.0, 256.0 * 256.0, 256.0);

//#define rgb2float(val)  dot(val.rgb, TORGB24)
float rgb2float(vec4 val)
{
    val.r *= 2.0;
    float sign = 2.0 * floor(val.r) - 1.0;
    val.r = fract(val.r);
    return sign * dot(val.rgb, TORGB24);
}

//#define float2rgba(rgb24)  mod(floor(vec4(rgb24 / 256.0 / 256.0, rgb24 / 256.0, rgb24, 255.0)), 256.0) / 255.0
vec4 float2rgba(float val)
{
    return mod(floor(vec4(val / 256.0 / 256.0, val / 256.0, val, 255.0)), 256.0) / 255.0;
}
#endif

#define SAMPLE(x, y)  texture2D(uSampler, vec2(float(x) / (NUM_UNIV - 1.0), float(y) / (UNIV_LEN - 1.0))) // * FLOAT2BITS
//    dot(texture2D(uSampler, vec2(float(x) / (NUM_UNIV - 1.0), float(y) / (UNIV_LEN - 1.0))).rgb, TORGB24) // * FLOAT2BITS
//#define SAMPLEx(x, y)  vec4(0.0) // /*(x < 8)? SAMPLE(x, y):*/ vec4(0)
//#define SAMPLEx(x, y)  SAMPLE(mod(float(x), 8.0), y)

//#define SAMPLE_OR_FX_PIVOT(var, x, y, bit)  \
//    var = SAMPLE(x, y); \
//    CUSTOM(var = gpufx(var.rgb)); \
//    vColor24.rgb += AND3(var.rgb, xmask) * MASK3f(bit)

//#define SAMPLE_OR_FX(var, ex, y) \
//    var = texture2D(uSampler, vec2(ex / (NUM_UNIV - 1.0), y / (UNIV_LEN - 1.0))); \
//    var = IIF(var.a == GPUFX.a, gpufx(var.rgb), var)
//#define SAMPLE_OR_FX_BIT(var, x, y, bit) \
//    SAMPLE_OR_FX(var, x, y); \
//    var = IIF(IFBIT(var, bit), MASK(x), ZERO)

//#define SAMPLE_RGB777(var1, var2, var3, x, y) \
//    SAMPLE(var1, x, y); \
//    var3 = var2 = var1
//   #define mediump //16-bit float: 1 sign + 5 exp + 10 mantissa


//const float NUM_UNIV_HALF = NUM_UNIV / 2.0;
//const float UNIV_LEN_HALF = UNIV_LEN / 2.0;
//const vec2 UNMAPXY = vec2(NUM_UNIV / 2.0, UNIV_LEN / 2.0);

//#define BIT(n)  pow(2.0, float(n))
//#define AND(val, mask)  greaterThanEqual(mod((val) * FLOAT2BITS, 2.0 * mask), mask)
//const float FLOAT2BITS = 255.0 / 256.0; //convert normalized [0..1] to sum of bits (negative powers of 2)
const vec4 COLs0_3 = vec4(BIT(-1), BIT(-2), BIT(-3), BIT(-4));
const vec4 COLs4_7 = vec4(BIT(-5), BIT(-6), BIT(-7), BIT(-8));

//kludge: RPi does not handle non-const indexes
//#ifdef GL_ES //RPi
// #define INDEX0_3(thing, inx)  (IF(inx == 0, thing[0]) + IF(inx == 1, thing[1]) + IF(inx == 2, thing[2]) + IF(inx == 3, thing[3]))
 #define INDEX0_3(thing, inx)  (IF(EQ(inx, 0.0), thing[0]) + IF(EQ(inx, 1.0), thing[1]) + IF(EQ(inx, 2.0), thing[2]) + IF(EQ(inx, 3.0), thing[3]))
//#else //desktop/laptop
// #define INDEX0_3(thing, inx)  thing[inx]
//#endif //def GL_ES


void main(void)
{
    gl_Position = uProjection * (uModelView * vec4(vXYZ, 1.0)); //[-1..1]
    gl_PointSize = max(NODEBIT_WIDTH, NODE_HEIGHT); //NODE_SIZE;
//NOTE: gl_Position.xy is not equivalent due to y axis orientation
//NOTE: use hUNM for whole-house, mXYWH for model-specific fx
    float x = hUNM.s; //[0..NUM_UNIV); universe#
    float y = hUNM.t; //UNIV_LEN - hUNM.t - 1.0; //[0..UNIV_LEN); node# within universe
//#ifdef WS281X_FMT //format as WS281X requires pivot of 24 adjacent bits
//    if (WS281X_FMT) //format as WS281X requires pivot of 24 adjacent bits
//    {
//ortho projection maps as-is to screen coordinates (except for origin correction):
//rewrite as MAD with swizzle:
//        float x = floor((gl_Position.x + 1.0) * NUM_UNIV / 2.0); //[0..24)
//        float y = floor((gl_Position.y + 1.0) * UNIV_LEN / 2.0); //[0..1152)
//        vec2 xy = floor(gl_Position.xy * UNMAPXY + UNMAPXY);

//get all pixels on this row:
//NOTE: RPi reports memory errors (GL 0x505) with > 8 Texture lookups :(
//In order to access 24 values, caller rearranges ARGB bytes so one lookup gets 4 pixels
//Other options were: only retrieve first 8 pixels (and set others to 0 or repeat), use color indexing, or pack more colors with less depth.
    float byte = floor((x + RPI_FIXUP) / 8.0); //[0..2]; which color byte (R, G, B) to pivot; A is at end
    float bit = -1.0 - MOD(x, 8.0); //kludge: RPi not expanding nested macros
    vec4 xmask = vec4(BIT(bit)); //[1/2..1/256]; which bit to get for this display column
//    vec4 xmask = vec4(BIT(-1.0 - mod(x, 8.0))); //[1/2..1/256]; which bit to get for this display column
//    float inx = floor(mod(x + RPI_FIXUP, 4.0)); //[0..3]

    vec4 rgb0_3 = SAMPLE(byte + 0.0, y); //vec4 nabe0 = SAMPLE(0, y);
    vec4 rgb4_7 = SAMPLE(byte + 4.0, y); //vec4 nabe0 = SAMPLE(1, y);
    vec4 rgb8_11 = SAMPLE(byte + 8.0, y); //vec4 nabe0 = SAMPLE(2, y);
    vec4 rgb12_15 = SAMPLE(byte + 12.0, y); //vec4 nabe0 = SAMPLE(3, y);
    vec4 rgb16_19 = SAMPLE(byte + 16.0, y); //vec4 nabe0 = SAMPLE(4, y);
    vec4 rgb20_23 = SAMPLE(byte + 20.0, y); //vec4 nabe0 = SAMPLE(5, y);
#ifdef CUSTOM_GPUFX //generate effects on GPU directly:
//pass CPU values to GPU allow CPU to influence effects generated by GPU:
//    vColor = IIF(vColor.a == 0.0), gpufx(vColor.rgb), vColor);
//        if (nabe0.a == 0.0) //transparency; let GPU fx show through
//TODO: use gpufx().a for blending?
    rgb0_3[0] = gpufx(0.0, rgb0_3[0])[0];
    rgb0_3[1] = gpufx(1.0, rgb0_3[1])[1];
    rgb0_3[2] = gpufx(2.0, rgb0_3[2])[2];
    rgb0_3[3] = gpufx(3.0, rgb0_3[3])[3];
    rgb4_7[0] = gpufx(4.0, rgb4_7[0])[0];
    rgb4_7[1] = gpufx(5.0, rgb4_7[1])[1];
    rgb4_7[2] = gpufx(6.0, rgb4_7[2])[2];
    rgb4_7[3] = gpufx(7.0, rgb4_7[3])[3];
    rgb8_11[0] = gpufx(8.0, rgb8_11[0])[0];
    rgb8_11[1] = gpufx(9.0, rgb8_11[1])[1];
    rgb8_11[2] = gpufx(10.0, rgb8_11[2])[2];
    rgb8_11[3] = gpufx(11.0, rgb8_11[3])[3];
    rgb12_15[0] = gpufx(12.0, rgb12_15[0])[0];
    rgb12_15[1] = gpufx(13.0, rgb12_15[1])[1];
    rgb12_15[2] = gpufx(14.0, rgb12_15[2])[2];
    rgb12_15[3] = gpufx(15.0, rgb12_15[3])[3];
    rgb16_19[0] = gpufx(16.0, rgb16_19[0])[0];
    rgb16_19[1] = gpufx(17.0, rgb16_19[1])[1];
    rgb16_19[2] = gpufx(18.0, rgb16_19[2])[2];
    rgb16_19[3] = gpufx(19.0, rgb16_19[3])[3];
    rgb20_23[0] = gpufx(20.0, rgb20_23[0])[0];
    rgb20_23[1] = gpufx(21.0, rgb20_23[1])[1];
    rgb20_23[2] = gpufx(22.0, rgb20_23[2])[2];
    rgb20_23[3] = gpufx(23.0, rgb20_23[3])[3];
#endif //def CUSTOM_GPUFX
//        vec3 xmask = MASK3f(x);
//TODO: swizzle + vec3
#if 0
        vColor24.r += AND(rgb0, xmask) * MASK(-1); //MASK(0); //vColor24.rgb += AND3(rgb0, xmask) * MASK0;
        vColor24.r += AND(rgb1, xmask) * MASK(-2); //vColor24.rgb += AND3(rgb1, xmask) * MASK1;
        vColor24.r += AND(rgb2, xmask) * MASK(-3); //vColor24.rgb += AND3(rgb2, xmask) * MASK2;
        vColor24.r += AND(rgb3, xmask) * MASK(-4); //vColor24.rgb += AND3(rgb3, xmask) * MASK3;
        vColor24.r += AND(rgb4, xmask) * MASK(-5); //vColor24.rgb += AND3(rgb4, xmask) * MASK4;
        vColor24.r += AND(rgb5, xmask) * MASK(-6); //vColor24.rgb += AND3(rgb5, xmask) * MASK5;
        vColor24.r += AND(rgb6, xmask) * MASK(-7); //vColor24.rgb += AND3(rgb6, xmask) * MASK6;
        vColor24.r += AND(rgb7, xmask) * MASK(-8); //vColor24.rgb += AND3(rgb7, xmask) * MASK7;
        vColor24.g += AND(rgb8, xmask) * MASK(-1); //vColor24.rgb += AND3(rgb8, xmask) * MASK8;
        vColor24.g += AND(rgb9, xmask) * MASK(-2); //vColor24.rgb += AND3(rgb9, xmask) * MASK9;
        vColor24.g += AND(rgb10, xmask) * MASK(-3); //vColor24.rgb += AND3(rgb10, xmask) * MASK10;
        vColor24.g += AND(rgb11, xmask) * MASK(-4); //vColor24.rgb += AND3(rgb11, xmask) * MASK11;
        vColor24.g += AND(rgb12, xmask) * MASK(-5); //vColor24.rgb += AND3(rgb12, xmask) * MASK12;
        vColor24.g += AND(rgb13, xmask) * MASK(-6); //vColor24.rgb += AND3(rgb13, xmask) * MASK13;
        vColor24.g += AND(rgb14, xmask) * MASK(-7); //vColor24.rgb += AND3(rgb14, xmask) * MASK14;
        vColor24.g += AND(rgb15, xmask) * MASK(-8); //vColor24.rgb += AND3(rgb15, xmask) * MASK15;
        vColor24.b += AND(rgb16, xmask) * MASK(-1); //vColor24.rgb += AND3(rgb16, xmask) * MASK16;
        vColor24.b += AND(rgb17, xmask) * MASK(-2); //vColor24.rgb += AND3(rgb17, xmask) * MASK17;
        vColor24.b += AND(rgb18, xmask) * MASK(-3); //vColor24.rgb += AND3(rgb18, xmask) * MASK18;
        vColor24.b += AND(rgb19, xmask) * MASK(-4); //vColor24.rgb += AND3(rgb19, xmask) * MASK19;
        vColor24.b += AND(rgb20, xmask) * MASK(-5); //vColor24.rgb += AND3(rgb20, xmask) * MASK20;
        vColor24.b += AND(rgb21, xmask) * MASK(-6); //vColor24.rgb += AND3(rgb21, xmask) * MASK21;
        vColor24.b += AND(rgb22, xmask) * MASK(-7); //vColor24.rgb += AND3(rgb22, xmask) * MASK22;
        vColor24.b += AND(rgb23, xmask) * MASK(-8); //vColor24.rgb += AND3(rgb23, xmask) * MASK23;
#endif
//#define MASK(n)  pow(2.0, float(n))
//#define AND(val, mask)  float(GE(mod(val, 2.0 * mask), mask))
//#define BTWN(val, lower, upper)  LE(abs((val) - MID(lower, upper)), MID(lower, upper) + FUD)
#if 0
        float xmask = MASK(23.0 - x); //needs 24 bit precision
        vColor24.rgb += vec3(AND(rgb0, xmask), AND(rgb8, xmask), AND(rgb16, xmask)) * MASK(-1);
        vColor24.rgb += vec3(AND(rgb1, xmask), AND(rgb9, xmask), AND(rgb17, xmask)) * MASK(-2);
        vColor24.rgb += vec3(AND(rgb2, xmask), AND(rgb10, xmask), AND(rgb18, xmask)) * MASK(-3);
        vColor24.rgb += vec3(AND(rgb3, xmask), AND(rgb11, xmask), AND(rgb19, xmask)) * MASK(-4);
        vColor24.rgb += vec3(AND(rgb4, xmask), AND(rgb12, xmask), AND(rgb20, xmask)) * MASK(-5);
        vColor24.rgb += vec3(AND(rgb5, xmask), AND(rgb13, xmask), AND(rgb21, xmask)) * MASK(-6);
        vColor24.rgb += vec3(AND(rgb6, xmask), AND(rgb14, xmask), AND(rgb22, xmask)) * MASK(-7);
        vColor24.rgb += vec3(AND(rgb7, xmask), AND(rgb15, xmask), AND(rgb23, xmask)) * MASK(-8);
#endif
    if (WS281X_FMT) //format as WS281X requires pivot of 24 adjacent bits
    {
//pivot 24 RGB888 colors onto 24 parallel GPIO pins by recombining bits:
        vColor24 = BLACK;
        vColor24.r += dot(vec4(AND(rgb0_3, xmask)), COLs0_3);
        vColor24.r += dot(vec4(AND(rgb4_7, xmask)), COLs4_7);
        vColor24.g += dot(vec4(AND(rgb8_11, xmask)), COLs0_3);
        vColor24.g += dot(vec4(AND(rgb12_15, xmask)), COLs4_7);
        vColor24.b += dot(vec4(AND(rgb16_19, xmask)), COLs0_3);
        vColor24.b += dot(vec4(AND(rgb20_23, xmask)), COLs4_7);
#ifndef WS281X_DEBUG
        return; //don't need original pixel color from caller
#endif //ndef WS281X_DEBUG
    }
//#else //just get one pixel
//    else //just get one pixel
//    {
//#ifdef WS281X_DEBUG //show original color and debug info
//map to WS281X node:
//    float hw_univ = hUNM.s; //[0..NUM_UNIV)
//    float hw_node = hUNM.t; //[0..UNIV_LEN)

//    vColor = BLACK;
//        SAMPLE_OR_FX(vColor, hw_univ, hw_node);
//    float rgb24 = GPUFX(SAMPLE(x, y));
//#ifdef CUSTOM_GPUFX //get colors from GPU:
//    rgb24 = GPUFX(rgb24);
//#endif //def CUSTOM_GPUFX
//    vColor = float2rgba(rgb24);
//use remaining 2 texture lookups to retrieve other parts of caller's original pixel color:
//    int inx = int(mod(x, 4.0)); //[0..3]
    float inx = MOD(x, 4.0); //[0..3]; avoid getting wrong result on RPi
//    if (GE(inx, 4.0)) inx = 0.0; //kludge: this should never happen, but it does on RPi; fix up incorrect results
//    inx = floor(inx + 0.1); //kludge: compensate for RPi precion here
    float which = x - inx; //float(inx); //[0, 4, 8, 12, 16, 20]
//if ((inx == 0.0) || (inx == 2.0)) inx = 2.0 - inx;
    vec4 part0 = IF(which == 0.0, rgb0_3) + IF(which == 4.0, rgb4_7) + IF(which == 8.0, rgb8_11) + IF(which == 12.0, rgb12_15) + IF(which == 16.0, rgb16_19) + IF(which == 20.0, rgb20_23);
#ifdef CUSTOM_GPUFX //generate effects on GPU directly:
//pass CPU values to GPU allow CPU to influence effects generated by GPU:
//    vColor = IIF(vColor.a == 0.0), gpufx(vColor.rgb), vColor);
//        if (nabe0.a == 0.0) //transparency; let GPU fx show through
//TODO: use gpufx().a for blending?
    vColor = gpufx(x, INDEX0_3(part0, inx));
#else
//get remaining parts and reconstruct caller's original pixel color:
    float wh1 = MOD(byte + 1.0, 3.0); //kludge: RPi not expanding nested macros *and* avoid getting wrong result
    float wh2 = MOD(byte + 2.0, 3.0); //kludge: RPi not expanding nested macros *and* avoid getting wrong result
    vec4 part1 = SAMPLE(which + wh1, y); //MOD(byte + 1.0, 3.0), y);
    vec4 part2 = SAMPLE(which + wh2, y); //MOD(byte + 2.0, 3.0), y);
//assumes A was 1.0 (not enough texture lookups left on RPi to get that)
//kludge: RPi not expanding nested macros
    vec4 choice0 = vec4(INDEX0_3(part0, inx), INDEX0_3(part1, inx), INDEX0_3(part2, inx), 1.0);
    vec4 choice1 = vec4(INDEX0_3(part2, inx), INDEX0_3(part0, inx), INDEX0_3(part1, inx), 1.0);
    vec4 choice2 = vec4(INDEX0_3(part1, inx), INDEX0_3(part2, inx), INDEX0_3(part0, inx), 1.0);
    vColor = IF(byte == 0.0, choice0) + IF(byte == 1.0, choice1) + IF(byte == 2.0, choice2);
#endif //def CUSTOM_GPUFX
#ifdef WS281X_DEBUG //check for errors
    debug_val = 0.0;
//vColor = BLACK; //only show errors
    if ((which != 0.0) && (which != 4.0) && (which != 8.0) && (which != 12.0) && (which != 16.0) && (which != 20.0)) vColor = RED; //ERROR(1);
    if ((inx != 0.0) && (inx != 1.0) && (inx != 2.0) && (inx != 3.0)) vColor = GREEN; //ERROR(1);
    if ((byte != 0.0) && (byte != 1.0) && (byte != 2.0)) vColor = BLUE; //ERROR(1);
//vColor = BLACK;
//vColor = IF(inx == 0.0, RED) + IF(inx == 1.0, GREEN) + IF(inx == 2.0, BLUE) + IF(inx == 3.0, YELLOW);
//vColor = IF(byte == 0.0, RED) + IF(byte == 1.0, GREEN) + IF(byte == 2.0, BLUE) + IF(byte == 3.0, YELLOW);
//vColor = IF(which == 0.0, RED) + IF(which == 4.0, GREEN) + IF(which == 8.0, BLUE) + IF(which == 12.0, YELLOW) + IF(which == 16.0, CYAN) + IF(which == 20.0, MAGENTA);
//vColor = (EQ(xmask.r, 0.5) && EQ(xmask.g, 0.5) && EQ(xmask.b, 0.5))? GREEN: RED;
//float v = 0.5; //pow(2.0, -1.0);
//vColor = vec4(v, 0.0, 0.0, 1.0); //EQ(pow(2.0, -2.0), 0.25)? GREEN: RED;
//debug_val = 10.0 * BIT(-9.0);
//if (EQ(inx, 4.0)) vColor = WHITE;
//debug_val = BIT(-1.0 - mod(1.01, 8.0));
    float p1 = MOD(byte + 1.0, 3.0);
    float p2 = MOD(byte + 2.0, 3.0);
    if ((p1 != 0.0) && (p1 != 1.0) && (p1 != 2.0)) vColor.r = 1.0;
    if ((p2 != 0.0) && (p2 != 1.0) && (p2 != 2.0)) vColor.g = 1.0;
//    if (p1 == 3.0) vColor.r = 1.0;
//    if (p2 == 3.0) vColor.g = 1.0;
    if (x != which + inx) vColor = WHITE;
//    vColor = (x == 2.0)? WHITE: BLUE;
//debug_val = 10.0 * p1 - 9.5;
#endif //def WS281X_DEBUG
//    vColor = GPUFX(SAMPLE(x, y));
//#endif //def WS281X_DEBUG
//    }
//#endif //def WS281X_FMT
}


/*
#ifndef CUSTOM_GPUFX
//dummy effects generator:
//placeholder for caller-supplied gpufx
vec4 gpufx(vec3 selectfx)
{
    return MAGENTA;
}
#endif //CUSTOM_GPUFX
*/


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

    vec4 color = vec4(hsv2rgb(vec3(hw_model / (NUM_UNIV - 1.0), 1.0, 1.0)), 1.0); //choose different color for each column
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
    vColor = texture2D(uSampler, vec2(hw_univ / (NUM_UNIV - 1.0), hw_node / (UNIV_LEN - 1.0))); //[0..1]
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


///////////////////////////////////////////////////////////////////////////////
////
/// Color space conversion for custom fx generated by GPU:
//

//from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
const vec4 K_rgb = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
const float e_rgb = 1.0e-10; //avoid div by 0
vec3 rgb2hsv(vec3 rgb)
{
//    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
//    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
//    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
//    vec4 K = vec4(0.0, -1.0, 2.0, -3.0) / 3.0; //TODO: make const?
    vec4 p = IIF(LT(rgb.g, rgb.b), vec4(rgb.bg, K_rgb.wz), vec4(rgb.gb, K_rgb.xy));
    vec4 q = IIF(LT(rgb.r, p.x), vec4(p.xyw, rgb.r), vec4(rgb.r, p.yzx));
//    vec4 p = IIF(greaterThanEqual(c.g, c.b), vec4(c.gb, K.xy), vec4(c.bg, K.wz));
//    vec4 q = IIF(greaterThanEqual(c.r, p.x), vec4(c.r, p.yzx), vec4(p.xyw, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e_rgb)), d / (q.x + e_rgb), q.x);
}


//from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
const vec4 K_hsv = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0); //kludge: prevent wrap (drops blue)
vec3 hsv2rgb(vec3 hsv)
{
//    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
//    vec4 K = vec4(3.0, 2.0, 1.0, 9.0) / 3.0; //TODO: make const?
    vec3 p = abs(fract(hsv.xxx + K_hsv.xyz) * 6.0 - K_hsv.www);
    return hsv.z * mix(K_hsv.xxx, clamp(p - K_hsv.xxx, 0.0, 1.0), hsv.y); //s 0 => grayscale
}

//eof
