#!/usr/bin/env node
//Javascript wrapper to expose GpuCanvas class and Screen config object, manage threads.
//Ideally, Node.js would support multiple threads within the same process so memory could be shared.
//However, only the foreground thread can use the event loop or v8 so use we are forced to use multiple processes instead.
//Shared memory is used to cut down on inter-process data communication overhead.
//That leaves IPC latency as the main challenge.  (60 FPS only allows 16 msec total for fx gen + render)

"use strict"; //find bugs easier
require('colors').enabled = true; //console output colors; allow bkg threads to use it also
const fs = require('fs');
const os = require('os');
const glob = require('glob');

const {PNG} = require('pngjs');
const pathlib = require('path');
//const Emitter = require('events');
const cluster = require('cluster');
const clock = require('posix-clock');
const shm = require('shm-typed-array');
//const {inherits} = require('util');
//const bindings = require('bindings');
const getOwnPropertyDescriptors = require('object.getownpropertydescriptors'); //ES7 shim (not available in Node.js yet)
const {Screen, GpuCanvas, UnivTypes, /*shmbuf, AtomicAdd,*/ usleep, RwlockOps, rwlock} = require('./build/Release/gpu-canvas.node'); //bindings('gpu-canvas'); //fixup(bindings('gpu-canvas'));
const {debug} = require('./demos/shared/debug');
//lazy-load Screen props to avoid unneeded SDL inits:
//module.exports.Screen = {};
//Object.defineProperties(module.exports.Screen,
/*
if (false) //not needed now that Screen doesn't call SDL_Init
module.exports.Screen =
new Proxy(function(){},
{
    get: function(target, propkey, rcvr)
    {
//        debug("get Screen.%s, cached? %s".blue_lt, propkey, typeof target[propkey]);
        if (typeof target[propkey] != "undefined") return target[propkey]; //use cached value, screen won't change
        switch (propkey)
        {
            case "gpio":
                return target[propkey] = toGPIO();
            case "isRPi":
                return target[propkey] = isRPi();
            default:
                if (!target._inner) target._inner = bindings("gpu-canvas").Screen; //calls SDL_Init()
                if (propkey == "toJSON") return function() { return target._inner; }; //kludge; see https://github.com/tvcutsem/harmony-reflect/issues/38
//                return target._inner[prop];
                return Reflect.get(target._inner, propkey, rcvr);
        }
    },
//    has: function(target, prop)
//    {
//        debug("has Screen.%s? %d", prop, typeof target[prop] != "undefined");
//        return typeof target[prop] != "undefined";
//    },
//    ownKeys: function(target)
//    {
//        debug("own keys");
//    },
});
*/
/*
Object.defineProperty(module.exports, "Screen",
{
//define getter so Screen can be lazy-loaded (to avoid extraneous SDL_init):
    get: function()
    {
//const {caller} = require('./demos/shared/caller');
//        console.log("mod exp", caller(1));
        const {Screen} = bindings('gpu-canvas'); //fixup(bindings('gpu-canvas'));
        Screen.gpio = toGPIO();
        Screen.isRPi = isRPi();
//adapted from node-framebuffer:
//        const ioctl = require('ioctl');
//        const fbstruct = require('framebuffer/lib/structs');
//        const fbconst = require('framebuffer/lib/constants');

//        const vinfo = new fbstruct.Vinfo(); //variable display info struct
//        var fbfd = fs.openSync("/dev/fb0", 'r');
//        var ret = ioctl(fbfd, fbconst.FBIOGET_VSCREENINFO, vinfo.ref());
//        fs.closeSync(fbfd);
//        if (ret) throw new Error("ioctl error".red_lt);
//  xres: ref.types.uint32,
//  yres: ref.types.uint32,
//  xres_virtual: ref.types.uint32,
//  yres_virtual: ref.types.uint32,
//  xoffset: ref.types.uint32,
//  yoffset: ref.types.uint32,
//  bits_per_pixel: ref.types.uint32,

//  pixclock: ref.types.uint32,
//  left_margin: ref.types.uint32,
//  right_margin: ref.types.uint32,
//  upper_margin: ref.types.uint32,
//  lower_margin: ref.types.uint32,
//  hsync_len: ref.types.uint32,
//  vsync_len: ref.types.uint32,
//        const Screen = vinfo; //{};
//        Screen.bits_per_pixel = vinfo.bits_per_pixel || 32;
//        if (vinfo.pixclock == 0xffffffff) vinfo.pixclock = 0; //kludge: fix bad value
//        Screen.pixclock = vinfo.pixclock || 50000000;
//        Screen.hblank = (vinfo.left_margin + vinfo.hsync_len + vinfo.right_margin) || Math.floor(Screen.width / 23.25);
//        Screen.vblank = (vinfo.upper_margin + vinfo.vsync_len + vinfo.lower_margin) || Math.floor(Screen.height / 23.25);
//        Screen.rowtime = (Screen.width + Screen.hblank) / Screen.pixclock; //must be ~ 30 usec for WS281X
//        Screen.frametime = (Screen.height + Screen.vblank) * Screen.rowtime;
//        Screen.fps = 1 / Screen.frametime;
//        Object.defineProperty(module.exports, "Screen", Screen); //info won't change; reuse it next time
debug("Screen info %s".blue_lt, JSON.stringify(Screen, null, 2));
        return Screen;
    },
    configurable: true, //allow modify/delete by caller
});
*/
module.exports.Screen = Screen;
module.exports.UnivTypes = UnivTypes;
//also expose these in case caller wants to use them:
//module.exports.shmbuf = shmbuf;
//module.exports.AtomicAdd = AtomicAdd;
//module.exports.rwlock = rwlock;
//module.exports.RWLOCK_SIZE32 = RWLOCK_SIZE32;
module.exports.usleep = usleep;
//attach config info as properties:
//no need for callable functions (config won't change until reboot)
module.exports.Screen.gpio = toGPIO();
module.exports.Screen.isRPi = isRPi();
//module.exports.GpuCanvas = GpuCanvas_shim;

//module.exports.atomic =
//function atomic(cb)

//display modes:
//module.exports.Modes =
//{
//    LIVE: true, 
//    DEV: false,
//};
//default config settings (caller can override):
//        NUM_UNIV: 24,
//        get UNIV_LEN() { return this.Screen.height; },
//        FPS: 60,
//        LATENCY: 1.4, //sec
//        get MODE() { return this.DEV; },


//check for RPi:
function isRPi()
{
    if (isRPi.hasOwnProperty('cached')) return isRPi.cached;
//TODO: is there a better way to check?
    return isRPi.cached = fs.existsSync("/boot/config.txt");
//    return isRPi.cached = (glob.sync("/boot/config.txt") || []).length;
}


//get number of VGA redirected GPIO pins:
//TODO: read memory; for now, just query device tree overlays
function toGPIO()
{
    if (toGPIO.hasOwnProperty('cached')) return toGPIO.cached;
//check if various device tree overlays are loaded:
//dpi24 or vga565/666 send video to GPIO pins
    var dpi24 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/dpi24*") || [];
    if (dpi24.length) return toGPIO.cached = 8+8+8;
    var vga666 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga666*") || [];
    if (vga666.length) return toGPIO.cached = 6+6+6;
    var vga565 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga565*") || [];
    if (vga565.length) return toGPIO.cached = 5+6+5;
//console.log("dpi", dpi);
    return toGPIO.cached = false;
}


//TODO:
//options for when OpenGL support is re-enabled:
const LIVEOK = 1, DEVONLY = 2;
const OPTS =
{
//    SHOW_INTRO: 10, //how long to show intro (on screen only)
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
//    WANT_SHARED: [LIVEOK, true], //share same canvas with child processes/threads
    SHOW_PROGRESS: [DEVONLY, true], //show progress bar at bottom of screen
    UNIV_TYPE: [LIVEOK, UnivTypes.WS281X], //set default univ type
//    WS281X_FMT: [LIVEOK, true], //force WS281X formatting on screen
    DEV_MODE: [LIVEOK, false], //apply WS281X or other formatting
    AUTO_PLAY: [LIVEOK, true], //start playback
    WANT_STATS: [LIVEOK, true], //run-time perf stats
//    WS281X_DEBUG: true, //show timing debug info
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
//    SHM_KEY: [LIVEOK, 0xface], //makes it easier to find in shm (for debug)
    EXTRA_LEN: [LIVEOK, 0], //caller-requrested extra space for frame#, #wkers, timestamp, etc
    TITLE: [DEVONLY, "GpuCanvas"], //only displayed in dev mode
    NUM_WKERS: [LIVEOK, os.cpus().length], //create 1 bkg wker for each core
    FPS: [LIVEOK, Screen.fps], //allow caller to throttle back frame render speed
};


//ARGB colors:
const BLACK = 0xff000000; //NOTE: need A to affect output
const WHITE = 0xffffffff;

const U32SIZE = Uint32Array.BYTES_PER_ELEMENT; //reduce verbosity
const MAGIC = 0x1234beef; //used to detect if shared memory is valid
const HDRLEN = 2; //magic + sync count

const QUIT = -1000; //0x7fffffff; //uint32 value to signal eof


//const jsGpuCanvas =
//module.exports.newGpuCanvas =
//function jsGpuCanvas(width, height, opts)
//{
//    if (!(this instanceof jsGpuCanvas)) return new jsGpuCanvas(width, height, opts);
//    if (cluster.isMaster) GpuCanvas.call(this, width, height, opts);
//    else 
//    Emitter.call(this); //super
//}
//inherits(GpuCanvas, Emitter);


//simpler shared canvas:
//proxy wrapper adds shared semantics (underlying SimplerCanvas is not thread-aware)
//proxy also allows methods and props to be intercepted
//for proxy info see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
//const GpuCanvas =
module.exports.GpuCanvas =
//function SharedCanvas(width, height, opts)
//class SharedCanvas extends SimplerCanvas
new Proxy(function(){},
//class GpuCanvas extends EventEmitter
{
    construct: function GpuCanvas_jsctor(target, args, newTarget) //ctor args: width, height, opts
//    constructor(args) //ctor args: width, height, opts
    {
//    if (!(this instanceof GpuCanvas_shim)) return new GpuCanvas_shim(title, width, height, opts); //convert factory call to class
//console.log("args", JSON.stringify(args));
        const [width, height, opts] = args;
//console.log("proxy ctor", width, height, opts);
        opts_check(opts);
        const title = firstOf((opts || {}).TITLE || OPTS.TITLE[1], "(title)");
        const num_wkers = firstOf((opts || {}).NUM_WKERS, OPTS.NUM_WKERS[1], 0);
        if (num_wkers > os.cpus().length) debug(`#wkers ${num_wkers} exceeds #cores ${os.cpus().length}`.yellow_lt);
//        GpuCanvas.call(this, title, width, height, !!opts.WS281X_FMT); //initialize base class
        const THIS = cluster.isMaster /*|| !num_wkers)*/? new GpuCanvas(title, width, height): {title, width, height}; //initialize base class; dummy wrapper for wker procs
        THIS.isMaster = cluster.isMaster; //(THIS.WKER_ID == -1);
        THIS.WKER_ID = cluster.isWorker? +process.env.WKER_ID: -1; //which wker thread is this
        THIS.prtype = cluster.isMaster? "master": "worker";
    //    this.isWorker = cluster.isWorker; //(THIS.WKER_ID != -1);
        debug(`GpuCanvas: ${THIS.prtype} '${process.pid}' is wker# ${THIS.WKER_ID}/${num_wkers}`.pink_lt);
        memory.call(THIS, opts, num_wkers);
//        Emitter.call(THIS); //event emitter mixin
//        Object.keys(Emitter.prototype).forEach(method => { THIS.prototype[method] = Emitter.prototype[method]; });
//        pushable.call(THIS, Object.assign({}, supported_OPTS, opts || {}));
//console.log("initial props:", THIS.WS281X_FMT, THIS.SHOW_PROGRESS, opts.UNIV_TYPE);
//if (opts.UNIV_TYPE) console.log("set all unv type to", opts.UNIV_TYPE);
//TODO
//        THIS.UnivType = THIS.UnivType_tofix;
//        if (THIS.UNIV_TYPE) for (var i = 0; i < THIS.width; ++i) THIS.UnivType(i, THIS.UNIV_TYPE);

//        if (cluster.isMaster)
//        for (var w = 0; w < num_wkers; ++w) cluster.fork({WKER_ID: w});
//NOTE: lock is left in shm; no destroy (not sure when to destroy it)
//        THIS.epoch = Date.now();
//        return THIS;
//    }
//};
//inherits(SharedCanvas, SimplerCanvas);
        THIS.devmode = firstOf((opts || {}).DEV_MODE, OPTS.DEV_MODE[1], true);

//        THIS.READ = false;
//        THIS.WRITE = true;
        THIS.atomic = atomic_method;
        THIS.lock = /*cluster.isMaster &&*/ num_wkers? lock_method: nop; //function(ignored) {};

//SharedCanvas.prototype.pixel =
//SharedCanvas.prototype.fill =
//SharedCanvas.prototype.load =
        THIS.pixel = pixel_method;
        THIS.fill = fill_method;
        THIS.load_img = load_img_method;

//    const super_paint = this.paint.bind(this);
//SharedCanvas.prototype.paint =
        const want_progress = firstOf((opts || {}).SHOW_PROGRESS, OPTS.SHOW_PROGRESS[1], true);
        THIS.paint = cluster.isMaster? (want_progress? paint_progress_method: paint_method): nop;

//SharedCanvas.prototype.render_stats =
//        THIS.render_stats = cluster.isMaster? render_stats_method: nop;

//don't really need to do this; shm seg container will be reclaimed anyway
//        THIS.close = (num_wkers && cluster.isMaster)? function() { rwlock(THIS.shmbuf, 1, RwlockOps.DESTROY); }: nop;

        if (firstOf((opts || {}).WANT_STATS, OPTS.WANT_STATS[1], false)) THIS.wait_stats = {};
        THIS.wait = wait_method; //cluster.isMaster? wait_method: nop;

//        THIS.FPS = 10;
        THIS.FPS = firstOf((opts || {}).FPS, OPTS.FPS[1], 1);
        const auto_play = firstOf((opts || {}).AUTO_PLAY, OPTS.AUTO_PLAY[1], true);
//        process.nextTick(delay_playback.bind(THIS, auto_play)); //give caller a chance to define custom methods
//debug(optimatizationStatus(THIS.));
//        /*process.nextTick*/ step(cluster.isMaster? master_async.bind(THIS, num_wkers, auto_play): worker_async.bind(THIS)); //give caller a chance to define custom methods
//for setImmediate vs. process.nextTick, see https://stackoverflow.com/questions/15349733/setimmediate-vs-nexttick
        THIS.playback = cluster.isMaster? master_async: worker_async; //allow caller to override these
        if (cluster.isMaster && auto_play) setImmediate(function() { step(THIS.playback); }); //defer execution so caller can override playback()
        return THIS;

        function nop() {} //dummy method for special methods on non-master copy
    },
//for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
    get: function(target, propname, receiver)
    {
        const targetValue = Reflect.get(target, propname, receiver);
        if (typeof targetValue == 'function')
        {
            return function (...args)
            {
                debug(`canvas: call '${propname}' with ${args.length} args`);
                return targetValue.apply(this, args);
            }
        }
        debug(`canvas: get '${propname}'`);
//        return Reflect.get(target, propname, receiver);
        return targetValue;
    },
//    apply: function(target, THIS, args)
//    {
//        debug("canvas: call ")
//        console.log(`Calculate sum: ${argumentsList}`);
//        return argumentsList[0] + argumentsList[1];
//    },
});


//set up shared memory:
function memory(opts, num_wkers)
{
//keep pixel data local to reduce #API calls into Nan/v8
//        THIS.pixels = new Uint32Array(THIS.width * THIS.height); //create pixel buf for caller; NOTE: platform byte order
//shared memory buffer for inter-process data:
//significantly reduces inter-process serialization and data passing overhead
//mem copying reportedly can take ~ 25 msec for 100KB; that is way too high for 60 FPS frame rate
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
//var sab = new SharedArrayBuffer(1024); //not implemented yet in Node.js :(
//    var shmkey = (opts || {}).SHM_KEY; //no default; //|| OPTS.SHM_KEY[1];
//    if (typeof shmkey != "undefined") shmkey = +shmkey; //kludge: shm wants number
    var shmkey = firstOf((opts || {}).SHM_KEY, process.env.SHM_KEY, /*OPTS.SHM_KEY[1],*/ function(){});
//    if (this.SHM_KEY === null) delete this.SHM_KEY; //kludge: shm wants number or undefined
    if (typeof shmkey != "undefined") shmkey = +shmkey; //kludge: shm wants key to be number or undefined
//console.log(typeof (opts || {}).SHM_KEY, typeof +(opts || {}).SHM_KEY);
//console.log(typeof process.env.SHM_KEY, +process.env.SHM_KEY);
//console.log(typeof null, +null);
//console.log(`${this.prtype} shm key %s 0x%s`, typeof this.SHM_KEY, (this.SHM_KEY || -1).toString(16));
    const extralen32 = firstOf((opts || {}).EXTRA_LEN, OPTS.EXTRA_LEN[1], 0); //1 + 1 + 1; //extra space for frame#, #wkers, and/or timestamp
    const buflen32 = HDRLEN + RwlockOps.SIZE32 + extralen32 + this.width * this.height;
//extralen += 10;
//        const alloc = num_wkers? shmbuf: function(ignored, len) { return new Buffer(len); }
//debug(`smh alloc: key 0x${shmkey.toString(16)} retry?`,  typeof cluster.isMaster, cluster.isMaster, typeof cluster.isWorker, cluster.isWorker);
//        THIS.shmbuf = new Uint32Array(alloc(shmkey, (THIS.width * THIS.height + RwlockOps.SIZE32 + extralen) * Uint32Array.BYTES_PER_ELEMENT, true)); //cluster.isMaster));
//NOTE: TypedArray (Uint32Array) ref to Buffer using native byte order is reportedly a lot faster than DataView ref to Buffer using explicitly defined byte order
    const shmbuf = !num_wkers? Buffer.allocUnsafe(buflen32 * U32SIZE): //NOTE: will initialize to non-0 values below, so don't init to 0 here; //new Buffer(buflen32 * U32SIZE): //Uint32Array(buflen32): //doesn't need to be shared
                  cluster.isMaster? shm.create(buflen32 * U32SIZE, "Buffer", shmkey): shm.get(shmkey, "Buffer");
    if (!shmbuf) throw new Error(`Can't create ${this.prtype} buf size ${buflen32}`.red_lt);
//        shmbuf.byteLength = shmbuf.length;
    /*if (num_wkers)*/ this.SHM_KEY = shmbuf.key; //actual key needed to fork wkers
//console.log("shm key-2 0x%s", THIS.SHM_KEY.toString(16));
    this.shmbuf = new Uint32Array(shmbuf.buffer, shmbuf.byteOffset, HDRLEN + RwlockOps.SIZE32); //shmbuf.byteLength / U32SIZE); //CAUTION: need ofs + len if buf is < 4K (due to shared node.js bufs?)
    if (extralen32) this.extra = new Uint32Array(shmbuf.buffer, shmbuf.byteOffset + (HDRLEN + RwlockOps.SIZE32) * U32SIZE, extralen32); //THIS.shmbuf.slice(RwlockOps.SIZE32, RwlockOps.SIZE32 + extralen32);
    this.pixels = new Uint32Array(shmbuf.buffer, shmbuf.byteOffset + (HDRLEN + RwlockOps.SIZE32 + extralen32) * U32SIZE, this.width * this.height); //THIS.shmbuf.slice(RwlockOps.SIZE32 + extralen32);
    debug(`GpuCanvas: ${this.prtype} #bkg wkers ${num_wkers}, alloc ${commas(shmbuf.byteLength)}'${shmbuf.byteOffset} bytes = ${HDRLEN * U32SIZE}'${shmbuf.byteOffset} for hdr + ${commas(RwlockOps.SIZE32 * U32SIZE)}'${shmbuf.byteOffset + HDRLEN * U32SIZE} for rwlock + ${commas((this.extra || {}).byteLength || 0)}'${shmbuf.byteOffset + (HDRLEN + RwlockOps.SIZE32) * U32SIZE} for extra data + ${commas(this.pixels.byteLength)}'${shmbuf.byteOffset + (HDRLEN + RwlockOps.SIZE32 + extralen32) * U32SIZE} for pixels`.blue_lt);
    if (cluster.isMaster) //initialize shm
    {
        shmbuf["writeUInt32" + os.endianness()](MAGIC, 0); //write via buf, read via typedarray for dual check; //THIS.shmbuf[0] = MAGIC;
//        this.shmbuf[1] = 0;
        var errstr = num_wkers? rwlock(this.shmbuf, HDRLEN, RwlockOps.INIT): false; //use shm to allow access by multiple procs
        if (errstr) throw new Error(`GpuCanvas: can't create shm lock: ${errstr}`.red_lt);
        this.pixels.fill(BLACK); //start with all pixels dark
        if (extralen32) this.extra.fill(0);
//            THIS.shmbuf.fill(0x12345678);
//debug("shmbuf fill-1: ", RwlockOps.SIZE32 + 0, THIS.shmbuf[RwlockOps.SIZE32 - 1].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 0].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 1].toString(16), RwlockOps.SIZE32 + 9, THIS.shmbuf[RwlockOps.SIZE32 + 8].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 9].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 10].toString(16));
//            THIS.shmbuf[RwlockOps.SIZE32 + 1] = 0xdead;
//            THIS.shmbuf[RwlockOps.SIZE32 + 9] = 0xbeef;
//debug("shmbuf fill-2: ", RwlockOps.SIZE32 + 0, THIS.shmbuf[RwlockOps.SIZE32 - 1].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 0].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 1].toString(16), RwlockOps.SIZE32 + 9, THIS.shmbuf[RwlockOps.SIZE32 + 8].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 9].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 10].toString(16));
//debug("shmbuf fill-2: ", THIS.extra[0].toString(16), THIS.extra[9].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 0], THIS.shmbuf[RwlockOps.SIZE32 + 9]);
//            AtomicAdd(THIS.extra, 0, 1);
//            AtomicAdd(THIS.extra, 9, -1);
//debug("shmbuf fill-3: ", THIS.extra[0].toString(16), THIS.extra[9].toString(16));
    }
//debug("'0 = %s", shmbuf.readUInt32LE(4 * (RwlockOps.SIZE32 + 1)).toString(16));
//var buf = [];
//for (var i = 0; i < THIS.shmbuf.length; ++i) if ((i == 56) || (i == 65) || (THIS.shmbuf[i] != 0x12345678)) buf.push(`${i} = 0x${THIS.shmbuf[i].toString(16)}`);
//for (var i = 0; i < extralen; ++i) if (THIS.extra[i] != 0x12345678) buf.push(`${i} = 0x${THIS.extra[i].toString(16)}`);
//debug(`smh check: ${THIS.prtype} ` + (!buf.length? `all ${THIS.extra.length} ok`: `wrong at(${buf.length}): ` + buf.join(", ")));
//debug(`${THIS.shmbuf[0].toString(16)} ${THIS.shmbuf[1].toString(16)} ${THIS.shmbuf[2].toString(16)} ${THIS.shmbuf[3].toString(16)}`)
//const ok = ((THIS.extra[0] != 0xdead) || (THIS.extra[9] != 0xbeef))? "bad": "good";
//debug("shmbuf fill-2: ", ok, THIS.extra[0].toString(16), THIS.extra[9].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 0].toString(16), THIS.shmbuf[RwlockOps.SIZE32 + 9].toString(16));
//shm paranoid check:
//debug("here4 0x%s", this.shmbuf[0].toString(16)); //extra[1].toString(16), THIS.extra[9].toString(16), (THIS.extra[1] != 0xdead) || (THIS.extra[9] != 0xbeef));
    if (this.shmbuf[0] != MAGIC) throw new Error(`${this.prtype} shm hdr bad: 0x${this.shmbuf[0].toString(16)}, should be 0x{MAGIC.toString(16)}`.red_lt);
//debug(`${THIS.shmbuf[224 + 0].toString(16)} ${THIS.shmbuf[224 + 1].toString(16)} ${THIS.shmbuf[224 + 2].toString(16)} ${THIS.shmbuf[224 + 3].toString(16)}`)
//        if (cluster.isMaster) THIS.pixels.fill(BLACK); //start with all pixels dark
//        if (cluster.isMaster) THIS.extra.fill(0);
//debug("here5");
//        if (cluster.isMaster) { THIS.shmbuf[0] = 0xdead; THIS.shmbuf[1] = 0xbeef; }
//        else if ((THIS.shmbuf[0] != 0xdead) || (THIS.shmbuf[1] != 0xbeef)) throw `shm bad: 0x${THIS.shmbuf[0].toString(16)} 0x${THIS.shmbuf[1].toString(16)}`.red_lt;
//        if (cluster.isMaster) { THIS.pixels[0] = 0xdead; THIS.pixels[1] = 0xbeef; }
//        else if ((THIS.pixels[0] != 0xdead) || (THIS.pixels[1] != 0xbeef)) throw `shm bad: 0x${THIS.pixels[0].toString(16)} 0x${THIS.pixels[1].toString(16)}`.red_lt;
}


//finish master init asynchronously:
//starts bkg wkers
function* master_async(num_wkers, auto_play)
{
    console.log("master playback".red_lt);
    return;
//    var clkRes = clock.getres(clock.MONOTONIC);
//    debug(`Resolution of CLOCK_MONOTONIC: ${clkRes.sec} sec + ${clkRes.nsec} nsec`, clkRes);
//    var clkTime = clock.gettime(clock.MONOTONIC);
//    debug(`Time from CLOCK_MONOTONIC: ${clkTime.sec} sec + ${clkTime.nsec} nsec`, clkTime);
////    clock.nanosleep(clock.REALTIME, clock.TIMER_ABSTIME, { sec: 1234567890, nsec: 0 });
//    clock.nanosleep(clock.REALTIME, 0, { sec: 0, nsec: 123 });   
//    debug("woke up");
//    return;
    if (!this.render) throw new Error(`GpuCanvas: need to define a render() method`.red_lt);
    if (auto_play && !this.playback) throw new Error(`GpuCanvas: need to define a playback() method`.red_lt);
//    if (!cluster.isMaster) return;
//    this.shmbuf[1] = 0; //set next frame# to render; //-num_wkers; //no-use dummy frame#s as placeholders for wker acks
    var frnum = 0;
    var pending = num_wkers; //#remaining replies
    for (var w = 0; w < num_wkers; ++w)
    {
        let wker = cluster.fork({WKER_ID: w, SHM_KEY: this.SHM_KEY, /*EPOCH,*/ NODE_ENV: process.env.NODE_ENV || ""}); //BkgWker.all.length});
//        debug(cluster.isMaster? `GpuCanvas: forked ${Object.keys(cluster.workers).length}/${num_wkers} bkg wker(s)`.pink_lt: `GpuCanvas: this is bkg wker# ${THIS.WKER_ID}/${num_wkers}`.pink_lt);
//        var wker2 = cluster.workers[Object.keys(cluster.workers)[0]]; //same thing
//NODE_CHANNEL_FD doesn't work here; https://stackoverflow.com/questions/41033716/send-ipc-message-with-sh-bash-to-parent-process-node-js
/*instead use mutex/condition vars for ipc
        wker.on('message', (msg) =>
        {
            wker.ipc1 = msg.ipc1;
            wker.ipc2 = msg.ipc2;
//            debug(`got msg ${JSON.stringify(msg)} from bkg wker`.blue_lt);
            debug(`master '${process.pid}' got msg ${JSON.stringify(msg)}, expecting fr# ${frnum}, pending ${pending}`.blue_lt);
//            if (msg.ack) { if (!--pending) step(); return; } //all wkers replied; wake up master
//            throw new Error(`Unhandled msg: ${JSON.stringify(msg)}`.red_lt);
            if (msg.rendered != frnum) throw new Error(`reply mismatch: got fr#${msg.rendered}, expected ${frnum}`.red_lt);
            if (--pending < 0) throw new Error(`too many replies for fr#${frnum}`.red_lt);
            if (!pending) step();
        });
*/
//    cluster.on('exit', (wker, code, signal) =>
        wker.on('exit', (code, signal) =>
        {
            console.log(`wker '${process.pid}' died (${code || signal || "unknown reason"})`.red_lt);
//        step.retval = {died: process.pid};
//        step();
//TODO: restart proc? state will be gone, might be problematic
        });
    }
/*
    debug(`GpuCanvas: ${num_wkers} wkers forked, now wait for acks`.blue_lt);
    for (;;)
    {
        var pending = -this.atomic(false, function() { return uint32toint(this.shmbuf[1]); }.bind(this)); //check #pending acks
        debug(`master '${process.pid}': ${pending} pending`.blue_lt);
        if (!pending) break;
        debug("TODO: master dummy paint".yellow_lt);
        yield this.wait(1 / this.FPS); //simulate 60 fps
    }
*/
    debug(`GpuCanvas: ${num_wkers} wkers launched, ready for playback`.blue_lt);
//    if (!auto_play) return; //TODO: emit event or caller cb
//wedge async paint, render in front of caller-supplied functions:
//handles ipc with bkg wkers and async call-backs
    const render_sync = this.render;
    const paint_sync = this.paint;
    this.render = function*(args)
    {
        
    }
    this.paint = function*(args)
    {

    }
    if (auto_play) this.playback();
    return;

//    yield* this.playback();
    this.elapsed = 0;
    const DURATION = 5; //TODO
//    for (/*var frnum = 0*/; frnum < DURATION * this.FPS; ++frnum) //++ is too late here
    while (frnum < DURATION * this.FPS)
    {
//        yield this.render(frnum);
        yield; //wait for all wkers to render+reply
//        encode_bkg(); //blocking
//        yield this.paint();
        ++frnum; //do this before broadcast; replies will (hopefully) come back before end of loop
        yield this.paint(this.pixels, frnum / this.FPS, (done) => //async, double-blocking (cb called 2x)
        {
            debug(`async paint cb: done ${done}`.blue_lt);
            if (!done) return broadcast({frnum}); //: frnum + 1}); //wake up children after encode
//            yield this.wait((frnum + 1) / this.FPS); //throttle
//            RenderPresent_bkg(); //blocking
            step(); //wake up master after RenderPresent
        });
    }
    debug("master send eof".blue_lt)
//    this.atomic(true, function() { this.shmbuf[1] = QUIT; }.bind(this));
    broadcast({frnum: QUIT});
/*
        this.render(frnum, frnum / this.FPS);
        yield this.wait(3/4 / this.FPS); //12.5 msec
//CAUTION: blocking on wrlock also blocks evt loop
        debug(`master wait for wr lock`.blue_lt);
        this.atomic(true, function()
        {
            usleep(3000); //3 msec simulate encode
            this.shmbuf[1] = frnum + 1; //request next frame# from wkers
            debug(`master req fr#${frnum + 1}`.blue_lt)
        }.bind(this));
        debug(`master release wr lock`.blue_lt);
    }
*/
    function broadcast(msg)
    {
        pending += num_wkers; //#replies expected
//        cluster.workers.forEach((wker) => { wker.send(msg); });
        Object.keys(cluster.workers).forEach(function(id)
        {
            cluster.workers[id].send(msg);
        });
    }
}


//finish wker init asynchronously:
//acks parent then starts render loop
function* worker_async()
{
    console.log("wker playback".red_lt);
    return;

    if (!this.render) throw new Error(`GpuCanvas: need to define a render() method`.red_lt);
//    if (!this.playback) throw new Error(`GpuCanvas: need to define a playback() method`.red_lt);
//handle msgs from main thread:
    process.on('message', (msg) =>
    {
//        debug("cur val: %d", canvas.extra[0]);
//        var remaining = canvas.atomic(Pending, 0); //, "rcv msg"); //AtomicAdd(canvas.extra, 0, 0);
//        debug(`${canvas.prtype} '${process.pid}' rcv msg ${JSON.stringify(msg)}, atomic chk ${remaining} still pending`.blue_lt);
        var frnum = uint32toint(msg.frnum);
        debug(`wker ${this.WKER_ID} '${process.pid}' rcv msg ${JSON.stringify(msg)}, fr# ${frnum}`.blue_lt);
        if (frnum == QUIT)
        {
            debug(`Worker '${process.pid}' quit`.blue_lt);
            process.exit(0);
        }
        this.render(frnum, frnum / this.FPS);
        process.send({rendered: frnum, WKER_ID: process.env.WKER_ID});
/*
        if (msg.render)
        {
//            if (!remaining) throw new Error("render msg: main not expecting a response".red_lt);
            debug(`BkgWker '${process.pid}' render req fr# ${msg.frnum}, ${models.length} model(s)`.blue_lt);
//            models.renderAll(msg.frnum);
//            process.send({reply: frnum});
            if (msg.frnum != (wait_bkgwker.frnum || 0)) throw new Error(`BkgWker '${process.pid}': out of sync: at frame# ${wait.frnum}, but main thread wants frame# ${msg.frnum}`.red_lt);
            step(); //let caller run
            return;
        }
        if (msg.quit)
        {
//            if (remaining) throw new Error(`quit msg: main expecting a response (${remaining} pending)`.red_lt);
            debug(`Bkg Wker '${process.pid}' quit`.blue_lt);
  //          process.send({quite: true});
            process.exit(0);
            return;
        }
        throw new Error(`Bkg wker unknown msg type: ${JSON.stringify(msg)}`.red_lt);
*/
    });
    this.render(0, 0);
    process.send({rendered: 0, WKER_ID: process.env.WKER_ID});
//    sync_bkgwker.init = true;
//    process.send({ack: true}); //tell main thread ready for work
/*
    this.render(0, 0); //pre-render first frame so it's ready to go
    var pending = -this.atomic(true, function() { return uint32toint(++this.shmbuf[1]); }.bind(this)); //send ready signal to master
    debug(`Worker '${process.pid}' ready, ${pending} pending`.blue_lt);
    if (pending < 0) throw new Error(`Wker ack: master wasn't waiting for me (${pending})`.red_lt);
    var last_rendered = 0;
    for (;;)
    {
//CAUTION: blocking on wrlock also blocks evt loop
        debug(`wker '${process.pid}' wait for rd lock`.blue_lt);
        var want_quit = this.atomic(false, function()
        {
//        this.lock(false); //acquire rd lock (multiple readers); used to sync with main thread
            var requested = uint32toint(this.shmbuf[1]); //(this.shmbuf[1] <= 0x7fffffff)? this.shmbuf[1]: this.shmbuf[1] - 0x100000000;
            var want_quit = (requested == QUIT), need_render = (last_rendered < requested); //&& !want_quit;
            debug(`wker '${process.pid}' acq rd lock, want quit? ${want_quit}, need render? ${need_render}, last rendered ${last_rendered} vs. ${requested}`.blue_lt);
            if (need_render) this.render(++last_rendered, last_rendered / this.FPS);
//            ++last_rendered; //= this.shmbuf[1];
//        this.lock(); //release rd lock to allow main thread to update
            return want_quit;
        }.bind(this));
        debug(`wker '${process.pid}' release rd lock, want_quit? ${want_quit}`.blue_lt);
        if (want_quit) break;
        yield this.wait(1 / this.FPS / 2);
    }
*/
/*
    var frnum = 0;
    for (;;)
    {
        this.render(frnum, frnum / this.FPS);
        frnum = yield this.paint(); //wait for master to paint (encode, actually) and request another frame
        if (frnum == QUIT) break;

        function this.paint()
        {
            if (++wkers_done == num_wkers) wakeup_master();
            wait_for_wkup(); //blocking
            step.retval = this.shmbuf[1]; //next fr#
        }
    }
    debug(`Worker '${process.pid}' quit`.blue_lt);
*/
}


/*
//start playback:
function delay_playback(auto_play)
{
    debug(`playback: master? ${cluster.isMaster}, auto play? ${auto_play}`.blue_lt);
    if (!this.render) throw new Error(`GpuCanvas: need to define a render() method`.red_lt);
    if (!cluster.isMaster) return;
    if (!this.playback) throw new Error(`GpuCanvas: need to define a playback() method`.red_lt);
    if (!auto_play) return;
    
    for (var w = 0, pending = num_wkers; w < num_wkers; ++w)
console.log("TODO: wait for wkers".red_lt);
    step(this.playback());
}
*/


//perform atomic op on shared memory:
function atomic_method(rw, func, args)
{
    this.lock(rw); //rd/wr lock
    try { return func.apply(null, Array.from(arguments).slice(1)); }
//    catch (exc) { var svexc = exc; }
    finally { this.lock(); } //unlock
    return retval;
}



//read/write lock:
function lock_method(wr)
{
//    const HDRLEN = this.shmbuf.byteLength / U32SIZE - RwlockOps.SIZE32;
    var errstr = rwlock(this.shmbuf, HDRLEN, (wr === true)? RwlockOps.WRLOCK: (wr === false)? RwlockOps.RDLOCK: RwlockOps.UNLOCK);
    if (errstr) throw new Error(`GpuCanvas: un/lock failed: ${errstr}`.red_lt);
//    const {caller/*, calledfrom, shortname*/} = require("./demos/shared/caller");
//    const from = caller(-2); //.replace("@pin-finder:", "");
//    debug("lock(%d) from %s", wr, from);
}


//get/set individual pixel:
//NOTE: slow; only use for single pixels
function pixel_method(x, y, color)
{
//console.log("pixel(%d, %d): w %d, h %d".blue_lt, x, y, this.width, this.height);
    var clip = ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height)); //throw "Index of out range";
    if (arguments.length > 2) //set color
    {
        if (!clip) this.pixels[x * this.height + y] = color >>> 0;
        return this; //fluent
    }
    return !clip? this.pixels[x * this.height + y]: BLACK;
} //.bind(this);


//fill all pixels or rectangular subset:
function fill_method(xstart, ystart, w, h, color)
{
    if (arguments.length > 1)
    {
        xstart = Math.max(xstart, 0);
        ystart = Math.max(ystart, 0);
        var xend = Math.min(xstart + w, this.width);
        var yend = Math.min(ystart + h, this.height);
        for (var x = xstart, xofs = x * this.height; x < xend; ++x, xofs += this.height)
            for (var y = ystart; y < yend; ++y)
                this.pixels[xofs + y] = color >>> 0;
        return this; //fluent
    }
    color = xstart; //shuffle params
    this.pixels.fill(color >>> 0);
    return this; //fluent
}


//load image file into memory (PNG only):
function load_img_method(path)
{
//    const {PNG} = require('pngjs'); //nested so it won't be needed unless load() is called
    var png = PNG.sync.read(fs.readFileSync(path));
    path = pathlib.relative(__dirname, path);
    if ((png.width != this.width) || (png.height != this.height))
        console.error(`image '${path}' ${pgn.width} x ${pgn.height} doesn't match canvas ${this.width} x ${this.height}`.yellow_lt);
        else debug(`image '${path}' size ${pgn.width} x ${pgn.height} matches canvas`.green_lt);
    png.pixels = new Uint32Array(png.data.buffer); //gives RGBA
    png.data.readUInt32_ARGB = function(ofs) { return ((this.readUInt32BE(ofs) >>> 8) | (this[ofs + 3] << 24)) >>> 0; } //RGBA -> ARGB
//        var seen = {}; //DEBUG ONLY
//NOTE: need to copy pixel-by-pixel to change byte order or if size doesn't match
    for (var y = 0, yofs = 0; y < png.height; ++y, yofs += 4 * png.width)
        for (var x = 0, xofs = 0; x < png.width; ++x, xofs += this.height)
        {
//                var color = png.pixels[yofs + x]; //gives ABGR
            var color = png.data.readUInt32_ARGB(yofs + 4 * x); //gives ARGB
            this.pixels[xofs + y] = color;
//                if (seen[png.pixels[yofs/4 + x]]) continue;
//                seen[png.pixels[yofs/4 + x]] = true;
//console.log("img[%d,%d]: 0x%s -> 0x%s", x, y, HH(png.data[yofs + 4 * x]) + HH(png.data[yofs + 4 * x + 1]) + HH(png.data[yofs + 4 * x + 2]) + HH(png.data[yofs + 4 * x + 3]), color.toString(16));
        }
//    function HH(val) { return ("00" + val.toString(16)).slice(-2); } //2 hex digits
    return this; //fluent
}


//send pixel data on screen:
function paint_method(pixels, delay, cb) //alt_pixels)
{
//        if (WANT_SHARED && !cluster.isMaster) return process.send({done: true});
//console.log(`paint: ${this.width} x ${this.height}, progress? ${this.SHOW_PROGRESS}, elapsed ${this.elapsed || 0}, duration ${this.duration}, x-limit %d %d`, this.width * (this.elapsed || 0) / this.duration, Math.round(this.width * (this.elapsed || 0) / this.duration));
//        if (!alt_pixels) //don't mess up caller's data
//    if (this.SHOW_PROGRESS && this.duration) //kludge: use last row of pixels to display progress bar
//        for (var x = 0; x < Math.round(this.width * (this.elapsed || 0) / this.duration); ++x)
//            this.pixels[(x + 1) * this.height -1] = WHITE;
//        return Object.getPrototypeOf(this).
//        super_paint(pixels); //CAUTION: blocks a few msec for bkg renderer
//        super.paint(alt_pixels || this.pixels); //CAUTION: blocks a few msec for bkg renderer
//    if (arguments.length) GpuCanvas.prototype.paint.call(this, alt_pixels); //CAUTION: blocks a few msec for bkg renderer
//    else GpuCanvas.prototype.paint.call(this); //CAUTION: blocks a few msec for bkg renderer
    GpuCanvas.prototype.paint.apply(this, arguments); //CAUTION: blocks a few msec for bkg renderer
    if (isNaN(++this.render_count)) this.render_count = 1;
//    debug(`paint# ${this.render_count}, ${arguments.length} arg(s)`.blue_lt);
    return this; //fluent
} //.bind(this);
function paint_progress_method(pixels, delay, cb) //alt_pixels)
{
//debug("paint[%d @%d]: prog 0..%d/%d", this.render_count || 0, this.elapsed, (this.width * (this.elapsed || 0) / this.duration), this.width);
    if (/*this.SHOW_PROGRESS &&*/ this.duration) //kludge: use last row of pixels to display progress bar
        for (var x = 0, xofs = x * this.height; x < Math.round(this.width * (this.elapsed || 0) / this.duration); ++x, xofs += this.height)
            this.pixels[xofs + this.height -1] = WHITE;
    return paint_method.apply(this, arguments);
}


//delay next step:
function wait_method(delay) //, frnum)
{
//    if (canvas.isWorker) return BkgWker.wait(nxtfr);
    delay *= 1000; //sec -> msec
//    const overdue = (delay <= 1); //1 msec is too close for comfort; treat it as overdue
//    if (isNaN(++wait.counts[overdue])) wait.counts = [!overdue, overdue];
//    if (overdue) if (isNaN(++wait.overdue_count)) wait.overdue_count = 1;
    if (this.wait_stats)
    {
        const overdue = (delay <= 1); //1 msec is too close for comfort; treat it as overdue
        if (isNaN(++this.wait_stats[overdue])) Object.defineProperties(this.wait_stats, getOwnPropertyDescriptors( //init stats, delay execution of getters; need descriptors
        {
            [overdue]: 1, [!overdue]: 0, get count() { return this.false + this.true; },
            total_delay: 0, max_delay: 0, min_delay: Number.MAX_SAFE_INTEGER,
            get report() { return `wait stats: overdue frames ${commas(this.true)}/${commas(this.count)} (${trunc(100 * this.true / this.count, 10)}%), \
                avg wait ${trunc(this.total_delay / this.count, 10)} msec, \
                min wait[${this.min_when}] ${trunc(this.min_delay, 10)} msec, \
                max wait[${this.max_when}] ${trunc(this.max_delay, 10)} msec`.replace(/\s+/gm, " ")[this.true? "red_lt": "green_lt"]; },
        }));
        this.wait_stats.total_delay += Math.max(delay, 0);
        if (delay > this.wait_stats.max_delay) { this.wait_stats.max_delay = delay; this.wait_stats.max_when = this.wait_stats.count; } //wait.stats.false + wait.stats.true; }
        if (delay < this.wait_stats.min_delay) { this.wait_stats.min_delay = delay; this.wait_stats.min_when = this.wait_stats.count; } //frnum; } //wait.stats.false + wait.stats.true; }
        if ((this.wait_stats[overdue] < 10) || overdue) //reduce verbosity: only show first 10 non-overdue
            debug(`${this.prtype} '${process.pid}' wait[${this.wait_stats.count}] ${commas(trunc(delay, 10))} msec`[overdue? "red_lt": "blue_lt"]); //, wait.stats.false + wait.stats.true - 1, commas(trunc(delay, 10))); //, JSON.stringify(wait.counts));
    }
    return (delay > 1)? setTimeout.bind(null, step, delay): setImmediate.bind(null, step);
}


//give render stats to caller for debug / performance analysis:
//function render_stats_method(start)
//{
//    if (start) this.save = {count: this.render_count || 0, sttime: Date.now()};
//    else
//    {
//        var num = this.render_count - this.save.count;
//        var den = (Date.now() - this.save.sttime) / 1000; //sec
//        this.StatsAdjust = -den;
//console.log("render.stats", num, den, num / den);
//        return num / den;
//    }
//}


//validate ctor opts:
function opts_check(opts)
{
//console.log("chk opts", JSON.stringify(opts));
    Object.keys(opts || {}).forEach((key, inx, all) =>
    {
        if (!(key in OPTS)) throw new Error(`GpuCanvas: option '${key}' not supported until maybe later`.red_lt);
//        const info = OPTS[key];
//console.log("info", key, inx, typeof OPTS[key]); //JSON.stringify(info));
        const [opt_mode, def_val] = OPTS[key];
        if (opts.DEV_MODE) return; //all options okay in dev mode
        if (opt_mode != DEVONLY) return; //option okay in live mode
        if (opts[key] == def_val) return; //default value ok in live mode
//            all[inx] = null; //don't show other stuff in live mode (interferes with output signal)
        console.error(`GpuCanvas: ignoring non-default option '${key}' in live mode`.yellow_lt);
    });
//        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
//        if (typeof opts.WANT_SHARED == "undefined") opts.WANT_SHARED = !cluster.isMaster;
//        if (opts.WS281X_FMT) delete opts.SHOW_PROGRESS; //don't show progress bar in live mode (interferes with LED output)
//        if (opts.WS281X_FMT) opts = {WS281X_FMT: opts.WS281X_FMT, UNIV_TYPE: opts.UNIV_TYPE}; //don't show other stuff in live mode (interferes with LED output)
//        this.opts = opts; //save opts for access by other methods; TODO: hide from caller
}


////////////////////////////////////////////////////////////////////////////////
////
/// Helper functions:
//

//stepper function:
//NOTE: only steps generator function 1 step; subsequent events must also caller step to finish executing generator
const step =
module.exports.step =
function step(gen)
{
//    if (canvas.isWorker) return; //stepper not needed by bkg threads
try{
//console.log("step");
//    if (step.done) throw "Generator function is already done.";
//	if (typeof gen == "function") gen = gen(); //invoke generator if not already
//for setImmediate vs. process.nextTick, see https://stackoverflow.com/questions/15349733/setimmediate-vs-nexttick
	if (typeof gen == "function") { setImmediate(function() { step(gen(/*function(cb) { step.onexit = cb; }*/)); }); return; } //invoke generator if not already
//        return setImmediate(function() { step(gen()); }); //avoid hoist errors
    if (typeof gen != "undefined")
    {
        if (typeof step.svgen != "undefined") throw new Error("step: previous generator not done yet".red_lt);
        step.svgen = gen; //save generator for subsequent steps
    }
//console.log("step:", typeof gen, JSON.stringify_tidy(gen));
//    if (!BkgWker.sync()) return; //coordinate bkg wkers before advancing; NOTE: must cache svgen first
    var {value, done} = step.svgen.next(step.retval); //send previous value to generator
//    {step.retval, step.done} = step.svgen.next(step.retval); //send previous value to generator
//    Object.assign(step, step.svgen.next(step.retval)); //send previous value to generator
//console.log("step: got value %s %s, done? %d", typeof value, value, done);
//    step.done = done; //prevent overrun
    if (done) delete step.svgen; // = null; //prevent overrun; do bedore value()
    if (typeof value == "function") value = value(); //execute caller code before returning
//    if (done /*&& step.onexit*/) BkgWker.close(); //step.onexit(); //do after value()
//console.log("here3");
    return step.retval = value; //return value to caller and to next yield
} catch(exc) { console.log(`step EXC: ${exc}`.red_lt); /*gen.*/throw(exc); } //CAUTION: without this, func gen might quit without telling anyone
}


//return first non-empty value:
function firstOf(args) //prevval, curval) //, curinx, all)
{
    for (var i = 0; i < arguments.length; ++i)
    {
        var arg = arguments[i];
        if (typeof arg == "undefined") continue;
        return (typeof arg == "function")? arg(): arg;
    }
//    return (typeof prevval != "undefined")? prevval: curval;
    throw new Error("firstOf: no value found".red_lt);
}


//show optimization status:
//see https://glebbahmutov.com/blog/detecting-function-optimizations-in-v8/
const optimizationStatus =
module.exports.optimizationStatus =
function optimizationStatus(fn)
{
    const STATES = {1: "", 2: "not ", 3: "always ", 4: "never ", 6: "maybe de"}
//    var name = fn.name;
    return `${fn.name} is ${STATES[GetOptimizationStatus(fn)] || "unknown "}optimizated`;
//    switch (%GetOptimizationStatus(fn))
//    {
//        case 1: console.log(fn.name, "function is optimized"); break;
//        case 2: console.log(fn.name, "function is not optimized"); break;
//        case 3: console.log(fn.name, "function is always optimized"); break;
//        case 4: console.log(fn.name, "function is never optimized"); break;
//        case 6: console.log(fn.name, "function is maybe deoptimized"); break;
//    }
}

function uint32toint(val)
{
    return (val <= 0x7fffffff)? val: val - 0x100000000;
}


//truncate decimal places:
function trunc(val, digits)
{
    return Math.floor(val * (digits || 1) + 0.5) / (digits || 1); //round to desired precision
}


//display commas for readability (debug):
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return val.toLocaleString();
}


//polyfill:
//no worky on getters without setters :(
//function Object_getOwnPropertyDescriptors(obj)
//{
//    for (var i in obj)
//        obj[i] = Object.getOwnPropertyDescriptor(obj, i);
//    return obj;
//}


////////////////////////////////////////////////////////////////////////////////
////
/// GpuCanvas (first try, fg/bg render):
//

//temp shim until OpenGL support is re-enabled:
//use ctor function instead of class syntax to allow hidden vars, factory call, etc
const fgbgGpuCanvas_shim =
module.exports.fgbgGpuCanvas =
//function GpuCanvas_shim(title, width, height, opts)
//class GpuCanvas_shim extends GpuCanvas
//proxy: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
new Proxy(function(){},
{
    construct: function fgbgGpuCanvas_shim(target, args, newTarget) //title, width, height, opts)
    {
//    if (!(this instanceof GpuCanvas_shim)) return new GpuCanvas_shim(title, width, height, opts); //convert factory call to class
//    constructor(title, width, height, opts)
//    {
//        const WANT_SHARED = true;
        var [title, width, height, opts] = args;
//console.log("proxy ctor", title, width, height, opts);
        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
        Object.keys(opts).forEach((key, inx, all) =>
        {
            if (/*opts[key] &&*/ !supported_OPTS[key]) throw new Error(`Option '${key}' not supported until maybe 2018 (OpenGL re-enabled)`.red_lt);
            if (!opts.WS281X_FMT || (['WS281X_FMT', 'UNIV_TYPE', 'WANT_SHARED'].indexOf(key) != -1)) return;
            all[inx] = null; //don't show other stuff in live mode (interferes with output signal)
            console.error(`Ignoring option '${key}' for live mode`.yellow_lt);
        });
//        if (typeof opts.WANT_SHARED == "undefined") opts.WANT_SHARED = !cluster.isMaster;
//        if (opts.WS281X_FMT) delete opts.SHOW_PROGRESS; //don't show progress bar in live mode (interferes with LED output)
//        if (opts.WS281X_FMT) opts = {WS281X_FMT: opts.WS281X_FMT, UNIV_TYPE: opts.UNIV_TYPE}; //don't show other stuff in live mode (interferes with LED output)
//        this.opts = opts; //save opts for access by other methods; TODO: hide from caller
//console.log(title, width, height, opts);
//        GpuCanvas.call(this, title, width, height, !!opts.WS281X_FMT); //initialize base class
        const THIS = ((opts.WANT_SHARED === false) || cluster.isMaster)? new fgbgGpuCanvas(title, width, height, !!opts.WS281X_FMT): {title, width, height}; //initialize base class
        if ((WANT_SHARED === false) || cluster.isMaster)
        {
//        super(title, width, height, !!opts.WS281X_FMT); //initialize base class
//        etters(Object.getPrototypeOf(this)); //kludge: can't get it to work within fixup; do it here instead
//        etters(THIS); //kludge: can't get it to work within fixup; do it here instead
            pushable.call(THIS, Object.assign({}, supported_OPTS, opts || {}));
//console.log("initial props:", THIS.WS281X_FMT, THIS.SHOW_PROGRESS, opts.UNIV_TYPE);
//if (opts.UNIV_TYPE) console.log("set all unv type to", opts.UNIV_TYPE);
            THIS.UnivType = THIS.UnivType_tofix;
            if (THIS.UNIV_TYPE) for (var i = 0; i < THIS.width; ++i) THIS.UnivType(i, THIS.UNIV_TYPE);
        }

//keep pixel data local to reduce #API calls into Nan/v8
//        THIS.pixels = new Uint32Array(THIS.width * THIS.height); //create pixel buf for caller; NOTE: platform byte order
//shared memory buffer for inter-process data:
//significantly reduces inter-process serialization and data passing overhead
//mem copying reportedly can take ~ 25 msec for 100KB; that is way too high for 60 FPS frame rate
//to see shm segs:  ipcs -a
//to delete:  ipcrm -M key
//var sab = new SharedArrayBuffer(1024); //not implemented yet in Node.js :(
        const SHMKEY = 0xbeef; //make value easy to find (for debug)
        THIS.pixels = WANT_SHARED?
            new Uint32Array(shmbuf(SHMKEY, THIS.width * THIS.height * Uint32Array.BYTES_PER_ELEMENT)):
            new Uint32Array(THIS.width * THIS.height); //create pixel buf for caller; NOTE: platform byte order
        if ((WANT_SHARED === false) || cluster.isMaster) THIS.pixels.fill(BLACK); //start with all pixels dark
        THIS.elapsed = 0;
//console.log("here1");
//        return THIS;
//    }
//};
//inherits(GpuCanvas_shim, GpuCanvas);

/*
//get/set universe types:
//TODO: use a special pixel() address instead?  (ie, y == 0x5554)
//GpuCanvas_shim.prototype.pixel = function
        THIS.UnivType = function
    UnivType(x, newtype)
    {
//console.log("UnivType(%d, %d): %d".blue_lt, x, newType, this);
        var clip = ((x < 0) || (x >= this.width)); //throw "Index of out range";
        if (arguments.length > 1) //set type
        {
            if (!clip) THIS.UnivType(x, newtype);
            return this; //fluent
        }
        return !clip? THIS.UnivType(x): 0;
    } //.bind(this);
*/

//get/set individual pixel:
//GpuCanvas_shim.prototype.pixel = function
        THIS.pixel = function
    pixel(x, y, color)
    {
//console.log("pixel(%d, %d): w %d, h %d".blue_lt, x, y, this.width, this.height);
        var clip = ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height)); //throw "Index of out range";
        if (arguments.length > 2) //set color
        {
            if (!clip) this.pixels[x * this.height + y] = color >>> 0;
            return this; //fluent
        }
        return !clip? this.pixels[x * this.height + y]: BLACK;
    } //.bind(this);

//fill all pixels:
//GpuCanvas_shim.prototype.fill = function
        THIS.fill = function
    fill(xstart, ystart, w, h, color)
    {
//use nested loop on partial (rect) fill:
        if (arguments.length > 1)
        {
            xstart = Math.max(xstart, 0);
            ystart = Math.max(ystart, 0);
            w = Math.min(w, this.width - xstart);
            h = Math.min(h, this.height - ystart);
            for (var x = 0, xofs = xstart * this.height; x < w; ++x, xofs += this.height)
                for (var y = 0; y < h; ++y)
                    this.pixels[xofs + y] = color >>> 0;
            return this; //fluent
        }
        color = x; //shuffle params
        this.pixels.fill(color >>> 0);
//        return this; //fluent
    }

//load image file into memory (PNG only):
//GpuCanvas_shim.prototype.load = function
        THIS.load = function
    load(path)
    {
        const {PNG} = require('pngjs'); //nested so it won't be needed unless load() is called
        var png = PNG.sync.read(fs.readFileSync(path));
        path = pathlib.relative(__dirname, path);
        if ((png.width != this.width) || (png.height != this.height))
            console.error("image '%s' %d x %d doesn't match canvas %d x %d".yellow_lt, path, png.width, png.height, this.width, this.height);
            else debug("image '%s' size %d x %d matches canvas".green_lt, path, png.width, png.height);
        png.pixels = new Uint32Array(png.data.buffer); //gives RGBA
        png.data.readUInt32_shuffle = function(ofs)
        {
            return ((this.readUInt32BE(ofs) >>> 8) | (this[ofs + 3] << 24)) >>> 0; //RGBA -> ARGB
        };
//        var seen = {}; //DEBUG ONLY
        for (var y = 0, yofs = 0; y < png.height; ++y, yofs += 4 * png.width)
            for (var x = 0, xofs = 0; x < png.width; ++x, xofs += this.height)
{
//                var color = png.pixels[yofs + x]; //gives ABGR
                var color = png.data.readUInt32_shuffle(yofs + 4 * x); //gives ARGB
                this.pixels[xofs + y] = color;
//                if (seen[png.pixels[yofs/4 + x]]) continue;
//                seen[png.pixels[yofs/4 + x]] = true;
//console.log("img[%d,%d]: 0x%s -> 0x%s", x, y, HH(png.data[yofs + 4 * x]) + HH(png.data[yofs + 4 * x + 1]) + HH(png.data[yofs + 4 * x + 2]) + HH(png.data[yofs + 4 * x + 3]), color.toString(16));
}
        function HH(val) { return ("00" + val.toString(16)).slice(-2); } //2 hex digits
//        return this; //fluent
    }

//send pixel data on screen:
//    const super_paint = this.paint.bind(this);
//GpuCanvas_shim.prototype.paint = function
        THIS.paint = function
    paint(alt_pixels)
    {
        if (WANT_SHARED && !cluster.isMaster) return process.send({done: true});
//console.log(`paint: ${this.width} x ${this.height}, progress? ${this.SHOW_PROGRESS}, elapsed ${this.elapsed || 0}, duration ${this.duration}, x-limit %d %d`, this.width * (this.elapsed || 0) / this.duration, Math.round(this.width * (this.elapsed || 0) / this.duration));
        if (!alt_pixels) //don't mess up caller's data
        if (this.SHOW_PROGRESS && this.duration) //kludge: use last row of pixels to display progress bar
            for (var x = 0; x < Math.round(this.width * (this.elapsed || 0) / this.duration); ++x)
                this.pixels[(x + 1) * this.height -1] = WHITE;
//        return Object.getPrototypeOf(this).
//        super_paint(pixels); //CAUTION: blocks a few msec for bkg renderer
//        super.paint(alt_pixels || this.pixels); //CAUTION: blocks a few msec for bkg renderer
        GpuCanvas.prototype.paint.call(this, alt_pixels || this.pixels); //CAUTION: blocks a few msec for bkg renderer
        if (isNaN(++this.render_count)) this.render_count = 1;
//        return this; //fluent
    } //.bind(this);


//give render stats to caller for debug / performance analysis:
//GpuCanvas_shim.prototype.render_stats = function
        THIS.render_stats = function
    render_stats(start)
    {
        if (start) this.save = {count: this.render_count || 0, sttime: Date.now()};
        else
        {
            var num = this.render_count - this.save.count;
            var den = (Date.now() - this.save.sttime) / 1000; //sec
//console.log("render.stats", num, den, num / den);
            return num / den;
        }
    }
//}
        return THIS;
    },
/*
    get: function(target, key)
    {
        switch (key) //TODO: method => getter
        {
            case "width": return target.width_tofix();
            case "height": return target.height_tofix();
            case "pivot": return target.pivot_tofix();
            default: return target[key];
        }
    },
    set: function(target, key, newvalue)
    {
        switch (key) //TODO: method => getter + setter
        {
            case "pivot": target.pivot_tofix(newvalue); return;
            default: target[key] = newvalue; return;
        }
    },
*/
});


//add push/pop method and getter/setter for each listed property:
function pushable(props)
{
    this._pushable = {};
//use push, pop namespace to avoid conflict with getters, setters:
    this.push = {};
    this.pop = {};
    if (!Array.prototype.top) Object.defineProperty(Array.prototype, "top",
    {
        get: function()
        {
            if (!this.length) throw new Error("Pushable stack underflow".red_lt);
            return this[this.length - 1]; //.slice(-1)[0];
        },
        set: function(newval)
        {
            if (!this.length) throw new Error("Pushable stack underflow".red_lt);
            this[this.length - 1] = newval;
        },
    });
//CAUTION: use let + const to force name + prop to be unique each time; see http://stackoverflow.com/questions/750486/javascript-closure-inside-loops-simple-practical-example
//console.log("pushables", JSON.stringify(props));
    for (let name in props) //CAUTION: need "let" here to force correct scope
    {
        const prop = this._pushable[name] = [props[name]]; //create stack with initial value
//console.log("pushable %s = %s", name, props[name]);
//set up getters, setters:
//ignore underflow; let caller get error
        Object.defineProperty(this, name,
        {
            get: function() { return prop.top; }.bind(this),
            set: function(newval) { prop.top = newval; update.call(this, name); }.bind(this),
        });
//set up push/pop:
        this.push[name] = function(newval) { prop.push(newval); update.call(this, name); }.bind(this);
        this.pop[name] = function() { var retval = prop.pop(); update.call(this, name); return retval; }.bind(this);
//        prop = null;
    }

    function update(name)
    {
        if (name == "WS281X_FMT") this.pivot = this[name];
//console.log("update %s = %s", name, this[name]);
        this.paint(); //kludge: handle var changes upstream
    }
}


////////////////////////////////////////////////////////////////////////////////
////
/// Fixups; TODO: below code should be moved back into C++ instead of doing it here
//

/*
//fix up methods exposed from C++ addon:
function fixup(imports)
{
    var retval = {};
//    retval.isRPi = imports.isRPi_tofix(); //TODO: function => value
    retval.Screen = imports.Screen_tofix(); //TODO: function => object
    retval.GpuCanvas = imports.GpuCanvas; //just pass it thru for now
    retval.UnivTypes = imports.UnivTypes_tofix(); //TODO: function => object
//no worky here:
//    etters(retval.GpuCanvas.prototype); //NOTE: object not instantiated yet; can't use Object.prototypeOf()?
    return retval;
}


//put getter/setter wrappers around methods:
function etters(clsproto)
{
    Object.defineProperties(clsproto,
    {
//NOTE: at run time "this" will be instantiated object:
        width: //TODO: method => getter
        {        
            get: function() { return this.width_tofix(); },
        },
        height: //TODO: method => getter
        {        
            get: function() { return this.height_tofix(); },
        },
        pivot: //TODO: method => getter + setter
        {        
            get: function() { return this.pivot_tofix(); },
            set: function(newval) { this.pivot_tofix(newval); },
        },
    });
}
*/


//EOF
