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
require('colors').enabled = true; //for console output (incl bkg threads)
const pathlib = require('path');
//const {Worker} = require('webworker-threads');
const cluster = require('cluster');
//const {blocking, wait} = require('blocking-style');
const ary2buf = require('typedarray-to-buffer');
const {debug} = require('./shared/debug');
//NOTE: it's preferential to put all requires() at top, but some are not needed for bkg wker processes
const {Screen, GpuCanvas, shmbuf} = restrict(require('gpu-friends-ws281x')); //: {Screen: {}, GpuCanvas: {}, shmbuf: {}};
//worker procs don't need access to GPU/Screen:
function restrict(imports)
{
    if (!cluster.isMaster) imports.Screen = imports.GpuCanvas = {};
    return imports;
}
//const {vec3, vec4, mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');
Screen.gpio = true; //TEMP: force full screen

//display settings:
//const SPEED = 1/60; //1/10; //1/30; //animation speed (sec); fps
const FPS = 60; //animation speed (performance testing)
const DURATION = 2; //60; //how long to run (sec)
const NUM_UNIV = !cluster.isMaster? process.env.NUM_UNIV: 24; //can't exceed #VGA output pins unless external mux used
const UNIV_LEN = !cluster.isMaster? process.env.UNIV_LEN: Screen.gpio? Screen.height: 24; //60; //Screen.height; //Math.round(Screen.height / Math.round(Screen.scanw / 24)); ///can't exceed #display lines; for dev try to use ~ square pixels (on-screen only, for debug)
if (cluster.isMaster)
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


//use shared memory buffer to reduce inter-process data:
//mem copying reported can take ~ 25 msec for 100K; that is way too high for this app
//var sab = new SharedArrayBuffer(1024); //not implemented yet in Node.js :(
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
var shared_buf = shmbuf(0xbeef, NUM_UNIV * UNIV_LEN * Uint32Array.BYTES_PER_ELEMENT); //.buffer);
var SharedPixels = new Uint32Array(shared_buf);


//dummy class to emulate real one:
//main purpose is to hold partial pixel data
class PartialCanvas
{
    constructor(title, w, h, opts)
    {
        this.title = title;
        this.width = w;
        this.height = h;
        this.opts = opts;
//        this.pixels = new Uint32Array(w * h);
        this.pixels = SharedPixels; //new Buffer(w * h * Uint32Array.BYTES_PER_ELEMENT);
//        this.pixels = new Uint32Array(pixel_buf); //thisb.buffer, b.byteOffset, b.byteLength / Uint32Array.BYTES_PER_ELEMENT)
//        if (this.pixels.buffer) throw "pixel array already has a buffer".red_lt;
//        this.pixels.my_buffer = pixel_buf;
//        this.pixels = new Uint32Array(new Buffer(w * h * Uint32Array.BYTES_PER_ELEMENT)); //thisb.buffer, b.byteOffset, b.byteLength / Uint32Array.BYTES_PER_ELEMENT)
//        if (!this.pixels.buffer) throw "pixel array no buffer".red_lt;
//this.pixels.writeUInt32BE(0x1234, 0); this.pixels.writeUInt32BE(0x5678, 1 * Uint32Array.BYTES_PER_ELEMENT);
//for (var i in this.pixels.buffer.prototype) console.log("buf", i);
//console.log(typeof this.pixels.buffer, this.pixels.buffer.length, this.pixels.length, JSON.stringify(this.pixels.my_buffer));
//console.log("partial", this.pixels.length, hex32(this.pixels.readUInt32BE(0)), hex32(this.pixels.readUInt32BE(4)));
        this.elapsed = 0;
    }
    pixel(x, y, color)
    {
        if ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height)) return 0;
        var ofs = x * this.height + y;
        var retval = this.pixels[ofs]; //return old value
        if (arguments.length > 2) this.pixels[ofs] = color >>> 0;
        return retval;
    }
    fill(color)
    {
//this.pixels.fill(color);
//        for (var i = 0; i < this.pixels.length; i += Uint32Array.BYTES_PER_ELEMENT)
//could be partial (rect) so use nested loop:
        for (var x = 0; x < this.width; ++x)
            for (var y = 0; y < this.height; ++y)
                this.pixels.writeUInt32BE(color >>> 0, i);
    }
    paint() {}
};

if (cluster.isWorker)
{
//    const XOFS = process.env.XPARTNOFS;
    debug(`hello from wker ${process.pid}: #univ ${NUM_UNIV}, univ len ${UNIV_LEN}`.green_lt);
//    SharedPixels[1] = 0x1234; SharedPixels[4] = 0xabcd;
//    var buf = Buffer.alloc(2 * 4);
//    var ary = new Uint32Array(2);
//    ary[0] = 0x1234; ary[1] = 0xabcd;
//    var buf = ary2buf(ary);
    process.send({im_here: true}); //, ary, buf});
//console.log("wker env", process.env);
    var canvas = new PartialCanvas("Butterfly", NUM_UNIV, UNIV_LEN, OPTS);
//var pb = new Buffer(2 * Uint32Array.BYTES_PER_ELEMENT);
//canvas.pixels_fake = new Uint32Array(2);
//canvas.pixels_fake.buffer = pb;
    
    process.on('message', (msg) =>
    {
//console.log(`wker ${process.pid} got req ${JSON.stringify(msg)}`);
        if (msg.render)
        {
//    for (var t = 0, ofs = 0; t <= DURATION * FPS; ++t, ++canvas.elapsed)
//    {
//        ofs += BF_opts.reverse? -BF_opts.speed: BF_opts.speed;
//pixel [0, 0] is upper left on screen
//        proctime -= elapsed();
//        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
            var {XSTART, XEND, ofs} = msg;
            for (var x = XSTART; x < XEND; ++x)
                for (var y = 0+1; y < canvas.height; ++y)
                    canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
            canvas.pixel(2 * msg.w + 1, 0, msg.w * 0x10101);
//        proctime += elapsed();
//            yield bkg({render: ofs});
//console.log("paint");
            canvas.paint();
//console.log(`wker ${process.pid} send reply`);
//canvas.pixels_fake[0] = XOFS;
//canvas.pixels_fake[1] = ofs;
//            process.send({pixels: canvas.pixels, XOFS, ofs});
            process.send({done: true});
            return;
        }
        if (msg.quit) { process.exit(0); return; }
//        process.send(msg);
        throw `Unknown msg type: '${Object.keys(msg)}'`.red_lt;
    });
    return;
}


//if (cluster.isMaster)
//{
const XPARTN = Math.ceil(NUM_UNIV / 2); //assign 12 cols to each wker
var numWkers = Math.ceil(NUM_UNIV / XPARTN); //require('os').cpus().length;
var wkers = {};
cluster.on('online', (wker) =>
{
    debug(`worker ${wker.process.pid} started`.green_lt);
    wkers[wker.process.pid] = wker;
});
cluster.on('message', (wker, msg, handle) =>
{
    if (arguments.length == 2) { handle = msg; msg = wker; wker = undefined; } //?? shown in example at https://nodejs.org/api/cluster.html#cluster_event_message_1
    step.retval = msg;
    step(); //wake up caller
});
cluster.on('disconnect', (wker) =>
{
    debug(`The worker #${wker.id} has disconnected`.cyan_lt);
});
cluster.on('exit', (wker, code, signal) =>
{
    delete wkers[wker.process.pid];
    var want_restart = (Object.keys(wkers).length < numWkers);
    debug(`worker ${wker.process.pid} died (${code || signal}), restart? ${want_restart}`.red_lt);
    if (want_restart) cluster.fork({NUM_UNIV, UNIV_LEN});
});
debug(`master proc '${process.pid}', starting ${numWkers} workers`.green_lt);
//divide canvas into vertical partitions:
debug(`dividing render work into ${numWkers} partitions`.blue_lt);
for (var x = 0; x < NUM_UNIV; x += XPARTN)
//    cluster.fork({NUM_UNIV: Math.min(NUM_UNIV - x, XPARTN), XPARTNOFS: x, UNIV_LEN});
    cluster.fork({NUM_UNIV, UNIV_LEN}); //NOTE: might be extra due to round up; make uniform so workers are interchangeable
//numWkers = 0; //don't want auto-restart
//var canvas = new GpuCanvas("Butterfly", NUM_UNIV, UNIV_LEN, OPTS);
//other logic here
//return;
//}


//main logic:
//written with synchronous coding style to simplify timing logic
//blocking(function*()
//const TRACE = true;
step(function*()
{
//wait for wkers:
//    for (var w in wkers) //get responses, overlay into composite pixel array
    for (var x = 0; x < NUM_UNIV; x += XPARTN)
    {
//        var ary = new Uint32Array(4);
//        var buf = ary2buf(ary);
//        ary[0] = 1111; ary[1] = 2222; ary[2] = 3333; ary[3] = 4444;
        var msg = yield;
        console.log(`got ack ${JSON.stringify(msg)} from wker`);
//        ary.set(msg.ary, 1);
//        console.log(ary);
//        ary.set(msg.buf, 1);
//        console.log(ary);
//        console.log("shared", SharedPixels[1].toString(16), SharedPixels[4].toString(16));
    }
//var buf0 = new Uint32Array(2);
//buf0[0] = 0x12; buf0[1] = 0x34;
//console.log("ui32ary0", JSON.stringify(buf0), hex32(buf0[0]), hex32(buf0[1]), JSON.stringify(buf0));
//var buf1 = new Buffer(buf0); //JSON.parse(JSON.stringify(buf0));
//console.log("ui32ary1", JSON.stringify(buf1), hex32(buf1[0]), hex32(buf1[1]), JSON.stringify(buf1));
//console.log("ui32ary", buf1);
//var buf2 = JSON.parse(buf1);
//console.log("ui32ary", hex32(buf2[0]), hex32(buf2[1]));
//var buf3 = new Uint32Array(6);
//console.log("dest", typeof buf3, hex32(buf3[0]), hex32(buf3[1]), hex32(buf3[2]), hex32(buf3[3]), hex32(buf3[4]), hex32(buf3[5]));
//buf3.set(buf1, 2);
//console.log("dest1", typeof buf1, JSON.stringify(buf1), hex32(buf3[0]), hex32(buf3[1]), hex32(buf3[2]), hex32(buf3[3]), hex32(buf3[4]), hex32(buf3[5]));
//buf3.set(buf0, 2);
//console.log("dest0", typeof buf0, JSON.stringify(buf0), hex32(buf3[0]), hex32(buf3[1]), hex32(buf3[2]), hex32(buf3[3]), hex32(buf3[4]), hex32(buf3[5]));

    var canvas = new GpuCanvas("Butterfly", NUM_UNIV, UNIV_LEN, OPTS);
    canvas.pixels = SharedPixels;
//console.log("canvas", Object.keys(canvas));
//    var all_pixels = new Uint32Array(canvas.width * canvas.height);
//    wker.postMessage("all");
//    var retval = yield bkg({ack: 1});
//    console.log("got from bkg:", retval);
//    console.log("got", yield bkg({eval: butterfly}));
//    bkg.canvas = canvas;

//OPTS.gpufx = true; //TEMP: bypass
    debug("begin, run for %d sec @%d fps".green_lt, DURATION, FPS);
    var started = now_sec();
    canvas.duration = DURATION * FPS; //progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
    var proctime = 0;
    for (var t = 0, ofs = 0; t <= 0 * DURATION * FPS; ++t, ++canvas.elapsed)
    {
        ofs += BF_opts.reverse? -BF_opts.speed: BF_opts.speed;
//pixel [0, 0] is upper left on screen
        proctime -= elapsed();
//canvas.pixels[0] = canvas.pixels[1] = canvas.pixels[2] = canvas.pixels[3] = canvas.pixels[4] = canvas.pixels[5] = -1;
canvas.fill(0x12345678);
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
        {
//	        for (var x = 0; x < canvas.width; ++x)
//	            for (var y = 0; y < canvas.height; ++y)
//	                canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
            for (var XSTART = 0, w = 0; XSTART < NUM_UNIV; XSTART += XPARTN, ++w) //send requests
            {
//                console.log(`sending render ofs ${XOFS} req to wker ${Object.keys(wkers)[w]} in '${Object.keys(wkers)}'`);
                wkers[Object.keys(wkers)[w]].send({render: true, w, ofs, XSTART, XEND: Math.min(XSTART + XPARTN, canvas.width)});
            }
            for (var w in wkers) //get responses, overlay into composite pixel array
            {
                var msg = yield;
//if (t < 2) console.log("got reply", JSON.stringify(msg).slice(0, 200));
//                var {pixels, XOFS} = msg; //yield;
//if (t < 2) console.log("before", hex32(canvas.pixels[0]), hex32(canvas.pixels[1]), hex32(canvas.pixels[2]), hex32(canvas.pixels[3]), hex32(canvas.pixels[4]), hex32(canvas.pixels[5]));
//                canvas.pixels.set(pixels, XOFS / XPARTN * 2); //XOFS * UNIV_LEN);
//if (t < 2) console.log("after", hex32(canvas.pixels[0]), hex32(canvas.pixels[1]), hex32(canvas.pixels[2]), hex32(canvas.pixels[3]), hex32(canvas.pixels[4]), hex32(canvas.pixels[5]));
            }
        }
if (t < 2)
{
var buf = [];
for (var i = 0; (i < canvas.pixels.length) && (i < 6); ++i) buf.push(hex32(canvas.pixels[i]));
console.log("pixels[%d] %s", t, buf.join(","));
}
        proctime += elapsed();
//            yield bkg({render: ofs});
//console.log("paint");
        canvas.paint();
//        yield wait((t + 1) / FPS - now_sec() + started); //canvas.elapsed); //avoid cumulative timing errors
//        yield wait(started + (t + 1) / FPS - now_sec()); ///use cumulative time to avoid drift
    }
console.log("proc time", proctime / DURATION / FPS);
    numWkers = 0; //don't want auto-restart
    for (var XOFS = 0, w = 0; XOFS < NUM_UNIV; XOFS += XPARTN, ++w) //send requests
        wkers[Object.keys(wkers)[w]].send({quit: true});
//    bkg(); //wker.postMessage(null); //.close();
    debug("end, pause 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too fast
    canvas.StatsAdjust = -10; //exclude pause in final stats
});


function elapsed()
{
    var parts = process.hrtime();
    return parts[0] + parts[1] / 1e9;
}


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
	if (typeof gen == "function") gen = gen(); //invoke generator if not already
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
function wait(delay)
{
console.log("wait %d sec", milli(delay));
    delay *= 1000; //sec -> msec
    return (delay > 1)? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
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


function hex32(val)
{
    return (val >>> 0).toString(16);
}


///////////////////////////////////////////////////////////////////////////////
////
/// Butterfly effect (not used if generated on GPU)
//

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


function toargb(vec3)
{
    return 0xff000000 | (Math.floor(vec3[0] * 0xff) << 16) | (Math.floor(vec3[1] * 0xff) << 8) | Math.floor(vec3[2] * 0xff);
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
