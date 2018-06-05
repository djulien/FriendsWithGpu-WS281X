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

//allow easier access to top of stack:
if (!Array.prototype.top)
    Object.defineProperty(Array.prototype, "top",
    {
        get: function() { return this[this.length - 1]; },
        set: function(newval) { this[this.length - 1] = newval; },
        enumerable: true, //make it visible in "for" loops (useful for debug)
    });


////////////////////////////////////////////////////////////////////////////////
////
/// Exports
//

//const {Screen, GpuCanvas, UnivTypes, /*shmbuf, AtomicAdd,*/ usleep, RwlockOps, rwlock} = require('./build/Release/gpu-canvas.node'); //bindings('gpu-canvas'); //fixup(bindings('gpu-canvas'));
const Screen = {}, UnivTypes = {};
function usleep() {}
const {debug} = require('./demos/shared/debug');


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
    const DVOs = "/sys/firmware/devicetree/base"; //device overlays
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
class GpuCanvas
{
//    constructor(title, w, h, opts)
    constructor(args)
    {
        const opts = ctor_opts(arguments);
        this.devmode = firstOf((opts || {}).DEV_MODE, OPTS.DEV_MODE[1], true);
        const title = firstOf((opts || {}).TITLE || OPTS.TITLE[1], "(title)");
        const num_wkers = firstOf((opts || {}).NUM_WKERS, OPTS.NUM_WKERS[1], 0);
        if (num_wkers > os.cpus().length) debug(`#wkers ${num_wkers} exceeds #cores ${os.cpus().length}`.yellow_lt);
//for proxy info see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
//for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
//        this.UnivType = []; //TODO -> C++
        this.UnivType = new Proxy(function(){}, //[]; //use proxy object to trigger updates
        {
            get: function(target, propname, receiver)
            {
                const targetValue = Reflect.get(target, propname, receiver);
                if (typeof targetValue == 'function')
                {
                    return function (...args)
                    {
                        debug(`GpuCanvas: call '${propname}' with ${args.length} args`);
                        return targetValue.apply(this, args);
                    }
                }
                debug(`GpuCanvas: get '${propname}'`);
            //        return Reflect.get(target, propname, receiver);
                return targetValue;
            },
        };
        if (opts.UNIV_TYPE) for (var i = 0; i < opts.width; ++i) this.UnivType[i] = opts.UNIV_TYPE;

//        const SHMKEY = 0xbeef; //make value easy to find (for debug)
//        THIS.pixels = WANT_SHARED?
//            new Uint32Array(shmbuf(SHMKEY, THIS.width * THIS.height * Uint32Array.BYTES_PER_ELEMENT)):
//            new Uint32Array(THIS.width * THIS.height); //create pixel buf for caller; NOTE: platform byte order
//        if ((WANT_SHARED === false) || cluster.isMaster) THIS.pixels.fill(BLACK); //start with all pixels dark
        this.elapsed = 0;
//make fmt pushable for simpler debug/control in caller:
        const fmt = [firstOf((opts || {}).WS281X_FMT, OPTS.WS281X_FMT[1], true);
        Object.defineProperty(this, "WS281X_FMT",
        {
            get: function() { return fmt.top; }.bind(this),
            set: function(newval) { fmt.top = newval; /*this.pivot = this[name]*/; this.paint(); }, //kludge: handle var changes upstream
        });
    }

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
}


//////////////////////////////////////////////////////////////////////////////////
////
/// Helper functions:
//

//get and validate ctor opts:
function ctor_opts(args)
{
    var narg = arguments.length;
    debug(`#args: ${narg}, last is '${typeof arguments[narg - 1]}'`.blue_lt);
//        const [title, width, height, opts] = arguments;
//        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
//combine, make local copy of args:
    const my_opts = {};
    if (typeof arguments[narg - 1] == "object") Object.assign(my_opts, arguments[--narg]); //named args at end
    switch (narg) //handle fixed position (unnamed) args
    {
        case 4: my_opts.WS281X_FMT = arguments[3]; //old api; backwards compatible
        case 3: my_opts.h = arguments[2];
        case 2: my_opts.w = arguments[1];
        case 1: my_opts.title = arguments[0];
        case 0: break;
        default: throw new Error(`GpuCanvas ctor only expects 3 unnamed args, got ${narg}`.red_lt);
    }
    debug(`GpuCanvas.ctor(opts ${JSON.stringify(my_opts)})`.blue_lt);
//validate:
    Object.keys(my_opts || {}).forEach((key, inx, all) =>
    {
//        if (/*opts[key] &&*/ !allowed_OPTS[key]) throw new Error(`Option '${key}' not supported until maybe 2018 (OpenGL re-enabled)`.red_lt);
        const [opt_mode, def_val] = OPTS[key];
        if (!opt_mode/*(key in OPTS)*/) throw new Error(`GpuCanvas: option '${key}' not supported until maybe later`.red_lt);
//        if (!all.WS281X_FMT || (['WS281X_FMT', 'UNIV_TYPE', 'WANT_SHARED'].indexOf(key) != -1)) return;
//        all[inx] = null; //don't show other stuff in live mode (interferes with output signal)
//        console.error(`Ignoring option '${key}' for live mode`.yellow_lt);
        if (all.DEV_MODE) return; //all options okay in dev mode
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