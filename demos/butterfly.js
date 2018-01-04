#!/usr/bin/env node
//Butterfly demo pattern: tests GPU effects
//based on xLights/NutCracker Butterfly effect
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes.
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture re-write working on RPi
//ver 1.0a DJ  9/24/17  minor clean up
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output (incl bkg threads)
//const pathlib = require('path');
//const {blocking, wait} = require('blocking-style');
const {debug} = require('./shared/debug');
//const memwatch = require('memwatch-next');
const {Screen, GpuCanvas, shmbuf} = require('gpu-friends-ws281x');
//const {vec3, vec4, mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');
Screen.gpio = true; //force full screen (test only)

//display settings:
//const SPEED = 1/60; //1/10; //1/30; //animation speed (sec); fps
const FPS = 60; //estimated animation speed; NOTE: won't go faster than video card refresh rate
const DURATION = 60/30; //how long to run (sec)
const NUM_UNIV = 24; //can't exceed #VGA output pins (24) unless external mux used
const UNIV_LEN = Screen.gpio? Screen.height: 30; //can't exceed #display lines; show larger pixels in dev mode
debug("Screen %d x %d, is RPi? %d, GPIO? %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//show extra debug info:
//NOTE: these only apply when GPIO pins are *not* presenting VGA signals (otherwise interferes with WS281X timing)
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


//shared memory buffer for inter-process data:
//significantly reduces inter-process serialization and data passing overhead
//mem copying reportedly can take ~ 25 msec for 100KB; that is way too high for 60 FPS frame rate
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
//var sab = new SharedArrayBuffer(1024); //not implemented yet in Node.js :(
const SHMKEY = 0xbeef; //make value easy to find (for debug)
var SharedPixels = new Uint32Array(shmbuf(SHMKEY, NUM_UNIV * UNIV_LEN * Uint32Array.BYTES_PER_ELEMENT));


//main logic:
//written with synchronous coding style to simplify timing logic
//blocking(function*()
//const TRACE = true;
step(function*()
{
    var canvas = new GpuCanvas("Butterfly", NUM_UNIV, UNIV_LEN, OPTS);
    canvas.pixels = SharedPixels;
//    wker.postMessage("all");
//    var retval = yield bkg({ack: 1});
//    console.log("got from bkg:", retval);
//    console.log("got", yield bkg({eval: butterfly}));
//    bkg.canvas = canvas;

//OPTS.gpufx = true; //TEMP: bypass
    debug("begin, run for %d sec @%d fps".green_lt, DURATION, FPS);
//    var started = now_sec();
    var total_time = 0, busy = 0;
    canvas.duration = DURATION * FPS; //set progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
//    var hd = new memwatch.HeapDiff();
    for (var t = 0, ofs = 0; t <= DURATION * FPS; ++t, ++canvas.elapsed) //update progress bar
    {
        ofs += BF_opts.reverse? -BF_opts.speed: BF_opts.speed;
        total_time -= elapsed();
//NOTE: pixel (0, 0) is upper left on screen
//NOTE: the code below will run up to ~ 32 FPS on a RPi 2
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
            for (var x = 0, xofs = 0; x < canvas.width; ++x, xofs += canvas.height)
	            for (var y = 0; y < canvas.height; ++y)
//+66 msec (~ 25%)	                canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
                    canvas.pixels[xofs + y] = butterfly(x, y, ofs, BF_opts);
//            yield bkg({render: ofs});
//console.log("paint");
        total_time += elapsed();
        canvas.paint();
//        yield wait((t + 1) / FPS - now_sec() + started); //canvas.elapsed); //avoid cumulative timing errors
//        yield wait(started + (t + 1) / FPS - now_sec()); ///use cumulative time to avoid drift
//        ++canvas.elapsed; //update progress bar
    }
//    debug("heap diff:", JSON.stringify(hd.end(), null, 2));
    var frtime = 1000 * total_time / (DURATION * FPS);
    debug(`avg frame gen time: ${tenths(frtime)} msec (${tenths(1000 / frtime)} fps), ${DURATION * FPS} frames`.blue_lt);
//    bkg(); //wker.postMessage(null); //.close();
//    debug(`fx cache: #fx calls ${butterfly.calls}, #cache misses ${Object.keys(butterfly.cache).length}, %cache hits ${tenths(100 * (butterfly.calls - Object.keys(butterfly.cache).length) / butterfly.calls)}%`.blue_lt);
//    debug(`min hue: ${sethue.minhue}, max hue: ${sethue.maxhue}`.blue_lt);
    debug("end, pause 10 sec".green_lt);
    yield wait(10-8); //pause at end so screen doesn't disappear too fast
    canvas.StatsAdjust = -10; //exclude pause in final stats
});


/*
//send msg to bkg worker thread:
function bkg(data)
{
    if (!bkg.wker) //create bkg worker thread
    {
        bkg.wker = new Worker(function()
        {
//            postMessage("before all");
            this.onmessage = function(evt)
            {try{
//                if (evt.data === null) { self.close(); return; }
                if ("eval" in evt.data) { eval(evt.data); postMessage("eval'ed"); return; }
                if ("render" in evt.data) //((typeof evt.data == "object") && ("render" in evt.data))
                {
                    var ofs = evt.data.render, canvas = {width: 1, height: 1, pixel: function(){}}; //bkg.canvas;
	                for (var x = 0; x < canvas.width; ++x)
	                    for (var y = 0; y < canvas.height; ++y)
	                        canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
                    postMessage("rendered");
                    return;
                }
//console.log("bkg got", evt.data);
                postMessage("bkg got:" + JSON.stringify(evt.data));
            } catch (exc) { postMessage("exc: " + exc); return; }}
        });
        bkg.wker.onmessage = function(evt)
        {
console.log("wker replied: " + evt.data);
            step.retval = evt.data; //give response back to caller's yield
            step(); //wake up caller
        };
    }
    if (!arguments.length) { bkg.wker.terminate(); delete bkg.wker; return; }
console.log("send2bkg", JSON.stringify(data));
    bkg.wker.postMessage(data);
//        if (data !== null) return yield;
}
*/


//generator function stepper:
function step(gen)
{
//console.log("step");
//    if (step.done) throw "Generator function is already done.";
	if (typeof gen == "function") { setImmediate(function() { step(gen()); }); return; } //invoke generator if not already
//        return setImmediate(function() { step(gen()); }); //avoid hoist errors
    if (gen) step.svgen = gen; //save generator for subsequent steps
//console.log("step:", typeof gen, JSON.stringify_tidy(gen));
	var {value, done} = step.svgen.next(step.retval); //send previous value to generator
//    {step.retval, step.done} = step.svgen.next(step.retval); //send previous value to generator
//    Object.assign(step, step.svgen.next(step.retval)); //send previous value to generator
//console.log("step: got value %s %s, done? %d", typeof value, value, done);
//    step.done = done; //prevent overrun
    if (typeof value == "function") value = value(); //execute caller code before returning
    if (done) delete step.svgen; //prevent continuation
if (done) debug("process %s done", process.pid);
    return step.retval = value; //return value to caller and to next yield
}


//delay next step:
//NOTE: only used for debug at end; everything else runs flat out :)
function wait(delay)
{
console.log("wait %d sec", milli(delay));
    delay *= 1000; //sec -> msec
    return (delay > 1)? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
}


//high-res elapsed time:
//NOTE: unknown epoch; useful for relative times only
function elapsed()
{
    var parts = process.hrtime();
    return parts[0] + parts[1] / 1e9; //sec, with nsec precision
}


//truncate after 3 dec places:
function milli(val)
{
    return Math.floor(val * 1e3) / 1e3;
}

function tenths(val)
{
    return Math.floor(val * 10) / 10;
}


//current time in seconds:
//function now_sec()
//{
//    return Date.now() / 1000;
//}


///////////////////////////////////////////////////////////////////////////////
////
/// Butterfly effect (if rendered on CPU instead of GPU)
//

//CPU effects generator:
function butterfly(x, y, ofs, opts)
{
//    var key = 24 * y + x; //~ 27K unique values
//    if (isNaN(++butterfly.calls)) { butterfly.calls = 1; butterfly.cache = {}; }
//    if (key in butterfly.cache) return butterfly.cache[key];
//    return hsv2rgb(scheme(sethue(x, y, ofs, opts), opts));
//+350 msec    var hue = sethue(x, y, ofs, opts);
//    return butterfly.cache[hue] || 
//        (butterfly.cache[hue] = toargb(hsv2rgb(sethue(x, y, ofs, opts), 1, 1)));
    return /*butterfly.cache[key] =*/ argb2abgr(hsv2rgb(sethue(x, y, ofs, opts), 1, 1));
//    return /*butterfly.cache[key] =*/ toargb(hsv2rgb_1_1(sethue(x, y, ofs, opts)));
}
//butterfly.cache = {};


//based on http://mathworld.wolfram.com/ButterflyFunction.html
//PERF: ~ 20 msec
function sethue(x, y, ofs, opts)
{
    y = UNIV_LEN - y - 1; //flip y axis; (0,0) is top of screen
//not needed? axis fixes: fix the colors for pixels at (0,1) and (1,0)
//    if ((x == 0) && (y == 1)) y = y + 1;
//    if ((x == 1) && (y == 0)) x = x + 1;

//    var num = Math.abs((x * x - y * y) * Math.sin(ofs + ((x + y) * Math.PI * 2 / (opts.height + opts.width))));
    const num = /*Math.abs*/((x * x - y * y) * Math.sin(((ofs + x + y) / /*(opts.height + opts.width)*/ (NUM_UNIV + UNIV_LEN) * 2 * Math.PI)));
    const den = x * x + y * y;

    const hue = (den > 0.001)? num / den: 0;
//if (hue > sethue.maxhue) sethue.maxhue = hue;
//if (hue < sethue.minhue) sethue.minhue = hue;
    return (hue < 0)? -hue: hue;
}
//sethue.minhue = 1e10;
//sethue.maxhue = 0;


//var buf = new Buffer([0xff, 0, 0, 0]);
//PERF: ~ 1 msec?
function X_argb2abgr(vec3)
{
//NOTE: bit shuffling is only 1 msec > buf read/write per frame
    return 0xff000000 | (Math.floor(vec3[0] * 0xff) << 16) | (Math.floor(vec3[1] * 0xff) << 8) | Math.floor(vec3[2] * 0xff);
//    buf[1] = vec3[0] * 0xff;
//    buf[2] = vec3[1] * 0xff;
//    buf[3] = vec3[2] * 0xff;
//    return buf.readUInt32BE(0);
}
function argb2abgr(color)
{
//NOTE: bit shuffling is only 1 msec > buf read/write per frame
//    return 0xff000000 | (Math.floor(vec3[0] * 0xff) << 16) | (Math.floor(vec3[1] * 0xff) << 8) | Math.floor(vec3[2] * 0xff);
    var retval = (color & 0xff00ff00) | ((color >> 16) & 0xff) | ((color & 0xff) << 16);
//if (++argb2abgr.count < 10) console.log(color.toString(16), retval.toString(16));
    return retval;
}
//argb2abgr.count = 0;


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helpers to simulate GLSL built-ins:
//

//PERF: 210 msec
function SLOW_hsv2rgb(h, s, v)
{
    const K = [1, 2/3, 1/3, 3]; //vec4
    var p = abs(sub(mul(fract(add([h, h, h], K /*[1, 2/3, 1/3]*/)), 6), [3, 3, 3]));
//    return mul(mix([1/3, 1/3, 1/3], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
    return mul(mix([1, 1, 1], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
}

//PERF: ~ 46 msec; *way* better than above
function hsv2rgb(h, s, v)
//from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
{
//    var      hh, p, q, t, ff;
//    var       i;
//    var rgb;

//    if(s <= 0.0) {       // < is bogus, just shuts up warnings
//        out.r = in.v;
//        out.g = in.v;
//        out.b = in.v;
//        return out;
//    }
//    hh = in.h;
//    if (h >= 360.0) h = 0.0;
//    if (h >= 1) h = 0;
//    h /= 60.0;
    h *= 6; //[0..6]
    const segment = h >>> 0; //(long)hh; //convert to int
    const angle = (segment & 1)? h - segment: 1 - (h - segment); //fractional part
//    if ((segment & 1) === 0) angle = 1 - angle;
//    if ((segment >> 1) === (segment >>> 1)) angle = 1 - angle;
//NOTE: it's faster to do the *0xff >>> 0 in here than in toargb
    const p = ((v * (1.0 - s)) * 0xff) >>> 0;
    const qt = ((v * (1.0 - (s * angle))) * 0xff) >>> 0;
//redundant    var t = (v * (1.0 - (s * (1.0 - angle))) * 0xff) >>> 0;
    v = (v * 0xff) >>> 0;

    switch (segment)
    {
        default: //h >= 1 comes in here also
        case 0: return toargb(v, qt, p); //[v, t, p];
        case 1: return toargb(qt, v, p); //[q, v, p];
        case 2: return toargb(p, v, qt); //[p, v, t];
        case 3: return toargb(p, qt, v); //[p, q, v];
        case 4: return toargb(qt, p, v); //[t, p, v];
        case 5: return toargb(v, p, qt); //[v, p, q];
    }
}

function toargb(r, g, b)
{
//if (++toargb.count < 10) console.log(color.toString(16));
    return (0xff000000 | (r << 16) | (g << 8) | b) >>> 0; //force convert to uint32
}
//toargb.count = 0;

/*
//signficant perf improvement:
function hsv2rgb_1_1(h) //, s, v)
{
//    const K = [1, 2/3, 1/3, 3]; //vec4
//    var p = abs(sub(mul(fract(add([h, h, h], [1, 2/3, 1/3])), 6), [3, 3, 3]));
    var p = [(h + 1 - Math.floor(h + 1)) * 6 - 3, (h + 2/3 - Math.floor(h + 2/3)) * 6 - 3, (h + 1/3 - Math.floor(h + 1/3)) * 6 - 3];
    if (p[0] < 0) p[0] = -p[0];
    if (p[1] < 0) p[1] = -p[1];
    if (p[2] < 0) p[2] = -p[2];
//    return mul(mix([1/3, 1/3, 1/3], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
//    return mul(mix([1, 1, 1], clamp(sub(p, [1, 1, 1]), 0, 1), s), v);
//    return clamp(sub(p, [1, 1, 1]), 0, 1);
    --p[0]; --p[1]; --p[2];
    if (p[0] < 0) p[0] = 0; if (p[0] > 1) p[0] = 1;
    if (p[1] < 0) p[1] = 0; if (p[1] > 1) p[1] = 1;
    if (p[2] < 0) p[2] = 0; if (p[2] > 1) p[2] = 1;
    return p;
}
*/


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
