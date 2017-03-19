//create graphics canvas:
//wrapper for OpenGL and shaders

'use strict'; //find bugs easier
require('colors'); //for console output
const WebGL = require('node-webgl'); //NOTE: for RPi must be lukaaash/node-webg or djulien/node-webgl version, not mikeseven/node-webgl version
const {mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');
//const Preproc = require("preprocessor");
const cpp = require("./cpp-1.0").create();
//require('buffer').INSPECT_MAX_BYTES = 1600;

const {debug} = require('./debug');
const {Screen} = require('./screen');

const document = WebGL.document();
const Image = WebGL.Image;
const alert = console.error;

const BLACK = 0xff000000; //NOTE: needs alpha to take effect

//const DEBUG = true;
const SHOW_SHSRC = true; //false;
const SHOW_VERTEX = true; //false;
const SHOW_LIMITS = true; //false;
const SHOW_PROGRESS = true; //false;
const WS281X_FMT = false; //whether to apply WS281X formatting to screen (when dpi24 overlay is *not* loaded)
//const VGROUP = 42; //enlarge display nodes for debug/demo; overridden when dpi24 overlay is loaded
//const OUTMODE = 1; //Screen.gpio? 1: -1;


///////////////////////////////////////////////////////////////////////////////
////
/// Main logic
//

//var gl, isGLES;

const Canvas =
module.exports.Canvas =
class Canvas
{
    constructor(title, w, h) //, vgroup)
    {
        document.setTitle(title || "WS281X demo");
//        if (gl) return; //only need one
//        console.log("canvas: '%s' %d x %d", title, w, h);
        initGL.call(this);
//        initTexture(w, h);
        this.txr = new Texture(this.gl, w, h); //, vgroup);
        glcheck("init txtr", this.getError());
        initShaders.call(this);
        initBuffers.call(this);
        initProjection.call(this);
        if (!Canvas.all) Canvas.all = [];
        Canvas.all.push(this);
        if (Canvas.all.length == 1) rAF(true); //start screen refresh loop
//    gl.enable(gl.DEPTH_TEST);
//    gl.clearColor(0.0, 0.0, 0.0, 1.0); //start out blank
    }

    destroy() //dtor
    {
        var inx = Canvas.all.indexOf(this);
        if (inx != -1) Canvas.all.splice(inx, 1);
        if (!Canvas.all.length) rAF(false); //cancel screen refresh loop
    }

    getError() { var err = this.gl.getError(); return err? -1: err; }

//elapsed, duration, interval can be used for progress bar or to drive animation
    get elapsed() { return this.prior_elapsed || 0; }
    set elapsed(newval)
    {
        var new_filter = this.interval? Math.floor(newval / this.duration / this.interval): newval / this.duration;
        if (new_filter == this.elapsed_filter) return;
        debug(10, "elapsed: was %d, is now %d / %d, #rAF %d".blue_lt, this.prior_elapsed, newval, this.duration, rAF.count); //, new_filter);
rAF.count = 0;
        this.elapsed_filter = new_filter;
        this.gl.uniform1f(this.shpgm.uniforms.elapsed, newval);
        this.prior_elapsed = newval;
    }

    get duration() { return this.prior_duration || 1; }
    set duration(newval)
    {
        this.gl.uniform1f(this.shpgm.uniforms.duration, newval);
        this.prior_duration = newval;
    }

//pass-thru graphics methods/properties to texture:
    get width() { return this.txr.width; }
    get height() { return this.txr.height; }
//    get vgroup() { return this.txr.vgroup || 1; }
//    draw(cb) { rAF.inner = cb; rAF(cb != null); }
    load(args) { return this.txr.load.apply(this.txr, arguments); }
    fill(args) { return this.txr.fill.apply(this.txr, arguments); }
    pixel(args) { return this.txr.pixel.apply(this.txr, arguments); }
};


Canvas.prototype.clear = 
function clear(color)
{
    var gl = this.gl;
//        txtr.xparent(BLACK);
    if (arguments.length && (color !== true)) gl.clearColor(R(color), G(color), B(color), A(color)); //color to clear buf to; clamped
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT); //uses clearColor
//    return this; //fluent
}


Canvas.prototype.render = 
function render(want_clear)
{
//    if (!this.txr.dirty) return; //NOTE: RPi needs refresh each time
    var gl = this.gl;
    if (arguments.length && (want_clear !== false)) this.clear(want_clear);
//NOTE: need to do this each time:
    gl.drawArrays(gl.POINTS, 0, this.vxbuf.numItems); //squareVertexPositionBuffer.numItems);
    this.txr.render();
    glcheck("render", this.getError());
//    return this; //fluent
}


/*
function draw(progress, flush) //, msec) //not_first)
{
    canvas.draw = function() //set up refresh function
{
//const NODEBIT_WIDTH = mediump(gl.viewportWidth / txtr.width);
//const NODE_HEIGHT = mediump(gl.viewportHeight / txtr.height);
//const ALL_WIDTH = mediump(gl.viewportWidth);
//const ALL_HEIGHT = mediump(gl.viewportHeight);
//const NUMW = mediump(ALL_WIDTH / NODEBIT_WIDTH);
//const NUMH = mediump(ALL_HEIGHT / NODE_HEIGHT);
//var node = Math.round(mediump(mediump(mediump(progress) * NUMW) * NUMH)); //not floor
//var nodex = Math.floor(mediump(node / NUMH)), nodey = node % NUMH;
//var msec = -1;
//console.log("draw: progr %s = %s => %s, %s sec, first? %s, %s", Math.round(1e6 * progress) / 1e6, mediump(progress), mediump(progress) * txtr.width * txtr.height, Math.round(msec) / 1e3, !draw.not_first, "n " + node); //, NUMW, NUMH);
        canvas.first();
//console.log("draw: txtr? %d, prog %s", txtr.dirty, progress);
        canvas.clear();

//no index buffer:
//    gl.drawArrays(gl.TRIANGLE_STRIP, 0, vxbuf.numItems); //squareVertexPositionBuffer.numItems);
//    gl.drawArrays(gl.TRIANGLES, 0, vxbuf.numItems); //squareVertexPositionBuffer.numItems);
//OpenGLES 2 only supports point sprites:
//http://gamedev.stackexchange.com/questions/11095/opengl-es-2-0-point-sprites-size
//    gl.drawElements(gl.POINTS, ixbuf.numItems, gl.UNSIGNED_INT, 0); //squareVertexPositionBuffer.numItems);
//    gl.drawElements(gl.POINTS, ixbuf.numItems, gl.UNSIGNED_INT, ixbuf.data); //squareVertexPositionBuffer.numItems);
//    gl.drawElements(gl.POINTS, ixbuf.numItems, gl.UNSIGNED_INT, 0); //squareVertexPositionBuffer.numItems);
//NOTE: need to do this each time:
//    gl.drawArrays(gl.POINTS, 0, vxbuf.numItems); //squareVertexPositionBuffer.numItems);
//    gl.disableVertexAttribArray(shpgm.vxinfo.xyz); //leave enabled
//    requestAnimationFrame(drawScene);

//if (elapsed > duration) return;
//    gl.uniform1f(shpgm.progressUniform, elapsed / duration);
//    if (!want_anim) return;
    canvas.render();

    canvas.progress = progress; //elapsed / duration);
    var timeslot = Math.floor(1e2 * progress) / 1e2; //elapsed / duration) / 100;
    if (timeslot == draw.prior) return;
    draw.prior = timeslot;
//    if ((timeslot - 1) * duration == 5) load_img("./texture.png");
//    if (timeslot > 1) return;
console.log("progress", timeslot);
}}
*/


/*
//Canvas.prototype.first = 
function setup()
{
//    if (this.not_first) return; //only need to set this once since it doesn't change
    var gl = this.gl;
//TODO: move matrices into shaders?
//        const ZOOM = 1; //1.05; //1;
//        const ZOOMX = NUM_UNIV
//        gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);

    this.mvMatrix = mat4.create();
    this.pMatrix = mat4.create();
//    mat4.perspective(45, gl.viewportWidth / gl.viewportHeight, 0.1, 100.0, pMatrix);
//    mat4.ortho(0, gl.viewportWidth, 0, gl.viewportHeight, 0.1, 100.0, pMatrix); // l,r,b,t,n,f
//        mat4.ortho(0 + 1 - ZOOM, 1 * ZOOM, 0 + 1 - ZOOM, 1 * ZOOM, 0.1, 100.0, pMatrix); // left, right, bottom, top, near, far
    mat4.ortho(0, 1, 0, 1, 0.1, 100.0, this.pMatrix); //project x, y as-is; left, right, bottom, top, near, far
//if (!drawScene.count) drawScene.count = 0;
//if (!drawScene.count++) console.log("ortho", pMatrix);
//if (!not_first) console.log("ortho", pMatrix);
    mat4.identity(this.mvMatrix);
//    mat4.translate(mvMatrix, [3.0, 0.0, 0.0]);
    mat4.translate(this.mvMatrix, [0, 0, -1]); //[+1.5-1.5, 0.0, -7.0+4-4]); //move away from camera so we can see it
//    mat4.rotate(mvMatrix, degToRad(xRot), [1, 0, 0]);
//    mat4.rotate(mvMatrix, degToRad(yRot), [0, 1, 0]);
//    mat4.rotate(mvMatrix, degToRad(zRot), [0, 0, 1]);
    setMatrixUniforms.call(this);
//    gl.uniform1f(this.shpgm.uniforms.outmode, OUTMODE);

//    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

/-*
//    gl.enableVertexAttribArray(shpgm.vxinfo.xyz); //already done
    gl.bindBuffer(gl.ARRAY_BUFFER, this.vxbuf);
//    gl.vertexAttribPointer(shpgm.vxinfo.xyz, 3, gl.FLOAT, false, fllen(8), fllen(0));
//    gl.vertexAttribPointer(shpgm.vxinfo.xyz, 3, gl.FLOAT, false, fllen(0), fllen(0));
//    gl.vertexAttribPointer(shpgm.vxinfo.xyz, vxbuf.itemSize, gl.FLOAT, false, fllen(0), fllen(0));
    gl.vertexAttribPointer(this.shpgm.vxinfo.vXYZ, 3, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(0));
//    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf.un);
    gl.vertexAttribPointer(this.shpgm.vxinfo.hUNM, 3, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(3));
//    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf.mxy);
    gl.vertexAttribPointer(this.shpgm.vxinfo.mXYWH, 4, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(3+3));

//    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ixbuf);
    gl.uniform1i(this.shpgm.uniforms.sampler, 0);
*-/
//    this.not_first = true;
//        draw.started = msec;
//    return this; //fluent
}
*/


//Canvas.prototype.initGL = 
function initGL() //canvas)
{
    try
    {
        var canvas = document.createElement("ws281x-canvas"); //name needs to include "canvas"
        var gl = this.gl = canvas.getContext("experimental-webgl");
        gl.isGLES = (gl.getParameter(gl.SHADING_LANGUAGE_VERSION).indexOf("GLSL ES") != -1); //check desktop (non-RPi) vs. embedded (RPi) version

        gl.viewportWidth = canvas.width;
        gl.viewportHeight = canvas.height;
        gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
        glcheck("init-1", this.getError());
//NOTE: both of the following are needed for gl_PointCoord; https://www.opengl.org/discussion_boards/showthread.php/182248-understanding-gl_PointCoord-(always-0)
//console.log(gl.POINT_SPRITE, gl.PROGRAM_POINT_SIZE);
        gl.enable(gl.POINT_SPRITE); //not valid/needed on RPi?
        glcheck("init-2", this.getError());
        gl.enable(gl.PROGRAM_POINT_SIZE); //not valid/needed on RPi?
        glcheck("init-3", this.getError());
        gl.enable(gl.DEPTH_TEST);
        glcheck("init-4", this.getError());
        gl.clearColor(R(BLACK), G(BLACK), B(BLACK), A(BLACK)); //color to clear buf to; clamped
        glcheck("init-5", this.getError());

        if (!SHOW_LIMITS) return;
        debug(10, "canvas: w %s, h %s, viewport: w %s, h %s".blue_lt, canvas.width, canvas.height, gl.viewportWidth, gl.viewportHeight);
        debug(10, "glsl ver# %s, is GL ES? %s".blue_lt, gl.getParameter(gl.SHADING_LANGUAGE_VERSION), gl.isGLES);
//        debug(10, "glsl exts".blue_lt, gl.getParameter(gl.EXTENSIONS));
        debug(10, "max viewport dims: %s".blue_lt, gl.getParameter(gl.MAX_VIEWPORT_DIMS)); //2K,2K on RPi
        debug(10, "max #textures: %d".blue_lt, gl.getParameter(gl.MAX_COMBINED_TEXTURE_IMAGE_UNITS)); //8 on RPi
        debug(10, "aliased line width range: %j".blue_lt, gl.getParameter(gl.ALIASED_LINE_WIDTH_RANGE)); //[0,256] on RPi
        debug(10, "aliased point size range: %j".blue_lt, gl.getParameter(gl.ALIASED_POINT_SIZE_RANGE)); //[0,256] on RPi
        debug(10, "max #var floats: %s (%d bytes)".blue_lt, gl.getParameter(gl.MAX_VERTEX_ATTRIBS), fllen(gl.getParameter(gl.MAX_VERTEX_ATTRIBS))); //8 on RPi
        debug(10, "max texture size: %s ^2 (%d MB)".blue_lt, gl.getParameter(gl.MAX_TEXTURE_SIZE), fllen(Math.pow(Math.floor(gl.getParameter(gl.MAX_TEXTURE_SIZE) / 1024), 2))); //2K sq (16 MB) on RPi
    }
    catch (e) { debug("exc", e); }
    if (!this.gl) alert("Init WebGL failed".red_lt);
}


///////////////////////////////////////////////////////////////////////////////
////
/// Shaders (runs on the GPU)
//

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

//Never ever branch in the fs if not absolutely avoidable.  Even on the most modern hardware fragments are evaluated in blocks of 4 and all branches that are taken in any of them forces the gpu to evaluate them for every fragment regardless of whether needed or not.
//To avoid branches one should always use functions like mix, step, smoothstep, etc.


//include this before other shader code:
function preamble()
{/*
//broken   #version 130 //bit-wise ops require GLSL 1.3 to be enabled
//nice analysis of precision: http://litherum.blogspot.com/2013/04/precision-qualifiers-in-opengl-es.html
#ifdef GL_ES
   #version 100 //GLSL ES 1.0 ~= GLSL 1.2
   #define lowp //10-bit float
   #define mediump //16-bit float: 1 sign + 5 exp + 10 mantissa
   #define highp //32-bit float: 1 sign + 8 exp + 23 mantissa
//   precision mediump float;
   #define PRECISION(stmt)  stmt;
#else
   #version 120 //gl_PointCoord requires GLSL 1.2 to be enabled
   #define PRECISION(stmt)
#endif
PRECISION(mediump float)

//try to avoid conditional branches:
//NOTE: bit-wise ops require GLSL 1.3
//#define IIF(expr, true, false)  ((expr) & (true) | ~(expr) & (false))
#define IIF(expr, true, false)  mix(false, true, BOOL(expr))

//compensate for floating point precision:
const float FUD = 1.0e-6;
#define NOT(expr)  (1.0 - BOOL(expr))
#define BOOL(value)  float(value) //convert bool expr to 0/1
#define EQ(lhs, rhs)  (abs((lhs) - (rhs)) < FUD)
#define NE(lhs, rhs)  (abs((lhs) - (rhs)) >= FUD)
//#define GE(lhs, rhs)  step(rhs - FUD, lhs) //convert lhs >= rhs to 0/1
//#define GT(lhs, rhs)  step(lhs + FUD, lhs)
//#define LE(lhs, rhs)  step(lhs + FUD, rhs)
//#define LT(lhs, rhs)  step(lhs - FUD, rhs)
#define GE(lhs, rhs)  ((lhs) >= (rhs) - FUD)
#define GT(lhs, rhs)  ((lhs) > (rhs) + FUD)
#define LE(lhs, rhs)  ((lhs) <= (rhs) + FUD)
#define LT(lhs, rhs)  ((lhs) < (rhs) - FUD)

//dimensions of nodes on screen:
//const float VGROUP = float(??);
const float SCR_WIDTH = float(??);
const float SCR_HEIGHT = float(??);
const float NUM_UNIV = float(??);
const float UNIV_LEN = float(??); //SCR_HEIGHT; // / VGROUP; //float(??);
const float NODEBIT_WIDTH = float(SCR_WIDTH / NUM_UNIV);
const float NODE_HEIGHT = float(SCR_HEIGHT / UNIV_LEN);
//#define PTSIZE  max(NODEBIT_WIDTH, NODE_HEIGHT)
const float PTSIZE = max(NODEBIT_WIDTH, NODE_HEIGHT);

//#define PROGRESS_BAR

//primary RGBA colors:
const vec4 RED = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 GREEN = vec4(0.0, 1.0, 0.0, 1.0);
const vec4 BLUE = vec4(0.0, 0.0, 1.0, 1.0);
const vec4 YELLOW = vec4(1.0, 1.0, 0.0, 1.0);
const vec4 CYAN = vec4(0.0, 1.0, 1.0, 1.0);
const vec4 MAGENTA = vec4(1.0, 0.0, 1.0, 1.0);
const vec4 WHITE = vec4(1.0, 1.0, 1.0, 1.0);
const vec4 BLACK = vec4(0.0, 0.0, 0.0, 1.0);

//from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
vec3 rgb2hsv(vec3 c)
{
//    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
//    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
//    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    vec4 K = vec4(0.0, -1.0, 2.0, -3.0) / 3.0; //TODO: make const?
    vec4 p = IIF(GE(c.g, c.b), vec4(c.gb, K.xy), vec4(c.bg, K.wz));
    vec4 q = IIF(GE(c.r, p.x), vec4(c.r, p.yzx), vec4(p.xyw, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
//    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec4 K = vec4(3.0, 2.0, 1.0, 9.0) / 3.0; //TODO: make const?
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
*/}


//vertex shader:
//effect generation (color selection) should be done here, *not* in fragment shader
//runs once per vertex, which is ~ 500 - 1000x less than fragment shader
function vertex_sh()
{/*
//attribute vec3 aVertexPosition;
//attribute vec4 aVertexColor;
//attribute vec2 aTextureCoord;
//varying vec2 vTextureCoord;
attribute vec3 vXYZ; //3D view (x, y, z)
attribute vec3 hUNM; //hw (univ#, node#, model#)
attribute vec4 mXYWH; //model (x, y, w, h)

uniform mat4 uModelView;
uniform mat4 uProjection;
uniform float elapsed, duration; //progress
//uniform float outmode;
uniform sampler2D uSampler;

varying vec4 vColor;
//varying vec3 vecpos;
//varying float thing;

//varying vec2 txcoord;

void main(void)
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

//    gl_Position = uProjection * uModelView * vec4(aVertexPosition, 1.0);
    gl_Position = uProjection * (uModelView * vec4(vXYZ, 1.0));
//    gl_Position.y = (gl_Position.y + 1.0) * VGROUP - 1.0; //gl_Position.y / 10.0; //VGROUP;
//    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
//    gl_Position = vec4(vxyz, 0.0, 1.0); //just use entire screen
//    gl_Position = vec4(vxyz, 1.0); //just use entire screen
//    float dummy = mXYWH.x; //kludge to prevent optimizing out
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
//pivot other pixels on this row:
    vec4 nabe0 = texture2D(uSampler, vec2(0.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe1 = texture2D(uSampler, vec2(1.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe2 = texture2D(uSampler, vec2(2.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe3 = texture2D(uSampler, vec2(3.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe4 = texture2D(uSampler, vec2(4.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe5 = texture2D(uSampler, vec2(5.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe6 = texture2D(uSampler, vec2(6.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe7 = texture2D(uSampler, vec2(7.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe8 = texture2D(uSampler, vec2(8.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe9 = texture2D(uSampler, vec2(9.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe10 = texture2D(uSampler, vec2(10.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe11 = texture2D(uSampler, vec2(11.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe12 = texture2D(uSampler, vec2(12.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe13 = texture2D(uSampler, vec2(13.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe14 = texture2D(uSampler, vec2(14.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe15 = texture2D(uSampler, vec2(15.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe16 = texture2D(uSampler, vec2(16.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe17 = texture2D(uSampler, vec2(17.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe18 = texture2D(uSampler, vec2(18.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe19 = texture2D(uSampler, vec2(19.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe20 = texture2D(uSampler, vec2(20.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe21 = texture2D(uSampler, vec2(21.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe22 = texture2D(uSampler, vec2(22.0 / NUMW, hUNM.t / NUMH));
    vec4 nabe23 = texture2D(uSampler, vec2(23.0 / NUMW, hUNM.t / NUMH));
        vec4 total = 0;
        total += nabe0 + nabe1 + nabe2 + nabe3;
        total += nabe4 + nabe5 + nabe6 + nabe7;
        total += nabe8 + nabe9 + nabe10 + nabe11;
        total += nabe12 + nabe13 + nabe14 + nabe15;
        total += nabe16 + nabe17 + nabe18 + nabe19;
        total += nabe20 + nabe21 + nabe22 + nabe23;
        total /= 24.0;
#endif
    if (LT(elapsed, 0.0) || GE(elapsed, duration)) //get color from texture
    {
//        vColor = texture2D(uSampler, vec2((hw_univ + 0.1) / NUMW, hw_node / NUMH));
        vColor = texture2D(uSampler, vec2(hw_univ / (NUM_UNIV - 1.0), hw_node / (UNIV_LEN - 1.0))); //[0..1]
//        txcoord = vec2(hw_univ / (NUM_UNIV - 1.0), hw_node / (UNIV_LEN - 1.0)); //[0..1]
//no        vColor = texture2D(uSampler, vec2(hw_univ, hw_node)); //[0..size]
//        vColor = texture2D(uSampler, vec2(0.0, 0.0)); //[0..1]
//        vColor.a = 1.0;
//        vColor.r = 1.0;
//        vColor = total; // / 24.0;
    }
    else //use effect logic to generate color
    {
        vColor = vec4(hsv2rgb(vec3(hw_model / NUM_UNIV, 1.0, 1.0)), 1.0); //choose different color for each model
#define round(thing)  thing  //RPi
//       float node = round(elapsed / duration * NUMW * NUMH); //not floor
       float node_inx = round(elapsed / duration * NUM_UNIV * UNIV_LEN); //TODO: round?
       float nodex = floor(node_inx / UNIV_LEN), nodey = mod(node_inx, UNIV_LEN);
       if (GT(hw_univ, nodex) || (EQ(hw_univ, nodex) && GT(hw_node, nodey))) vColor = BLACK;
    }
//    if (hUNM.y > nodey) vColor = BLACK;
//    vColor = vec4(vxyz, 1.0);
//    vColor = vec4(0.0, 0.5, 0.5, 1.0);
//    vColor = vec4(vxyz, 1.0);
//    vTextureCoord = aTextureCoord;
//    vecpos = aVertexPosition;
//    vecpos = vec3(vxyz);
//    if (outmode == 0.0) gl_PointSize = min(NODEBIT_WIDTH, NODE_HEIGHT);
//    else gl_PointSize = NODEBIT_WIDTH; //TODO: screen_width / 23.25
   gl_PointSize = PTSIZE; //max(NODEBIT_WIDTH, NODE_HEIGHT); //PTSIZE;
}
*/}

	
//fragment shader:
//format pixels for screen or WS281X
//runs once per screen pixel; should only do screen formatting in here
function fragment_sh()
{/*
//uniform sampler2D uSampler;
uniform float elapsed, duration; //progress
//uniform float outmode;
//varying vec3 vecpos;
varying vec4 vColor;
//varying float thing;
//varying vec2 vTextureCoord;

//varying vec2 txcoord;
//uniform sampler2D uSampler;

void main(void)
{
    float x = gl_FragCoord.x / SCR_WIDTH; //[0..1]
    float y = gl_FragCoord.y / SCR_HEIGHT; //[0..1]
//    gl_FragColor = texture2D(uSampler, vec2(vTextureCoord.s, vTextureCoord.t));
//    gl_FragColor = vec4(0.0, 0.5, 1.0, 1.0);
    gl_FragColor = vColor;
//gl_FragColor = texture2D(uSampler, txcoord);
//gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);

#ifdef PROGRESS_BAR
    if (LT(y, 0.005) && LE(x, elapsed / duration))
    {
        gl_FragColor = WHITE; //progress bar (debug)
//    else if (LT(y, 0.13) && GT(y, 0.1) && GE(x, 0.5) && LE(x, 0.503)) gl_FragColor = WHITE;
//    else
        return;
    }
#endif //def PROGRESS_BAR
//    if (EQ(outmode, 0.0)) // == 0.0) //show raw pixels like light bulbs
#ifdef WS281X_FMT //format as WS281X
   gl_FragColor = MAGENTA;
#else //show raw pixels as light bulbs
    const float EDGE = min(NODEBIT_WIDTH, NODE_HEIGHT) / max(NODEBIT_WIDTH, NODE_HEIGHT) / 2.0;
    vec2 coord = gl_PointCoord - 0.5; //vec2(0.5, 0.5); //from [0,1] to [-0.5,0.5]
	float dist = sqrt(dot(coord, coord));
//        if (dist > 0.5 * FUD) gl_FragColor = MAGENTA;", //discard;
   if (GT(dist, EDGE)) discard;
   gl_FragColor *= 1.0 - dist / (1.2 * EDGE); //diffuse to look more like light bulb
#endif //def WS281X_FMT
//    gl_FragColor = vec4(gl_PointCoord, 0.0, 1.0);
//    if (vecpos.x < 0.0) gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
//    gl_FragColor = vec4(gl_PointCoord.s, gl_PointCoord.t, 0.0, 1.0);
//    vec3 st = (vecpos + 1.0) / 2.0;
//    if ((st.t < .02) && (st.s <= progress)) gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
//gl_FragCoord is window space [0..size],[0..size]
//    if (gl_FragCoord.x < 100.0) gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);", //discard;
//    if (gl_FragCoord.x > 100.0) gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);", //discard;
}
*/}


//var shpgm;

//Canvas.prototype.initShaders = 
function initShaders(vzoom)
{
    var gl = this.gl;
    var vertexShader = getShader.call(this, gl.VERTEX_SHADER, vertex_sh); //"shader-vs");
    var fragmentShader = getShader.call(this, gl.FRAGMENT_SHADER, fragment_sh); //"shader-fs");

    var shpgm = this.shpgm = glcheck("shpgm", gl.createProgram());
    gl.attachShader(shpgm, vertexShader);
    gl.attachShader(shpgm, fragmentShader);
    gl.linkProgram(shpgm);

    if (!gl.getProgramParameter(shpgm, gl.LINK_STATUS))
    {
        var reason = "";
        if (gl.getProgramParameter(shpgm, gl.INFO_LOG_LENGTH) > 1)
            reason = "; reason: " + gl.getProgramInfoLog(shpgm);
        alert("Could not initialise shaders %s".red_lt, reason);
		gl.deleteProgram(shpgm);
        this.shpgm = null;
		return 0;
	}

    gl.useProgram(shpgm);

//    var va;
    shpgm.vxinfo = {};
    shpgm.uniforms = {};
//cut down on verbosity:
    gl.gAL = gl.getAttribLocation;
    gl.gUL = gl.getUniformLocation;
    gl.eVAA = gl.enableVertexAttribArray;
//these will always be used, so just enable them once here
    gl.eVAA(shpgm.vxinfo.vXYZ = glcheck("vXYZ", gl.gAL(shpgm, "vXYZ")));
    gl.eVAA(shpgm.vxinfo.hUNM = glcheck("hUNM", gl.gAL(shpgm, "hUNM")));
    gl.eVAA(shpgm.vxinfo.mXYWH = glcheck("mXYWH", gl.gAL(shpgm, "mXYWH")));

    shpgm.uniforms.pMatrix = glcheck("uProjection", gl.gUL(shpgm, "uProjection"));
    shpgm.uniforms.mvMatrix = glcheck("uModelView", gl.gUL(shpgm, "uModelView"));
    shpgm.uniforms.elapsed = glcheck("elapsed", gl.gUL(shpgm, "elapsed"));
    shpgm.uniforms.duration = glcheck("duration", gl.gUL(shpgm, "duration"));
//    shpgm.uniforms.outmode = glcheck("outmode", gl.gUL(shpgm, "outmode"));

    shpgm.uniforms.sampler = glcheck("uSampler", gl.gUL(shpgm, "uSampler"));
//    shpgm.textureCoordAttribute = gl.getAttribLocation(shpgm, "aTextureCoord");
//    gl.enableVertexAttribArray(shpgm.textureCoordAttribute);
    gl.uniform1i(shpgm.uniforms.sampler, 0); //this.txr.id); //won't change, so set it now
    glcheck("sampler", this.getError()); //invalid?
}


//Canvas.prototype.getShader = 
function getShader(type, str)
{
    var gl = this.gl;
    var ShaderTypes = {};
    ShaderTypes[gl.FRAGMENT_SHADER] = "fragment";
    ShaderTypes[gl.VERTEX_SHADER] = "vertex";

//prepend common defs:
    str = heredoc(preamble) + "\n" + heredoc(str);
//expand macros:
//    str = str.replace(/%SWIDTH%/gm, gl.viewportWidth);
//    str = str.replace(/%SHEIGHT%/gm, gl.viewportHeight);
//    str = str.replace(/%NWIDTH%/gm, gl.viewportWidth / this.width); //NUM_UNIV);
//    str = str.replace(/%NHEIGHT%/gm, gl.viewportHeight / this.height); // * VGROUP / UNIV_LEN);
//unfortunately ver# must be on first line, so we need to pre-process #ifdef:
//https://loune.net/2011/02/match-any-character-including-new-line-in-javascript-regexp/
//    var verdep = str.match(/^([\s\S]*\n)?\s*#ifdef GL_ES.*\n([\s\S]*\n)?\s*#else.*\n([\s\S]*\n)?\s*#endif.*\n([\s\S]*)$/m);
//    if (verdep) str = (verdep[1] || "") + (verdep[isGLES? 2: 3] || "") + (verdep[4] || "");
/*
    str = str.replace(/\n\s*#(if|else|endif)/g, "\n// #$1");
//    if (true || false) showsrc(str, type);
//console.log("preproc");
    var pp = new Preproc(str, ".");
    str = pp.process(
    {
        GL_ES: gl.isGLES,
        WS281X_FMT: Screen.gpio != 0,
//        NUM_UNIV: this.width,
//        UNIV_LEN: this.height,
//        SCR_WIDTH: gl.viewportWidth,
//        SCR_HEIGHT: gl.viewportHeight,
    }); //, console.log);
//kludge: preprocessor uses strange #put syntax, so just expand numeric macros manually:
*/
//    if (gl.isGLES) str = "#define GL_ES\n" + str;
//    if (Screen.gpio) str = "#define WS281X_FMT\n" + str;
//    if (SHOW_PROGRESS) str = "#define PROGRESS_BAR\n" + str;
    if (gl.isGLES) cpp.define("GL_ES", "");
    if (Screen.gpio) cpp.define("WS281X_FMT", "");
    if (SHOW_PROGRESS) cpp.define("PROGRESS_BAR", "");
    str = cpp.run(str);
    cpp.clear();
//(?<=a)b  //look-behind no worky
//NOTE: using non-greedy regexp to match first "??" only
    str = str.replace(/(\WSCR_WIDTH\s*=.*?)\?\?/, "$1" + (Screen.gpio? Screen.horiz.disp: Screen.width));
    str = str.replace(/(\WSCR_HEIGHT\s*=.*?)\?\?/, "$1" + (Screen.gpio? Screen.vert.disp: Screen.height));
//    str = str.replace(/(\W)univ_len(\W)/g, "$1" + this.height + "$2");
//    str = str.replace(/(\WNUM_UNIV = .*)\?\?(.*)$/, "$1" + gl.viewportWidth + "$2");
    str = str.replace(/(\WNUM_UNIV\s*=.*?)\?\?/, "$1" + this.width); //gl.viewportWidth);
    str = str.replace(/(\WUNIV_LEN\s*=.*?)\?\?/, "$1" + this.height); //gl.viewportHeight);
//    str = str.replace(/(\WVGROUP\s*=.*?)\?\?/, "$1" + this.vgroup);
    if (SHOW_SHSRC) showsrc(str, ShaderTypes[type]);
//process.exit(0);

    var shader = glcheck("shader", gl.createShader(type)); //gl.FRAGMENT_SHADER);
    gl.shaderSource(shader, str);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
    {
        if (!SHOW_SHSRC) showsrc(str, ShaderTypes[type]);
        alert(gl.getShaderInfoLog(shader));
        return null;
    }
    return shader;
}


///////////////////////////////////////////////////////////////////////////////
////
/// Vertex buffers
//

//var vxbuf; //, ixbuf;

//Canvas.prototype.initBuffers = 
function initBuffers()
{
    var gl = this.gl;
    var vxbuf = this.vxbuf = glcheck("vxbuf", gl.createBuffer()); //{xyz: gl.createBuffer(), un: gl.createBuffer(), mxy: gl.createBuffer()};
//    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf.xyz);
//    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf.un);
//    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf.mxy);
    gl.bindBuffer(gl.ARRAY_BUFFER, vxbuf);
/*    var vertices = //3D view (x, y, z), hw (model#, univ#, node#), model (x, y, w, h)
    [
/-*
        1.0, 1.0, 0.0,   0.0, 0.0,  1.0, 0.0, 0.0,
        1.0, 0.75, 0.0,   0.0, 1.0,  1.0, 1.0, 0.0,
        1.0, 0.5, 0.0,   0.0, 2.0,  1.0, 2.0, 0.0,
        1.0, 0.25, 0.0,   0.0, 3.0,  1.0, 0.0, 1.0,
        1.0, 0.0, 0.0,   0.0, 4.0,  1.0, 1.0, 1.0,

        0.67, 1.0, 0.0,   1.0, 0.0,  2.0, 0.0, 0.0,
        0.67, 0.67, 0.0,   1.0, 1.0,  2.0, 0.0, 1.0,
        0.67, 0.33, 0.0,   1.0, 2.0,  2.0, 0.0, 2.0,
        0.67, 0.0, 0.0,   1.0, 3.0,  2.0, 0.0, 3.0,

        0.33, 1.0, 0.0,   2.0, 0.0,  3.0, 0.0, 0.0,
        0.33, 0.67, 0.0,   2.0, 0.1,  3.0, 0.0, 1.0,
        0.33, 0.33, 0.0,   2.0, 0.2,  4.0, 0.0, 0.0,
        0.33, 0.0, 0.0,   2.0, 0.3,  4.0, 0.0, 1.0,

        0.0, 1.0, 0.0,   3.0, 0.0,  5.0, 0.0, 0.0,
        0.0, 0.5, 0.0,   3.0, 1.0,  6.0, 0.0, 0.0,
        0.0, 0.0, 0.0,   3.0, 2.0,  7.0, 0.0, 0.0,
*-/
        1.0, 1.0, 0.0,   0.0, 0.0, 0.0,   1.0, 0.0, 2.0, 2.0,
        -1.0, 0.5, 0.0,   1.0, 1.0, 0.0,  0.0, 1.0, 2.0, 2.0,
        1.0, -1.0, 0.0,   2.0, 2.0, 0.0,  0.0, 0.0, 2.0, 2.0,
        -0.5, -1.0, 0.0,   3.0, 3.0, 0.0,  1.0, 1.0, 2.0, 2.0,
    ];
*/
    var vertices = []; //3D view (x, y, z), hw (model#, univ#, node#), model (x, y, w, h)
/*
    vertices.push(1.0, 1.0, 0.0,   0.0, 0.0, 0.0,   1.0, 0.0, 2.0, 2.0);
    vertices.push(0.0, 0.0, 0.0,   3.0, 3.0, 0.0,  1.0, 1.0, 2.0, 2.0);
    vertices.push(0.0, 1.0, 0.0,   1.0, 1.0, 0.0,  0.0, 1.0, 2.0, 2.0);
    vertices.push(1.0, 0.0, 0.0,   2.0, 2.0, 0.0,  0.0, 0.0, 2.0, 2.0);
*/
//console.log("init buf %d x %d", this.width, this.height);
    for (var x = 0; x < this.width; ++x)
        for (var y = 0; y < this.height; ++y)
            {
            vertices.push((x + 0.5) / this.width, (y + 0.5) / this.height, 0,   x, y, x,   0, 0, 0, 0);
            if (SHOW_VERTEX)
                if (((x < 2) || (x >= this.width - 2)) && ((y < 2) || (y >= this.height - 2)))
                    console.log("vert[%s, %s]: xyz %j,  hw %j,  model %j", x, y, vertices.slice(-10, -7), vertices.slice(-7, -4), vertices.slice(-4));
            }

    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
    glcheck("initbufs-1", this.getError());
    vxbuf.itemSize = 3+3+4; //vec3 + vec3 + vec4 per vertex
    vxbuf.numItems = vertices.length / vxbuf.itemSize;
debug("vertex: data size %s bytes, #items %d x %d = %s, %j".cyan_lt, fllen(vxbuf.itemSize), this.width, this.height, vxbuf.numItems, this.shpgm.vxinfo);
//    gl.bindBuffer(gl.ARRAY_BUFFER, this.vxbuf);
    gl.vertexAttribPointer(this.shpgm.vxinfo.vXYZ, 3, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(0));
    glcheck("initbufs-2", this.getError());
    gl.vertexAttribPointer(this.shpgm.vxinfo.hUNM, 3, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(3));
    glcheck("initbufs-3", this.getError());
    gl.vertexAttribPointer(this.shpgm.vxinfo.mXYWH, 4, gl.FLOAT, false, fllen(this.vxbuf.itemSize), fllen(3+3));
    glcheck("initbufs-4", this.getError());

/* don't need indexing
    ixbuf = glcheck("ixbuf", gl.createBuffer());
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ixbuf);
    var inx = []; //[0, 1, 2, 3]; //, 2, 1, 1, 1, 0, 2, 3]; //, 5, 6, 7, 8];
    for (var i = 0; i < vxbuf.numItems; ++i) inx[i] = i;
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint32Array(inx), gl.STATIC_DRAW);
//    ixbuf = {};
//    ixbuf.data = new Uint32Array(inx);
    ixbuf.itemSize = i32len(1); //2;
    ixbuf.numItems = inx.length;
console.log("index data size", i32len(1), "#items", ixbuf.numItems);
*/

//only need one texture, so set it now:
//    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ixbuf);
//    gl.uniform1i(this.shpgm.uniforms.sampler, 0);
//    glcheck("initbufs", this.getError());
}


///////////////////////////////////////////////////////////////////////////////
////
/// Projection matrices
//

//http://www.learnopengles.com/tag/projection-matrix/
//https://www.opengl.org/discussion_boards/showthread.php/146801-2d-graphics-with-perspective-projection
//var mvMatrix = mat4.create();
//var pMatrix = mat4.create();

//create a projection matrix to map world coords to screen coords:
//http://stackoverflow.com/questions/7317119/how-to-achieve-glorthof-in-opengl-es-2-0
/*
function ortho(left, right, top, bottom, near far)
{
    GLfloat r_l = right - left;
    GLfloat t_b = top - bottom;
    GLfloat f_n = far - near;
    GLfloat tx = - (right + left) / (right - left);
    GLfloat ty = - (top + bottom) / (top - bottom);
    GLfloat tz = - (far + near) / (far - near);

    matrix[0] = 2.0f / r_l;
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = tx;

    matrix[4] = 0.0f;
    matrix[5] = 2.0f / t_b;
    matrix[6] = 0.0f;
    matrix[7] = ty;

    matrix[8] = 0.0f;
    matrix[9] = 0.0f;
    matrix[10] = 2.0f / f_n;
    matrix[11] = tz;

    matrix[12] = 0.0f;
    matrix[13] = 0.0f;
    matrix[14] = 0.0f;
    matrix[15] = 1.0f;
}
*/

/*
function mvPushMatrix() {
  var copy = mat4.create();
  mat4.set(mvMatrix, copy);
  mvMatrixStack.push(copy);
}

function mvPopMatrix() {
  if (mvMatrixStack.length == 0) {
    throw "Invalid popMatrix!";
  }
  mvMatrix = mvMatrixStack.pop();
}
*/

//Canvas.prototype.setMatrixUniforms = 
//function setMatrixUniforms()
function initProjection()
{
//    this.mvMatrix = mat4.create();
    this.pMatrix = mat4.create();
//    mat4.perspective(45, gl.viewportWidth / gl.viewportHeight, 0.1, 100.0, pMatrix);
//    mat4.ortho(0, gl.viewportWidth, 0, gl.viewportHeight, 0.1, 100.0, pMatrix); // l,r,b,t,n,f
//        mat4.ortho(0 + 1 - ZOOM, 1 * ZOOM, 0 + 1 - ZOOM, 1 * ZOOM, 0.1, 100.0, pMatrix); // left, right, bottom, top, near, far
    mat4.ortho(0, 1, 0, 1, 0.1, 100.0, this.pMatrix); //project x, y as-is; left, right, bottom, top, near, far

//if (!drawScene.count) drawScene.count = 0;
//if (!drawScene.count++) console.log("ortho", pMatrix);
//if (!not_first) console.log("ortho", pMatrix);
    this.mvMatrix = mat4.create();
    mat4.identity(this.mvMatrix);
//    mat4.translate(mvMatrix, [3.0, 0.0, 0.0]);
    mat4.translate(this.mvMatrix, [0, 0, -1]); //[+1.5-1.5, 0.0, -7.0+4-4]); //move away from camera so we can see it
//    mat4.rotate(mvMatrix, degToRad(xRot), [1, 0, 0]);
//    mat4.rotate(mvMatrix, degToRad(yRot), [0, 1, 0]);
//    mat4.rotate(mvMatrix, degToRad(zRot), [0, 0, 1]);
//    setMatrixUniforms.call(this);

//    if (!this.mvMatrix) this.mvMatrix = mat4.create();
//    if (!this.pMatrix) this.pMatrix = mat4.create();
//doesn't change, so set it here:
    this.gl.uniformMatrix4fv(this.shpgm.uniforms.pMatrix, false, this.pMatrix);
    this.gl.uniformMatrix4fv(this.shpgm.uniforms.mvMatrix, false, this.mvMatrix);
    glcheck("init proj", this.getError());
}


///////////////////////////////////////////////////////////////////////////////
////
/// Texture handling (1 vertex for each LED pixel)
//

//var txtr = {};
//function initTexture(w, h) { txtr.init(w, h, true); }

class Texture
{
    constructor(gl, w, h, grp)
    {
        this.init(gl, w, h, grp, true);
    }

    getError() { var err = this.gl.getError(); return err? -1: err; }
};


//L-to-R, T-to-B fill order:
//NOTE: this gives locality of reference to horizontal neighbors (improves GPU cache performance)
Texture.prototype.xy = 
function xy(x, y)
{
    return x + y * this.width;
}


//NOTE:
//leave pixel manip in JavaScript so JIT compiler can optimize surrounding code
//want to minimize traffic from JavaScript to C++ (not sure if JIT can optimize that)


Texture.prototype.render = 
function render()
{
    if (!this.dirty) return;
    var gl = this.gl;
//    if (Pixels().dirty()) //(txtr || {}).image.dirty)
//    txtr_init();
//        gl.activeTexture(gl.TEXTURE0);
//        gl.bindTexture(gl.TEXTURE_2D, txtr);
//        gl.uniform1i(shpgm.samplerUniform, 0);
//NOTE: better performance to reuse texture rather than recreating each time
//    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.pixels);
//var buf = "";
//for (var i = 0; i < this.pixels.length; ++i)
//    buf += "," + (this.pixels[i] >>> 0).toString(16);
//console.log("render txtr from"); //, buf); //typeof this.pixels, this.pixels);
//GL_RGB, GL_BGR, GL_RGBA, GL_BGRA
//console.log("send %d x %d texture", this.width, this.height);
//    gl.activeTexture(gl.TEXTURE0); //redundant?
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, this.width, this.height, this.fmt, gl.UNSIGNED_BYTE, this.pixels);
    glcheck("sub img", this.getError());  //invalid value on RPi?
    this.dirty = false;
//    return this; //fluent
}


Texture.prototype.fill = 
function fill(xofs, yofs, w, h, rgba)
{
//    this.init(true);
    if (arguments.length == 1) { rgba = xofs; xofs = yofs = 0; w = this.width; h = this.height; }
//rgba = (rgba & 0xFF00FF00) | R(color) | (B(color) << 16); //ABGR <-> ARGB
//rgba = 0xff0000ff;
    debug(10, "fill 0x%s".blue_lt, rgba.toString(16));
//clip:
    if (xofs < 0) xofs = 0;
    if (xofs + w > this.width) w = this.width - xofs;
    if (yofs < 0) yofs = 0;
    if (yofs + h > this.height) h = this.height - yofs;
    for (var y = yofs, ofs = 0; y < yofs + h; ++y)
        for (var x = xofs; x < xofs + w; ++x, ++ofs)
            this.pixels[ofs] = rgba;
    this.dirty = true; //just assume it changed
//    txtr.image.fill.apply(null, arguments);
    return this; //fluent
}


/* TODO
txtr.line = function(x1, y1, x2, y2, rgba)
{
    this.init(true);
    if (Math.abs(x1 - x2) > Math.abs(y1 - y2)))
    {
        var dx = 
        for (var x = Math.max
    else
    return this; //fluent
}


txtr.circle = function(x, y, r, rgba)
{
    this.init(true);
    for (var a = 0; a < 
}
*/


Texture.prototype.pixel = 
function pixel(x, y, rgba)
{
//    this.init(true);
    if ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height))
        return (arguments.length > 2)? 0: this; //fluent setter only, not getter
//    var ofs = x + y * this.width;
    if (arguments.length > 2) //set color
    {
//        if (this.pixels[ofs] != rgba)
//        {
//console.log("pixel(%d,%d): ofs %d", x, y, this.xy(x, y));
            this.pixels[this.xy(x, y)] = rgba;
            this.dirty = true; //assume caller is changing it
//        }
        return this; //fluent setter
    }
    return this.pixels[this.xy(x, y)]; //non-fluent getter
//    return txtr.image.pixel.apply(null, arguments);
}


//load image from file:
//this should be most efficient for RPi byte order
//can be slower on non-RPi since that is dev or test only
Texture.prototype.load = 
function load(path)
{
    var gl = this.gl;
//console.log("load img", path);
    if (!this.image)
    {
//        this.init(false);
        this.image = new Image();
//NOTE: according to the docs, FreeImage defaults to RGB for big endian, BGR for little endian
//        var swap = (this.fmt == gl.BGRA)? "readUInt32LE": "readUInt32BE";
//NOTE: can't swizzle because caller might also be setting some pixels explicitly
        this.image.pixel = function(x, y) //get ARGB pixel color
        {
            var ofs = 4 * (x + y * this.width);
            if (!this.img_pixels) { this.img_pixels = new Buffer(this.data.buffer); this.seen = {}; }
//            var color = this.img_pixels[x + y * this.image.width];
//            if (need_swap) color = 
//            return this.img_pixels[swap](4 * (x + y * this.width));
//NOTE: OpenGL ES doesn't support BGR(A); always use RGBA
//NOTE: FreeImage stores data in BGR, so node-webgl converts from BGR to RGB??
//            return this.img_pixels.readUInt32LE(4 * (x + y * this.width));
//            var color = this.img_pixels.readUInt32BE(ofs); //RGBA
            var color = this.img_pixels.readUInt32LE(ofs) >>> 0; //RGBA
            color = ((color & 0xFF00FF00) | R(color) | (B(color) << 16)) >>> 0; //ABGR <-> ARGB
if (!this.seen[color])
    console.log("pixel[%d,%d] = %s => %s", x, y, this.img_pixels.readUInt32BE(ofs).toString(16), color.toString(16));
this.seen[color] = true;
//            return color | 0xff000000;
//            return 0xffff00ff;
            return color;
        }

        this.image.onload = function()
        {
//console.log("loaded", path);
            if ((this.image.width != this.width) || (this.image.height != this.height))
                debug("image '%s' size %d x %d doesn't match canvas size %d x %d".yellow_lt, path, this.image.width, this.image.height, this.width, this.height);
                else debug("image '%s' size %d x %d matches canvas".green_lt, path, this.image.width, this.image.height);
//            this.pixels = this.image.data; //gl.getImageData(this.image);
//            var img_pixels = new Uint32Array(this.image.data.buffer); //, 0, this.image.data.length); //gl.getImageData(this.image);
//console.log("buf", this.image.data.buffer);
/*
var img_pixels_buf = "";
for (var i = 0; i < img_pixels.length; ++i)
{
    if (img_pixels[i] == BLACK) continue;
    img_pixels_buf += ", ";
    if (i && img_pixels[i - 1] == BLACK) img_pixels_buf += "'" + i + ": ";
    img_pixels_buf += "x" + img_pixels[i].toString(16);
}
console.log("img px", img_pixels_buf.substr(2));
*/
//            this.width = this.image.width;
//            this.height = this.image.height;
//            this.fmt = gl.RGBA; //TODO: check byte order
//            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, this.fmt, gl.UNSIGNED_BYTE, this.image);
//            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, this.width, this.height, 0, this.fmt, gl.UNSIGNED_BYTE, this.pixels);
//copy to texture in case image is smaller:
            for (var y = 0; y < this.image.height; ++y)
                for (var x = 0; x < this.image.width; ++x)
                    this.pixels[this.xy(x, y)] = this.image.pixel(x, y);
            this.image.img_pixels = null;
            this.dirty = true;
            debug("loaded image '%s', size %dx%d".blue_lt, this.image.src, this.image.width, this.image.height); //, this.pixels);
//var buf = "";
//for (var i = 0; i < this.pixels.length; ++i) buf += "," + (this.pixels[i] >>> 0).toString(16);
//console.log("img pixels", this.pixels.length, buf);
//            this.dirty = true;
//            blocking(this.svgen); //resume parent generator
        }.bind(this);
    }
//console.log("loading img ", path);
    this.image.src = path; //NOTE: this triggers onload event handler; operates like synchronous function
//no    return this; //fluent
//    return function(gen)
//    {
////        console.log("got gen", typeof gen);
//        this.svgen = gen; //save it so caller can continue
//        this.image.src = path; //NOTE: this triggers onload event handler; need to save gen *before* doing this
//    }.bind(this);
}


Texture.prototype.init = 
function init(gl, width, height, want_pixels)
{
//    if (this.txtr) return;
    this.gl = gl;
    this.txtr = gl.createTexture();
    this.width = Math.floor(width);
    this.height = Math.floor(height); //UNIV_LEN / VGROUP);
//    this.id = this.txtr._;
//    this.vgroup = vgroup;

//    if ((width != gl.viewportWidth) || (height != gl.viewportHeight))
//        console.log("
debug(10, "cre txtr %dx%d pixels? %s, id %s %j %s".blue_lt, this.width, this.height, want_pixels, typeof this.txtr, this.txtr, this.id);
    gl.activeTexture(gl.TEXTURE0); //redundant?
    gl.bindTexture(gl.TEXTURE_2D, this.txtr);
    glcheck("bind txtr", this.getError());
//    gl.activeTexture(gl.TEXTURE0); //redundant?
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true); //make Y orientation consistent; no worky in X Windows?
    glcheck("flip txtr", this.getError()); //invalid?
//    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, pixels); //do this later
//allow textures that are not power of 2 (don't need mipmaps):
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    glcheck("wrap txtr-1", this.getError());
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    glcheck("wrap txtr-2", this.getError());
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    glcheck("wrap txtr-3", this.getError()); //not valid on RPi?
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    glcheck("wrap txtr-4", this.getError()); //not valid on RPi?
//    gl.bindTexture(gl.TEXTURE_2D, null);
//    if (!want_pixels) return;

    var buf = new ArrayBuffer(this.width * this.height * Uint32Array.BYTES_PER_ELEMENT);
    this.pixels = new Uint32Array(buf);
    this.pixels[0] = 0x11223344; //check byte order
    var ordbuf = new Uint8Array(buf);
console.log("byte order", ordbuf[0].toString(16), ordbuf[1].toString(16), ordbuf[2].toString(16), ordbuf[3].toString(16));
//NOTE: OpenGL ES doesn't support BGR(A)??
    this.fmt = (ordbuf[0] == 0x11)? gl.RGBA: (ordbuf[0] == 0x44)? gl.BGRA: 1/0; //throw error if can't figure out byte order
//this.fmt = gl.RGBA;
    this.pixels.fill(BLACK);
//    this.fmt = gl.RGBA;
//    this.fmt = gl.BGRA;
//GL_RGB, GL_BGR, GL_RGBA, GL_BGRA
    var fmt2 = gl.isGLES? gl.BGRA: gl.RGBA; //NOTE: RPi requires BGRA here
    gl.texImage2D(gl.TEXTURE_2D, 0, fmt2, this.width, this.height, 0, this.fmt, gl.UNSIGNED_BYTE, this.pixels);
    glcheck("img txtr", this.getError()); //invalid op on RPi?
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//

//screen refresh:
//typically 60 Hz (when window is active), but controlled by browser
function rAF(onoff)
{
    if (typeof onoff == "boolean") rAF.isrunning = onoff;
    if (rAF.isrunning) document.requestAnimationFrame(rAF);
//    draw(my_draw.prog || 0);
//    if (rAF.inner) rAF.inner();
    ++rAF.count; //for debug
    Canvas.all.forEach(canvas =>
    {
//        canvas.first();
//        canvas.clear();
//        canvas.gl.uniform1i(canvas.shpgm.uniforms.sampler, 0);

//		glVertexAttribPointer(state.positionLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].x); ERRCHK("flush"); //vVertices);
//		glVertexAttribPointer(state.texCoordLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].s); ERRCHK("flush"); //&vVertices[3]); //(U,V) interleaved with (X,Y,Z)
//		glEnableVertexAttribArray(state.positionLoc); ERRCHK("flush");
//		glEnableVertexAttribArray(state.texCoordLoc); ERRCHK("flush");
//		glUniform1i(state.samplerLoc, 0); ERRCHK("flush"); //set sampler texture unit to 0

        canvas.render(true);
    });

//if (isNaN(++rAF.count)) rAF.count = 0;
//console.log("rAF %s? %s", rAF.count, rAF.isrunning);
}


function heredoc(func)
{
    func = func.toString();
    var parse = func.match(/^[^]*\/\*([^]*)\*\/\}$/m);
    return parse? parse[1]: func;
}


//add line numbers for easier debug:
function showsrc(str, type)
{
//    var ShaderTypes = {};
//    ShaderTypes[this.gl.FRAGMENT_SHADER] = "fragment";
//    ShaderTypes[this.gl.VERTEX_SHADER] = "vertex";
    if (showsrc["seen_" + type]) return; //only show it once
    showsrc["seen_" + type] = true;
    var lines = [];
    str.split("\n").forEach(function(line, inx)
    {
        lines.push(inx + ": " + line); //add line#s for easier debug
    });
    debug("%s shader:\n%s".green_lt, type, lines.join("\n"));
}


//check for OpenGL error:
function glcheck(desc, id)
{
    if (id < 0) debug("bad id: %s".red_lt, desc);
    return id;
}


//function degToRad(degrees)
//{
//    return degrees * Math.PI / 180;
//}


function fllen(count)
{
    return Float32Array.BYTES_PER_ELEMENT * count;
}


function i32len(count)
{
    return Uint32Array.BYTES_PER_ELEMENT * count;
}


function R(color) { return (color >>> 16) & 0xff; }
function G(color) { return (color >>> 8) & 0xff; }
function B(color) { return (color >>> 0) & 0xff; }
function A(color) { return (color >>> 24) & 0xff; }


function clamp(val, minval, maxval)
{
    return Math.min(Math.max(val, minval), maxval);
}


//simulate GPU floating precision (for debug):
//highp = 32-bit float: 1 sign + 23 mantissa + 8 exp
//mediump = 16-bit float: 1 sign + 10 mantissa + 5 exp
//analysis: http://litherum.blogspot.com/2013/04/precision-qualifiers-in-opengl-es.html
//NO WORKY http://stackoverflow.com/questions/9383593/extracting-the-exponent-and-mantissa-of-a-javascript-number
//NO WORKY https://blog.coolmuse.com/2012/06/21/getting-the-exponent-and-mantissa-from-a-javascript-number/
//works: http://www.w3resource.com/php-exercises/php-math-exercise-5.php
function highp(float) { return ieeefloat(float, 23, 126); }
function mediump(float) { return ieeefloat(float, 10, 14); }
function ieeefloat(val, mant_bits, max_exp)
{
    if (typeof val != "number") throw new TypeError("expected a number"); //return {mantissa: -6755399441055744, exponent: 972};
//    var parts = getNumberParts(val);
    var sgn = Math.sign(val);
    var exp = sgn? clamp(Math.floor(Math.log2(sgn * val)), -max_exp, max_exp) + 1: 0;
    var max_mant = Math.pow(2, mant_bits);
    var mant = Math.floor(sgn * val * Math.pow(2, -exp) * max_mant) / max_mant;
    var newval = sgn * mant * Math.pow(2, exp);
//console.log("float %s => %j => %s", val, {sgn, mant, exp}, newval);
    return newval;
}

//eof
