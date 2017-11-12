//include this file before other shader code:

//NOTES:
//desktop GLSL version 1.20 is similar to GLSL ES version 1.00 according to:
//http://stackoverflow.com/questions/10383113/differences-between-glsl-and-glsl-es-2
//example desktop preamble:
//#version 120
//#define lowp
//#define mediump
//#define highp
//example ES GLSL 1.00 preamble:
//#version 100 //must be first line
//Shaders that do not include a #version directive will be treated as targeting version 1.10.
// Shaders that specify #version 100 will be treated as targeting version 1.00 of the OpenGL ES Shading Language

//https://www.khronos.org/opengl/wiki/Detecting_the_Shader_Model
//OpenGL Version 	GLSL Version
//2.0 	1.10
//2.1 	1.20    ~= GLSL ES 1.0
//3.0 	1.30
//3.1 	1.40
//3.2 	1.50

//GLSL 1.2: https://www.opengl.org/registry/doc/GLSLangSpec.Full.1.20.8.pdf
//passing data: http://stackoverflow.com/questions/7954927/glsl-passing-a-list-of-values-to-fragment-shader
//built-in vars: http://www.shaderific.com/glsl-variables/
//https://www.khronos.org/opengl/wiki/Built-in_Variable_(GLSL)
//https://www.opengl.org/discussion_boards/showthread.php/182248-understanding-gl_PointCoord-(always-0)
//http://webglfactory.blogspot.com/2011/05/how-to-convert-world-to-screen.html
//http://stackoverflow.com/questions/7317119/how-to-achieve-glorthof-in-opengl-es-2-0

//Note from above:
//"Never branch in the fs unless absolutely necessary.  Even on the most modern hardware fragments are evaluated in blocks of 4 and any branches in any of them forces the gpu to evaluate them for every fragment regardless of whether needed or not.
//To avoid branches one should always use functions like mix, step, smoothstep, etc."

//useful info:
//https://www.khronos.org/opengles/sdk/docs/reference_cards/OpenGL-ES-2_0-Reference-card.pdf
// https://www.khronos.org/opengl/wiki/OpenGL_Error
// https://www.khronos.org/opengl/wiki/GLSL_Optimizations
// http://aras-p.info/blog/2010/09/29/glsl-optimizer/
// http://blog.hvidtfeldts.net/index.php/2011/07/optimizing-glsl-code/
// http://gpuopen.com/archive/gpu-shaderanalyzer/
// https://github.com/aras-p/glsl-optimizer.git

//There is an interesting GLSL compiler/disassembler at the link below.
//It is a Windows tool, but it seems to run okay on Wine on Ubuntu as well:
//	http://gpuopen.com/archive/gpu-shaderanalyzer/
// http://developer.amd.com/tools-and-sdks/graphics-development/gpu-shaderanalyzer/


//broken   #version 130 //bit-wise ops require GLSL 1.3 to be enabled
//nice analysis of precision: http://litherum.blogspot.com/2013/04/precision-qualifiers-in-opengl-es.html
#ifdef GL_ES //RPi
   #version 100 //GLSL ES 1.0 ~= GLSL 1.2
//   #define lowp //10-bit float
//   #define mediump //16-bit float: 1 sign + 5 exp + 10 mantissa
//   #define highp //32-bit float: 1 sign + 8 exp + 23 mantissa
//   precision mediump float;
//   #define PRECISION(stmt)  stmt;
#else //desktop/laptop
   #version 120 //gl_PointCoord requires GLSL 1.2 to be enabled
//   #define PRECISION(stmt)
#endif
//PRECISION(mediump float)
//NOTE: Support for a high precision floating-point format is mandatory for vertex shaders.
//see https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glGetShaderPrecisionFormat.xml

const float PI = 3.1415926535897932384626433832795;

#define round(val)  floor(val + 0.5)

//compensate for floating point precision:
const float FUD = 1.0e-6;
//#define NOT(expr)  (1.0 - BOOL(expr))
#define BOOL(value)  float(value) //convert bool expr to 0/1
#define EQ(lhs, rhs)  (abs((lhs) - (rhs)) < FUD)
#define NE(lhs, rhs)  (abs((lhs) - (rhs)) >= FUD)
//#define EQ3f(lhs, rhs)  lessThan(abs((lhs) - (rhs)), vec3(FUD))
//#define GE(lhs, rhs)  step(rhs - FUD, lhs) //convert lhs >= rhs to 0/1
//#define GT(lhs, rhs)  step(lhs + FUD, lhs)
//#define LE(lhs, rhs)  step(lhs + FUD, rhs)
//#define LT(lhs, rhs)  step(lhs - FUD, rhs)
#define GE(lhs, rhs)  ((lhs) >= (rhs) - FUD)
#define GT(lhs, rhs)  ((lhs) > (rhs) + FUD)
#define LE(lhs, rhs)  ((lhs) <= (rhs) + FUD)
#define LT(lhs, rhs)  ((lhs) < (rhs) - FUD)
#define MID(lower, upper)  (((upper) + (lower)) / 2.0)
#define BTWN(val, lower, upper)  LE(abs((val) - ((lower) + (upper)) / 2.0), abs((upper) - (lower)) / 2.0 + FUD)

const float RPI_FIXUP = 0.1; //kludge: fix up incorrect results on RPi
#define MOD(num, den)  floor(mod((num) + RPI_FIXUP, den))

//force val to be non-0:
//doesn't seem to matter; GPU allows "infinity" result, doesn't throw exception with divide by 0
//#define is0(val)  vec3(equal(val, vec3(0.0)))
//#define NZ(val)  (val + is0(val))
//#define isnon0(val)  notEqual(val, vec3(0.0))

//avoid conditional branches by using arithmetic:
//NOTE: bit-wise ops require GLSL 1.3
//#define IIF(expr, true, false)  ((expr) & (true) | ~(expr) & (false))
#define IIF(expr, true, false)  mix(false, true, BOOL(expr))
#define IF(expr, val)  (BOOL(expr) * (val))

//compensate for lack of bit-wise ops:
//bit-wise ops require GLSL 1.3 to be enabled; not available on RPi
//NOTE: RPi (GL ES 1.2) doesn't support ivec, uvec either :(
//CAUTION: use functions if nested macros not being expanded
//#define MASK24(n)  pow(2.0, 23.0 - n) //exceeds mediump precision :(
#define BIT(n)  pow(2.0, float(n))
//#define AND(val, mask)  greaterThanEqual(mod((val) * FLOAT2BITS, 2.0 * mask), mask)
//const float FLOAT2BITS = 255.0 / 256.0; //convert normalized [0..1] to sum of bits (negative powers of 2)


//dimensions of vertices on screen:
//NOTE: values below will be filled in by GpuCanvas; example numbers are from example RPi config.txt
const float SCAN_WIDTH = float(CALLER_SCAN_WIDTH); //1536
const float SCAN_HEIGHT = float(CALLER_SCAN_HEIGHT); //1152; should be ~ 3/4 of SCR_WIDTH?
const float WND_WIDTH = float(CALLER_WND_WIDTH); //1488; portion of hscan that is actually visible; should be 23.25/24 of SCR_WIDTH
const float WND_HEIGHT = float(CALLER_WND_HEIGHT); //1104; portion of vscan that is actually visible
#define OVERSCAN(x)  ((x) * SCAN_WIDTH / WND_WIDTH) //effective horizontal value with no hblank period
//const float VERTEX_WIDTH = float(CALLER_VERTEX_WIDTH); //floor(SCR_WIDTH / NUM_UNIV); //64
//const float VERTEX_HEIGHT = float(CALLER_VERTEX_HEIGHT); //floor(SCR_HEIGHT / UNIV_LEN); //1 or 46
//const float VERTEX_SIZE = max(VERTEX_WIDTH, VERTEX_HEIGHT); //on-screen vertex size
const float NUM_UNIV = float(CALLER_TXR_WIDTH); //ceil(SCR_WIDTH / VERTEX_WIDTH); //float(??); //24
const float UNIV_LEN = float(CALLER_TXR_HEIGHT); //ceil(SCR_HEIGHT / VERTEX_HEIGHT); //float(??); //1104 for GPIO, ~ 18 - 24 for dev/screen

const float VERTEX_WIDTH = round(SCAN_WIDTH / NUM_UNIV); //64; uses hblank period as well
const float VERTEX_HEIGHT = round(WND_HEIGHT / UNIV_LEN); //1; doesn't use vblank period
const float VERTEX_SIZE = max(VERTEX_WIDTH, VERTEX_HEIGHT); //on-screen vertex bounding box size
//const float MAX_UNIV = NUM_UNIV - 1.0;
//const float UNIV_MAX = UNIV_LEN - 1.0;
//uniform float SCR_WIDTH, SCR_HEIGHT;
//uniform float WND_WIDTH, WND_HEIGHT;
//uniform float TXR_WIDTH, TXR_HEIGHT;
//const float VERTEX_WIDTH = float(CALLER_VERTEX_WIDTH); //floor(SCR_WIDTH / NUM_UNIV); //64
//const float VERTEX_HEIGHT = float(CALLER_VERTEX_HEIGHT); //floor(SCR_HEIGHT / UNIV_LEN); //1 or 46
//const float VERTEX_SIZE = max(VERTEX_WIDTH, VERTEX_HEIGHT); //on-screen vertex size


//caller config options:
//#define WS281X_FMT
//#define PROGRESS_BAR

#define ERROR(n)  vec4(1.0, 0.2 * float(n), 1.0, 1.0) //show bugs on sccreen

//primary RGBA colors:
//NOTE: A must be on for color to show up
const vec4 RED = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 GREEN = vec4(0.0, 1.0, 0.0, 1.0);
const vec4 BLUE = vec4(0.0, 0.0, 1.0, 1.0);
const vec4 YELLOW = vec4(1.0, 1.0, 0.0, 1.0);
const vec4 CYAN = vec4(0.0, 1.0, 1.0, 1.0);
const vec4 MAGENTA = vec4(1.0, 0.0, 1.0, 1.0);
#define gray(val)  vec4(val, val, val, 1.0)
const vec4 WHITE = gray(1.0); //vec4(1.0, 1.0, 1.0, 1.0);
const vec4 BLACK = gray(0.0); //vec4(0.0, 0.0, 0.0, 1.0);
//const vec4 GPUFX = vec4(0.0); //tranparent texture color allowing GPU fx to show thru

//eof
