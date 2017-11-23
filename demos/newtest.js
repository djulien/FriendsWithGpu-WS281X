#!/usr/bin/env node
//GpuStream tests

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
//Date.now_usec = require("performance-now"); //override with hi-res version (usec)
const {now, elapsed, milli} = require('./shared/elapsed');
const {isRPi, Screen, Modes, GpuCanvas, GpuStream, GpuEncoder, GpuDecoder} = require("gpu-stream");
//const GpuTestStream = require("./GpuTestStream");
console.log("req", require("gpu-stream"));

const FPS = 60;
const DURATION = 60-50; //sec
//const NUM_UNIV = 24;
//const UNIV_LEN = Screen.height;
//const PIVOT = true;
const NUM_UNIV = 4;
const UNIV_LEN = 4;
const MODE = Modes.DEV;
//TODO canvas.Type = 1; //Chipiplexed SSRs

console.error(`screen ${Screen.width} x ${Screen.height}, #univ ${NUM_UNIV}, univ len ${UNIV_LEN}, live mode? ${MODE}, isRPi? ${isRPi}`.blue_lt);

//primary colors (ARGB):
const BLACK = 0xff000000; //NOTE: need A set for color to take effect
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const CYAN = 0xff00ffff;
const MAGENTA = 0xffff00ff;
const WHITE = 0xffffffff;

const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE];


////////////////////////////////////////////////////////////////////////////////
////
/// Direct test: test pattern -> gpu stream
//

const pixels = new Uint32Array(NUM_UNIV * UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
//this.pixel_view32 = new DataView(this.pixel_buf); //for faster access (quad bytes); //caller-selectable byte order

const canvas = new GpuCanvas("GpuFriends2017", NUM_UNIV, UNIV_LEN, MODE);
console.log("pivot = %d", canvas.pivot);

var timer; //= setInterval(function() { /*console.log("toggle")*/; canvas.pivot = !canvas.pivot; }, 100);

//burn some CPU cycles:
function fibo(n) { return (n < 3)? n: fibo(n - 1) + fibo(n - 2); }

step(function*()
{
    elapsed(0); //set time base
    console.log(`loop for ${DURATION} sec (${DURATION * FPS} frames) ...`.green_lt);
    for (var xy = 0; xy < DURATION * FPS; ++xy)
    {
        var x = Math.floor(xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru
//    if (eventh(1)) break; //user quit
        var color = PALETTE[(x + y + Math.floor(xy / pixels.length)) % PALETTE.length]; //vary each cycle
//        fibo(25); //simulate some CPU work
        pixels[xy % pixels.length] = color;
        canvas.paint(pixels); //, cb, {fr: xy, started: Date.now_usec()}); //60 FPS
//        SDL_Delay(1000);
        yield xy; //give event queue some time
    }
    console.log(`done after ${milli(elapsed())} sec, now wait 5 more sec`.green_lt);
    yield wait(5);
});
console.log(`exit after ${milli(elapsed())} sec`.green_lt);
setTimeout(function() { if (timer) clearInterval(timer); }, (DURATION + 1) * 1000);

//pixels.fill(0xffff00ff);
//canvas.paint(pixels);
//setTimeout(function() { pixels.fill(0xff00ffff); canvas.paint(pixels); }, 2000);
//setTimeout(function() { canvas.release(); }, 5000);


//step thru generator function:
function step(gen, wait)
{
    if (typeof gen == "function") gen = gen();
    if (typeof gen != "undefined") step.gen = gen;
    for (;;)
    {
        var {value, done} = step.gen.next(step.retval);
//console.log("step got: {val %d, done %d}", value, done);
        if (typeof value == "function") value = value(); //execute action from caller
        else if (!done) setImmediate(step); //schedule another step *after* event queue is processed (process.nextTick would step *before* other events)
        if (done) { delete step.gen; return value; } //prevent overrun
        if (!wait) return value;
//??        process.nextTick(step, gen); //do another step *before* processing events
        throw "sync step not implemented".red_lt; //TODO: do something that will block thread but not event queue
    }
}


//async wait function:
//delays next step of generator
function wait(msec)
{
    return setTimeout.bind(null, step, msec); //NOTE: need to return a function so step() will evaluate it outside of generator
}


////////////////////////////////////////////////////////////////////////////////
////
/// Full test path: test pattern -> encoder -> decoder -> gpu stream
//

function later()
{
elapsed(0); //set time base
console.error("running GpuStream test ...".green_lt);
new GpuTestStream({NUM_UNIV, UNIV_LEN, FPS, DURATION, SPEED: 0}) //run full speed to check if back pressure works
    .pipe(new GpuDecoder())
    .once("config", function(opts)
    {          
        debug("config opts".blue_lt, opts); //.title, .num_univ, .univ_len, .pivot, .fps, etc
        this.pipe(new GpuStream(opts)) //{title: "GpuStream test", NUM_UNIV, UNIV_LEN, PIVOT}))
            .on('open', function() { cb("open".blue_lt); }.bind(this)) //pbstart = elapsed(); /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
            .on('progress', function(progress) { cb.call(this, "progress".cyan_lt, progress); }.bind(this)) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
            .on('flush', function() { cb.call(this, "flush".blue_lt); }.bind(this)) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
            .on('close', function() { cb("close".blue_lt); }) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker closed".yellow_light, timescale(elapsed() - started)); });
//            elapsed(0);
        cb("video start".green_lt);
    })
    .on('end', function() { cb("test stream done".green_lt); }) //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
    .on('error', function(err) { cb(`ERROR: ${err}`.red_lt); }); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });
console.error("test finishes asynchronously".green_lt);


function cb(msg, details)
{
    var gpustm = ((this || {})._readableState || {}).pipes;
    if (Array.isArray(gpustm)) gpustm = gpustm[0];
    if (msg.match(/open/i))
        debug(`(open) num univ ${gpustm.NUM_UNIV}, univ len ${gpustm.UNIV_LEN}, pivot ${gpustm.PIVOT}, fps ${gpustm.FPS} => pxSize ${gpustm.NUM_UNIV * gpustm.UNIV_LEN}`.pink_lt);
    if (msg.match(/progress/i))
    {
//        var chunksize = spkr.blockAlign * spkr.samplesPerFrame;
//        var sampsize = spkr.channels * Math.ceil(spkr.bitDepth / 8);
//        var missing = chunksize - details.wrlen; //last buf was not full; pad it to make calculations simpler
//        if (isNaN(cb.missing += missing)) cb.missing = missing; //store cumulative

//        details.wps = milli(details.numwr / elapsed()); ///#writes per sec
//        details.numsamp = (details.wrtotal + cb.missing) / sampsize;
//        details.numfr = details.numsamp / spkr.samplesPerFrame;
//        if (details.numwr == details.numfr) delete details.numwr; //remove redundant info
//        details.datatime = milli(details.numsamp / spkr.sampleRate); ///#sec (nearest msec)
//        details.delta = milli(details.datatime - elapsed()); //latency (nearest msec)
//        "wrtotal,wrlen,buflen,numsamp,datatime".split(",").forEach(name => { delete details[name]; }); //remove unneeded data
        debug(`progress ${JSON.stringify(details).replace(/"/g, "").replace(/,/g, ", ")}`.pink_lt);
        return;
    }
    debug(`elapsed ${milli(elapsed())}, msg ${msg}`.pink_lt);
//if (this) console.log("decoder".pink_lt, this);
}
}

//EOF
