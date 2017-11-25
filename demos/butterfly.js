#!/usr/bin/env node
//Butterfly demo pattern: tests GPU effects
//based on xLights/NutCracker Butterfly effect
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture working on RPi
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas

'use strict'; //find bugs easier
require('colors'); //for console output
const pathlib = require('path');
const {blocking, wait} = require('blocking-style');
const {debug} = require('./shared/debug');
const {Screen, GpuCanvas} = require('gpu-friends-ws281x');
//const {vec3, vec4, mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');

//display settings:
const SPEED = 1/10; //1/30; //animation speed (sec); fps
const DURATION = 60; //how long to run (sec)
const VGROUP = !Screen.gpio? Screen.height / 24: 1; //node grouping; used to increase effective pixel size or reduce resolution for demo/debug
const UNIV_LEN = Screen.height / VGROUP; //can't exceed #display lines; get rid of useless pixels when VGROUP != 1
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
debug("Screen %d x %d, is RPi? %d, GPIO? %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//show extra debug info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
//    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info
//    gpufx: pathlib.resolve(__dirname, "butterfly.glsl"), //generate fx on GPU instead of CPU
};

//ARGB primary colors:
//const RED = 0xffff0000;
//const GREEN = 0xff00ff00;
//const BLUE = 0xff0000ff;
//const YELLOW = 0xffffff00;
//const CYAN = 0xff00ffff;
//const MAGENTA = 0xffff00ff;
//const WHITE = 0xffffffff;
//const BLACK = 0xff000000; //NOTE: alpha must be on to take effect

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//butterfly options:
const BF_opts =
{
//    palette: null, //"Rainbow",
//    circpal: false, //circular palette
//    reverse: true, //false, //direction: "Normal",
//    duration: 10, //sec
//    style: 1,
//    chunks: 1,
//    skip: 2,
    speed: 1, //10,
//    fps: 30,
    width: NUM_UNIV,
    height: UNIV_LEN,
};


//main logic:
//written with synchronous coding style to simplify timing logic
blocking(function*()
{
    var canvas = new GpuCanvas("Butterfly", NUM_UNIV, UNIV_LEN, OPTS);

    debug("begin, run for %d sec".green_lt, DURATION);
    var started = now_sec();
    canvas.duration = DURATION / SPEED; //progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
    for (var t = 0, ofs = 0; t <= DURATION / SPEED; ++t)
    {
        ofs += BF_opts.reverse? -BF_opts.speed: BF_opts.speed;
//pixel [0, 0] is upper left on screen
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
	        for (var x = 0; x < canvas.width; ++x)
	            for (var y = 0; y < canvas.height; ++y)
	                canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
        canvas.paint();
        yield wait((t + 1) * SPEED - now_sec() + started); //canvas.elapsed); //avoid cumulative timing errors
        ++canvas.elapsed; //= now_sec() - started; //update progress bar
    }
    debug("end, pause 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too fast
});


//CPU effects generator:
function butterfly(x, y, ofs, opts)
{
//    return hsv2rgb(scheme(sethue(x, y, ofs, opts), opts));
    return toargb(hsv2rgb(sethue(x, y, ofs, opts), 1, 1));
}


//based on http://mathworld.wolfram.com/ButterflyFunction.html
function sethue(x, y, ofs, opts)
{
    y = UNIV_LEN - y - 1; //flip y axis; (0,0) is top of screen
//axis fixes: fix the colors for pixels at (0,1) and (1,0)
    if ((x == 0) && (y == 1)) y = y + 1;
    if ((x == 1) && (y == 0)) x = x + 1;

//    var num = Math.abs((x * x - y * y) * Math.sin(ofs + ((x + y) * Math.PI * 2 / (opts.height + opts.width))));
    var num = Math.abs((x * x - y * y) * Math.sin(((ofs + x + y) * Math.PI * 2 / (opts.height + opts.width))));
    var den = x * x + y * y;

    var hue = (den > 0.001)? num / den: 0;
    return hue;
}


function toargb(vec)
{
    return 0xff000000 | (Math.floor(vec[0] * 0xff) << 16) | (Math.floor(vec[1] * 0xff) << 8) | Math.floor(vec[2] * 0xff);
}


//truncate after 3 dec places:
function milli(val)
{
    return Math.floor(val * 1e3) / 1e3;
}


//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helpers to simulate GLSL built-ins:
//

function hsv2rgb(h, s, v)
{
    const K = [1, 2/3, 1/3, 3]; //vec4
    var p = abs(sub(mul(fract(add([h, h, h], [1, 2/3, 1/3])), 6), [3, 3, 3]));
//    return mul(mix([1/3, 1/3, 1/3], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
    return mul(mix([1, 1, 1], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
}


function abs(vec)
{
    var retval = [];
    for (var i = 0; i < vec.length; ++i) retval[i] = Math.abs(vec[i]);
    return retval;
}


function sub(vec1, vec2)
{
    var retval = [];
    for (var i = 0; i < vec1.length; ++i) retval[i] = vec1[i] - vec2[i];
    return retval;
}


function add(vec1, vec2)
{
    var retval = [];
    for (var i = 0; i < vec1.length; ++i) retval[i] = vec1[i] + vec2[i];
    return retval;
}

function mul(vec, scalar)
{
    var retval = [];
    for (var i = 0; i < vec.length; ++i) retval[i] = vec[i] * scalar;
    return retval;
}


//GLSL function to return fractional part of a float:
function fract(vec)
{
    var retval = [];
    for (var i = 0; i < vec.length; ++i) retval[i] = vec[i] - Math.floor(vec[i]);
    return retval;
}


//GLSL function to clamp a float to min/max values:
function clamp(vec, minVal, maxVal)
{
    var retval = [];
    for (var i = 0; i < vec.length; ++i) retval[i] = Math.max(minVal, Math.min(maxVal, vec[i]));
    return retval;
}


//GLSL function to return linear blend:
function mix(vec1, vec2, a)
{
    var retval = [];
    for (var i = 0; i < vec1.length; ++i) retval[i] = (1 - a) * vec1[i] + a * vec2[i];
    return retval;
}

//eof
