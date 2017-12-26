#!/usr/bin/env node
//DEBUG=*,-speaker,-lame:encoder,-lame:decoder  demos/butt*core.js

//Butterfly demo pattern: tests GPU effects
//based on xLights/NutCracker Butterfly effect
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes.
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture working on RPi
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas
//ver 1.2  DJ  12/20/17  multi-core version: add shared memory, cluster, fg/bkg thread, misc fixes
//ver 1.2a DJ  12/24/17  performance improvements; will now run 24 1K universes @60 FPS with audio on RPi 2
//ver 1.2b DJ  12/26/17  restructure code to consolidate bkg wker mgmt

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output (incl bkg threads)
//const {Worker} = require('webworker-threads');
const cluster = require('cluster');
const JSON = require('circular-json'); //CAUTION: replace std JSON with circular-safe version
const {debug} = require('./shared/debug');
const {mp3playback, mp3len, DefaultAudioFile} = require('./shared/mp3playback');
const {Screen, GpuCanvas} = cluster.isMaster? require('gpu-friends-ws281x'): {Screen: {}, GpuCanvas: {}};
//const {blocking, wait} = require('blocking-style');
//const ary2buf = require('typedarray-to-buffer');
//const {vec3, vec4, mat4} = require('node-webgl/test/glMatrix-0.9.5.min.js');

//display settings:
Screen.gpio = true; //force full screen (test only)
//const SPEED = 1/60; //1/10; //1/30; //animation speed (sec); fps
const FPS = 60; //estimated animation speed; NOTE: won't go faster than video card refresh rate
const NUM_UNIV = !cluster.isMaster? process.env.NUM_UNIV: 24; //can't exceed #VGA output pins unless external mux used
const UNIV_LEN = !cluster.isMaster? process.env.UNIV_LEN: Screen.gpio? Screen.height: 24; //60; //Screen.height; //Math.round(Screen.height / Math.round(Screen.scanw / 24)); ///can't exceed #display lines; for dev try to use ~ square pixels (on-screen only, for debug)
if (cluster.isMaster)
    debug("Screen %d x %d, is RPi? %d, GPIO? %d, #CPU %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//other run-time options:
const MP3 = DefaultAudioFile; //defaults to mp3 file in current folder
const DURATION = MP3? mp3len(MP3): 60; //how long to run (sec)
const MP3BKG = false; //true; //false; //true; //use bkg thread to protect against timing jitter
const NUM_WKERS = 1; //0; //1; //1; //2; //3; //render wkers: 43 fps 1 wker, 50 fps 2 wkers on RPi
const XPARTN = NUM_WKERS? Math.ceil(NUM_UNIV / NUM_WKERS): NUM_UNIV; //too much rendering in fg for 1 CPU @60 FPS; use bkg wkers; assign 12 cols to each wker
const END_PAUSE = 2; //10; //how long to pause at end (only for debug)

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
//    width: NUM_UNIV,
//    height: UNIV_LEN,
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


//bkg wker mgmt:
//use bkg wkers when full render takes longer than target frame time
//render deadline ~= 16 msec @60 FPS or 33 msec @30 FPS
//NOTE: RPi bkg render seems to run 3x as fast as fg render; maybe due to libuv traffic in main event loop?
//var wkers = {};
//var WkerRestart = true;
//var numWkers = Math.ceil(NUM_UNIV / XPARTN); //require('os').cpus().length;
const bkg_wkers =
{
    wkers: {},
    AutoRestart: true,
//create bkg wkers:
    open: function*(canvas)
    {
        if (!NUM_WKERS) return; //no bkg render
/*
cluster.on('online', (wker) =>
{
    debug(`worker ${wker.process.pid} started`.green_lt);
    wkers[wker.process.pid] = wker;
});
cluster.on('disconnect', (wker) =>
{
    debug(`The worker #${wker.id} pid '${wker.process.pid}' has disconnected`.cyan_lt);
});
*/
        cluster.on('message', (wker, msg, handle) =>
        {
//try{
//??        if (arguments.length == 2) { handle = msg; msg = wker; wker = undefined; } //?? shown in example at https://nodejs.org/api/cluster.html#cluster_event_message_1
            var pid = wker.process._handle.pid; //kludge: pid is different place
//console.log("got reply from wker %s, has proc? %d", pid, !!wkers[pid].process, JSON.stringify(msg, null, 2).slice(0, 100));
//    if (!wkers[pid].process) wkers[pid].process = {pid};
            if (this.wkers[pid].process && (this.wkers[pid].process.pid != pid)) throw "wker pid mismatch";
//        if (msg.env) { wkers[pid].process.env = msg.env; delete msg.env; } //save in case caller wants access to wker's env
            step.retval = {msg, wker: this.wkers[pid]}; //{process: {pid, env: wkers[pid].env}}}; //, narg: arguments.length};
//console.log("got reply", JSON.stringify(step.retval).slice(0, 800));
//            if (msg.mp3quit) { console.log("rcv quit"); return mp3playback.done = true; } //just set flag; don't interfere with step
            step(); //wake up caller
//}catch(exc) {console.log(`cluster onmsg EXC: ${exc}`.red_lt); }
        });
        cluster.on('exit', (wker, code, signal) =>
        {
            delete this.wkers[wker.process.pid];
//    var want_restart = (Object.keys(wkers).length < numWkers);
            debug(`worker ${wker.process.pid} died (${code || signal}), restart? ${this.AutoRestart}`.red_lt);
            if (this.AutoRestart) cluster.fork({UNIV_UNIV, UNIV_LEN}); //preserve settings
        });

        debug(`master proc '${process.pid}', starting ${NUM_WKERS} workers`.green_lt);
//divide canvas into vertical partitions:
//NOTE: 24 cols is evenly divisible by 2, 3, 4, and 8 so it is nice for spreading across CPUs/cores
//        debug(`dividing render work into ${NUM_WKERS} partitions`.blue_lt);
//    for (var w in wkers) //wait for all wkers to ack
//    for (var x = MP3BKG? 0-XPARTN: 0; x < NUM_UNIV; x += XPARTN) //wait for all wkers to ack; NOTE: need to use "x" because wkers list not populated yet
//    for (var i = 0; i < NUM_WKERS; ++i)
        for (var x = 0; x < NUM_UNIV; x += XPARTN) //launch bkg wkers
        {
//    cluster.fork({NUM_UNIV: Math.min(NUM_UNIV - x, XPARTN), XPARTNOFS: x, UNIV_LEN});
//        cluster.fork(); //{NUM_UNIV, UNIV_LEN}); //NOTE: might be extra due to round up; make uniform so workers are interchangeable
            var wker = cluster.fork();
            debug(`bkg wker '${wker.process.pid}' forked`.green_lt, JSON.stringify(wker));
            this.wkers[wker.process.pid] = wker;
            wker.on('online', () =>
            {
                debug(`bkg wker started`.green_lt);
                step(); //wake fg thread
//            wkers[wker.process.pid] = wker;
            });
            wker.on('disconnect', () =>
            {
                debug(`bkg wker disconnected`.cyan_lt);
            });
            wker.on('message', (msg) =>
            {
                debug(`got msg ${JSON.stringify(msg)} from bkg wker`.blue_lt);
//send msgs back to caller:
//            if (msg.mp3done) done_cb.apply(null, msg.mp3done);
//            if (msg.mp3progess) progress_cb.apply(null, msg.mp3progress);
            });
        }
        for (var x = 0; x < NUM_UNIV; x += XPARTN) //wait for startup + ack
            var {msg, wker} = yield;
//        console.log(`got wker ack ${JSON.stringify(msg)} from wker '${wker.process.pid}'`); //, env ${JSON.stringify(wker.process.env)}`);
    },
//close bkg wkers:
    close: function*()
    {
        this.AutoRestart = false; //don't want auto-restart
//        for (var x = 0, w = 0; x < NUM_UNIV; x += XPARTN, ++w) //send quit requests
        for (var w in this.wkers) //send quit requests
            this.wkers[w].send({quit: true});
        for (var w in this.wkers) //wait for wkers to ack + quit
            var {msg, wker} = yield;
//        console.log(`wker quit ack ${JSON.stringify(msg)} from wker '${wker.process.pid}'`);
//    bkg(); //wker.postMessage(null); //.close();
    },
};


//main logic:
//written with synchronous coding style to simplify timing and flow
//blocking(function*()
//const TRACE = true;
step(function* main()
{
    var total_time = 0, busy = 0, numreq = 0;
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
//    canvas.pixels = SharedPixels;
//console.log("canvas", Object.keys(canvas));DEBUG=*,-speaker,-lame:encoder,-lame:decoder  demos/butt*core.js
    if (!cluster.isMaster) //bkg wker
    {
        debug(`hello from wker ${process.pid}: #univ ${NUM_UNIV}, univ len ${UNIV_LEN}`.green_lt);
        process.send({im_here: true}); //, env: process.env}); //send back my env so master has a copy
        total_time -= elapsed();
        process.on('message', (msg) =>
        {
//console.log(`wker ${process.pid} got req ${JSON.stringify(msg)}`);
            if (msg.render)
            {
                busy -= elapsed();
                yield* render(numreq++, canvas, msg.XSTART);
                busy += elapsed();
                return;
            }
            if (msg.quit)
            {
                total_time += elapsed();
                busy = milli(busy);
                total_time = milli(total_time);
                var frtime = milli(1000 * busy / numreq);
                process.send({quit: true, busy, total_time, numreq, frtime, '%busy': tenths(100 * busy / total_time)});
                process.exit(0);
                return;
            }
            throw `Unknown msg type: '${Object.keys(msg)}'`.red_lt;
        });
        return;
    }
    yield* bkg_wkers.open(canvas);

//    var all_pixels = new Uint32Array(canvas.width * canvas.height);
//    wker.postMessage("all");
//    var retval = yield bkg({ack: 1});
//    console.log("got from bkg:", retval);
//    console.log("got", yield bkg({eval: butterfly}));
//    bkg.canvas = canvas;

//OPTS.gpufx = true; //TEMP: bypass
//    if (MP3) mp3playback(MP3); //TODO: sync, account for latency, add visualizer; move to bkg process?
    if (MP3) yield mp3playback(MP3); //don't continue until speaker data queued up
    debug("begin, run for %d sec @%d fps".green_lt, DURATION, FPS);
//    var started = now_sec();
    canvas.elapsed = 0; //reset progress bar
    canvas.duration = DURATION * FPS; //set progress bar limit
//    if (OPTS.gpufx) canvas.fill(GPUFX); //generate fx on GPU
//    var hd = new memwatch.HeapDiff();
    total_time -= elapsed();
//NOTE: logic below must render each frame fast enough to sustain target frame rate (30 or 60 fps)
//NOTE: rendering by 1 bkg thread is 3x as fast as fg render, so node.js or libuv or window mgr has *significant* overhead
    for (var t = 0; t <= DURATION * FPS; ++t, ++canvas.elapsed) //update progress bar
    {
//console.log("main loop %d, mp3 done? %s", t, mp3playback.done);
        if (mp3playback.done) { debug("whoops, audio is done".cyan_lt); break; }
//        ofs += BF_opts.reverse? -BF_opts.speed: BF_opts.speed;
//console.log(`parent: render t ${t}/${DURATION * FPS}, ofs ${ofs}`);
//NOTE: pixel (0, 0) is upper left on screen
//NOTE: the code below will run up to ~ 32 FPS on a RPi 2
        busy -= elapsed();
//canvas.pixels[0] = canvas.pixels[1] = canvas.pixels[2] = canvas.pixels[3] = canvas.pixels[4] = canvas.pixels[5] = -1;
//canvas.fill(0x12345678);
        yield* render(t, canvas);
        busy += elapsed(); //time spent actually rendering
//            yield bkg({render: ofs});
//console.log("paint");
        canvas.paint();
//        if (!NUM_WKERS) yield wait(0); //kludge: need to give mp3 player some CPU
//        yield wait((t + 1) / FPS - now_sec() + started); //canvas.elapsed); //avoid cumulative timing errors
//        yield wait(started + (t + 1) / FPS - now_sec()); ///use cumulative time to avoid drift
    }
    total_time += elapsed(); //total processing time
//NOTE: not meaningful with bkg processes
    debug(`total proc time ${milli(total_time)} sec, avg proc time ${milli(1000 * total_time / canvas.elapsed)} msec, %busy ${tenths(100 * busy / total_time)}`.cyan_lt);
    var frtime = 1000 * total_time / canvas.elapsed; //(DURATION * FPS);
    debug(`avg frame gen time: ${tenths(1000 * busy / canvas.elapsed)} msec, frame rate: ${tenths(frtime)} msec (${tenths(1000 / frtime)} fps), ${/*DURATION * FPS*/ canvas.elapsed} frames`.blue_lt);
    yield* bkg_wkers.close();
    debug("end, pause %d sec".green_lt, END_PAUSE);
    yield wait(END_PAUSE); //pause at end so screen doesn't disappear too fast
    canvas.StatsAdjust = -END_PAUSE; //exclude pause in final stats
});


function* render(t, canvas, XSTART)
{
    var ani_ofs = t * BF_opts.speed * BF_opts.reverse? -1: 1;
    if (OPTS.gpufx) return; //generate fx on GPU instead of CPU
//	        for (var x = 0; x < canvas.width; ++x)
//	            for (var y = 0; y < canvas.height; ++y)
//	                canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));

//partial render by bkg wker:
    if (XSTART != undefined)
    {
        for (var x = XSTART, xofs = XSTART * canvas.height; x < Math.min(XSTART + XPARTN, NUM_UNIV); ++x, xofs += canvas.height)
            for (var y = 0; y < canvas.height; ++y)
//                    canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
                canvas.pixels[xofs + y] = butterfly(x, y, ani_ofs, BF_opts);
        canvas.paint();
        return;
    }

    if (!NUM_WKERS) //fg render
    {
        for (var x = 0, xofs = 0; x < canvas.width; ++x, xofs += canvas.height)
        {
            if (!MP3BKG && !(x % 8)) yield wait(0); //kludge: give mp3 playback some CPU cycles
            for (var y = 0; y < canvas.height; ++y)
//+66 msec (~ 25%)	                canvas.pixel(x, y, butterfly(x, y, ofs, BF_opts));
                canvas.pixels[xofs + y] = butterfly(x, y, ani_ofs, BF_opts);
        }
        return;
    }

//delegate render to bkg wkers:
    for (var x = 0, w = 0; x < NUM_UNIV; x += XPARTN, ++w) //send requests
        wker.send({render: true, ani_ofs, XSTART: x}); //, XEND: Math.min(XSTART + XPARTN, canvas.width)});
//                console.log(`sending render ofs ${XOFS} req to wker ${Object.keys(wkers)[w]} in '${Object.keys(wkers)}'`);
//                wkers[Object.keys(wkers)[w]].send({render: true, w, ofs, XSTART, XEND: Math.min(XSTART + XPARTN, canvas.width)});
//            for (var XSTART = 0; XSTART < NUM_UNIV; XSTART += XPARTN) //get responses, overlay into composite pixel array
    for (var x = 0; x < NUM_UNIV; x += XPARTN) //wait for responses; wkers place results into shared pixel array, so this yield is just for sync
        var {msg, wker} = yield;
}


//generator function stepper:
//NOTE: only moves one step
//async callbacks must make additional steps
function step(gen)
{
try{
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
if (done) debug("process %s done".cyan_lt, process.pid);
    return step.retval = value; //return value to caller and to next yield
} catch(exc) { console.log(`step EXC: ${exc}`.red_lt); } //CAUTION: without this function gen might quit without telling anyone
}


//delay next step:
//NOTE: only used for debug at end; everything else is V-synced for precision timing :)
function wait(delay)
{
//console.log("wait %d sec", milli(delay));
    delay *= 1000; //sec -> msec
    return (delay > 1)? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
}


///////////////////////////////////////////////////////////////////////////////
////
/// Butterfly effect (if rendered on CPU instead of GPU)
//

//CPU effects generator:
function butterfly(x, y, ani_ofs, opts)
{
    return argb2abgr(hsv2rgb(sethue(x, y, ani_ofs, opts), 1, 1));
}


//choose color (hue) for butterfloy pattern:
//based on http://mathworld.wolfram.com/ButterflyFunction.html
//PERF: ~ 20 msec
function sethue(x, y, ani_ofs, opts)
{
    y = UNIV_LEN - y - 1; //flip y axis; (0,0) is top of screen
//not needed? axis fixes: fix the colors for pixels at (0,1) and (1,0)
//    if ((x == 0) && (y == 1)) y = y + 1;
//    if ((x == 1) && (y == 0)) x = x + 1;

//    var num = Math.abs((x * x - y * y) * Math.sin(ofs + ((x + y) * Math.PI * 2 / (opts.height + opts.width))));
    const num = /*Math.abs*/((x * x - y * y) * Math.sin(((ani_ofs + x + y) / /*(opts.height + opts.width)*/ (NUM_UNIV + UNIV_LEN) * 2 * Math.PI)));
    const den = x * x + y * y;

    const hue = (den > 0.001)? num / den: 0;
    return (hue < 0)? -hue: hue;
}


//swap RGB byte order:
function argb2abgr(color)
{
//NOTE: bit shuffling is only 1 msec > buf read/write per frame
//    return 0xff000000 | (Math.floor(vec3[0] * 0xff) << 16) | (Math.floor(vec3[1] * 0xff) << 8) | Math.floor(vec3[2] * 0xff);
    var retval = (color & 0xff00ff00) | ((color >> 16) & 0xff) | ((color & 0xff) << 16);
//if (++argb2abgr.count < 10) console.log(color.toString(16), retval.toString(16));
    return retval;
}


//convert color space:
//HSV is convenient for color selection during fx gen
//display hardware requires RGB
function hsv2rgb(h, s, v)
//based on sample code from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
{
    h *= 6; //[0..6]
    const segment = h >>> 0; //(long)hh; //convert to int
    const angle = (segment & 1)? h - segment: 1 - (h - segment); //fractional part
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


//convert (r, g, b) to 32-bit ARGB color:
function toargb(r, g, b)
{
    return (0xff000000 | (r << 16) | (g << 8) | b) >>> 0; //force convert to uint32
}


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helpers
//


//high-res elapsed time:
//NOTE: unknown epoch; used for relative time only, abs time not needed
function elapsed()
{
    var parts = process.hrtime();
    return parts[0] + parts[1] / 1e9; //sec, with nsec precision
}


//truncate after 3 dec places:
function milli(val)
{
//    return Math.floor(val * 1e3) / 1e3;
    return (val * 1e3 | 0) / 1e3;
}


//truncate after 1 dec place:
function tenths(val)
{
//    return Math.floor(val * 10) / 10;
    return (val * 10 | 0) / 10;
}


function hex32(val)
{
    return (val >>> 0).toString(16);
}


//eof
