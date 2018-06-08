#!/usr/bin/env node
//Javascript wrapper to expose GpuCanvas class and Screen config object, manage threads.
//Ideally, Node.js would support multiple threads within the same process so memory could be shared.
//However, only the foreground thread can use the event loop or v8 so use we are forced to use multiple processes instead.
//Shared memory is used to cut down on inter-process data communication overhead.
//That leaves IPC latency as the main challenge.  (60 FPS only allows 16 msec total for fx gen + render)

"use strict"; //find bugs easier
require('colors').enabled = true; //console output colors; allow bkg threads to use it also
const os = require('os');
const fs = require('fs');
const glob = require('glob');

//cut down on verbosity:
//const {debug} = require('./demos/shared/debug');
const debug = function(...args)
{
    const debug_inner = require('./demos/shared/debug').debug;
//    args[0] = (args[0] || "").replace(/undefined(:undefined)?|string:|number:|object:|boolean:/g, str => str.slice(0, 3) + ((str.slice(-1) == ":")? ":": ""));
//    args[0] = (args[0] || "").replace(/undef(ined(:undefined)?)|str(ing):|num(ber):|obj(ect):|bool(ean):/g, (match, p1, p2, p3, p4, p5, p6) => match.replace(p1 || p2 || p3 || p4 || p5 || p6, ""));
    args[0] = (args[0] || "").replace(/undef(ined)(:undefined)?|str(ing):|num(ber):|obj(ect):|bool(ean):/g, (match, ...todrop) => /*todrop.slice(0, 6).join("").red_lt +*/ match.replace(todrop.slice(0, 6).join(""), "")); //.replace(parts.join("+", ""));
    ++debug_inner.nested; //kludge: compensate for extra stack level
    debug_inner.apply(null, args);
}
//debug("undefined  undefined:undefined  undefined:x");
//debug("string  string:string  string:x");

//allow easier access to top of stack:
if (!Array.prototype.top)
    Object.defineProperty(Array.prototype, "top",
    {
        get: function() { return this[this.length - 1]; }, //this.slice(-1)[0]
        set: function(newval) { this[this.length - 1] = newval; },
        enumerable: true, //make it visible in "for" loops (useful for debug)
    });


//ARGB colors:
const BLACK = 0xff000000; //NOTE: need A to affect output
//const WHITE = 0xffffffff;

//const U32SIZE = Uint32Array.BYTES_PER_ELEMENT; //reduce verbosity
//const MAGIC = 0x1234beef; //used to detect if shared memory is valid
//const HDRLEN = 2; //magic + sync count

//const QUIT = -1000; //0x7fffffff; //uint32 value to signal eof


////////////////////////////////////////////////////////////////////////////////
////
/// Exports
//

//const {Screen, GpuCanvas, UnivTypes, /*shmbuf, AtomicAdd,*/ usleep, RwlockOps, rwlock} = require('./build/Release/gpu-canvas.node'); //bindings('gpu-canvas'); //fixup(bindings('gpu-canvas'));
const Screen = {};
Screen.width = 1024; Screen.height = 768; Screen.fps = 30; //TODO:
const UnivTypes = {};
function usleep() {}


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
module.exports.Screen.vgaIO = vgaIO();
module.exports.Screen.isRPi = isRPi();
//module.exports.GpuCanvas = GpuCanvas_shim;


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
function vgaIO()
{
    const DVOs = "/sys/firmware/devicetree/base"; //folder for device overlays
    if (vgaIO.hasOwnProperty('cached')) return vgaIO.cached;
//check if various device tree overlays are loaded:
//dpi24 or vga565/666 send video to GPIO pins
    var dpi24 = glob.sync(DVOs + "/soc/gpio*/dpi24*") || [];
    if (dpi24.length) return vgaIO.cached = 8+8+8;
    var vga666 = glob.sync(DVOs + "/soc/gpio*/vga666*") || [];
    if (vga666.length) return vgaIO.cached = 6+6+6;
    var vga565 = glob.sync(DVOs + "/soc/gpio*/vga565*") || [];
    if (vga565.length) return vgaIO.cached = 5+6+5;
//console.log("dpi", dpi);
    return vgaIO.cached = false;
}


/////////////////////////////////////////////////////////////////////////////////
////
/// GpuCanvas class:
//

//options for when OpenGL support is re-enabled:
const LIVEOK = 1, DEVONLY = 2; //when an option can be used
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


const GpuCanvas = //allow refs elsewhere in file
module.exports.GpuCanvas =
//CAUTION: classes are not hoisted
class GpuCanvas
{
//    if (!(this instanceof Array2D)) return new Array2D(opts);
//    Proxy.call(this, function(){ this.isAry2D = true; }, hooks);
    constructor(opts) //constructor(title, w, h, want_fmt, opts)
{
//        console.log(`Array2D ctor: ${JSON.stringify(opts)}`.blue_lt);
        opts = ctor_opts(Array.from(arguments));
        this.devmode = firstOf((opts || {}).DEV_MODE, OPTS.DEV_MODE[1], true);
        this.title = firstOf((opts || {}).TITLE || OPTS.TITLE[1], "(title)");
        this.num_wkers = firstOf((opts || {}).NUM_WKERS, OPTS.NUM_WKERS[1], 0);
        if (this.num_wkers > os.cpus().length) debug(`#wkers ${num_wkers} exceeds #cores ${os.cpus().length}`.yellow_lt);
//https://stackoverflow.com/questions/37714787/can-i-extend-proxy-with-an-es2015-class
//for proxy info see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
//for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
//        this.UnivType = []; //TODO -> C++; see https://github.com/TooTallNate/ref-array/
//new Proxy(function(){ this.isAry2D = true; },
//        const {w, h} = opts || {};
//        const defaults = {w: 1, h: 1};
        const want_debug = true; //reduce xyz() overhead
        Object.assign(this, /*defaults,*/ opts || {}); //{width, height, depth}
//store 3D dimensions for closure use; also protects against caller changing dimensions after array is allocated:
        const width = this.w || this.width || 1, height = this.h || this.height || 1, depth = this.d || this.depth || 1;
        const pixary /*this.ary*/ = new Array(width * height * depth); //[]; //contiguous for better xfr perf to GPU
//        const xyz /*this.xy = this.xyz*/ = /*(isNaN(this.w || this.width) || isNaN(this.h || this.height))*/(height * depth == 1)? inx1D: /*isNaN(this.d || this.depth)*/(depth == 1)? inx2D: inx3D;
//        var [x, y, z] = key.split(","); //[[...]] in caller becomes comma-separated string; //.reduce((prev, cur, inx, all) => { return }, 0);
        this.WKER_ID = -1; //TODO: main (parent) thread; 0..n-1 are wkers
        pixary.fill(BLACK);
        if (opts.UNIV_TYPE) for (var i = 0; i < opts.width; ++i) this.UnivType[i] = opts.UNIV_TYPE;

//        const SHMKEY = 0xbeef; //make value easy to find (for debug)
//        THIS.pixels = WANT_SHARED?
//            new Uint32Array(shmbuf(SHMKEY, THIS.width * THIS.height * Uint32Array.BYTES_PER_ELEMENT)):
//            new Uint32Array(THIS.width * THIS.height); //create pixel buf for caller; NOTE: platform byte order
//        if ((WANT_SHARED === false) || cluster.isMaster) THIS.pixels.fill(BLACK); //start with all pixels dark
        this.elapsed = 0;
//make fmt pushable for simpler debug/control in caller:
        const fmt = [firstOf((opts || {}).WS281X_FMT, OPTS.WS281X_FMT[1], true)]; //stack
        Object.defineProperty(this, "WS281X_FMT",
        {
            get: function() { return fmt.top; }.bind(this),
            set: function(newval) { fmt.top = newval; /*this.pivot = this[name]*/; this.paint(); }, //kludge: handle var changes upstream
        });

        debug(`after ctor: ${JSON.stringify(this)}, #pixels ${pixary.length}`.blue_lt);
        if (want_debug) this.dump = function(desc) { console.log(`GpuCanvas dump pixels '${desc || ""}': ${pixary.length} ${JSON.stringify(pixary)}`.blue_lt); }
//create proxy to intercept property accesses:
//most accesses will be for pixel array, so extra proxy overhead for non-pixel properties is assumed to be insignificant
//for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
//proxy object can also trigger method calls on property updates
        return new Proxy(this, //return proxy object instead of default "this"; see https://stackoverflow.com/questions/40961778/returning-es6-proxy-from-the-es6-class-constructor?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
/*NO; ctor already happened (caller)
            construct: function(target, ctor_args, newTarget) //ctor args: [{width, height}]
            {
                const {w, h} = ctor_args[0] || {};
                const THIS = {pxy_w: w, pxy_h: h}; //wrapped obj
//                THIS.dump = function() { console.log(`Array2D dump: w ${this.w}. h ${this.h}`.blue_lt); }
//                THIS.func = function() { console.log("func".blue_lt); }
                console.log(`Array2D ctor trap: ${typeof target} => ${typeof newTarget}, ${JSON.stringify(THIS)} vs ${JSON.stringify(this)}`.cyan_lt);
                return THIS;
            },
*/    
            get: function(target, key, rcvr)
            {
    //        if (!(propKey in target)) throw new ReferenceError('Unknown property: '+propKey);
    //        return Reflect.get(target, propKey, receiver);
//                console.log("get " + key);
//                if (!(key in target)) target = target.ary; //if not at top level, delegate to array contents
                const inx = xyz(key), istop = isNaN(inx);
                const value = istop? target[key]: (inx >= 0)? pixary[inx]: BLACK; //null;
                if (typeof value == 'function') return function (...args) { return value.apply(this, args); } //Reflect[key](...args);
                debug(shorten(`got-${istop? "obj": (inx < 0)? "noop": "ary"}[${typeof key}:${key} => ${typeof inx}:${inx}] = ${typeof value}:${value}`.blue_lt));
      //        console.log(`ary2d: get '${key}'`);
    //        return Reflect.get(target, propname, receiver);
                return value;
            },
            set: function(target, key, newval, rcvr)
            {
                const inx = xyz(key), istop = isNaN(inx);
                debug(shorten(`set-${istop? "obj": (inx < 0)? "noop": "ary"}[${typeof key}:${key} => ${typeof inx}:${inx}] = ${newval}`.blue_lt));
                return istop? target[key] = newval: (inx >= 0)? /*target.*/pixary[inx] = newval: newval; //Reflect.set(target, key, value, rcvr);
            },
        });

//multi-dim array indexing:
        function xyz(key)
        {
            const [x, y, z] = key.split(","); //[[x,y,z]] in caller becomes comma-separated string
            const inx = (typeof y == "undefined")? (isNaN(x)? x: (+x < pixary.length)? +x: -1): //1D; range check or as-is
                            (isNaN(x) || isNaN(y))? key: //prop name; not array index
                            (typeof z == "undefined")? //2D
                                (/*(+x >= 0) &&*/ (+x < width) && (+y >= 0) && (+y < height))? +x * height + +y: -1: //range check
                            isNaN(z)? key: //prop name; not array index
                                (/*(+x >= 0) &&*/ (+x < width) && (+y >= 0) && (+y < height) && (+z >= 0) && (+z < depth))? (+x * height + +y) * depth + +z: -1; //range check
            if (want_debug && isNaN(++(xyz.seen || {})[key])) //reduce redundant debug info
            {
                debug(shorten(`xyz '${key}' => x ${typeof x}:${x}, y ${typeof y}:${y}, z ${typeof z}:${z} => inx ${typeof inx}:${inx}`.blue_lt));
                if (!xyz.seen) xyz.seen = {};
                xyz.seen[key] = 1;
            }
            return inx;
        }
    }
};
//inherits(Array2D, Proxy);


/*
//??        THIS.atomic = atomic_method;    
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
//        / *process.nextTick* / step(cluster.isMaster? master_async.bind(THIS, num_wkers, auto_play): worker_async.bind(THIS)); //give caller a chance to define custom methods
//for setImmediate vs. process.nextTick, see https://stackoverflow.com/questions/15349733/setimmediate-vs-nexttick
    THIS.playback = cluster.isMaster? master_async: worker_async; //allow caller to override these
    if (cluster.isMaster && auto_play) setImmediate(function() { step(THIS.playback); }); //defer execution so caller can override playback()
    return THIS;
*/


//////////////////////////////////////////////////////////////////////////////////
////
/// Helper functions:
//

//get and validate ctor opts:
function ctor_opts(args)
{
    debug(`#args: ${args.length}, last is '${typeof args.top}'`.blue_lt);
//        const [title, width, height, opts] = arguments;
//        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
//combine, make local copy of args:
    const my_opts = {};
    if (typeof args.top == "object") { Object.assign(my_opts, args.top); args.pop(); } //named args at end
    switch (args.length) //handle fixed position (unnamed) args (old api, backwards compatible)
    {
        case 2: my_opts.w = args[0]; my_opts.h = args[1]; break; //w, h
        case 4: my_opts.WS281X_FMT = args[3]; //title, w, h, fmt (falls thru)
        case 3: my_opts.h = args[2]; my_opts.w = args[1]; //title, w, h (falls thru)
        case 1: my_opts.title = args[0]; //title (falls thru)
        case 0: break;
        default: throw new Error(`GpuCanvas ctor only expects 4 unnamed args, got ${args.length}`.red_lt);
    }
    debug(`ctor(my ${args.length} opts ${JSON.stringify(my_opts)})`.blue_lt);
//validate:
    const devmode = firstOf((my_opts || {}).DEV_MODE, OPTS.DEV_MODE[1], true);
    Object.keys(my_opts || {}).forEach((key, inx, all) =>
    {
//        if (/*opts[key] &&*/ !allowed_OPTS[key]) throw new Error(`Option '${key}' not supported until maybe 2018 (OpenGL re-enabled)`.red_lt);
        const [opt_mode, def_val] = OPTS[key] || [];
        if (!opt_mode/*(key in OPTS)*/) throw new Error(`GpuCanvas: option '${key}' not supported until maybe later`.red_lt);
//        if (!all.WS281X_FMT || (['WS281X_FMT', 'UNIV_TYPE', 'WANT_SHARED'].indexOf(key) != -1)) return;
//        all[inx] = null; //don't show other stuff in live mode (interferes with output signal)
//        console.error(`Ignoring option '${key}' for live mode`.yellow_lt);
        debug(`check opt[${inx}/${all.length}] '${key}' ${typeof my_opts[key]}:${my_opts[key]} vs. allowed mode ${typeof opt_mode}:${opt_mode}, def val ${typeof def_val}:${def_val}, dev mode? ${devmode /*my_opts.DEV_MODE*/}`.blue_lt)
        if (/*my_opts.DEV_MODE*/ devmode) return; //all options okay in dev mode
        if (opt_mode != DEVONLY) return; //option okay in live mode
        if (all[key] == def_val) return; //default value ok in live mode
//            all[inx] = null; //don't show other stuff in live mode (interferes with output signal)
        console.error(`GpuCanvas: ignoring non-default option '${key}' in live mode`.yellow_lt);
    });
//        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
//        if (typeof opts.WANT_SHARED == "undefined") opts.WANT_SHARED = !cluster.isMaster;
//        if (opts.WS281X_FMT) delete opts.SHOW_PROGRESS; //don't show progress bar in live mode (interferes with LED output)
//        if (opts.WS281X_FMT) opts = {WS281X_FMT: opts.WS281X_FMT, UNIV_TYPE: opts.UNIV_TYPE}; //don't show other stuff in live mode (interferes with LED output)
//        this.opts = opts; //save opts for access by other methods; TODO: hide from caller
      return my_opts;
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

//eof