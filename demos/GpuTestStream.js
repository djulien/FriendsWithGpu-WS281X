#!/usr/bin/env node
//generate GPU test data stream
//output format = approximately ndjson (for easier debug readability and piping/streaming compatibility)

require("colors").enabled = true; //https://github.com/Marak/colors.js/issues/127
//const fs = require("fs"); //"fs-extra");
//const ndjson = require("ndjson");
const {Readable} = require('stream');
const {Screen} = require("gpu-stream");
// /*const sprintf =*/ require('sprintf.js'); //.sprintf;
//const {step, pause} = require("blocking-style");

//const vix2loader = require("./vix2loader");
//const bufdiff = require("yalp/utils/buf-diff");
//const {calledfrom} = require("yalp/utils/caller");

//default settings (caller can override most):
const DEFAULTS =
{
    FPS: 60,
    NUM_UNIV: 24,
    UNIV_LEN: Screen.height,
    get DURATION() { return Math.ceil(this.NUM_UNIV * this.UNIV_LEN / this.FPS); }, //enough for one cycle (sec)
    LATENCY: 1.4, //estimated startup latency (sec)
    VERSION: 1.0, //file fmt
    COLOR_FMT: "ARGB", //ARGB is easier (for me) to read (for debug), but RPi wants BGRA so an extra conversion step is needed
    DEBUG_INFO: true,
};
//const COMPRESSION_STATS = true;


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
/// Module exports
//

const GpuTestStream =
module.exports =
function GpuTestStream(opts)
{
//step 1x to create stream for caller:
    return step(streamer(opts)); //execute asynchronously
}


//CLI for testing:
//delay to avoid hoisting errors
if (!module.parent)
setImmediate(function()
{
    console.error("running CLI test".green_lt);
    var gpustm = GpuTestStream();
//console.error("json str:", JSON.stringify_tidy(jsonstr));
//    yalp.SendData(); //NOTE: need to call pause() before read()
    gpustm
        .pipe(process.stdout)
        .on("error", function(err)
        {
            console.error(`ERROR on read#${gpustm.numrd}: ${err}`.red_lt); // - 0x60);
            process.exit();
        });
    console.error("CLI test continues asynchronously".green_lt);
});


function step(gen)
{
    if (step.done) throw "Generator function already completed.".red_lt;
	if (typeof gen == "function") gen = gen(); //invoke generator if not already
//        return setImmediate(function() { step(gen()); }); //avoid hoist errors
    if (gen) step.gen = gen; //save generator for subsequent steps
//console.log("step:", typeof gen, JSON.stringify_tidy(gen));
	var {value, done} = step.gen.next(step.retval); //send previous value to generator
//    {step.retval, step.done} = step.svgen.next(step.retval); //send previous value to generator
//    Object.assign(step, step.svgen.next(step.retval)); //send previous value to generator
    if (typeof value == "function") value = value();
    step.done = done; //prevent overrun
    return step.retval = value; //return value to caller and to next yield
}


////////////////////////////////////////////////////////////////////////////////
////
/// Readable stream
//

//create readable stream for caller:
//pumps data asynchronously on-demand to consumer
//NOTE: this is a generator function
function* streamer(opts)
{
    Object.assign(this, DEFAULTS, opts || {});
//    if (!opts) opts = {};
//    const fps = defaultas(opts.fps, FPS);
//    const speed = defaultas(opts.speed, 1 / FPS);
//    const latency = defaultas(opts.latency, LATENCY);
//    const duration = defaultas(opts.duration, DURATION);
//    const num_univ = defaultas(opts.num_univ, NUM_UNIV);
//    const univ_len = defaultas(opts.univ_len, UNIV_LEN);

    const started = now_sec();
    const pixels = new Uint32Array(this.NUM_UNIV * this.UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
    pixels.fill(BLACK); //start all dark

//instantiate Readable stream:
//don't need a full class definition, so just customize a regular Readable object
    const rs = Readable(); //{ objectMode: true , highWaterMark: 16384});
    rs._read = function(size)
    {
//        chkthis.call(this);
        if (isNaN(++this.numrd)) this.numrd = 1;
//console.log(`read# ${this.numreads}`);
        step(); //wake up streamer on behalf of consumer
    }
//format data as ndjson
    rs.pushJSON = function(data)
    {
//        chkthis.call(this);
        if (!(typeof data == "string")) data = JSON.stringify_tidy(data);
//console.log("rs push json", data);
        return this.push(data + "\n");
    }
//console.log("stream created, yielding now");
//    var seq = vix2loader(opts); //WANT_CHINFO? {filename: opts, dump_ch: true}: opts); //load seq before yielding to reduce latency
//    if (seq.numch != MAXCH) throw `Layout maxch ${MAXCH} != actual numch ${seq.numch}`.red_lt; //kludge: detect since not used at run-time

//    Object.keys(Models).forEach(name => { delete Models[name]; });
//    Models.All = {start: 1, end: seq.numch, name: "All"}; //dummy model using all channels
//    if ((opts || {}).want_palette) palettes(seq); //needs second pass; expensive, so do it before first yield
    yield rs; //return stream object to caller and pause until stream consumer wants data

//    for (var i = 0; i < 5; ++i)
//        yield rs.pushJSON({i});
//console.log("stream gen done");
//    return;

//write header info:
    yield rs.pushJSON(
    {
        time: milli(-this.LATENCY), //predate startup to compensate for latency
        fps: this.FPS,
        num_univ: this.NUM_UNIV,
        univ_len: this.UNIV_LEN,
        ver: `${VERSION}`, //file format
        cfmt: this.COLOR_FMT, //color format
        timestamp: new Date().toShortString(true), //allows cache validation
    });

//write frame data (UNcompressed):
    console.error("loop for %d sec (%d frames) ...".green_lt, this.DURATION, this.DURATION * this.FPS);
    for (var xy = 0; xy < this.DURATION * this.FPS; ++xy) //11sec @60 FPS
    {
        var x = Math.floor(xy / this.UNIV_LEN) % this.NUM_UNIV, y = xy % this.UNIV_LEN; ///cycle thru cols and rows
        const color = PALETTE[(x + y + Math.floor(xy / pixels.length)) % PALETTE.length]; //vary each cycle
//        fibo(25); //simulate some CPU work
        if (this.SPEED) yield wait(this.SPEED); //throttle (caller wants explicit control over back pressure)
        pixels[xy % pixels.length] = color; //turn on one pixel at a time

        var buf = hex32(pixels);
//        yield rs.pushJSON(`{sttime:${timestamp(fr * seq.FixedFrameInterval / 1000)}, fr: ${fr}, buf: ${buf}}`); //delta${rawbuf}}`);
        yield rs.pushJSON(`{time: ${milli(xy / this.FPS)}, fr: ${xy}, buf: ${buf}}`); //use string to preserve buf fmting; //delta${rawbuf}}`);
    }

//write trailer info (pad last frame, include stats):
//    var total = (compress.by_block || 0) + (compress.by_color || 0); //+ (compress.by_ofs || 0);
    yield rs.pushJSON(
    {
        time: this.DURATION, ///pad out last (fractional) interval
        eof: true,
//stats (mainly for debug):
        reads: commas(rs.numrd),
  //      dups: commas(dedupe.dups),
        gentime: milli(now_sec() - started), //sprintf("%4.3f", now_sec() - started), //sec
/*
        compression:
        {
            B: `${commas(compress.by_block || 0)} (${percent(compress.by_block / total)}%)`,
            C: `${commas(compress.by_color || 0)} (${percent(compress.by_color / total)}%)`,
//            O: `${commas(compress.by_ofs || 0)} (${percent(compress.by_ofs / total)}%)`,
        },
*/
    });

    rs.push(null); //eof
//    return rs;
//    console.log("streamer done");
}


//burn some CPU cycles:
//function fibo(n)
//{
//    return (n < 3)? n: fibo(n - 1) + fibo(n - 2);
//}


//function chkthis(where)
//{
//    if (!(this instanceof Readable)) throw `bad this @${where || calledfrom(2, true)}: ${typeof this}`;
////console.log(`this okay @${where || calledfrom(2, true)}`.yellow_lt);
//    return this; //fluent
//}


////////////////////////////////////////////////////////////////////////////////
////
/// Helper functions, extensions
//

//Array getter for last element:
//if (!Array.prototype.last)
//    Object.defineProperty(Array.prototype, "last", {get() { return this[this.length - 1]; }, enumerable: false});

//Array-like join() for Object:
//CAUTION: don't add to Object prototype; affects too many objects
//if (!Object.join)
//    Object.join = function(obj, sep) { return Object.values(obj).join(sep); };

//add parens only if needed:
//Array.prototype.join_parens = function(sep) { return (this.length > 1)? "[" + this.join(sep) + "]": this[0]; };

//NOTE: Object.values() not supported yet
//if (!Object.values)
//    Object.values = function(obj) { return Object.keys(obj).map(key => { return obj[key]; }); };

//delay next step of generator:
function wait(msec)
{
    return setTimeout.bind(null, step, msec); //NOTE: need to return a function so step() will evaluate it outside of generator
}


//improve readability (for debug):
//puts spaces after commas and colons (for human readability), removes quotes from keys
//NOTE: requires undo on receiving end
JSON.stringify_tidy = function(thing)
{
    return (JSON.stringify(thing) || "")
        .replace(/,?"([A-Z0-9$@_]+)":/gi, (match, name) =>
        {
            return ((match.charAt(0) == ",")? ", ": "") + name + ": ";
        }); //"$1:");
};


//shorter string format (US fmt):
if (!Date.prototype.toShortString) Date.prototype.toShortString = 
function toShortString(want_time)
{
//    return `${this.getMonth() + 1}/${this.getDate()}/${this.getFullYear()} ${this.getHours()}:${("00" + this.getMinutes()).slice(-2)}:${("00" + this.getSeconds()).slice(-2)} ${this.toString().replace(/^.*\(|\).*$/g, "")}`;
    var date = `${this.getMonth() + 1}/${this.getDate()}/${this.getFullYear()}`;
    var time = `${this.getHours()}:${NN(this.getMinutes())}:${NN(this.getSeconds())}`;
    var tz = this.toString().replace(/^.*\(|\).*$/g, "");
    return want_time? `${date} ${time} ${tz}`: date;
}


//convert a scalar or array/buffer to hex string:
function hex32(val) //, want_compress)
{
    if (!hex32.order) //figure out byte order
    {
        const byteorder = new Uint32Array([0x12345678]);
        const bytes = new Uint8Array(byteorder.buffer);
        hex32.order = (bytes[0] == 0x12)? "BE": //big endian
                      (bytes[0] == 0x78)? "LE": //little endian
                      function unhandled() { throw `Unhandled byte order: 0x${NN(bytes[0], 16)}${NN(bytes[1], 16)}${NN(bytes[2], 16)}${NN(bytes[3], 16)}`.red_lt; }();
//        const byteorder = Buffer.from([0x12, 0x34, 0x56, 0x78]);
//(bytes(0x12345678)[0] == 0x12)? , 0byteorder.readUInt32BE(0) == 0x12
//    const pixels = new Uint32Array(NUM_UNIV * UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
    }
    if (typeof val.length == "undefined") return val? "0x" + val.toString(16): "0"; //scalar
    var str = [];
    if (Buffer.isBuffer(val))
    {
        var fmter = val["readUInt32" + hex32.order].bind(val);
        for (var ofs = 0; ofs < val.length; ofs += 4) //uint32.BYTES_PER_ELEMENT
            str.push(hex32(fmter(ofs))); //.toString(16);
    }
    else //real or typed array
    {
        for (var ofs = 0; ofs < val.length; ++ofs)
            str.push(hex32(val[ofs])); //.toString(16);
    }
    return (str.length > 1)? "[" + str.join(", ") + "]": str[0];
}


//display commas for easier debug:
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return (val < 1000)? val: val.toLocaleString();
}


//function defaultas(val, defval)
//{
//    return (typeof val != "undefined")? val: defval; //"val || defval" doesn't work for val == 0
//}


//function percent(val)
//{
//    if (!val) return 0;
//    var scale = (val >= 0.1)? 1: (val >= 0.01)? 10: 100;
//    return Math.round(100 * scale * val) / scale;
//}


function NN(str, radix)
{
    if (radix) str = str.toString(radix);
    return ("00" + str).slice(-2);
}


//truncate after 3 dec places:
function milli(val)
{
    return Math.floor(val * 1e3 + 0.5) / 1e3; ///rounded
}


//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}

//EOF
