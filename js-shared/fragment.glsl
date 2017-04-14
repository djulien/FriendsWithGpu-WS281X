//fragment shader:
#include "shared.glsl"

//#ifdef GL_ES //RPi
//http://stackoverflow.com/questions/28540290/why-it-is-necessary-to-set-precision-for-the-fragment-shader
//precision mediump float; //defaults to mediump in OpenGL ES; illegal in OpenGL desktop
//#endif //def GL_ES

//format pixels for screen or WS281X
//runs once per screen pixel (~ 500 - 1000x more than vertex shader)
//should only do final formatting in here, not fx gen

//uniform sampler2D uSampler;
uniform float elapsed, duration; //progress bar
//uniform float outmode;
//varying vec3 vecpos;
varying vec4 vColor; //vertex color data from vertex shader
varying vec4 vColor24; //pivoted vertex color data from vertex shader
varying float debug_val;
//varying float thing;
//varying vec2 vTextureCoord;

//varying vec2 txcoord;
//uniform sampler2D uSampler;
uniform bool WS281X_FMT; //allow turn on/off at run-time for debug purposes

//WS281X bit timing:

//WS281X low, high bit times:
//bit time is 1.25 usec and divided into 3 parts:
//          ______ ______               _____
//   prev  / lead \ data \    trail    / next
//   bit _/        \______\___________/  bit
//all bits start with low-to-high transition
//"0" bits fall after 1/4 of bit time; "1" bits fall after 5/8 of bit time
//all bits end in a low state

//below timing is compliant with *both* W2811 and WS2812
//this allows them to be interchangeable wrt software
//however, certain types of LED strips have R <-> G swap

//TODO: configurable by caller or at least config.txt
const float BIT_0H = 16.0/64.0; //middle of shared timeslot range; %
//#define BIT_0L  48.0/64.0
const float BIT_1H = 36.0/64.0; //middle of shared timeslot range; %
//#define BIT_1L  28.0/64.0
const float BIT_TIME = 1.28; //64 * 50 MHz pixel clock (rounded up); usec

//only middle portion of bit time actually depends on bit value:
//this defines the real data window
const float BIT_DATA_BEGIN = min(BIT_0H, BIT_1H); // * BIT_TIME; //before this time is high; %
const float BIT_DATA_END = max(BIT_0H, BIT_1H); // * BIT_TIME; //after this time is low; %


#ifdef WS281X_DEBUG
//#define ERROR(n)  vec4(1.0, 0.2 * float(n), 1.0, 1.0) //bug exists if this shows up
const float BORDERW = 0.05;
const vec2 LEGEND = vec2(0.9, 0.1);
vec4 legend(float x, float y);
#endif //def WS281X_DEBUG


//#define RGB(val)  _RGB(val, floor(nodebit / 8.0))
//vec4 _RGB(float val, float which)
//{
//    if (which == 3.0) return vec4(0.0, 1.0, 1.0, 1.0);
//    if ((which != 0.0) && (which != 1.0) && (which != 2.0)) return ERROR(4);
//    return vec4((which == 0.0)? val: 0.0, (which == 1.0)? val: 0.0, (which == 2.0)? val: 0.0, 1.0); //show R/G/B for debug
//}
//#define SELECT1of3(which)  vec3(equal(vec3(float(which)), vec3(0.0, 1.0, 2.0))) //, VEC3(1), VEC3(1e30))
//#define RGB(val)  vec4(val * SELECT1of3(floor(nodebit / 8.0)), 1.0)

#define RGB(val)  vec4(vec3(equal(vec3(floor((nodebit + RPI_FIXUP) / 8.0)), vec3(0.0, 1.0, 2.0))) * val, 1.0)


void main(void)
{
//gl_FragCoord is window space [0..size],[0..size]
    float x = gl_FragCoord.x / SCR_WIDTH; //[0..1]
    float y = gl_FragCoord.y / SCR_HEIGHT; //[0..1]; NOTE: 0 at bottom of screen

#ifdef PROGRESS_BAR
    if ((y < 0.005) && (x <= elapsed / duration))
    {
        gl_FragColor = WHITE; //progress bar (debug/test only)
        return;
    }
#endif //def PROGRESS_BAR

#ifdef WS281X_DEBUG //debug a float value
    if (debug_val != 0.0)
    {
        if ((y >= 0.20) && (y < 0.22)) { gl_FragColor = LT(x, debug_val)? GREEN: RED; return; }
        if ((y >= 0.22) && (y < 0.24)) { gl_FragColor = (abs(x - floor(x * 10.0 + FUD) / 10.0) < 0.002)? CYAN: (abs(x - floor(x * 100.0 + FUD) / 100.0) < 0.002)? MAGENTA: BLACK; return; }
    }
#endif

//#ifndef WS281X_FMT //format as light bulbs on screen
    if (!WS281X_FMT) //format as light bulbs on screen
    {
        const float EDGE = min(NODEBIT_WIDTH, NODE_HEIGHT) / max(NODEBIT_WIDTH, NODE_HEIGHT) / 2.0;
        vec2 coord = gl_PointCoord - 0.5; //vec2(0.5, 0.5); //from [0,1] to [-0.5,0.5]
	    float dist = sqrt(dot(coord, coord));
//        if (dist > 0.5 * FUD) gl_FragColor = MAGENTA;", //discard;
        if (dist > EDGE) discard;
        gl_FragColor = vColor; //start with color from vertex
        gl_FragColor *= 1.0 - dist / (1.2 * EDGE); //diffuse to look more like light bulb
        return;
    }
//#endif //def WS281X_FMT

//#ifdef WS281X_FMT //format as WS281X
//format as WS281X:
    float bitangle = fract(x * NUM_UNIV); //position within current bit timeslot (horizontal)
//gl_FragColor = ERROR(1);
#ifdef WS281X_DEBUG //show timing debug info
    float nodebit = floor(x * NUM_UNIV); //bit# within node; last bit is 75% off-screen (during h sync period); use floor to control rounding (RPi was rounding UP sometimes)
//    float nodemask = pow(0.5, mod(nodebit, 8.0) + 1.0); //which WS281X bit to process this time
    float bit = -1.0 - MOD(nodebit, 8.0); //kludge: RPi not expanding nested macros
    float nodemask = BIT(bit); //[1/2..1/256]; which WS281X bit to process this time
//    float nodemask = BIT(-1.0 - mod(x, 8.0)); //[1/2..1/256]; which WS281X bit to process this time
    float bity = fract(y * UNIV_LEN); //position with current node timeslot (vertical)

    if ((nodebit < 0.0) || (nodebit > 23.0)) gl_FragColor = ERROR(1); //logic error; shouldn't happen
    else if ((bitangle < 0.0) || (bitangle > 1.0)) gl_FragColor = ERROR(2); //logic error; shouldn't happen
    else if (vColor.a != 1.0) gl_FragColor = ERROR(3); //vertex shader should set full alpha
//    else if (!chkcolor(outcolor)) outcolor = ERROR(4); //data integrity check during debug
    else if ((x >= LEGEND.x) && (y <= LEGEND.y)) gl_FragColor = legend(x, y); //show legend
//"else if ((rawimg.y >= 0.45) && (rawimg.y < 0.5)) outcolor = RGB((test != 0)? 1.0: 0.0);\n"
    else if ((y >= 0.49) && (y < 0.50) && ((nodebit == 1.0) || (nodebit == 8.0) || (nodebit == 22.0))) gl_FragColor = RGB(0.75);
    else if ((y >= 0.46) && (y < 0.50)) gl_FragColor = gray(nodebit / NUM_UNIV);
    else if ((y >= 0.42) && (y < 0.46)) gl_FragColor = gray(1.0 - nodebit / NUM_UNIV);
    else if ((y >= 0.40) && (y < 0.42)) gl_FragColor = RGB(1.0);
    else if ((y >= 0.35) && (y < 0.40)) gl_FragColor = RGB(bitangle);
    else if ((y >= 0.30) && (y < 0.35)) gl_FragColor = RGB(nodemask);
//    else if ((bitangle >= BIT_DATA_BEGIN) && (bitangle >= 0.65)) gl_FragColor = ((bitangle >= 0.7) && (bitangle <= 0.95))? vColor: BLACK; //original rendering; (S, T) swizzle
    else if ((bitangle >= 0.75) && (bitangle <= 0.95) && (bity < 0.92)) gl_FragColor = vColor; //original pixel color; leave h + v gaps for easier debug
#else
    if (false); //dummy if to satisfy "else" below
#endif //def WS281X_DEBUG
//generate timing signals (vertical areas):
//leader (on), encoded data, trailer (off), original color (debug only)
    else if (bitangle < BIT_DATA_BEGIN) gl_FragColor = WHITE; //WS281X bits start high
    else if (bitangle >= BIT_DATA_END) gl_FragColor = BLACK; //WS281X bits end low
    else gl_FragColor = vColor24; //pivoted pixel data
//    "   else if (!want_ws281x) outcolor.rgb = texture2D(s_texture, v_texCoord).rgb;\n" //normal rendering overlayed with start/stop bars (for debug)
//    gl_FragColor = RGB(bitangle);
}


#ifdef WS281X_DEBUG //show timing debug info
//error code legend:
//  1 | 3
// ---+---
//  2 | 4
vec4 legend(float x, float y)
{
    vec2 ofs = vec2(x - LEGEND.x, y) / vec2(1.0 - LEGEND.x, LEGEND.y); //relative position
    if ((ofs.x < BORDERW) || (ofs.x > 1.0 - BORDERW) || (ofs.y < BORDERW) || (ofs.y > 1.0 - BORDERW)) return GREEN; //vec4(0.0, 1.0, 0.0, 1.0);
    if ((ofs.x < 2.0 * BORDERW) || (ofs.x > 1.0 - 2.0 * BORDERW) || (ofs.y < 2.0 * BORDERW) || (ofs.y > 1.0 - 2.0 * BORDERW)) return BLACK; //vec4(0.0, 0.0, 0.0, 1.0);
    return ERROR((ofs.x < 0.5)? ((ofs.y < 0.5)? 1: 2): ((ofs.y < 0.5)? 3: 4));
}
#endif //def WS281X_DEBUG


//    gl_FragColor = texture2D(uSampler, vec2(vTextureCoord.s, vTextureCoord.t));
//    gl_FragColor = vec4(0.0, 0.5, 1.0, 1.0);
//gl_FragColor = texture2D(uSampler, txcoord);
//gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
//    else if (LT(y, 0.13) && GT(y, 0.1) && GE(x, 0.5) && LE(x, 0.503)) gl_FragColor = WHITE;
//    else
//    if (EQ(outmode, 0.0)) // == 0.0) //show raw pixels like light bulbs

//eof
