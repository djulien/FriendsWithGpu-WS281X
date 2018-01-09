#!/usr/bin/env node
//Pin finder RGB patterns: tests GPU config, GPIO pins, and WS281X connections
//Sends out a different pattern on each GPIO pin to make them easier to identify.
//Copyright (c) 2016-2018 Don Julien
//Can be used for non-commercial purposes.
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture re-write working on RPi
//ver 1.0a DJ  9/24/17  minor clean up
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas
//ver 1.0.18 DJ  1/9/18  updated for multi-threading, simplified


'use strict'; //find bugs easier
require('colors'); //for console output
const os = require('os');
//const pathlib = require('path');
//const {blocking, wait} = require('blocking-style');
const {debug} = require('./shared/debug');
//const memwatch = require('memwatch-next');
//const {Screen, GpuCanvas, UnivTypes} = require('gpu-friends-ws281x');
const {Screen, GpuCanvas} = require('gpu-friends-ws281x');
//console.log(JSON.stringify(Screen));
//process.exit();

////////////////////////////////////////////////////////////////////////////////
////
/// Config settings:
//

const DURATION = 30; //10; //60; //how long to run (sec)
const PAUSE = 5; //how long to pause at end
const FPS = 30; //2; //animation speed


//display settings:
//NOTE: most of these only apply when GPIO pins are *not* presenting VGA signals (otherwise interferes with WS281X signal timing)
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
//    SHOW_PROGRESS: true, //show progress bar at bottom of screen
    DEV_MODE: true, //disable WS281X formatting on screen (for demo purposes)
//    WS281X_DEBUG: true, //show timing debug info
//    UNIV_TYPE: UnivTypes.CHPLEX_SSR,
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
    TITLE: "PinFinder",
    NUM_WKERS: os.cpus().length - 1, //1 bkg wker for each core, leave 1 core free for evt loop
};
if (OPTS.DEV_MODE) Screen.gpio = true; //force full screen (test only)


//canvas settings:
//const SPEED = 0.5; //animation step speed (sec)
//const FPS = 30; //estimated animation speed
const NUM_UNIV = 24; //#universes; can't exceed #VGA output pins (24) without external mux
const UNIV_LEN = Screen.gpio? Screen.height: 30; //universe length; can't exceed #display lines; show larger pixels in dev mode
debug("Screen %d x %d, is RPi? %d, GPIO? %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

const models = [];
const canvas = new GpuCanvas(NUM_UNIV, UNIV_LEN, OPTS);


////////////////////////////////////////////////////////////////////////////////
////
/// Models/effects rendering:
//

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


//pin finder model/fx:
//generates a different 1-of-N pattern for each GPIO pin
//usage:
//  new PinFinderModel(univ_num, univ_len, color)
//  where:
//    univ_num = universe# (screen column); 0..23; also determines pattern repeat factor
//    univ_len = universe length (screen height)
//    color = RGB color to use for this universe (column)
class PinFinderModel
{
    constructor(univ_num, univ_len, color)
    {
//        this.univ_num = univ_num; //stofs = univ_num * univ_len; //0..23 * screen height (screen column)
        this.pixels = canvas.pixels.slice(univ_num * univlen, univlen); //universe pixels (screen column)
        this.repeat = 9 - (univ & 7); //patterm repeat factor; 9..2
        this.height = univ_len; //universe length (screen height)
        this.color = color;
    }

    render(frnum)
    {
//        if (isNaN(++this.frnum)) this.frnum = 0; //frame# (1 animation step per frame)
        for (var y = 0; y < this.height; ++y)
//, csel = -this.count++ % this.repeat
//            if (csel >= this.repeat) csel = 0;
//if ((t < 3) && !x && (y < 10)) console.log(t, typeof c, c);
//                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
//                    var repeat = 9 - (x % 8);
//                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//                    canvas.pixel(x, y, c? BLACK: color);
//                    canvas.pixels[xofs + y] = ((y - t) % repeat)? BLACK: color;
//            canvas.pixels[univ_ofs + y] = csel? BLACK: this.color;
            this.pixels[y] = ((y - frnum) % this.repeat)? BLACK: this.color;
    }
};


//as above, but for all universes:
class WholeHouseModel
{
    render(frnum)
    {
        for (var x = 0, xofs = 0; x < canvas.width; ++x, xofs += canvas.height)
        {
            var color = [RED, GREEN, BLUE][x >> 3]; //Math.floor(x / 8)];
            var repeat = 9 - (x & 7); //(x % 8);
            for (var y = 0, c = -frnum % repeat; y < canvas.height; ++y, ++c)
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
        }
    }
};


//model/effect rendering:
//use only one of below options

//whole-house on main thread:
models.push(new WholeHouseModel());
//whole-house on bkg thread:
if (false)
models.push(BkgWker(new WholeHouseModel()));
//split up and assign models to multiple bkg threads:
//assign models (effects) to be rendered:
if (false)
for (var x = 0; x < NUM_UNIV; ++x)
    models.push(BkgWker(new MyModel(x, Screen.height, [RED, GREEN, BLUE][x >> 3])));


////////////////////////////////////////////////////////////////////////////////
////
/// Main logic (synchronous coding style to simplify timing logic):
//

//blocking(function*()
//const TRACE = true;
step(function*()
{
    var render_time = 0, idle_time = 0;
    debug("begin: startup took %d sec, now run fx for %d sec".green_lt, trunc(process.uptime(), 10), DURATION);
//    for (var i = 0; i < 100; ++i)
//        models.forEach((model) => { model.render(i); });

//    var started = now_sec();
    canvas.elapsed = 0;
    canvas.duration = DURATION; //set progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
//    var hd = new memwatch.HeapDiff();
    for (var frnum = 0; canvas.elapsed <= DURATION; ++frnum)
    {
        render_time -= canvas.elapsed; //exclude non-rendering (paint + V-sync wait) time
        models.forEach((model) => { model.render(frnum); });
//        yield canvas.render(frnum);
        render_time += canvas.elapsed;
//NOTE: pixel (0, 0) is upper left on screen
//NOTE: the code below will run up to ~ 35 FPS on RPi 2
//        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
/*
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
*/
//        yield wait(started + (t + 1) / FPS - now_sec()); //avoid cumulative timing errors
        idle_time -= canvas.elapsed; //exclude non-waiting time
        yield wait((frnum + 1) / FPS - canvas.elapsed); //throttle to target frame rate; avoid cumulative timing errors
        idle_time += canvas.elapsed;
        canvas.paint();
//        yield wait(1);
//        ++canvas.elapsed; //update progress bar
    }
//    debug("heap diff:", JSON.stringify(hd.end(), null, 2));
    render_time = 1000 * render_time / frnum; //(DURATION * FPS);
    var frtime = 1000 * canvas.elapsed / frnum;
    idle_time = 1000 * idle_time / frnum;
    if (wait.overdue) debug(`overdue frames: ${wait.overdue} (${trunc(100 * wait.overdue / frnum, 10)}%)`.red_lt);
    debug(`avg render time: ${trunc(render_time, 10)} msec (${trunc(1000 / render_time, 10)} fps), avg frame rate: ${trunc(frtime, 10)} msec (${trunc(1000 / frtime, 10)} fps), avg idle: ${trunc(idle_time, 10)} msec (${trunc(100 * idle_time / frtime, 10)}%), ${frnum} frames`.blue_lt);
    debug(`end: ran for ${trunc(canvas.elapsed, 10)} sec, now pause for ${PAUSE} sec`.green_lt);
    yield wait(PAUSE); //pause to show screen stats longer
    canvas.StatsAdjust = DURATION - canvas.elapsed; //-END_DELAY; //exclude pause from final stats
});


//stepper function:
//NOTE: only steps generator function 1 step; subsequent events must also caller step to finish executing generator
function step(gen)
{
try{
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
} catch(exc) { console.log(`step EXC: ${exc}`.red_lt); gen.throw(exc); } //CAUTION: without this function gen might quit without telling anyone
}


//delay next step:
function wait(delay)
{
    delay *= 1000; //sec -> msec
    if (delay <= 1) if (isNaN(++wait.overdue)) wait.overdue = 1;
    debug("wait %d msec"[(delay > 1)? "blue_lt": "red_lt"], trunc(delay, 10));
    return (delay > 1)? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
}


////////////////////////////////////////////////////////////////////////////////
////
/// Helpers:
//

//truncate decimal places:
function trunc(val, digits)
{
    return Math.floor(val * (digits || 1)) / (digits || 1);
}


//high-res elapsed time:
//NOTE: unknown epoch; useful for relative times only
//function elapsed()
//{
//    var parts = process.hrtime();
//    return parts[0] + parts[1] / 1e9; //sec, with nsec precision
//}


//truncate after 3 dec places:
//function milli(val)
//{
//    return Math.floor(val * 1e3) / 1e3;
//}


//truncate after 1 dec place:
//function tenths(val)
//{
//    return Math.floor(val * 10) / 10;
//}


//current time in seconds:
//function now_sec()
//{
//    return Date.now() / 1000;
//}

//eof
