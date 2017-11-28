#!/usr/bin/env node
//Javascript wrapper to expose GpuCanvas class and Screen config object

"use strict"; //find bugs easier
var fs = require('fs');
const glob = require('glob');
const pathlib = require('path');
require('colors').enabled = true; //for console output colors
//const {inherits} = require('util');
const bindings = require('bindings');
//const {debug} = require('./shared/debug');
const {Screen, GpuCanvas} = fixup(bindings('gpu-canvas'));

module.exports.Screen = Screen;
//attach config info as properties:
//no need for callable functions (config won't change until reboot)
module.exports.Screen.gpio = toGPIO();
module.exports.Screen.isRPi = isRPi();
//module.exports.GpuCanvas = GpuCanvas_shim;

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
const supported_OPTS =
{
    SHOW_INTRO: 10, //how long to show intro (on screen only)
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
};


//ARGB colors:
const BLACK = 0xff000000; //NOTE: need A to affect output
const WHITE = 0xffffffff;

//temp shim until OpenGL support is re-enabled:
//use ctor function instead of class syntax to allow hidden vars, factory call, etc
//function GpuCanvas_shim(title, width, height, opts)
module.exports.GpuCanvas =
class GpuCanvas_shim extends GpuCanvas
{
//    if (!(this instanceof GpuCanvas_shim)) return new GpuCanvas_shim(title, width, height, opts); //convert factory call to class
    constructor(title, width, height, opts)
    {
        if (!opts) opts = (typeof opts == "boolean")? {WS281X_FMT: opts}: {};
        Object.keys(opts).map(key => { if (opts[key] && !supported_OPTS[key]) throw `Opt '${key}' not supported until OpenGL re-enabled`.red_lt; });
//        if (opts.WS281X_FMT) delete opts.SHOW_PROGRESS; //don't show progress bar in live mode (interferes with LED output)
        if (opts.WS281X_FMT) opts = {WS281X_FMT: opts.WS281X_FMT}; //don't show other stuff in live mode (interferes with LED output)
//        this.opts = opts; //save opts for access by other methods; TODO: hide from caller
//console.log(title, width, height, opts);
//    GpuCanvas.call(this, title, width, height, opts.WS281X_FMT); //initialize base class
        super(title, width, height, !!opts.WS281X_FMT); //initialize base class
//        etters(Object.getPrototypeOf(this)); //kludge: can't get it to work within fixup; do it here instead
        pushable.call(this, Object.assign(supported_OPTS, opts || {}));

        this.pixels = new Uint32Array(this.width * this.height); //create pixel buf for caller; NOTE: platform byte order
        this.pixels.fill(BLACK); //start with all pixels dark
        this.elapsed = 0;
    }

//get/set individual pixel:
    pixel(x, y, color)
    {
//console.log("pixel(%d, %d): w %d, h %d".blue_lt, x, y, this.width, this.height);
        var clip = ((x < 0) || (x >= this.width) || (y < 0) || (y >= this.height)); //throw "Index of out range";
        if (arguments.length > 2) //set color
        {
            if (!clip) this.pixels[x * this.height + y] = color;
            return this; //fluent
        }
        return !clip? this.pixels[x * this.height + y]: BLACK;
    } //.bind(this);

//fill all pixels:
    fill(color)
    {
        this.pixels.fill(color);
//        return this; //fluent
    }

//load image file into memory (PNG only):
    load(path)
    {
        const {PNG} = require('pngjs'); //nested so it won't be needed unless load() is called
        var png = PNG.sync.read(fs.readFileSync(path));
        path = pathlib.relative(__dirname, path);
        if ((png.width != this.width) || (png.height != this.height))
            console.error("image '%s' %d x %d doesn't match canvas %d x %d".yellow_lt, path, png.width, png.height, this.width, this.height);
            else console.error("image '%s' size %d x %d matches canvas".green_lt, path, png.width, png.height);
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
    paint(alt_pixels)
    {
//console.log("paint: w %d, h %d", this.width, this.height);
        if (!alt_pixels) //don't mess up caller's data
        if (this.SHOW_PROGRESS && this.duration) //kludge: use last row of pixels to display progress bar
            for (var x = 0; x < this.width * (this.elapsed || 0) / this.duration; ++x)
                this.pixels[(x + 1) * this.height -1] = WHITE;
//        return Object.getPrototypeOf(this).
//        super_paint(pixels); //CAUTION: blocks a few msec for bkg renderer
        super.paint(alt_pixels || this.pixels); //CAUTION: blocks a few msec for bkg renderer
        if (isNaN(++this.render_count)) this.render_count = 1;
//        return this; //fluent
    } //.bind(this);


//give render stats to caller for debug / performance analysis:
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
}
//inherits(GpuCanvas_shim, GpuCanvas);


//add push/pop method and getter/setter for each listed property:
function pushable(props)
{
    this._pushable = {};
//use push, pop namespace to avoid conflict with getters, setters:
    this.push = {};
    this.pop = {};
//CAUTION: use let + const to force name + prop to be unique each time; see http://stackoverflow.com/questions/750486/javascript-closure-inside-loops-simple-practical-example
    for (let name in props)
    {
        const prop = this._pushable[name] = [props[name]]; //create stack with initial value
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
//console.log("update %s = %s %s", name, this[name], this.pivot);
        this.paint(); //kludge: handle var changes upstream
    }
}


////////////////////////////////////////////////////////////////////////////////
////
/// Fixups; TODO: below code should be moved back into C++ instead of doing it here
//

//fix up methods exposed from C++ addon:
function fixup(imports)
{
    var retval = {};
//    retval.isRPi = imports.isRPi_tofix(); //TODO: function => value
    retval.Screen = imports.Screen_tofix(); //TODO: function => object
    retval.GpuCanvas = imports.GpuCanvas; //just pass it thru for now
//no worky here:
    etters(retval.GpuCanvas.prototype); //NOTE: object not instantiated yet; can't use Object.prototypeOf()?
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


//EOF
