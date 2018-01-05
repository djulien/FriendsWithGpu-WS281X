#!/usr/bin/env node
//sudo DEBUG=*,-ref,-ref:struct,-ref:array demos/friends.js

'use strict';
require('colors').enabled = true; //console output colors; allow bkg threads to use it also
const os = require('os');
//require('require-rebuild')(); //kludge: work-around for "module was compiled against a different Node.js version" errors
const cluster = require('cluster');
const {debug} = require('./shared/debug');
//const Semaphore = require('posix-semaphore');
//const shm = require('shm-typed-array');
//const fs = require('fs');
//const mmapio = require('mmap-io');
//const framebuffer = require('framebuffer');
//const {Screen, GpuCanvas, shmbuf} = require('gpu-friends-ws281x');
//const {GpuCanvas, SimplerCanvas} = require('gpu-friends-ws281x');
const GpuCanvas = require('gpu-friends-ws281x').SimplerCanvas;
const Screen = cluster.isMaster? require('gpu-friends-ws281x').Screen: {}; //avoid extra SDL_init for bkg wkers
//const Screen = require('gpu-friends-ws281x').Screen;
const usleep = require('gpu-friends-ws281x').usleep;

const NUM_UNIV = cluster.isWorker? +process.env.NUM_UNIV: 24;
const UNIV_LEN = cluster.isWorker? +process.env.UNIV_LEN: Screen.gpio? Screen.height: 24; //60; //Screen.height; //Math.round(Screen.height / Math.round(Screen.scanw / 24)); ///can't exceed #display lines; for dev try to use ~ square pixels (on-screen only, for debug)
const NUM_WKER = 1; //os.cpus().length - 1 +3; //leave 1 core for render, audio, ui
const WKER_UNIV = NUM_WKER? Math.ceil(NUM_UNIV / NUM_WKER): NUM_UNIV; //#univ rendered by each bkg wker
const FPS = cluster.isWorker? +process.env.FPS: Screen.fps; //1000000 / ((UNIV_LEN + 4) * 30);

const EPOCH = cluster.isWorker? +process.env.EPOCH: elapsed(); //use master for shared time base
const WKER_CFG = cluster.isWorker? JSON.parse(process.env.WKER_CFG): {};
if (!WKER_CFG.wkinx) WKER_CFG.wkinx = 0;
if (!WKER_CFG.first_univ) WKER_CFG.first_univ = 0;
WKER_CFG.last_univ = Math.min(NUM_UNIV, WKER_CFG.first_univ + WKER_UNIV);

//show extra debug info:
//NOTE: these only apply when GPIO pins are *not* presenting VGA signals (otherwise interferes with WS281X timing)
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
    WANT_SHARED: false,
//    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info
//    gpufx: pathlib.resolve(__dirname, "butterfly.glsl"), //generate fx on GPU instead of CPU
};

//const shmKey = parseInt(process.env.SHM_KEY);
//const SHM_KEY = cluster.isWorker? +process.env.SHM_KEY: 0x2811;
//console.log("shm key %s", SHM_KEY.toString());
//const shmbuf = cluster.isWorker? shm.get(SHM_KEY): shm.create(NUM_UNIV * UNIV_LEN + 1, "Uint32Array", SHM_KEY);
//const SHMKEY = 0xfeed; //make value easy to find (for debug)
//const shmbuf = new Uint32Array(shmbuf(SHMKEY, (NUM_UNIV * UNIV_LEN + 1) * Uint32Array.BYTES_PER_ELEMENT)):
const canvas = new GpuCanvas("Friends", NUM_UNIV, UNIV_LEN, OPTS);

//const cpu = new Semaphore('mySema-cpu', { xdebug: true, xsilent: true, value: os.cpus().length - 1});
//const idle = new Semaphore('mySema-idle', { xdebug: true, xsilent: true, value: os.cpus().length - 1});
if (cluster.isMaster) step(master());
else if (cluster.isWorker) worker();
else throw "What am i?".red_lt;


function* master()
{
//    var fb = new framebuffer("/dev/fb0");
//    for (var y = 10; y < 100; ++y)
//        for (var x = 10; x < 100; ++x)
//            fb.fbp[fb.line_length * y + x] = 0xff00ff00;
//    setTimeout(function() { process.exit(0); }, 10000); return;

    const DURATION = 2;
//    master.AutoRestart = true;
//    sema.acquire();
    console.log(`master hello, shm key 0x%s, launch ${NUM_WKER} wker(s) (${os.cpus().length} core(s)) @${micro(elapsed(EPOCH))} sec`.green_lt, 0); //shmbuf.key.toString(16));
//    bufParent.write('hi there'.pink_lt);
//    setTimeout(function() { sema.release(); }, 5000);

    var time = 0, time2 = 0;
    for (var i = 0; i < 1000; ++i)
    {
        time -= elapsed();
        time2 += usleep(2500);
        time += elapsed();
    }
    console.log(`2500 usec delay took ${time} or ${time2} msec avg`.blue_lt);
return;

    canvas.fill(0xffff0000);
    canvas.paint(canvas.pixels);
    yield wait(1);
    canvas.fill(0xff00ff00);
    canvas.paint(canvas.pixels);
    yield wait(1);
    canvas.fill(0xff0000ff);
    canvas.paint(canvas.pixels);

    yield wait(1);
    canvas.fill(0);
    canvas.paint(canvas.pixels);
//return;

//set exit handler < fork in case wker dies immediately:
    cluster.on('exit', (wker, code, signal) =>
    {
//console.log(cluster.workers);
//console.log(wker);
        console.log(`wker[${wker.id}], cfg ${JSON.stringify(wker.cfg)}, pid '${wker.process.pid}' died (${code || signal}), restart? ${master.AutoRestart}`.red_lt);
        fork(wkid.cfg); //preserve cfg for process affinity
    });
    for (var w = 0; w < NUM_WKER; ++w) fork({first_univ: w * WKER_UNIV, inx: w}); //launch bkg wkers

//    setTimeout(() => { child.kill('SIGINT') }, 10000);
    for (var frnum = 0; frnum < DURATION * FPS; ++frnum)
    {
console.log(`master render[${frnum}]`.blue_lt);
        render(frnum);
console.log(`master paint[${frnum}]`.blue_lt);
        paint();
    }
}


function worker()
{
//    const sema = new Semaphore('mySemaphore', { xdebug: true, xsilent: true });
//    const shmKey = parseInt(process.env.SHM_KEY);
//    const bufChild = shm.get(shmKey);
    console.log(`wker hello, cfg ${JSON.stringify(WKER_CFG)}, pid '${process.pid}' started @${micro(elapsed(EPOCH))}`.green_lt);
  
    process.on("message", (msg) =>
    {
        if (msg.quit) return quit();
        throw `Unhandled message: ${JSON.stringify(msg)}`.red_lt;
    });

//    cpu.acquire();
//console.log(`wker '${process.pid}' acq @${elapsed(EPOCH)}`.cyan_lt);
//    console.log(bufChild.toString());
//setTimeout(function() { console.log(`child '${process.pid}' rel @${elapsed(EPOCH)}`.blue_lt); cpu.release(); process.exit(0); }, 3000);
//    setTimeout(function() { quit(); }, 2000);
    for (var frnum = 0;; ++frnum)
    {
console.log(`child '${process.pid}' render[${frnum}]`.blue_lt);
        render(frnum);
console.log(`child '${process.pid}' paint[${frnum}]`.blue_lt);
        paint();
    }
}


//partial render:
//each wker will render a group of universes
function render(frnum)
{
    if (cluster.isMaster)
    {
        /*shmbuf*/ canvas.pixels[NUM_UNIV * UNIV_LEN] = frnum; //tell wkers which frame to render
        if (NUM_WKER) return;
    }

    if (/*shmbuf*/ canvas.pixels[NUM_UNIV * UNIV_LEN] != frnum) throw `render out of sync: expected fr ${frnum}, got fr ${/*shmbuf*/ canvas.pixels[NUM_UNIV * UNIV_LEN]}`.red_lt;
    for (var x = WKER_CFG.first_univ, xofs = x * UNIV_LEN; x < WKER_CFG.last_univ; ++x, xofs += UNIV_LEN)
        for (var y = 0; y < 20; ++y)
            /*shmbuf*/ canvas.pixels[xofs + y] = (WKER_CFG.inx << 16) | frnum; //TEST ONLY
}


//wait for paint to finish:
//paint is V-synced to prevent tearing; refresh rate will exactly match screen
function paint()
{
    if (cluster.isMaster)
    {
        var frnum = canvas.pixels[NUM_UNIV * UNIV_LEN];
        validate(frnum);
        canvas.paint(canvas.pixels); //encode and paint; NOTE: does not return until V-sync
        return;
    }
    canvas.paint(); //dummy paint to sync with master; NOTE: does not return until V-sync
}


//validate rendered data (debug only):
function validate(frnum)
{
    for (var x = 0, xofs = 0; x < NUM_UNIV; ++x, xofs += UNIV_LEN)
    {
        var data = (Math.floor(x / WKER_UNIV) << 16) | frnum; //TEST ONLY
        for (var y = 0; y < 10; ++y)
            if (canvas.pixels[xofs + y] != data)
                throw `incorrect render: canvas[${x},${y}] = ${canvas.pixels[xofs + y]}, should be ${data}`.red_lt;
    }
}


//launch a wker process:
function fork(cfg)
{
    if (!cluster.isMaster) throw "Not master".red_lt;
    if (master.AutoRestart === false) return;

    if (isNaN(++fork.count)) fork.count = 1;
    if (fork.count > 100) throw "Too many forks".red_lt;
    var wker = cluster.fork({NUM_UNIV, UNIV_LEN, EPOCH, FPS, WKER_CFG: JSON.stringify(cfg)}); //, SHM_KEY});
//        console.log("forked", wker); //cluster.workers); //cluster[1..4].id, .process.pid
    wker.cfg = cfg; //cache wker cfg
}


function quit()
{
    if (cluster.isMaster)
    {
        master.AutoRestart = false;
        for (var w in cluster.workers)
        {
            cluster.workers[w].send({quit: true});
            cluster.workers[w].destroy();
        }
    }
    process.exit();
}


//generator function stepper:
//NOTE: only moves one step
//async callbacks must make additional steps
function step(gen)
{
try{
//console.log("step");
	if (typeof gen == "function") { setImmediate(function() { step(gen()); }); return; } //invoke generator if not already
    if (gen) step.svgen = gen; //save generator for subsequent steps
	var {value, done} = step.svgen.next(step.retval); //send previous value to generator
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


//high-res elapsed time:
//NOTE: use for relative times only (unknown epoch)
function elapsed(epoch)
{
    var parts = process.hrtime();
    var now = parts[0] + parts[1] / 1e9; //sec, with nsec precision
    return now - (epoch || 0);
}


function micro(val)
{
    return Math.floor(val * 1e6) / 1e6;
}

//eof
