#!/usr/bin/env node
//writable GpuCanvas stream; this is a stream wrapper around GpuCanvas object

/*
TODO:
//newtest.js -> gpu-test-stream.js
mp3player.js(lame + speaker) + GpuStream.js(index.js) + latency balancer -> gpu-player.js
vix2loader.js + vix2models.js = vix2stream.js
vix2stream.js + GpuPlayer.js = show
*/

"use strict"; //find bugs easier
//const os = require('os');
const JSON5 = require("json5");
const {inherits} = require('util');
//const Writable = require('readable-stream/writable');
const {Writable} = require('stream');
const {LineStream} = require('byline');
const thru2 = require("through2"); //https://www.npmjs.com/package/through2
const debug = require('debug')('gpu-canvas');
const bindings = require('bindings');
const {isRPi, Screen, GpuCanvas} = fixup(bindings('gpu-canvas'));

//exports = module.exports = GpuStream;
module.exports.isRPi = isRPi;
module.exports.Screen = Screen;
module.exports.GpuCanvas = GpuCanvas;


//const os = require('os')
//const endianness = os.endianness()

//default settings (caller can override):
const CANVAS_DEFAULTS =
{
//stream options:
    lowWaterMark: 0,
    highWaterMark: 0,
//TODO?    objectMode: true,
//gpu canvas options:
    FPS: 60,
    NUM_UNIV: 24,
    UNIV_LEN: Screen.height,
    title: "GpuStream",
    PIVOT: true,
//    LATENCY: 1.4, //estimated startup latency (sec)
//    VERSION: 1.0, //file fmt
//    COLOR_FMT: "ARGB", //ARGB is easier (for me) to read (for debug), but RPi wants BGRA so an extra conversion step is needed
//    DEBUG_INFO: true,
};

const MAX_UNIV = 24; //#univ limited to #VGA GPIO pins without external mux hw


////////////////////////////////////////////////////////////////////////////////
////
/// Writable GPU stream
//

/**
 * The `GpuStream` class accepts arrays of pixel colors (ARGB) and writes them to the video display.
 * (full screen on RPi, windowed elsewhere)
 *
 * @param {Object} opts options object
 * @api public
 */
//NOTE: new "class" syntax prevents access to "this" before calling super(); maybe also doesn't allow calling ctor as function?
//use old syntax instead
//class GpuStream extends Writable
const GpuStream =
module.exports.GpuStream =
function GpuStream(opts)
{
    if (!(this instanceof GpuStream)) return new GpuStream(opts);
//    constructor(opts)
//    {
    Object.assign(this, CANVAS_DEFAULTS, opts || {});
//        if (!opts) opts = {};
//        if (!(this instanceof GpuStream)) return new GpuStream(opts);
//        if (typeof opts.lowWaterMark == "undefined") opts.lowWaterMark = 0;
//        if (typeof opts.highWaterMark == "undefined") opts.highWaterMark = 0;
    Writable.call(this, opts); //initialize base class
//        super(opts); //initialize base class

//        this.title = opts.title || "GpuStream";
//        this.num_univ = opts.num_univ || 24;
    if (this.NUM_UNIV > MAX_UNIV) throw new Error(`Too many universes: ${this.NUM_UNIV}.  (this version only supports ${MAX_UNIV})`.red_lt);
//        this.univ_len = opts.univ_len || Screen.height;
    if (this.UNIV_LEN > Screen.height) throw new Error(`Universe too long: ${this.UNIV_LEN}.  (screen height is ${Screen.height})`.red_lt);
//        this.pivot = opts.pivot;
    this._closed = false; //no write() calls allowed after close()

//bind event listeners:
    this.on("pipe", this._pipe);
    this.on("unpipe", this._unpipe);
    this.on("finish", this._flush);
//    }
}
inherits(GpuStream, Writable);


/**
 * Opens a window (full screen on RPi), and then emits an "open" event.
 *
 * @api private
 */
GpuStream.prototype._open =
function _open()
{
    debug("open()");
    if (this.canvas) throw new Error("GpuCanvas is already open.  _open() called more than once?".red_lt);
    this.canvas = new GpuCanvas(this.title, this.NUM_UNIV, this.UNIV_LEN, this.PIVOT);
    if (!this.canvas) throw new Error("GpuCanvas open() failed".red_lt);
    this.emit("open");
    return this.canvas;
}


/**
 * `_write()` callback for the Writable base class.
 *
 * @param {Buffer} chunk
 * @param {String} encoding
 * @param {Function} done
 * @api private
 */
GpuStream.prototype._write =
function _write(chunk, encoding, done)
{
    debug("_write() (%o bytes)", chunk.length);
    if (this._closed) return done(new Error("write() called after close()".red_lt));
    if (!this.canvas) //return done(new Error("write() called before open()".red_lt));
        try { this._open(); }
        catch (exc) { return done(exc); }
//TODO: allow commands/params embedded within stream?
    const chunkSize = this.NUM_UNIV * this.UNIV_LEN;
    for (var ofs = 0; ofs < chunk.length; ofs += chunkSize)
    {
        if (this._closed)
        {
            debug("aborting remainder of write() (%o bytes) after close()", chunk.length - ofs);
            return done();
        }
        var wrlen = Math.min(chunk.length - ofs, chunkSize);
        debug("writing %o bytes, ofs %o", wrlen, ofs);
        this.canvas.paint(chunk.slice(ofs, wrlen));
//            done(new Error(`write() failed: ${r}`))
        debug("wrote %o bytes", wrlen);
        if (isNaN(++this.numwr)) this.numwr = 1; //track #writes
        if (isNaN(this.wrtotal += wrlen)) this.wrtotal = wrlen; //track total data written
        this.emit("progress", {numwr: this.numwr, wrlen, wrtotal: this.wrtotal, buflen: chunk.length - ofs}); //give caller some progress info
    }
    debug("done with this chunk");
    done();
}


//TODO: methods for setting pivot, UnivType mid-stream?


//TODO: are these needed?
/**
 * Called when this stream is pipe()d to from another readable stream.
 * @api private
 */
GpuStream.prototype._pipe =
function _pipe(src)
{
    debug("_pipe()");
}


/**
 * Called when this stream is pipe()d to from another readable stream.
 * @api private
 */
GpuStream.prototype._unpipe =
function _unpipe(src)
{
    debug("_unpipe()");
}


/**
 * Emits a "flush" event and then calls the `.close()` function.
 * @api private
 */
GpuStream.prototype._flush =
function _flush()
{
    debug("_flush()");
    this.emit("flush");
    this.close(false);
}


/**
 * Closes the GPU canvas.  Normally called automatically after GPU canvas
 * has painted the last set of pixels on screen.
 * @param {Boolean} flush - if `false`, then don't call the `flush()` native binding call. Defaults to `true`.
 * @api public
 */
GpuStream.prototype.close =
function close(flush)
{
    debug("close(%o)", flush);
    if (this._closed) return debug("already closed...");
    if (this.canvas)
    {
/*
        if (flush)
        {
            debug('invoking flush() native binding')
            this.canvas.flush();
        }
        debug("invoking close() native binding");
        this.canvas.close();
        this.canvas = null;
*/
        delete this.canvas;
    }
    else debug("nothing to flush() or close()");
    this._closed = true;
    this.emit("close");
}


module.exports.GpuStream =
class debug_GpuStream extends Writable
{
    constructor(opts) { super(opts); }

    _open() {}
    _flush() { this.close(false); }
    _write(chunk, encoding, done)
    {
        console.log("got %s:", encoding, chunk);
        done();
    }
    close(flush) { this._closed = true; }
}


//temp work-arounds:
//TODO: fix up .cpp source file instead of doing this here
function fixup(imports)
{
    var retval = {};
    retval.isRPi = imports.isRPi_tofix(); //TODO: function => value
    retval.Screen = imports.Screen_tofix(); //TODO: function => object
    retval.GpuCanvas = imports.GpuCanvas;
    Object.defineProperty(retval.GpuCanvas.prototype, "pivot", //TODO: methods => setters, getters
    {        
        get() { return this.pivot_tofix(); },
        set(newval) { this.pivot_tofix(newval); },
    });
console.log("retval", retval);
    return retval;
}

//EOF
