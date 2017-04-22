//fragment shader:
#include "shared.glsl"

//#ifdef GL_ES //RPi
//http://stackoverflow.com/questions/28540290/why-it-is-necessary-to-set-precision-for-the-fragment-shader
//precision mediump float; //defaults to mediump in OpenGL ES; illegal in OpenGL desktop
//#endif //def GL_ES

//format pixels for screen or WS281X
//runs once per screen pixel (~ 500 - 1000x more than vertex shader)
//NOTE: should only do final formatting in here, not fx gen

//from caller, unchanged across vertices:
//uniform sampler2D uSampler;
uniform float elapsed, duration; //progress bar
uniform bool WS281X_FMT; //allow turn on/off at run-time for demo/debug purposes

//from vertex shader:
varying vec4 vColor; //vertex color data from vertex shader
varying vec4 vColor24; //pivoted vertex color data from vertex shader (24 adjacent pixels)
varying float debug_val;

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
//however, certain types of LED strips have R <-> G swapped

//TODO: make configurable by caller or at least config.txt
//#define BIT_0L  48.0/64.0
//#define BIT_1L  28.0/64.0
//const float BIT_TIME = 1.28; //64 * 50 MHz pixel clock (rounded up); usec
const float BIT_0H = 16.0/64.0; //middle of shared timeslot range; [0..1]
const float BIT_1H = 36.0/64.0; //middle of shared timeslot range; [0..1]

//only middle portion of bit time actually depends on bit value:
//this defines the "real" data window
const float BIT_DATA_BEGIN = min(BIT_0H, BIT_1H); //before this time is high; [0..1]
const float BIT_DATA_END = max(BIT_0H, BIT_1H) - 0.03; //after this time is low; [0..1]


#ifdef WS281X_DEBUG
//#define ERROR(n)  vec4(1.0, 0.2 * float(n), 1.0, 1.0) //bug exists if this shows up
 #define RGB(val)  vec4(vec3(equal(vec3(floor((nodebit + RPI_FIXUP) / 8.0)), vec3(0.0, 1.0, 2.0))) * val, 1.0)
 const float BORDERW = 0.05;
 #define NO_OVERSCAN(x)  ((x) * 24.0 / 23.25)
 const vec2 LEGEND = vec2(0.9, 0.1);
 vec4 legend(float x, float y); //fwd ref
#endif //def WS281X_DEBUG


//edges for drawing vertices:
//NOTE: shape could be elliptical when vertex width != height
const float EDGE = 0.5 * 0.5; //to match distance, not using sqrt()
//const float EDGE = min(VERTEX_WIDTH, VERTEX_HEIGHT) / VERTEX_SIZE; //max(VERTEX_WIDTH, VERTEX_HEIGHT) / 2.0;
const vec2 ELLIPSE = vec2(VERTEX_WIDTH / VERTEX_SIZE, VERTEX_HEIGHT / VERTEX_SIZE);
const float NODE_BLANK = (VERTEX_SIZE - VERTEX_WIDTH) / VERTEX_SIZE; //portion of vertex height to leave empty
//const float NODE_BLANK = fract(VERTEX_HEIGHTy * UNIV_LEN); //position within current node timeslot (vertical)


void main(void)
{
//gl_FragCoord is window space [0..size),[0..size) with pixels centered at (0.5, 0.5)
    float x = gl_FragCoord.x / SCR_WIDTH; //[0..1); NOTE: overscan
    float y = gl_FragCoord.y / SCR_HEIGHT; //[0..1); NOTE: 0 at bottom of screen
//    y -= 0.5 / SCR_HEIGHT; //kludge: y coord seems to be off by 1/2 pixel

#ifdef PROGRESS_BAR //show progress at bottom of screen
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

//if (gl_PointCoord.y > 0.95) { gl_FragColor = CYAN; return; }
//if ((gl_PointCoord.y > 0.45) && (gl_PointCoord.y < 0.55)) { gl_FragColor = YELLOW; return; }
//gl_FragColor = gray(min(gl_PointCoord.x, gl_PointCoord.y)); return;
//gl_FragColor = gray(gl_PointCoord.y); return;

    if (!WS281X_FMT) //format as light bulbs on screen; no overscan
    {
//        if (vColor.a == 0.0) discard; //transparent pixel; http://stackoverflow.com/a/5985195/806988
        vec2 coord = (gl_PointCoord - 0.5) / ELLIPSE; //xlate from [0,1] to [-0.5,0.5]
	    float dist = dot(coord, coord); //sqrt()
//        if (dist > 0.5 * FUD) gl_FragColor = MAGENTA;", //discard;
        if (dist > EDGE) discard;
        gl_FragColor = vColor; //use pixel color from vertex shader
        gl_FragColor *= 1.0 - dist / (1.2 * EDGE); //diffuse to look more like light bulbs
        return;
    }

//format as WS281X:
    float bitangle = fract(x * NUM_UNIV); //position within current bit timeslot (horizontal); [0..1)
    float bity = fract(y * UNIV_LEN); //position within current node timeslot (vertical); [0..1)
//    float bity = gl_PointCoord.y;
//    if (LT(bity, NODE_BLANK)) { gl_FragColor = MAGENTA; return; } //discard; //compensate for overlapped vertices

#ifdef WS281X_DEBUG //show timing debug info
    float nodebit = floor(x * NUM_UNIV); //bit# within WS281X node; last bit is 75% off-screen (during h sync period); use floor to control rounding (RPi was rounding UP sometimes)
//    float nodemask = pow(0.5, mod(nodebit, 8.0) + 1.0); //which WS281X bit to process this time
    float bit = -1.0 - MOD(nodebit, 8.0); //kludge: RPi not expanding nested macros
    float nodemask = BIT(bit); //[1/2..1/256]; which WS281X bit to process this time
//    float nodemask = BIT(-1.0 - mod(x, 8.0)); //[1/2..1/256]; which WS281X bit to process this time

    if ((nodebit < 0.0) || (nodebit > 23.0)) gl_FragColor = ERROR(1); //logic error; shouldn't happen
    else if ((bitangle < 0.0) || (bitangle > 1.0)) gl_FragColor = ERROR(2); //logic error; shouldn't happen
    else if (vColor.a != 1.0) gl_FragColor = ERROR(3); //vertex shader should set full alpha
//    else if (!chkcolor(outcolor)) outcolor = ERROR(4); //data integrity check during debug
    else if ((NO_OVERSCAN(x) >= LEGEND.x) && (y <= LEGEND.y)) gl_FragColor = legend(NO_OVERSCAN(x), y); //show legend
//"else if ((rawimg.y >= 0.45) && (rawimg.y < 0.5)) outcolor = RGB((test != 0)? 1.0: 0.0);\n"
    else if ((y >= 0.49) && (y < 0.50) && ((nodebit == 1.0) || (nodebit == 8.0) || (nodebit == 22.0))) gl_FragColor = RGB(0.75);
    else if ((y >= 0.46) && (y < 0.50)) gl_FragColor = gray(nodebit / NUM_UNIV);
    else if ((y >= 0.42) && (y < 0.46)) gl_FragColor = gray(1.0 - nodebit / NUM_UNIV);
    else if ((y >= 0.40) && (y < 0.42)) gl_FragColor = RGB(1.0);
    else if ((y >= 0.35) && (y < 0.40)) gl_FragColor = RGB(bitangle);
    else if ((y >= 0.30) && (y < 0.35)) gl_FragColor = RGB(nodemask);
//    else if ((bitangle >= BIT_DATA_BEGIN) && (bitangle >= 0.65)) gl_FragColor = ((bitangle >= 0.7) && (bitangle <= 0.95))? vColor: BLACK; //original rendering; (S, T) swizzle
    else if ((bitangle >= 0.75) && (bitangle <= 0.95) && (bity < 0.92)) gl_FragColor = vColor; //original pixel color; leave h + v gaps for easier debug
    else //CAUTION: joins with stmt below
#endif //def WS281X_DEBUG

//generate timing signals (vertical areas):
//leader (on), encoded data, trailer (off), original color (debug only)
    gl_FragColor = IF(bitangle <= BIT_DATA_BEGIN, WHITE) + //WS281X bits start high
                   IF(bitangle <= BIT_DATA_END, vColor24) + //pivoted pixel data in middle
                   BLACK; //WS281X bits end low
}


#ifdef WS281X_DEBUG //show error code legend for debug
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

//eof
