//create graphics GPU canvas:
//works as a wrapper for OpenGL and shaders

//"universe" mapping on RPi pins:
// [0] = R7 = GPIO27 (GEN2)
// [1] = R6 = GPIO26 (absent on GoWhoops board)
// [2] = R5 = GPIO25 (GEN6)
// [3] = R4 = GPIO24 (GEN5)
// [4] = R3 = GPIO23 (GEN4)
// [5] = R2 = GPIO22 (GEN3)
// [6] = R1 = GPIO21
// [7] = R0 = GPIO20
// [8] = G7 = GPIO19 (PWM)
// [9] = G6 = GPIO18 (GEN1)
// [10] = G5 = GPIO17 (GEN0)
// [11] = G4 = GPIO16
// [12] = G3 = GPIO15 (RXD0)
// [13] = G2 = GPIO14 (TXD0)
// [14] = G1 = GPIO13 (PWM)
// [15] = G0 = GPIO12 (PWM)
// [16] = B7 = GPIO11 (SPI_CLK)
// [17] = B6 = GPIO10 (SPI_MOSI)
// [18] = B5 = GPIO09 (SPI_MISO)
// [19] = B4 = GPIO08 (SPI_CE0_N)
// [20] = B3 = GPIO07 (SPI_CE1_N)
// [21] = B2 = GPIO06
// [22] = B1 = GPIO05
// [23] = B0 = GPIO04 (GPIO_GCLK)
//---------------------------------
//    H SYNC = GPIO03 (SCL1, I2C)
//    V SYNC = GPIO02 (SDA1, I2C)
//        DE = ID_SD (I2C ID EEPROM)
//     PXCLK = ID_SC (I2C ID EEPROM)


'use strict'; //find bugs easier
require('colors'); //for console output
const fs = require('fs');
const path = require('path');
const WebGL = require('node-webgl'); //NOTE: for RPi must be lukaaash/node-webgl or djulien/node-webgl version, not mikeseven/node-webgl version
const {mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');
//const Preproc = require("preprocessor");
const {onexit} = require('blocking-style');
const cpp = require("./cpp-1.0").create(); //https://github.com/acgessler/cpp.js
//require('buffer').INSPECT_MAX_BYTES = 1600;

const {debug} = require('./debug');
const {Screen} = require('./screen');
const {caller, calledfrom} = require("./caller");

//compatibility with browser env (from node-webgl examples):
const document = WebGL.document();
const Image = WebGL.Image;
const alert = console.error;

const BLACK = 0xff000000; //NOTE: needs alpha to take effect (ARGB fmt)

//default debug control info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const DEFAULT_OPTS =
{
//    DEBUG: false,
    SHOW_SHSRC: false, //show shader source code
    SHOW_VERTEX: false, //show vertex info (corners)
    SHOW_LIMITS: false, //show various GLES/GLSL limits
    SHOW_PROGRESS: false, //show progress bar at bottom of screen
    WS281X_FMT: false, //force WS281X formatting on screen (when dpi24 overlay is *not* loaded)
    WS281X_DEBUG: false, //show timing debug info
    gpufx: null, //generate fx on GPU instead of CPU
    VERTEX_SHADER: path.resolve(__dirname, "vertex.glsl"),
    FRAGMENT_SHADER: path.resolve(__dirname, "fragment.glsl"),
};
//const VGROUP = 42; //enlarge display nodes for debug/demo; overridden when dpi24 overlay is loaded
//const OUTMODE = 1; //Screen.gpio? 1: -1;


///////////////////////////////////////////////////////////////////////////////
////
/// Main logic
//

//var gl, isGLES;
//var all = [];

const GpuCanvas =
module.exports.GpuCanvas =
class GpuCanvas
{
    constructor(title, w, h, opts) //, vgroup)
    {
//        this.opts = opts || {};
        this.size =
        {
//    if (!Screen.gpio /*&& this.WS281X_DEBUG*/)
            get hoverscan() { return Screen.horiz.res / Screen.horiz.disp; }, //need overscan to align with WS281X T0L
            get scrw() { return Screen.width * this.hoverscan; }, //NOTE: always use h res for consistent results; preserve/calculate horiz overscan in case config file not active
            get scrh() { return Screen.height; }, //screen height used as-is; don't care about vert overscan
//TODO: use floor or ceil?
            get vtxw() { return this.scrw / w; },
            get vtxh() { return this.scrh / h; }, //screen height is always used, so vgroup can be reconstructed from h parameter
        };
        var gpufx = opts.gpufx && path.resolve(path.dirname(calledfrom(2)), opts.gpufx); //convert relative to abs path wrt caller
        pushable.call(this, Object.assign(DEFAULT_OPTS, opts || {}, gpufx && {gpufx}));
        if (Screen.gpio) this.WS281X_FMT = true; //can't see debug info, so turn on formatting
//        if (gl) return; //only need one
//        console.log("canvas: '%s' %d x %d", title, w, h);
//        console.log("canvas opts: fmt %j", this.WS281X_FMT);
//        console.log("canvas opts: limits %j", this.SHOW_LIMITS);
        initGL.call(this);
        document.setTitle(title || "WS281X demo");
//        initTexture(w, h);
//??        h = Math.max(w, h); //kludge: make width <= height to simplify overlap logic in shader
        this.txr = new Texture(this.gl, Math.ceil(w), Math.ceil(h)); //round up; can't have fractional pixels
        glcheck("init txtr", this.getError());
        initShaders.call(this);
        initBuffers.call(this);
        initProjection.call(this);
        if (!GpuCanvas.all) GpuCanvas.all = [];
//        GpuCanvas.all.push(this);
//        if (isNaN(++all.uniqid)) all.uniqid = 1;
//        this.id = all.uniqid;
        GpuCanvas.all.push(this);
        if (GpuCanvas.all.length == 1) rAF(true); //start screen refresh loop
        else debug("possible canvas conflict: %d".red_lt, GpuCanvas.all.length);
//    gl.enable(gl.DEPTH_TEST);
//    gl.clearColor(0.0, 0.0, 0.0, 1.0); //start out blank
        onexit(this.destroy.bind(this)); //stop rAF loop for clean exit
    }

    destroy() //dtor
    {
        var inx = GpuCanvas.all.indexOf(this); //findIndex(function(that) { return that.id == this.id; }.bind(this)); //all.indexOf(this);
        debug("destroy: found@ %d, #canv %d, del? %s".yellow_lt, inx, GpuCanvas.all.length, inx != -1);
        if (inx != -1) GpuCanvas.all.splice(inx, 1);
        if (!GpuCanvas.all.length) rAF(false); //cancel screen refresh loop
    }

    getError() { var err = this.gl.getError(); return err? -1: err; }

//elapsed, duration, interval can be used for progress bar or to drive animation
    get elapsed() { return this.prior_elapsed || 0; }
    set elapsed(newval)
    {
//console.log("elapsed", newval);
        var new_filter = this.interval? Math.floor(newval / this.duration / this.interval): newval / this.duration;
        if (new_filter == this.elapsed_filter) return;
        debug(10, "elapsed: was %d, is now %d / %d, #rAF %d".blue_lt, micro(this.prior_elapsed), micro(newval), this.duration, rAF.count); //, new_filter);
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


GpuCanvas.prototype.clear = 
function clear(color)
{
    var gl = this.gl;
//        txtr.xparent(BLACK);
    if (arguments.length && (color !== true)) gl.clearColor(R(color), G(color), B(color), A(color)); //color to clear buf to; clamped
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT); //uses clearColor
//    return this; //fluent
}


GpuCanvas.prototype.render = 
function render(want_clear)
{
//    if (!this.txr.dirty) return; //NOTE: RPi needs refresh each time
    var gl = this.gl;
    if (arguments.length && (want_clear !== false)) this.clear(want_clear);
//NOTE: need to do this each time:
    gl.drawArrays(gl.POINTS, 0, this.vxbuf.numItems); //squareVertexPositionBuffer.numItems);
    this.txr.render();
    return (glcheck("render", this.getError()) >= 0);
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
//GpuCanvas.prototype.first = 
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


//GpuCanvas.prototype.initGL = 
function initGL() //canvas)
{
    try
    {
        var canvas = document.createElement("ws281x-canvas"); //NOTE: name needs to include "canvas" for node-webgl to work
        var gl = this.gl = canvas.getContext("experimental-webgl");
        gl.isGLES = (gl.getParameter(gl.SHADING_LANGUAGE_VERSION).indexOf("GLSL ES") != -1); //check desktop (non-RPi) vs. embedded (RPi) version

        gl.viewportWidth = canvas.width;
        gl.viewportHeight = canvas.height;
        gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
        glcheck("init-1", this.getError());
//NOTE: both of the following are needed for gl_PointCoord; https://www.opengl.org/discussion_boards/showthread.php/182248-understanding-gl_PointCoord-(always-0)
//console.log(gl.POINT_SPRITE, gl.PROGRAM_POINT_SIZE);
        if (!gl.isGLES) gl.enable(gl.POINT_SPRITE); //not valid/needed on RPi?
        glcheck("init-2", this.getError());
        if (!gl.isGLES) gl.enable(gl.PROGRAM_POINT_SIZE); //not valid/needed on RPi?
        glcheck("init-3", this.getError());
        gl.enable(gl.DEPTH_TEST);
        glcheck("init-4", this.getError());
        gl.clearColor(R(BLACK), G(BLACK), B(BLACK), A(BLACK)); //color to clear buf to; clamped
        glcheck("init-5", this.getError());

        if (!this.SHOW_LIMITS) return;
        debug(10, "canvas: w %s, h %s, viewport: w %s, h %s".blue_lt, canvas.width, canvas.height, gl.viewportWidth, gl.viewportHeight);
        debug(10, "glsl ver# %s, is GL ES? %s".blue_lt, gl.getParameter(gl.SHADING_LANGUAGE_VERSION), gl.isGLES);
//        debug(10, "glsl exts".blue_lt, gl.getParameter(gl.EXTENSIONS));
        debug(10, "medium float prec/range: vsh %j, fsh %j".blue_lt, gl.getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.MEDIUM_FLOAT), gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.MEDIUM_FLOAT)); //[-126,127], -23 on RPi
        debug(10, "medium int precision/range: vsh %j, fsh %j".blue_lt, gl.getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.MEDIUM_INT), gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.MEDIUM_INT)); //[-126,127], 0 on RPi
        debug(10, "high float prec/range: vsh %j, fsh %j".blue_lt, gl.getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.HIGH_FLOAT), gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT)); //[-126,127], -23 on RPi
        debug(10, "high int precision/range: vsh %j, fsh %j".blue_lt, gl.getShaderPrecisionFormat(gl.VERTEX_SHADER, gl.HIGH_INT), gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_INT)); //[-126,127], 0 on RPi
        debug(10, "max viewport dims: %s".blue_lt, gl.getParameter(gl.MAX_VIEWPORT_DIMS)); //2K,2K on RPi
        debug(10, "max #textures: vertex %d, fragment %d, combined %d".blue_lt, gl.getParameter(gl.MAX_VERTEX_TEXTURE_IMAGE_UNITS), gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS), gl.getParameter(gl.MAX_COMBINED_TEXTURE_IMAGE_UNITS)); //8 on RPi
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


//var shpgm;
//const vertex_sh = require("./vertex.glsl");
//const fragment_sh = require("./fragment.glsl");


//GpuCanvas.prototype.initShaders = 
function initShaders() //vzoom)
{
    var gl = this.gl;
//    if (!initShaders.ShaderTypes)
//    {
//        const ist = initShaders.ShaderTypes = {};
//        ist[gl.FRAGMENT_SHADER] = "fragment";
//        ist[gl.VERTEX_SHADER] = "vertex";
        const ist = initShaders.ShaderTypes = //NOTE: var keys requires ES6
        {
            [gl.FRAGMENT_SHADER]: "fragment",
            [gl.VERTEX_SHADER]: "vertex",
        };
//        ist[gl.FRAGMENT_SHADER] = "fragment";
//        ist[gl.VERTEX_SHADER] = "vertex";
//    }
    var vertexShader = getShader.call(this, gl.VERTEX_SHADER, this.VERTEX_SHADER); //vertex_sh); //"shader-vs");
    var fragmentShader = getShader.call(this, gl.FRAGMENT_SHADER, this.FRAGMENT_SHADER); //fragment_sh); //"shader-fs");

    var shpgm = this.shpgm = glcheck("shpgm", gl.createProgram());
    gl.attachShader(shpgm, vertexShader);
    gl.attachShader(shpgm, fragmentShader);
    gl.linkProgram(shpgm);

    if (!gl.getProgramParameter(shpgm, gl.LINK_STATUS))
    {
        var reason = "";
        if (gl.getProgramParameter(shpgm, gl.INFO_LOG_LENGTH) > 1)
            reason = "; reason: " + gl.getProgramInfoLog(shpgm);
        if (!this.SHOW_SHSRC)
        {
            showsrc(null, ist[gl.VERTEX_SHADER], true);
            showsrc(null, ist[gl.FRAGMENT_SHADER], true);
        }
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
    shpgm.uniforms.WS281X_FMT = glcheck("WS281X_FMT", gl.gUL(shpgm, "WS281X_FMT"));
    debug(10, "WS281X_fmt: %s".blue_lt, this.WS281X_FMT);
    if (this.WS281X_FMT) this.WS281X_FMT = this.WS281X_FMT; //kludge: set this *after* shaders and uniforms are created

//    shpgm.textureCoordAttribute = gl.getAttribLocation(shpgm, "aTextureCoord");
//    gl.enableVertexAttribArray(shpgm.textureCoordAttribute);
    gl.uniform1i(shpgm.uniforms.sampler, 0); //this.txr.id); //won't change, so set it now
    glcheck("sampler", this.getError()); //invalid?
}


//GpuCanvas.prototype.getShader = 
function getShader(type, filename) //str)
{
    var gl = this.gl;
//prepend common defs:
//    str = heredoc(preamble) + "\n" + heredoc(str);
    var str = fs.readFileSync(filename).toString();
//kludge: cpp-js #includes are async, so expand them here synchronously:
    str = str.replace(/^\s*#include\s+"([^"]+)"\s*.*$/m, function(matching, sub1, ofs, entire)
    {
//console.log("TODO: repl '%s' with contents".red_lt, path.resolve(__dirname, sub1));
        var lines = entire.substr(0, ofs).split("\n");
        return "//" + matching + "\n" + fs.readFileSync(path.resolve(__dirname, sub1)) + "\n#line " + lines.length + "\n";
    });
    
//    if (this.gpufx) //insert GPU fx
//console.log("pl holder", str.indexOf("placeholder"), heredoc(this.gpufx));
//        str = str.replace(/(placeholder for gpufx.*?)/, "$1\n" + heredoc(this.gpufx));
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
//    if (this.SHOW_PROGRESS) str = "#define PROGRESS_BAR\n" + str;
    if (gl.isGLES) cpp.define("GL_ES", "");
//    if (Screen.gpio || this.WS281X_FMT) cpp.define("WS281X_FMT", ""); //allow this one to change at run-time (for test/debug)
    if (!Screen.gpio && this.WS281X_DEBUG) cpp.define("WS281X_DEBUG", "");
    if (!Screen.gpio && this.SHOW_PROGRESS) cpp.define("PROGRESS_BAR", "");
    cpp.define("CALLER_SCR_WIDTH", this.size.scrw); //Screen.width); //(Screen.gpio? Screen.horiz.res: scrw)); //NOTE: use h res here; need overscan to align with WS281X T0L
    cpp.define("CALLER_SCR_HEIGHT", this.size.scrh); //(Screen.gpio? Screen.vert.disp: Screen.height));
    cpp.define("CALLER_VERTEX_WIDTH", this.size.vtxw); //gl.viewportWidth);
    cpp.define("CALLER_VERTEX_HEIGHT", this.size.vtxh); //gl.viewportHeight);
    if (this.gpufx && (type == gl.VERTEX_SHADER)) //insert custom GPU fx
    {
        cpp.define("CUSTOM_GPUFX", "");
        str += "\n#line 1\n" + fs.readFileSync(this.gpufx);
    }
    str = str.replace(/^(#line.*?)$/mg, "//$1"); //kludge: cpp-js doesn't like #line, so comment it out
    str = cpp.run(str);
    cpp.clear();
//(?<=a)b  //look-behind no worky :(
//NOTE: using non-greedy regexp to match first "??" only
//    var scrw = Screen.width;
//    if (!Screen.gpio /*&& this.WS281X_DEBUG*/) 
//    scrw *= Screen.horiz.res / Screen.horiz.disp; //kludge: simulate h overscan
//    str = str.replace(/(\WSCR_WIDTH\s*=.*?)\?\?/, "$1" + this.size.scrw); //Screen.width); //(Screen.gpio? Screen.horiz.res: scrw)); //NOTE: use h res here; need overscan to align with WS281X T0L
//    str = str.replace(/(\WSCR_HEIGHT\s*=.*?)\?\?/, "$1" + this.size.scrh); //(Screen.gpio? Screen.vert.disp: Screen.height));
//    str = str.replace(/(\W)univ_len(\W)/g, "$1" + this.height + "$2");
//    str = str.replace(/(\WNUM_UNIV = .*)\?\?(.*)$/, "$1" + gl.viewportWidth + "$2");
//    str = str.replace(/(\WNUM_UNIV\s*=.*?)\?\?/, "$1" + this.width); //gl.viewportWidth);
//    str = str.replace(/(\WUNIV_LEN\s*=.*?)\?\?/, "$1" + this.height); //gl.viewportHeight);
//    str = str.replace(/(\WNUM_UNIV\s*=.*?)\?\?/, "$1" + this.width); //gl.viewportWidth);
//    str = str.replace(/(\WUNIV_LEN\s*=.*?)\?\?/, "$1" + this.height); //gl.viewportHeight);
//    var defs = getShader.defs || //info for debug display
//    {
//        shown: 0,
//        SCR_WIDTH: micro(scrw), //1536
//        SCR_HEIGHT: Screen.height, //1104
//        NUM_UNIV: this.width, //24
//        UNIV_LEN: this.height, //1104 for GPIO or 24 for screen
//    };
//    defs.VERTEX_WIDTH = Math.floor(defs.SCR_WIDTH / defs.NUM_UNIV); //64
//    defs.VERTEX_HEIGHT = Math.floor(defs.SCR_HEIGHT / defs.UNIV_LEN); //1 or 46
//    defs.VERTEX_SIZE = Math.max(defs.VERTEX_WIDTH, defs.VERTEX_HEIGHT); //on-screen vertex size
//    defs.NODE_BLANK = micro((defs.VERTEX_SIZE - defs.VERTEX_HEIGHT) / defs.VERTEX_SIZE); //portion of vertex height to leave empty
//    if (defs.shown++ < 1) debug("vertex defs used by shaders: %j".cyan_lt, defs); //show values shaders will use (for debug)
//    getShader.defs = defs;
//    str = str.replace(/(\WVERTEX_WIDTH\s*=.*?)\?\?/, "$1" + this.size.vtxw); //gl.viewportWidth);
//    str = str.replace(/(\WVERTEX_HEIGHT\s*=.*?)\?\?/, "$1" + this.size.vtxh); //gl.viewportHeight);
    getShader[initShaders.ShaderTypes[type] + "_src"] = str; //save src in case error shows up later
    if (this.SHOW_SHSRC) showsrc(str, initShaders.ShaderTypes[type]);
//process.exit(0);

    var shader = glcheck("shader", gl.createShader(type)); //gl.FRAGMENT_SHADER);
    gl.shaderSource(shader, str);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
    {
        if (!this.SHOW_SHSRC) showsrc(str, initShaders.ShaderTypes[type], true);
        alert(gl.getShaderInfoLog(shader));
        return null;
    }
    return shader;
}


//add push/pop method for listed properties:
function pushable(props)
{
//    console.log("make pushable: %j", props);
/*
//http://stackoverflow.com/questions/3112793/how-can-i-define-a-default-getter-and-setter-using-ecmascript-5
    this._opts = new Proxy(this,
    {
        get: function(obj, name)
        {
            console.log("get '%s' from stack of %d", name, );
            get: function() { return prop[prop.length - 1]; },
        return target[name];
    },
    set: function(obj, name, value) {
        console.log("you're setting property " + name);
        target[name] = value;
    }
    });
*/
    this._pushable = {};
//use push, pop namespace to avoid conflict with getters, setters:
    this.push = {};
    this.pop = {};
//CAUTION: use let + const to force name + prop to be unique each time; see http://stackoverflow.com/questions/750486/javascript-closure-inside-loops-simple-practical-example
    for (let name in props)
    {
        const prop = this._pushable[name] = [props[name]]; //create stack with initial value
//set up getters, setters:
//ignore underflow; let caller get error
        Object.defineProperty(this, name,
        {
            get: function() { /*console.log("top of '%s' %j is %j %s", name, prop, prop.top, caller(-2))*/; return prop.top; }.bind(this), //[prop.length - 1]; },
            set: function(newval) { prop.top = newval; update.call(this, name); /*console.log("top of '%s' %j now is %j @%s", name, prop, prop.top, caller(-2))*/; }.bind(this), //[prop.length - 1] = newval; },
        });
//set up push/pop:
        this.push[name] = function(newval) { /*console.log("push '%s' %j", name, newval)*/; prop.push(newval); update.call(this, name); }.bind(this);
        this.pop[name] = function() { /*console.log("pop '%s'", name)*/; var retval = prop.pop(); update.call(this, name); return retval; }.bind(this);
//        prop = null;
    }
}


//update a uniform:
function update(name)
{
//console.log("update: shpgm %j", this.shpgm);
    if (!this.shpgm || !this.shpgm.uniforms || !this.shpgm.uniforms[name]) return;
    this.gl.uniform1i(this.shpgm.uniforms[name], this[name]);
    if (name == "WS281X_FMT") this.gl.texPivot24(!this.gl.isGLES && this[name]); //.txr[name] = this[name]; //kludge: texture needs to know whether to pivot RGB bits
}


///////////////////////////////////////////////////////////////////////////////
////
/// Vertex buffers (only sent to GPU once)
//

//var vxbuf; //, ixbuf;

//GpuCanvas.prototype.initBuffers = 
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
//    var txtw = this.width;
//    if (!Screen.gpio /*&& this.WS281X_DEBUG*/)
//    txtw *= Screen.horiz.disp / Screen.horiz.res; //kludge: simulate h overscan (inverted)
//NOTE: use correct coordinates here; don't overscan, for simpler logic when displaying to screen
//console.log("w %s, txtw %s", this.width, txtw);
    var numvert = this.width * this.height;
//    var vtxw = Screen.horiz.res / this.width, vtxh = Screen.vert.disp / this.height;
//    var xofs = (vtxw < vtxh)? 0: 0.5, yofs = (vtxw < vtxh)? 0: 0;
    var xofs = 0, yofs = 0;
//console.log("vertex shift: w %s, h %s, xofs %s, yofs %s", micro(vtxw), micro(vtxh), xofs, yofs);
        for (var y = 0; y < this.height; ++y)
    for (var x = 0; x < this.width; ++x)
        {
//hw (univ#, node#, model#) is for whole-house model
//TODO: model (x, y, w, h) is for single-prop models
//??            vertices.push((x + 0.5) / this.width, 1 - (y + 0.5) / this.height, 0,   x, this.height - y - 1, x,   0, 0, 0, 0);
//            vertices.push((x + 0.5) / this.width, (y + 0.5) / this.height, 0,   x, this.height - y - 1, x,   0, 0, 0, 0);
//NOTE: first pixel is (0,0), last is (w-1,h-1) in vertex array
//put pixel (0,0) of texture at top of screen so caller can index starting at 0 and still work with incorrect size info
//NOTE: node# is flipped so that node# 0 is closest to GPU, but XY (0,0) is bottom left corner of screen so screen coordinates (X, Y) are oriented differently from texture coordinates (S, T)
//            vertices.push((x + 0.5) / txtw, 1 - (y + 0.5) / this.height, 0,   x, y, x,   0, 0, 0, 0);
//            vertices.push(x / txtw, 1 - y / this.height, 0,   x, y, x,   0, 0, 0, 0);
//NOTE: the right-most pixel needs to be clipped, so use lower left coordinate for each vertex rather than center
//            vertices.push(x, this.height - y - 1, 0,   x, y, x,   0, 0, 0, 0);
//NOTE: viewport inverts y so 0 is at top; x-TODO: not sure why x needs 1/2 pixel offset here
//no-NOTE: use pixel centers for easier paint logic; CAUTION: right-most vertex is on edge, do not clip
//set Z to force earlier pixels to overlap later pixels; needed due to overlapping pixels
//not sure why must use centers here
            vertices.push(x + xofs, y + yofs, (vertices.length + 1) / numvert / 10,   x, y, x,   0, 0, 0, 0);
            if (!this.SHOW_VERTEX) continue;
            if ((x >= 2) && (x < this.width - 2)) continue;
            if ((y >= 2) && (y < this.height - 2)) continue;
            debug("vert[%s, %s]: xyz %j,  hw %j,  model %j".cyan, x, y, micro(vertices.slice(-10, -7)), vertices.slice(-7, -4), vertices.slice(-4));
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
/// Projection matrices (can be updated, but demo doesn't need to)
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

//GpuCanvas.prototype.setMatrixUniforms = 
//function setMatrixUniforms()
function initProjection()
{
//    this.mvMatrix = mat4.create();
    this.pMatrix = mat4.create();
//    mat4.perspective(45, gl.viewportWidth / gl.viewportHeight, 0.1, 100.0, pMatrix);
//    mat4.ortho(0, gl.viewportWidth, 0, gl.viewportHeight, 0.1, 100.0, pMatrix); // l,r,b,t,n,f
//        mat4.ortho(0 + 1 - ZOOM, 1 * ZOOM, 0 + 1 - ZOOM, 1 * ZOOM, 0.1, 100.0, pMatrix); // left, right, bottom, top, near, far
//    var overscan = 1; //Screen.horiz.res / Screen.horiz.disp; //1; //DON'T use projection to force h overscan so other code doesn't need to deal with it
//    mat4.ortho(0, overscan, 0, 1, 0.1, 100.0, this.pMatrix); //project x, y as-is; left, right, bottom, top, near, far
//reduce viewport width to clip right-most pixel:
//    var overscan = Screen.horiz.res / Screen.horiz.disp; //overscan factor; //use viewport + projection to force h overscan so other code doesn't need to deal with it
//NOTE: viewport is inverted here to put 0 at top; this allows caller to still work if max# pixels is configured incorrectly
//NOTE: set viewport narrower to force overscan on right-most pixel; put center at right-most edge
//viewport tuning: first set vertex coordinates + z-order, then viewport boundaries; want left, top + bottom edges to align, and right edge to clip 1/2 pixel
//pixels are drawn as rectangular areas; width != height so one of the axes overlaps
//    mat4.ortho(0, this.width / overscan, this.height, 0, 0.1, 100.0, this.pMatrix); //project x, y as-is; left, right, bottom, top, near, far
//    var vtxw = Screen.horiz.res / this.width, vtxh = Screen.vert.disp / this.height;
//    var xofs = (vtxw < vtxh)? -1: 0, yofs = (vtxw < vtxh)? 0: 0;
    var margins = (this.size.vtxw < this.size.vtxh)? {L: -1, R: -0.5, T: -1, B: -0.5}: {L: -0.5, R: -0.5, T: -1, B: -1}; //viewport tuning
console.log("viewport adjust: vertex w %s, h %s -> margins %j", micro(this.size.vtxw), micro(this.size.vtxh), margins);
    mat4.ortho(margins.L, this.width - 0.5 + margins.R, this.height + margins.B, margins.T, 0, 100, this.pMatrix); //project x, y as-is; left, right, bottom, top, near, far
//    debug("viewport projection: overscan %s vs. %s".cyan_lt, this.width - 0.5, micro(this.width / overscan));  //overscan should be 3/4 pixel

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
/// Texture handling (1 vertex + textel for each LED pixel); updated frequently by caller if fx generated by CPU
//

//var txtr = {};
//function initTexture(w, h) { txtr.init(w, h, true); }

//byte splitter:
const splitter =
{
    LE: false, //true, //RPi is bi-endian (running as little endian); Intel is little endian; seems backwards
    buf: new ArrayBuffer(4),
//    get buf() { return this.bytes; },
//no worky    uint32: new Uint32Array(this.bytes), //DataView(bytes),
//    view: new DataView(this.buf), //this.bytes),
//    read: function() { return this.view.getUint32(0, false); },
//    write: function(val) { this.view.setUint32(0, val, false); },
//    bytes: new Uint8Array(this.buf),
    get view() { Object.defineProperty(this, "view", {value: new DataView(this.buf)}); return this.view; }, //replace getter with data after first time
    get bytes() { Object.defineProperty(this, "bytes", {value: new Uint8Array(this.buf)}); return this.bytes; }, //replace getter with data after first time
//    uint32: new Uint32Array(this.buf), //always little endian; see http://stackoverflow.com/questions/7869752/javascript-typed-arrays-and-endianness
    get uint32() { return this.view.getUint32(0, this.LE); },
    set uint32(val) { this.view.setUint32(0, val, this.LE); },
};
//splitter.bytes = new ArrayBuffer(4);
//splitter.view = new DataView(splitter.bytes);
//splitter.uint32 = new Uint32Array(splitter.bytes); //DataView(bytes);
//splitter.read = function() { return this.view.getUint32(0, false); };
//splitter.write = function(val) { this.view.setUint32(0, val, false); };
//splitter.bytes = new Uint8Array(splitter.buf);
//splitter.uint32 = new Uint32Array(splitter.buf);
//splitter.view = new DataView(splitter.buf);
//splitter.LE = false; //true; //RPi is bi-endian (running as little endian); Intel is little endian; seems backwards

//http://stackoverflow.com/questions/15761790/convert-a-32bit-integer-into-4-bytes-of-data-in-javascript
//function toBytesInt32 (num) {
//    arr = new ArrayBuffer(4); // an Int32 takes 4 bytes
//    view = new DataView(arr);
//    view.setUint32(0, num, false); // byteOffset = 0; litteEndian = false
//    return arr;
//}


class Texture
{
    constructor(gl, w, h, grp)
    {
        this.init(gl, w, h, grp, true);
    }

    getError() { var err = this.gl.getError(); return err? -1: err; }
//    set WS281X_FMT(val) { this.gl.texPivot(this.gl.isGLES && val); } //this.WS281X_FMT); //GPU can do pivot or not needed
};


//L-to-R, T-to-B fill order:
//NOTE: this gives locality of reference to horizontal neighbors (improves cache performance)
//NOTE: this puts (0,0) at the start of the array
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
//NOTE: first element = lower left, last element = upper right
    gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, this.width, this.height, this.fmt, gl.UNSIGNED_BYTE, this.pixel_bytes);
    glcheck("sub img", this.getError());  //invalid value on RPi?
//    this.debug_tx("render");
    this.dirty = false;
//    return this; //fluent
}


Texture.prototype.debug_tx = 
function debug_tx(desc)
{
    var buf = "";
    var uint32s = new Uint32Array(this.pixel_bytes.buffer);
    for (var y = 0, ofs = 0, seen = -1; y < this.height; ++y)
        for (var x = 0; x < this.width; ++x, ++ofs)
        {
//            var argb = rdpixel.call(this, ofs);
//            if (argb == BLACK) continue;
            var argb = uint32s[ofs];
            buf += ", " + ((seen < y)? "[" + y + "/" + this.height + "]: ": "") + argb.toString(16);
            seen = y;
        }
    debug(10, "%s: %j".blue_lt, desc, buf.substr(2, 450) + ((buf.length > 450)? "...": ""));
}
//var buf = "";
//for (var i = 0; i < 16; ++i)
//    buf += " " + (this.pixel_bytes[i] + 0).toString(16);


Texture.prototype.fill = 
function fill(xofs, yofs, w, h, argb)
{
//    this.init(true);
    if (arguments.length == 1) { argb = xofs; xofs = yofs = 0; w = this.width; h = this.height; }
//rgba = (rgba & 0xFF00FF00) | R(color) | (B(color) << 16); //ABGR <-> ARGB
//rgba = 0xff0000ff;
    debug(10, "fill 0x%s".blue_lt, argb.toString(16));
//clip:
    if (xofs < 0) xofs = 0;
    if (xofs + w > this.width) w = this.width - xofs;
    if (yofs < 0) yofs = 0;
    if (yofs + h > this.height) h = this.height - yofs;
//TODO: fill first row and then array copy the others
//    for (var y = yofs, ofs = 0; y < yofs + h; ++y)
//        for (var x = xofs; x < xofs + w; ++x, ++ofs)
    for (var y = 0, ofs = this.xy(xofs, yofs); y < h; ++y, ofs += this.width)
        for (var x = 0; x < w; ++x)
//            this.pixels[ofs + x] = argb;
            this.wrpixel(ofs + x, argb);
//    this.dirty = true; //just assume it changed
//    txtr.image.fill.apply(null, arguments);
//    this.debug_tx("after fill");
    return this; //fluent
}


/* TODO: use SDL2-gfx
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
function pixel(x, y, argb)
{
//    this.init(true);
    if ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height))
        return (arguments.length > 2)? 0: this; //fluent setter only, not getter
//    var ofs = x + y * this.width;
    if (arguments.length > 2) //set color
    {
//        if (this.pixels[ofs] != argb)
//        {
//console.log("pixel(%d,%d): ofs %d, set color 0x%s", x, y, this.xy(x, y), argb.toString(16));
//            this.pixels[this.xy(x, y)] = argb; //>>> 0;
            this.wrpixel(this.xy(x, y), argb);
//            this.dirty = true; //just assume it changed
//        }
        return this; //fluent setter
    }
//    return this.pixels[this.xy(x, y)]; //non-fluent getter
    return this.rdpixel(this.xy(x, y)); //non-fluent getter
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
//if (!this.seen[color])
//    console.log("pixel[%d,%d] = %s => %s", x, y, this.img_pixels.readUInt32BE(ofs).toString(16), color.toString(16));
//this.seen[color] = true;
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
//NOTE: assumes order matches xy()
            for (var y = 0, ofs = 0; y < this.image.height; ++y)
                for (var x = 0; x < this.image.width; ++x, ++ofs)
                {
                    var argb = this.image.pixel(x, this.image.height - y - 1); //flip y
//                    this.pixels[this.xy(x, y)] = argb;
//                    wrpixel.call(this, this.xy(x, y), argb);
                    this.wrpixel(ofs, argb);
                }
//TODO: array copy
            this.image.img_pixels = null;
//            this.dirty = true;
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
    this.WS281X_FMT = 
    this.txtr = gl.createTexture();
    this.width = Math.floor(width);
    this.height = Math.floor(height); //UNIV_LEN / VGROUP);
//    this.id = this.txtr._;
//    this.vgroup = vgroup;

//    if ((width != gl.viewportWidth) || (height != gl.viewportHeight))
//        console.log("
    debug(10, "cre txtr %dx%d pixels? %s, id %s".blue_lt, this.width, this.height, !!want_pixels, this.txtr._); //typeof this.txtr, this.txtr, this.txtr._);
    gl.activeTexture(gl.TEXTURE0); //redundant?
    gl.bindTexture(gl.TEXTURE_2D, this.txtr);
    glcheck("bind txtr", this.getError());
//    gl.activeTexture(gl.TEXTURE0); //redundant?
    if (gl.isGLES) gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true); //make Y orientation consistent; no worky in X Windows?
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

//    splitter.LE = this.isGL_ES; //RPi is bi-endian; Intel is little endian
//    var buf = new ArrayBuffer(this.width * this.height * Uint32Array.BYTES_PER_ELEMENT);
//    var uint32s = new Uint32Array(buf); //this.pixel_uint32s =
//console.log(typeof buf, typeof uint32s);
//console.log(uint32s);
    this.pixel_bytes = new Uint8Array(this.width * this.height * Uint32Array.BYTES_PER_ELEMENT);
//byte shuffling to reduce GPU texture lookups:
//NOTE: RPi (GLES) wants BGRA, R <-> B
    this.rdpixel = rdpixel_RPi.bind(this); //(this.gl.isGLES? rdpixel_GLES: rdpixel_GL).bind(this); //rdpixel_RPi;
    this.wrpixel = wrpixel_RPi.bind(this); //(this.gl.isGLES? wrpixel_GLES: wrpixel_GL).bind(this); //wrpixel_RPi;
//    this.pivot24 = this.gl.isGLES? pivot24_GLES: pivot24_GL;
//RPi GPU doesn't have enough bandwidth for a 24x24 bit pivot, so use CPU instead
//NOTE: this puts a greater load on RPi CPU and probably requires a background worker thread
//    if (!this.gl.isGLES) this.gl.texPivot24(1); //tell CPU to do 24x24 pivot to offload GPU
//    this.gl.texPivot(this.isGLES && this.WS281X_FMT); //GPU can do pivot or not needed
//console.log(typeof this.pixel_bytes, typeof this.pixel_bytes.buffer);
//    uint32s[0] = 0x11223344; //check byte order
//    splitter.uint32[0] = 0x11223344; //check byte order
//splitter.LE = false;
//    for (var retry in [false, true, -1])
//    {
//        if (retry == -1) throw "can't figure out byte order";
    this.wrpixel(0, 0x11223344); //check byte order
    debug(10, "byte order[%s]: %s %s %s %s".blue_lt, 0, (this.pixel_bytes[0] + 0).toString(16), (this.pixel_bytes[4] + 0).toString(16), (this.pixel_bytes[8] + 0).toString(16), (this.pixel_bytes[12] + 0).toString(16));
//        if (this.pixel_bytes[0] == 0x22) break; //that's where we want it
//        splitter.LE = !splitter.LE;
//    }
//    var ordbuf = new Uint8Array(this.pixel_bytes);
//NOTE: OpenGL ES doesn't support BGR(A)??
//    if ((this.pixel_bytes[0] != 0x22) && (this.pixel_bytes[0] != 0x33)) throw "can't figure out byte order";
//    this.fmt = (this.pixel_bytes[0] == 0x22)? gl.RGBA: (this.pixel_bytes[0] == 0x33)? gl.BGRA: 1/0; //throw error if can't figure out byte order
//this.fmt = gl.RGBA;
//    this.pixels.fill(BLACK);
    this.fill(BLACK); //shader will receive valid texture coordinates without a texture, but send it first time anyway
//    this.fmt = gl.RGBA;
//    this.fmt = gl.BGRA;
//GL_RGB, GL_BGR, GL_RGBA, GL_BGRA
    var fmt2 = gl.isGLES? gl.BGRA: gl.RGBA; //NOTE: RPi requires BGRA here
    this.fmt = fmt2;
    const FMTS = {}; FMTS[gl.BGRA] = "BGRA"; FMTS[gl.RGBA] = "RGBA";
    debug(10, "fmts: %s %s".blue_lt, FMTS[this.fmt], FMTS[fmt2]);
//this.fmt = fmt2;
//NOTE: first element = lower left, last element = upper right
//NOTE: pivot flag not set yet, but texture should be updated by caller before first render anyway
    gl.texImage2D(gl.TEXTURE_2D, 0, fmt2, this.width, this.height, 0, this.fmt, gl.UNSIGNED_BYTE, this.pixel_bytes);
    glcheck("img txtr", this.getError()); //invalid op on RPi?
}


//read/write pixel ARGB value:
//NOTE: RPi GPU has limited memory bandwidth (only allows 8 texture lookups)
//in order to access 24 values, ARGB bytes are rearranged here so one lookup gets 4 pixels
function rdpixel_RPi(ofs)
{
    if (this.gl.isGLES && !(ofs & 1)) ofs ^= 2; //NOTE: RPi wants BGRA here, R <-> B
    ofs = ((ofs & ~3) << 2) + (ofs & 3); //offset into cluster of 4 pixels
    splitter.bytes[1] = this.pixel_bytes[ofs + 0];
    splitter.bytes[2] = this.pixel_bytes[ofs + 4];
    splitter.bytes[3] = this.pixel_bytes[ofs + 8];
    splitter.bytes[0] = this.pixel_bytes[ofs + 12];
//    return splitter.uint32.getUint32(0, false); //byteOffset = 0, litteEndian = false
//    return splitter.read(); //splitter.uint32[0];
    return splitter.uint32; //[0];
}


function wrpixel_RPi(ofs, argb)
{
    if (this.gl.isGLES && !(ofs & 1)) ofs ^= 2; //NOTE: RPi wants BGRA here, R <-> B
//    var rearr = ofs & 3;
    ofs = ((ofs & ~3) << 2) + (ofs & 3); //offset into cluster of 4 pixels
//    splitter.uint32.setUint32(0, argb >>> 0, false); //byteOffset = 0, litteEndian = false
    splitter.uint32 = argb; //[0] = argb; // >>> 0;
//    /*if (arguments.length > 1)*/ splitter.write(argb);
//console.log((splitter.uint32[0] + 0).toString(16));
    this.pixel_bytes[ofs + 0] = splitter.bytes[1];
    this.pixel_bytes[ofs + 4] = splitter.bytes[2];
    this.pixel_bytes[ofs + 8] = splitter.bytes[3];
    this.pixel_bytes[ofs + 12] = splitter.bytes[0]; //NOTE: ignored by shader, but save it anyway; put at end
//console.log((splitter.bytes[0] + 0).toString(16), (splitter.bytes[1] + 0).toString(16), (splitter.bytes[2] + 0).toString(16), (splitter.bytes[3] + 0).toString(16));
//console.log((this.pixel_bytes[ofs + 12] + 0).toString(16), (this.pixel_bytes[ofs + 0] + 0).toString(16), (this.pixel_bytes[ofs + 4] + 0).toString(16), (this.pixel_bytes[ofs + 8] + 0).toString(16));
//var buf = "";
//for (var i = 0; i < 16; ++i)
//    buf += " " + (this.pixel_bytes[i] + 0).toString(16);
//if (ofs < 48) console.log("wrpx", arguments[0], arguments[1].toString(16), ofs, buf);
    this.dirty = true; //assume caller is changing it
}


//read/write pixel ARGB value:
function rdpixel_GL(ofs)
{
    splitter.bytes[1] = this.pixel_bytes[ofs + 0];
    splitter.bytes[2] = this.pixel_bytes[ofs + 4];
    splitter.bytes[3] = this.pixel_bytes[ofs + 8];
    splitter.bytes[0] = this.pixel_bytes[ofs + 12];
//    return splitter.uint32.getUint32(0, false); //byteOffset = 0, litteEndian = false
//    return splitter.read(); //splitter.uint32[0];
    return splitter.uint32; //[0];
}


function wrpixel_GL(ofs, argb)
{
    if (this.gl.isGLES && !(ofs & 1)) ofs ^= 2; //NOTE: RPi wants BGRA here, R <-> B
//    var rearr = ofs & 3;
//    splitter.uint32.setUint32(0, argb >>> 0, false); //byteOffset = 0, litteEndian = false
    splitter.uint32 = argb; //[0] = argb; // >>> 0;
//    /*if (arguments.length > 1)*/ splitter.write(argb);
//console.log((splitter.uint32[0] + 0).toString(16));
    this.pixel_bytes[ofs + 0] = splitter.bytes[1];
    this.pixel_bytes[ofs + 4] = splitter.bytes[2];
    this.pixel_bytes[ofs + 8] = splitter.bytes[3];
    this.pixel_bytes[ofs + 12] = splitter.bytes[0]; //NOTE: ignored by shader, but save it anyway; put at end
//console.log((splitter.bytes[0] + 0).toString(16), (splitter.bytes[1] + 0).toString(16), (splitter.bytes[2] + 0).toString(16), (splitter.bytes[3] + 0).toString(16));
//console.log((this.pixel_bytes[ofs + 12] + 0).toString(16), (this.pixel_bytes[ofs + 0] + 0).toString(16), (this.pixel_bytes[ofs + 4] + 0).toString(16), (this.pixel_bytes[ofs + 8] + 0).toString(16));
//var buf = "";
//for (var i = 0; i < 16; ++i)
//    buf += " " + (this.pixel_bytes[i] + 0).toString(16);
//if (ofs < 48) console.log("wrpx", arguments[0], arguments[1].toString(16), ofs, buf);
    this.dirty = true; //assume caller is changing it
}


//read/write pixel ARGB value:
function rdpixel_GLES(ofs)
{
    if (this.gl.isGLES && !(ofs & 1)) ofs ^= 2; //NOTE: RPi wants BGRA here, R <-> B
    splitter.bytes[1] = this.pixel_bytes[ofs + 0];
    splitter.bytes[2] = this.pixel_bytes[ofs + 4];
    splitter.bytes[3] = this.pixel_bytes[ofs + 8];
    splitter.bytes[0] = this.pixel_bytes[ofs + 12];
//    return splitter.uint32.getUint32(0, false); //byteOffset = 0, litteEndian = false
//    return splitter.read(); //splitter.uint32[0];
    return splitter.uint32; //[0];
}


function wrpixel_GLES(ofs, argb)
{
    if (this.gl.isGLES && !(ofs & 1)) ofs ^= 2; //NOTE: RPi wants BGRA here, R <-> B
//    var rearr = ofs & 3;
//    splitter.uint32.setUint32(0, argb >>> 0, false); //byteOffset = 0, litteEndian = false
    splitter.uint32 = argb; //[0] = argb; // >>> 0;
//    /*if (arguments.length > 1)*/ splitter.write(argb);
//console.log((splitter.uint32[0] + 0).toString(16));
    this.pixel_bytes[ofs + 0] = splitter.bytes[1];
    this.pixel_bytes[ofs + 4] = splitter.bytes[2];
    this.pixel_bytes[ofs + 8] = splitter.bytes[3];
    this.pixel_bytes[ofs + 12] = splitter.bytes[0]; //NOTE: ignored by shader, but save it anyway; put at end
//console.log((splitter.bytes[0] + 0).toString(16), (splitter.bytes[1] + 0).toString(16), (splitter.bytes[2] + 0).toString(16), (splitter.bytes[3] + 0).toString(16));
//console.log((this.pixel_bytes[ofs + 12] + 0).toString(16), (this.pixel_bytes[ofs + 0] + 0).toString(16), (this.pixel_bytes[ofs + 4] + 0).toString(16), (this.pixel_bytes[ofs + 8] + 0).toString(16));
//var buf = "";
//for (var i = 0; i < 16; ++i)
//    buf += " " + (this.pixel_bytes[i] + 0).toString(16);
//if (ofs < 48) console.log("wrpx", arguments[0], arguments[1].toString(16), ofs, buf);
    this.dirty = true; //assume caller is changing it
}


///////////////////////////////////////////////////////////////////////////////
////
/// Graphics helper functions
//

//screen refresh:
//typically 60 Hz (when window is active), but controlled by browser or caller
function rAF(onoff)
{
    if (typeof onoff == "boolean") rAF.isrunning = onoff;
    if (rAF.isrunning) document.requestAnimationFrame(rAF);
//    draw(my_draw.prog || 0);
//    if (rAF.inner) rAF.inner();
    ++rAF.count; //for debug
    GpuCanvas.all.forEach(canvas =>
    {
//        canvas.first();
//        canvas.clear();
//        canvas.gl.uniform1i(canvas.shpgm.uniforms.sampler, 0);

//		glVertexAttribPointer(state.positionLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].x); ERRCHK("flush"); //vVertices);
//		glVertexAttribPointer(state.texCoordLoc, 3, GL_FLOAT, GL_FALSE, sizeof(mverts[0]) /*5 * sizeof(GLfloat)*/, &mverts[0].s); ERRCHK("flush"); //&vVertices[3]); //(U,V) interleaved with (X,Y,Z)
//		glEnableVertexAttribArray(state.positionLoc); ERRCHK("flush");
//		glEnableVertexAttribArray(state.texCoordLoc); ERRCHK("flush");
//		glUniform1i(state.samplerLoc, 0); ERRCHK("flush"); //set sampler texture unit to 0

        if (!canvas.render(true)) rAF.isrunning = false; //stop rendering loop if error
    });

//if (isNaN(++rAF.count)) rAF.count = 0;
//console.log("rAF %s? %s", rAF.count, rAF.isrunning);
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
//    if (mant < 1) { mant *= 2; --exp; } //normalize
    var newval = sgn * mant * Math.pow(2, exp); //verify correct representation
//var rep = (((sgn? 0x800000: 0) | ((exp + 127) << 23) | (mant - 1)) >>> 0).toString(16);
//if ((mant >>> 0) & ~0x7fffff) rep += "??";
//console.log("float %s => %j as 0x%s => %s", val, {sgn, mant, exp}, rep, newval);
    return newval;
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//

//add line numbers for easier debug:
function showsrc(str, type, error)
{
//    var ShaderTypes = {};
//    ShaderTypes[this.gl.FRAGMENT_SHADER] = "fragment";
//    ShaderTypes[this.gl.VERTEX_SHADER] = "vertex";
    if (!str) str = getShader[type + "_src"];
    if (!error && showsrc["seen_" + type]) return; //only show it once unless error
    showsrc["seen_" + type] = true;
    var lines = [];
    str.split("\n").forEach(function(line, inx)
    {
        lines.push(inx + ": " + line); //add line#s for easier debug
    });
    (error? console.log: debug)("%s shader:\n%s".green_lt, type, lines.join("\n"));
}


//extract source code:
function heredoc(func)
{
    func = func.toString();
    var parse = func.match(/^[^]*\/\*([^]*)\*\/\}$/m);
    return parse? parse[1]: func;
}


//round off:
//(mainly for display purposes, to cut down on verbosity)
function micro(val)
{
    if (Array.isArray(val))
    {
        var retval = [];
        val.forEach(elmt => { retval.push(micro(elmt)); });
        return retval;
    }
    return Math.floor(val * 1e6) / 1e6;
}


//allow easier access to top of stack:
if (!Array.prototype.top)
    Object.defineProperty(Array.prototype, "top",
    {
        get: function() { return this[this.length - 1]; },
        set: function(newval) { this[this.length - 1] = newval; },
    });


//cut down on dup console output:
//const svlog = console.log; //equiv to this._stdout.write(`${util.format.apply(null, args)}\n`);
const util = require('util');
console.log = function(args)
{
    var outbuf = util.format.apply(null, arguments);
    if (console.log._previous)
    {
        if (console.log._previous.outbuf == outbuf)
        {
            ++console.log._previous.count;
            return console.log._previous.retval;
        }
        if (console.log._previous.count != 1)
            console._stdout.write(`x${console.log._previous.count}\n`); //flush previous repeat count
    }
//    else
//        process.on('beforeExit', console.log); //flush previous line
    var retval = console._stdout.write(outbuf + "\n");
    console.log._previous = {count: 1, retval, outbuf};
    return retval;
}


//var limit = 0;
//for (var i = 0; i < 8; ++i) { var p = Math.pow(.5, i + 1); limit += p; console.log(p); }
//console.log(limit, 256 * limit / 255);

//const RGB_BITS = 255 / 256;
//function AND(val, mask) { return (Math.min(val, RGB_BITS) % (2 * mask)) >= mask; }
//for (var i = 0; i <= 256; ++i)
//{
//    var val = i / 256;
//    console.log("%d. %d", i, val, AND(val, 1/2), AND(val, 1/16), AND(val, 1/256));
//}

//eof
