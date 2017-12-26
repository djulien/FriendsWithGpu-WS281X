#!/usr/bin/env node
//Pin finder RGB patterns: tests GPU config, GPIO pins, and WS281X connections
//Sends out a different pattern on each GPIO pin to make them easier to identify.
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
require('colors'); //for console output
const pathlib = require('path');
//const {blocking, wait} = require('blocking-style');
const {debug} = require('./shared/debug');
//const memwatch = require('memwatch-next');
const {Screen, GpuCanvas, UnivTypes} = require('gpu-friends-ws281x');
Screen.gpio = true; //force full screen (test only)

//display settings:
//const SPEED = 0.5; //animation step speed (sec)
const FPS = 30; //estimated animation speed
const DURATION = 60/6; //how long to run (sec)
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
//    SHOW_PROGRESS: true, //show progress bar at bottom of screen
    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info
//    UNIV_TYPE: UnivTypes.CHPLEX_SSR,
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
};

//ARGB primary colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
//const YELLOW = 0xffffff00;
//const CYAN = 0xff00ffff;
//const MAGENTA = 0xffff00ff;
//const WHITE = 0xffffffff;
const BLACK = 0xff000000; //NOTE: alpha must be on to take effect

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//main logic:
//written with synchronous coding style to simplify timing logic
//blocking(function*()
//const TRACE = true;
step(function*()
{
    var canvas = new GpuCanvas("Pin Finder", NUM_UNIV, UNIV_LEN, OPTS);

    debug("begin, run for %d sec @%d fps".green_lt, DURATION, FPS);
//    var started = now_sec();
    var total_time = 0;
    canvas.duration = DURATION * FPS; //set progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
//    var hd = new memwatch.HeapDiff();
    for (var t = 0; t <= DURATION * FPS; ++t)
    {
        total_time -= elapsed();
//NOTE: pixel (0, 0) is upper left on screen
//NOTE: the code below will run up to ~ 35 FPS on RPi 2
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
            for (var x = 0, xofs = 0; x < canvas.width; ++x, xofs += canvas.height)
            {
                var color = [RED, GREEN, BLUE][x >> 3]; //Math.floor(x / 8)];
                var repeat = 9 - (x & 7); //(x % 8);
                for (var y = 0, c = -t % repeat; y < canvas.height; ++y, ++c)
                {
                    if (c >= repeat) c = 0;
//if ((t < 3) && !x && (y < 10)) console.log(t, typeof c, c);
//                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
//                    var repeat = 9 - (x % 8);
//                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//                    canvas.pixel(x, y, c? BLACK: color);
//                    canvas.pixels[xofs + y] = ((y - t) % repeat)? BLACK: color;
                    canvas.pixels[xofs + y] = c? BLACK: color;
                }
//if (y > 1) break; //59 fps
//if (y > 100) break; //55 fps, 30%-40% CPU
//if (y > 200) break; //44 fps, 60% CPU
//if (y > 300) break; //29 fps, 70% CPU
//9 fps, 50% CPU (~110 msec / iter)
//30 fps * 72 w * 1024 h ~= 2.2M loop iterations/sec
//72 w * 1024 h ~= 74K loop iter/frame (33 msec)
//need ~ 0.45 usec / iter
                }
        total_time += elapsed();
        canvas.paint();
//        yield wait(started + (t + 1) / FPS - now_sec()); //avoid cumulative timing errors
//        yield wait(1);
        ++canvas.elapsed; //update progress bar
    }
//    debug("heap diff:", JSON.stringify(hd.end(), null, 2));
    var frtime = 1000 * total_time / (DURATION * FPS);
    debug(`avg frame gen time: ${tenths(frtime)} msec (${tenths(1000 / frtime)} fps), ${DURATION * FPS} frames`.blue_lt);
    debug("end, pause 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too fast
    canvas.StatsAdjust = -10; //exclude pause in final stats
});


//generator function stepper:
function step(gen)
{
//console.log("step");
//    if (step.done) throw "Generator function is already done.";
//	if (typeof gen == "function") gen = gen(); //invoke generator if not already
	if (typeof gen == "function") { setImmediate(function() { step(gen()); }); return; } //invoke generator if not already
//        return setImmediate(function() { step(gen()); }); //avoid hoist errors
    if (gen) step.svgen = gen; //save generator for subsequent steps
//console.log("step:", typeof gen, JSON.stringify_tidy(gen));
	var {value, done} = step.svgen.next(step.retval); //send previous value to generator
//    {step.retval, step.done} = step.svgen.next(step.retval); //send previous value to generator
//    Object.assign(step, step.svgen.next(step.retval)); //send previous value to generator
//console.log("step: got value %s %s, done? %d", typeof value, value, done);
//    step.done = done; //prevent overrun
    if (done) step.svgen = null; //prevent overrun
    if (typeof value == "function") value = value(); //execute caller code before returning
    return step.retval = value; //return value to caller and to next yield
}


//delay next step:
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


//truncate after 1 dec place:
function tenths(val)
{
    return Math.floor(val * 10) / 10;
}


//current time in seconds:
//function now_sec()
//{
//    return Date.now() / 1000;
//}

//eof
