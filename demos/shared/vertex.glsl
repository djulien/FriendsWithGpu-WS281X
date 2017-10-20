//vertex shader:
#include "shared.glsl"

//#ifdef GL_ES //RPi
//precision highp float; //defaults to highp in OpenGL ES; illegal in OpenGL desktop
//#endif //def GL_ES

//effect generation (color selection) should be done in here, *not* in fragment shader
//NOTE: runs once per vertex, which is ~ 500 - 1000x less than fragment shader

//from caller, unchanged across vertices:
uniform mat4 uModelView;
uniform mat4 uProjection;
uniform float elapsed, duration; //progress bar; can also be used to drive fx animation
uniform sampler2D uSampler; //texture (individual pixel colors) from caller
//uniform bool WS281X_FMT; //allow turn on/off at run-time for demo/debug purposes

//from caller, changes from one vertex to another:
attribute vec3 vXYZ; //3D view (x, y, z); for 3D viewer
//varying vec2 txcoord;
attribute vec3 hUNM; //hw (univ#, node#, model#); use this for "whole house" effects
attribute vec4 mXYWH; //model (x, y, w, h); use this for model-specific effects

//send to fragment shader:
varying vec4 vColor; //vertex color data for fragment shader; pivoted if formatted for WS281X
//varying vec4 vColor24; //pivoted color data x24 for fragment shader (final formatting)
varying float debug_val;

//color space conversion:
//color manipulation is more natural in HSV color space, but hardware wants RGB so convert back and forth
vec3 rgb2hsv(vec3 c);
vec3 hsv2rgb(vec3 c);


#ifdef CUSTOM_GPUFX
 vec4 gpufx(float x, float fromcpu); //fwd ref
// #define CUSTOM(stmt)  stmt
//CAUTION: GLSL cpp bug is expanding the "x" in "gpufx", so use different param name
// #define GPUFX(val)  gpufx(val) //dot(gpufx(val).rgb, TORGB24)
//#else
// #define CUSTOM(stmt) //noop
// #define GPUFX(val)  val //dot(gpufx(val).rgb, TORGB24)
#endif //def CUSTOM_GPUFX


//look up pixel color from caller:
//caller uses a texture to pass pixel colors to shader
#define SAMPLE(x, y)  texture2D(uSampler, vec2(float(x) / (NUM_UNIV - 1.0), float(y) / (UNIV_LEN - 1.0))) // * FLOAT2BITS

#if 0
//#define BIT(n)  pow(2.0, float(n))
//#define AND(val, mask)  greaterThanEqual(mod((val) * FLOAT2BITS, 2.0 * mask), mask)
//const float FLOAT2BITS = 255.0 / 256.0; //convert normalized [0..1] to sum of bits (negative powers of 2)
const vec4 COLs0_3 = vec4(BIT(-1), BIT(-2), BIT(-3), BIT(-4));
const vec4 COLs4_7 = vec4(BIT(-5), BIT(-6), BIT(-7), BIT(-8));

//kludge: RPi does not handle non-const indexes :(
//#ifdef GL_ES //RPi
#define INDEX3(thing, inx)  (IF(inx == 0.0, thing[0]) + IF(inx == 1.0, thing[1]) + IF(inx == 2.0, thing[2]) + IF(inx == 3.0, thing[3]))
//#define INDEX3(thing, inx)  (IF(EQ(inx, 0.0), thing[0]) + IF(EQ(inx, 1.0), thing[1]) + IF(EQ(inx, 2.0), thing[2]) + IF(EQ(inx, 3.0), thing[3]))
//#else //desktop/laptop
// #define INDEX3(thing, inx)  thing[inx]
//#endif //def GL_ES
#endif


void main(void)
{
    gl_Position = uProjection * uModelView * vec4(vXYZ, 1.0); //[-1..1]
    gl_PointSize = VERTEX_SIZE; //render box size for each vertex; / Position.w; always 1.0

//NOTE: gl_Position.xy is not equivalent due to y axis orientation
//NOTE: use hUNM for whole-house, or mXYWH for model-specific fx coordinates
    float univ = hUNM.s; //[0..NUM_UNIV); universe#
    float node = hUNM.t; //UNIV_LEN - hUNM.t - 1.0; //[0..UNIV_LEN); node# within universe
//ortho projection maps as-is to screen coordinates (except for origin correction):
//rewrite as MAD with swizzle:
//        float s = floor((gl_Position.x + 1.0) * NUM_UNIV / 2.0); //[0..24)
//        float t = floor((gl_Position.y + 1.0) * UNIV_LEN / 2.0); //[0..1152)
//        vec2 st = floor(gl_Position.xy * UNMAPXY + UNMAPXY);

#if 0
//get all pixels on this row:
//NOTE: RPi reports memory errors (GL 0x505) with > 8 Texture lookups :(
//In order to access 24 values, caller rearranges ARGB bytes so one lookup gets 4 adjacent pixels
//Other options were: only retrieve first 8 pixels (and set others to 0 or repeat), use color indexing, or pack more colors with less depth.
    float byte = floor((s + RPI_FIXUP) / 8.0); //[0..2]; which color byte (R, G, B) to pivot; A is at end
//R, G, or B color elements for 4 adjacent pixels:
    vec4 rgb0_3 = SAMPLE(byte + 0.0, t);
    vec4 rgb4_7 = SAMPLE(byte + 4.0, t);
    vec4 rgb8_11 = SAMPLE(byte + 8.0, t);
    vec4 rgb12_15 = SAMPLE(byte + 12.0, t);
    vec4 rgb16_19 = SAMPLE(byte + 16.0, t);
    vec4 rgb20_23 = SAMPLE(byte + 20.0, t);

//get remaining parts of this pixel and reconstruct caller's original pixel color:
//    float inx = floor(mod(x + RPI_FIXUP, 4.0)); //[0..3]
    float inx = MOD(s, 4.0); //[0..3]; avoid getting wrong result on RPi
//    if (GE(inx, 4.0)) inx = 0.0; //kludge: this should never happen, but it does on RPi; fix up incorrect results
//    inx = floor(inx + 0.1); //kludge: compensate for RPi precion here
    float which = s - inx; //float(inx); //[0, 4, 8, 12, 16, 20]
//if ((inx == 0.0) || (inx == 2.0)) inx = 2.0 - inx;
    vec4 part0 = IF(which == 0.0, rgb0_3) + IF(which == 4.0, rgb4_7) + IF(which == 8.0, rgb8_11) + IF(which == 12.0, rgb12_15) + IF(which == 16.0, rgb16_19) + IF(which == 20.0, rgb20_23);
    float wh1 = MOD(byte + 1.0, 3.0); //kludge: RPi not expanding nested macros *and* avoid getting wrong result
    float wh2 = MOD(byte + 2.0, 3.0); //kludge: RPi not expanding nested macros *and* avoid getting wrong result
    vec4 part1 = SAMPLE(which + wh1, t);
    vec4 part2 = SAMPLE(which + wh2, t);
//assumes A was 1.0 (not enough texture lookups left on RPi to check it)
//kludge: RPi not expanding nested macros
    vec4 choice0 = vec4(INDEX3(part0, inx), INDEX3(part1, inx), INDEX3(part2, inx), 1.0);
    vec4 choice1 = vec4(INDEX3(part2, inx), INDEX3(part0, inx), INDEX3(part1, inx), 1.0);
    vec4 choice2 = vec4(INDEX3(part1, inx), INDEX3(part2, inx), INDEX3(part0, inx), 1.0);
    vColor = IF(byte == 0.0, choice0) + IF(byte == 1.0, choice1) + IF(byte == 2.0, choice2);

#ifdef CUSTOM_GPUFX //generate effects on GPU directly:
//pass CPU values to GPU to allow CPU to influence effects generated by GPU:
//TODO: use gpufx().a for blending?
//    vColor = IIF(vColor.a == 0.0), gpufx(vColor.rgb), vColor);
//        if (nabe0.a == 0.0) //transparency; let GPU fx show through
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
    vColor = gpufx(s, INDEX3(vColor, inx)); //use entire color for current pixel
#endif //def CUSTOM_GPUFX

//pivot 24 RGB888 colors onto 24 parallel GPIO pins by recombining bits:
//this will send out 24 adjacent pixels in parallel (one from each universe)
    float bit = -1.0 - MOD(s, 8.0); //kludge: RPi not expanding nested macros
    vec4 xmask = vec4(BIT(bit)); //[1/2..1/256]; which bit to get for this display column
//    vec4 xmask = vec4(BIT(-1.0 - mod(x, 8.0))); //[1/2..1/256]; which bit to get for this display column
    vColor24 = BLACK;
    vColor24.r += dot(vec4(AND(rgb0_3, xmask)), COLs0_3);
    vColor24.r += dot(vec4(AND(rgb4_7, xmask)), COLs4_7);
    vColor24.g += dot(vec4(AND(rgb8_11, xmask)), COLs0_3);
    vColor24.g += dot(vec4(AND(rgb12_15, xmask)), COLs4_7);
    vColor24.b += dot(vec4(AND(rgb16_19, xmask)), COLs0_3);
    vColor24.b += dot(vec4(AND(rgb20_23, xmask)), COLs4_7);

#ifdef WS281X_DEBUG //check for errors
//    debug_val = VERTEX_HEIGHT / VERTEX_SIZE;
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
    if (s != which + inx) vColor = WHITE;
//    vColor = (s == 2.0)? WHITE: BLUE;
//debug_val = 10.0 * p1 - 9.5;
//vColor = vec4((gl_Position.x + 1.0) / 2.0, 0.0, 0.0, 1.0);
#endif //def WS281X_DEBUG
#endif
    debug_val = 0.0; //needs a value even if not used (glsl compiler complains)
    vColor = SAMPLE(univ, node); //lookups less expensive in vertex shader than fragment shader
}


///////////////////////////////////////////////////////////////////////////////
////
/// Color space conversion for custom fx generated by GPU:
//

//from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
const vec4 K_rgb = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
const float e_rgb = 1.0e-10; //avoid div by 0
vec3 rgb2hsv(vec3 rgb)
{
//    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    vec4 p = IIF(LT(rgb.g, rgb.b), vec4(rgb.bg, K_rgb.wz), vec4(rgb.gb, K_rgb.xy));
    vec4 q = IIF(LT(rgb.r, p.x), vec4(p.xyw, rgb.r), vec4(rgb.r, p.yzx));
//    vec4 p = IIF(greaterThanEqual(c.g, c.b), vec4(c.gb, K.xy), vec4(c.bg, K.wz));
//    vec4 q = IIF(greaterThanEqual(c.r, p.x), vec4(c.r, p.yzx), vec4(p.xyw, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e_rgb)), d / (q.x + e_rgb), q.x);
}


//from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
const vec4 K_hsv = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
vec3 hsv2rgb(vec3 hsv)
{
    vec3 p = abs(fract(hsv.xxx + K_hsv.xyz) * 6.0 - K_hsv.www);
    return hsv.z * mix(K_hsv.xxx, clamp(p - K_hsv.xxx, 0.0, 1.0), hsv.y); //s 0 => grayscale
}

//eof
