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
module.exports.Canvas = GpuCanvas;


//const os = require('os')
//const endianness = os.endianness()

//default settings (caller can override):
const DECODER_DEFAULTS =
{
//stream options:
//    lowWaterMark: 0,
//    highWaterMark: 0,
//TODO?    objectMode: true,
//gpu canvas options:
    FPS: 60,
    title: "GpuStream",
    PIVOT: true,
//    LATENCY: 1.4, //estimated startup latency (sec)
//    DEBUG_INFO: true,
};


////////////////////////////////////////////////////////////////////////////////
////
/// Decoder stream
//


const GpuDecoder =
module.exports.GpuDecoder =
function GpuDecoder(opts)
{
    if (!(this instanceof GpuDecoder)) return new GpuDecoder(opts);
    Object.assign(this, DECODER_DEFAULTS, opts || {});
    Transform.call(this, opts); //initialize base class
    debug('created new GpuDecoder instance');
}
inherits(GpuDecoder, Transform);


module.exports.LineStream = LineStream; //might be useful so let caller have it


//always need ndjson parser; wrap it so caller doesn't need to:
module.exports.GpuStream =
function wrapper(opts)
{
    return (new LineStream({keepEmptyLines: false}))
        .pipe(ndjson())
        .pipe(new debug_GpuStream(opts));
}


//decoder:
const ndjson =
module.exports.ndjson =
function parse_stream()
{
//    return thru2({ objectMode: true}, xform, flush); //allowHalfOpen: false },
    return thru2(xform, flush); //{ objectMode: true, allowHalfOpen: false },
};


function xform(chunk, enc, cb)
{
    if (typeof chunk != "string") chunk = chunk.toString(); //TODO: enc?
//        chunk = chunk.replace(/[{,]\s*([a-z\d]+)\s*:/g, (match, val) => { return match[0] + '"' + val + '":'; }); //JSON fixup: numeric keys need to be quoted :(
    var data = JSON5.parse(chunk); //JSON.fixup(chunk));
//    delete data.eof; //remove eof marker
//TODO: allow embedded commands; strip off stats and pass to GpuCanvas object
    if (data.buf)
    {
//        if (!dedupe(data.buf)) { cb(); return; } //no update needed
//        data.buf = compress(data.buf); //differing part of frame buf
//            yield rs.pushJSON(`{sttime:${timestamp(fr * seq.FixedFrameInterval / 1000)}, fr: ${fr}, buf: ${buf}}`); //delta${rawbuf}}`);
//        data = `{sttime: ${data.sttime}, fr: ${data.fr}, delta${data.buf}}`; //preserve formatting
//        pushJSON.call(this, data.buf);
        push.call(this, data.buf);
    }
    cb(); //null, chunk);
}


function flush(cb)
{
//        this.push('tacking on an extra buffer to the end');
//    var total = (compress.by_block || 0) + (compress.by_color || 0); //+ (compress.by_ofs || 0);
//    pushJSON.call(this,
//    {
//        sttime: this.lasttime,
//        eof: true,
//stats (mainly for debug):
//        dups: commas(dedupe.dups || 0),
//    });
//    pushJSON.call(this, null);
    push.call(this, null);
    cb();
}


//fix up JSON string before parsing:
//add quotes back onto keys
JSON.fixup = function(str)
{
//    chunk = chunk.replace(/[{,]\s*(\w+)\s*:/gi, (match, name) => { return match[0] + '"' + name + '":'; }).replace(/\/\*); //JSON fixups: put names in quotes, remove comments
    return str.replace(/[{,]\s*([a-z\d]+)\s*:/g, (match, val) => { return match[0] + '"' + val + '":'; }); //numeric keys need to be quoted :(
};


/*
function pushJSON(data)
{
//    chkthis.call(this);
    if (!(typeof data == "string")) data = JSON.stringify_tidy(data);
//console.log("rs push json", data);
    return this.push(data + "\n");
}
*/


//EOF
