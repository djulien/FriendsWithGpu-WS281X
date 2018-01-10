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
require('colors').enabled = true; //for console output (all threads)
//const os = require('os');
//const pathlib = require('path');
//const {blocking, wait} = require('blocking-style');
const cluster = require('cluster');
//const JSON = require('circular-json'); //CAUTION: replace std JSON with circular-safe version
const {debug} = require('./shared/debug');
//const memwatch = require('memwatch-next');
//const {Screen, GpuCanvas, UnivTypes} = require('gpu-friends-ws281x');
const {Screen, GpuCanvas/*, cluster*/} = require('gpu-friends-ws281x');
//console.log(JSON.stringify(Screen));
//process.exit();


////////////////////////////////////////////////////////////////////////////////
////
/// Config settings:
//

const DURATION = 5; //30; //10; //60; //how long to run (sec)
const PAUSE = 5; //how long to pause at end
const FPS = 30; //2; //animation speed (max depends on screen settings)


//display settings:
//NOTE: most of these only apply when GPIO pins are *not* presenting VGA signals (ie, dev mode); otherwise, they interfere with WS281X signal fmt
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
//choose one of options below for performance tuning:
//    NUM_WKERS: 0, //whole-house fg render
    NUM_WKERS: 1, //whole-house bg render
//    NUM_WKERS: os.cpus().length, //1 bkg wker for each core
};
if (OPTS.DEV_MODE) Screen.gpio = true; //force full screen (test only)


//canvas settings:
//const SPEED = 0.5; //animation step speed (sec)
//const FPS = 30; //estimated animation speed
const NUM_UNIV = 24; //#universes; can't exceed #VGA output pins (24) without external mux
const UNIV_LEN = Screen.gpio? Screen.height: 30; //universe length; can't exceed #display lines; show larger pixels in dev mode
const WKER_UNIV = OPTS.NUM_WKERS? Math.ceil(NUM_UNIV / OPTS.NUM_WKERS): NUM_UNIV; //#univ to render by each bkg wker
debug("Screen %d x %d, refresh %d Hz, is RPi? %d, GPIO? %d".cyan_lt, Screen.width, Screen.height, trunc(Screen.fps, 10), Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//GPU canvas:
//global to reduce parameter passing (only one needed)
//NOTE: must occur before creating models and bkg wkers (sets up shm, etc)
const canvas = new GpuCanvas(NUM_UNIV, UNIV_LEN, OPTS);


////////////////////////////////////////////////////////////////////////////////
////
/// Models/effects rendering:
//


const models = []; //list of models/effects to be rendered by *this* thread

//TODO: use WKER_UNIV here
if (OPTS.NUM_WKERs > 1) //split up models across multiple bkg threads
    for (var u = 0; u < NUM_UNIV; ++u)
        models.push((canvas.WKER_ID == u >> 3)? new PinFinder({univ_num: u}): canvas.isMaster? new BkgWker(u >> 3): null);
else //whole-house on main or bkg thread
    models.push(/*new*/ BkgWker(/*new*/ PinFinder()));
//    models.push((!OPTS.NUM_WKERS || canvas.isWorker)? new PinFinder(): new BkgWker());
debug(`${canvas.prtype}# ${canvas.WKER_ID} '${process.pid}' has ${models.length} model(s) to render`.blue_lt);


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
//NOTE: use function ctor instead of ES6 class syntax to allow hoisting
function PinFinder(opts)
{
    if (!(this instanceof PinFinder))
    {
        if (OPTS.NUM_WKERS && canvas.isMaster) return opts; //don't need real model; return placeholder
        return new PinFinder(opts); //{univ_num}
    }
    const whole_house = (typeof (opts || {}).univ_num == "undefined");
//        this.univ_num = univ_num; //stofs = univ_num * univ_len; //0..23 * screen height (screen column)
    this.begin_univ = !whole_house? opts.univ_num: 0;
    this.end_univ = !whole_house? opts.univ_num + 1: canvas.width; //single univ vs. whole-house
//        this.pixels = canvas.pixels.slice(begin_univ * UNIV_LEN, end_univ * UNIV_LEN); //pixels for this/these universe(s) (screen column(s))
//        this.repeat = 9 - (univ & 7); //patterm repeat factor; 9..2
//        this.height = univ_len; //universe length (screen height)
//        this.color = (opts || {}).color || WHITE;
//        this.frnum = 0; //frame# (1 animation step per frame)
}
//synchronous render:
PinFinder.prototype.render =
function render(frnum)
{
//        if (isNaN(++this.frnum)) this.frnum = 0; //frame# (1 animation step per frame)
    for (var u = this.begin_univ, uofs = u * canvas.height; u < this.end_univ; ++u, uofs += canvas.height)
    {
        var color = [RED, GREEN, BLUE][u >> 3]; //Math.floor(x / 8)];
        var repeat = 9 - (u & 7); //(x % 8);
        for (var n = 0, c = -frnum % repeat; n < canvas.height; ++n, ++c)
//, csel = -this.count++ % this.repeat
//            if (csel >= this.repeat) csel = 0;
//if ((t < 3) && !x && (y < 10)) console.log(t, typeof c, c);
//                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
//                    var repeat = 9 - (x % 8);
//                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//                    canvas.pixel(x, y, c? BLACK: color);
            canvas.pixels[uofs + n] = ((n - frnum) % repeat)? BLACK: color;
//                canvas.pixels[uofs + n] = csel? BLACK: this.color;
//            this.pixels[y] = ((y - frnum) % this.repeat)? BLACK: this.color;
    }
}


////////////////////////////TODO: move to index.js
//bkg wker:
//treated as a pseudo-model by main thread
//NOTE: use function ctor instead of ES6 class syntax to allow hoisting
function BkgWker(opts)
{
    if (!(this instanceof BkgWker))
    {
        if (OPTS.NUM_WKERS && canvas.isMaster) return new BkgWker(opts);
        return opts; //don't need bkg wker; use model as-is
    }
//    if (!BkgWker.all) BkgWker.all = [];
    this.wker = cluster.fork({WKER_ID: Object.keys(cluster.workers).length}); //BkgWker.all.length});
//    if (all.id) return null; //already have this wker
//    BkgWker.all.push(this);
//    cluster.on('message', (wker, msg, handle) =>
    this.wker.on('message', (msg) =>
    {
//        debug(`got reply ${JSON.stringify(msg)} from bkg wker`.blue_lt);
//no                step(); //wake fg thread
//send msgs back to caller:
//            if (msg.mp3done) done_cb.apply(null, msg.mp3done);
//            if (msg.mp3progess) progress_cb.apply(null, msg.mp3progress);
        console.log(`${canvas.prtype} '${process.pid}' got msg ${JSON.stringify(msg)}`.red_lt);
        step.retval = msg;
        if (AtomicAdd(canvas.extra, 0, -1) == 1) step(); //wake up caller after all outstanding render req completed
    });
//    cluster.on('exit', (wker, code, signal) =>
    this.wker.on('exit', (code, signal) =>
    {
        console.log(`${canvas.prtype} '${process.pid}' died (${code || signal || "unknown"})`.red_lt);
        step.retval = {died: process.pid};
        step();
    });
}
//ipc render async
BkgWker.prototype.render =
function render(frnum)
{
//    models.forEach((model) => { model.render(frnum); });
//        all.forEach(wker => wker.render_req(frnum));
//        all.forEach(wker => wker.render_wait());
//if (canvas.isWorker)
//    Object.keys(cluster.workers).forEach(wkid =>
    AtomicAdd(canvas.extra, 0, 1); //#outstanding render req
    wker.send({render: true, frnum});
//    var reply = yield;
//    debug(`got reply ${JSON.stringify(reply)} from bkg wker`.blue_lt);
}
BkgWker.closeAll = 
function closeAll()
{
    if (!canvas.isMaster) return;
    for (var w in cluster.workers)
        cluster.workers[w].send({quit: true});
//    for (var w in cluster.workers)
//        yield;
}


if (canvas.isWorker)
{
/*
    wait for req;
    models.forEach((model) => { model.render(frnum); });
    reply;
*/
//    throw "TODO".red_lt;
//    debug(`Worker ${process.pid} started`);
    process.on('message', function(msg)
    {
        debug(`Wker ${process.pid} rcv msg ${JSON.stringify(msg)}`.blue_lt);
        if (msg.render)
        {
            debug(`Wker ${process.pid} render ${models.length} models`.blue_lt);
            models.renderAll(msg.frnum);
            process.send({reply: frnum});
            return;
        }
        if (msg.quit)
        {
            debug(`Wker ${process.pid} quit`.blue_lt);
  //          process.send({quite: true});
            process.exit(0);
            return;
        }
        throw `unknown msg type: ${JSON.stringify(msg)}`.red_lt;
  });
}


////////////////////////////////////////////////////////////////////////////////
////
/// Main logic (synchronous coding style to simplify timing logic):
//

step(function*() //onexit)
{
    debug(`begin: ${canvas.prtype} '${process.pid}' startup took %d sec, now run fx for %d sec`.green_lt, trunc(process.uptime(), 10), DURATION);
    var render_time = 0, idle_time = 0;
//    onexit(BkgWker.closeAll);
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
//        models.renderAll(canvas, frnum); //forEach((model) => { model.render(frnum); });
//render all models (synchronous):
        models.forEach(model => { model.render(frnum); }); //bkg wker: sync render; main thread: sync render or async ipc
//        render(frnum);
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
//TODO: bkg wker: set reply status for main (atomic dec); main thread: sync wait and check all ipc reply rcvd
        yield wait((frnum + 1) / FPS - canvas.elapsed); //throttle to target frame rate; avoid cumulative timing errors
        idle_time += canvas.elapsed;
//TODO: bkg wker: wait for ipc req (rdlock?); main thread: gpu update
        canvas.paint();
//        yield wait(1);
//        ++canvas.elapsed; //update progress bar
    }
//    debug("heap diff:", JSON.stringify(hd.end(), null, 2));
    render_time = 1000 * render_time / frnum; //(DURATION * FPS);
    var frtime = 1000 * canvas.elapsed / frnum;
    idle_time = 1000 * idle_time / frnum;
    if (wait.stats) wait.stats.report(); //debug(`overdue frames: ${commas(wait.stats.true)}/${commas(wait.stats.false + wait.stats.true)} (${trunc(100 * wait.stats.true / (wait.stats.false + wait.stats.true), 10)}%), avg delay ${trunc(wait.stats.total, 10)} msec, min delay[${}] ${} msec, max delay[] ${} msec`[wait.stats.false? "red_lt": "green_lt"]);
    debug(`avg render time: ${trunc(render_time, 10)} msec (${trunc(1000 / render_time, 10)} fps), avg frame rate: ${trunc(frtime, 10)} msec (${trunc(1000 / frtime, 10)} fps), avg idle: ${trunc(idle_time, 10)} msec (${trunc(100 * idle_time / frtime, 10)}%), ${frnum} frames`.blue_lt);
    debug(`end: ${canvas.prtype} '${process.pid}' ran for ${trunc(canvas.elapsed, 10)} sec, now pause for ${PAUSE} sec`.green_lt);
    yield wait(PAUSE); //pause to show screen stats longer
    canvas.StatsAdjust = DURATION - canvas.elapsed; //-END_DELAY; //exclude pause from final stats
});


//stepper function:
//NOTE: only steps generator function 1 step; subsequent events must also caller step to finish executing generator
function step(gen)
{
//    if (canvas.isWorker) return; //stepper not needed by bkg threads
try{
//console.log("step");
//    if (step.done) throw "Generator function is already done.";
//	if (typeof gen == "function") gen = gen(); //invoke generator if not already
	if (typeof gen == "function") { setImmediate(function() { step(gen(/*function(cb) { step.onexit = cb; }*/)); }); return; } //invoke generator if not already
//        return setImmediate(function() { step(gen()); }); //avoid hoist errors
    if (gen) step.svgen = gen; //save generator for subsequent steps
//console.log("step:", typeof gen, JSON.stringify_tidy(gen));
	var {value, done} = step.svgen.next(step.retval); //send previous value to generator
//    {step.retval, step.done} = step.svgen.next(step.retval); //send previous value to generator
//    Object.assign(step, step.svgen.next(step.retval)); //send previous value to generator
//console.log("step: got value %s %s, done? %d", typeof value, value, done);
//    step.done = done; //prevent overrun
    if (done) step.svgen = null; //prevent overrun; do bedore value()
    if (typeof value == "function") value = value(); //execute caller code before returning
    if (done /*&& step.onexit*/) BkgWker.closeAll(); //step.onexit(); //do after value()
    return step.retval = value; //return value to caller and to next yield
} catch(exc) { console.log(`step EXC: ${exc}`.red_lt); gen.throw(exc); } //CAUTION: without this function gen might quit without telling anyone
}


//delay next step:
function wait(delay)
{
    if (canvas.isWorker) return; //main thread will send wakeup msg

    delay *= 1000; //sec -> msec
    const overdue = (delay <= 1); //1 msec is too close for comfort; treat it as overdue
//    if (isNaN(++wait.counts[overdue])) wait.counts = [!overdue, overdue];
//    if (overdue) if (isNaN(++wait.overdue_count)) wait.overdue_count = 1;
    if (wait.stats)
    {
        wait.stats.total_delay += Math.max(delay, 0);
        if (delay > wait.stats.max_delay) { wait.stats.max_delay = delay; wait.stats.max_when = wait.stats.false + wait.stats.true; }
        if (delay < wait.stats.min_delay) { wait.stats.min_delay = delay; wait.stats.min_when = wait.stats.false + wait.stats.true; }
    }
    else wait.stats =
    {
        false: 0, //non-overdue count
        true: 0, //overdue count
        total_delay: Math.max(delay, 0),
        max_delay: delay,
        max_when: 0,
        min_delay: delay,
        min_when: 0,
        get count() { return this.false + this.true; },
        report: function() { debug(`overdue frames: ${commas(this.true)}/${commas(this.count)} (${trunc(100 * this.true / this.count, 10)}%), avg delay ${trunc(this.total_delay / this.count, 10)} msec, min delay[${this.min_when}] ${trunc(this.min_delay, 10)} msec, max delay[${this.max_when}] ${trunc(this.max_delay, 10)} msec`[this.true? "red_lt": "green_lt"]); },
    }; //{[overdue]: 1, [!overdue]: 0}; //false: 0, true: 1]: [1, 0]; //[!overdue, overdue];
    if ((wait.stats[overdue]++ < 10) || overdue) //reduce verbosity: only show first 10 non-overdue
//    {
        debug("wait[%d] %d msec"[overdue? "red_lt": "blue_lt"], wait.stats.false + wait.stats.true - 1, commas(trunc(delay, 10))); //, JSON.stringify(wait.counts));
//        if (isNaN(++wait.counts[overdue])) wait.counts = [!overdue, overdue];
//    }
    return !overdue? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
}
//const x = {};
//if ((++x.y || 0) < 10) console.log("yes-1");
//else console.log("no-1");
//if (++x.z > 10) console.log("yes-2");
//else console.log("no-2");
//process.exit();


////////////////////////////////////////////////////////////////////////////////
////
/// Helpers:
//


//display commas for readability (debug):
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return val.toLocaleString();
}


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
