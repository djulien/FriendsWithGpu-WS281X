#!/usr/bin/env node
//writable GpuCanvas stream

/*
TODO:
//newtest.js -> gpu-test-stream.js
mp3player.js(lame + speaker) + GpuStream.js(index.js) + latency balancer -> gpu-player.js
vix2loader.js + vix2models.js = vix2stream.js
vix2stream.js + GpuPlayer.js = show
*/

"use strict"; //find bugs easier
const os = require('os');
const debug = require('debug')('gpu-canvas');
const bindings = require('bindings');
const {isRPi, Screen, GpuCanvas} = fixup(bindings('gpu-canvas.node'));
const Writable = require('readable-stream/writable');
const inherits = require('util').inherits;

module.exports.isRPi = isRPi;
module.exports.Screen = Screen;
module.exports.Canvas = GpuCanvas;
module.exports.GpuStream = GpuStream;


////////////////////////////////////////////////////////////////////////////////
////
/// Writable GPU stream
//

/**
 * The `GpuStream` class accepts an array of ARGB pixel colors and writes them to the video display.
 *
 * @param {Object} opts options object
 * @api public
 */
function GpuStream(opts)
{
    if (!opts) opts = {};
    if (!(this instanceof GpuStream)) return new GpuStream(opts);
    if (typeof opts.lowWaterMark == "undefined") opts.lowWaterMark = 0;
    if (typeof opts.highWaterMark == "undefined") opts.highWaterMark = 0;
    Writable.call(this, opts);

    this._title = opts.title || "GpuStream";
    this._num_univ = opts.num_univ || 24;
    this._univ_len = opts.univ_len || Screen.height;
    this._pivot24 = opts.pivot24;
    this._chunksize = this._num_univ * this._univ_len;
    this._closed = false; //no write() calls allowed after close()

// bind event listeners
    this.on('pipe', this._pipe);
    this.on('unpipe', this._unpipe);
    this.on('finish', this._flush);
}
inherits(GpuStream, Writable);


/**
 * Calls the audio backend's `open()` function, and then emits an "open" event.
 *
 * @api private
 */

GpuStream.prototype._open = 
function _open()
{
    debug('open()');
    this._canvas = new GpuCanvas(this._title, this._num_univ, this._univ_len, this._pivot24);
    if (!this._canvas) throw new Error('GpuCanvas open() failed');
    this.emit('open');
    return this._canvas;
}


/**
 * Turn dev mode on/off.  Dev more shows simulated pixels.  Live mode applies extra data signal formatting.
 *
 * @param {Object} opts
 * @api private
 */

GpuStream.prototype._format = function (opts) {
  debug('format(object keys = %o)', Object.keys(opts));
  if (null != opts.channels) {
    debug('setting %o: %o', 'channels', opts.channels);
    this.channels = opts.channels;
  }
  if (null != opts.bitDepth) {
    debug('setting %o: %o', "bitDepth", opts.bitDepth);
    this.bitDepth = opts.bitDepth;
  }
  if (null != opts.sampleRate) {
    debug('setting %o: %o', "sampleRate", opts.sampleRate);
    this.sampleRate = opts.sampleRate;
  }
  if (null != opts.float) {
    debug('setting %o: %o', "float", opts.float);
    this.float = opts.float;
  }
  if (null != opts.signed) {
    debug('setting %o: %o', "signed", opts.signed);
    this.signed = opts.signed;
  }
  if (null != opts.samplesPerFrame) {
    debug('setting %o: %o', "samplesPerFrame", opts.samplesPerFrame);
    this.samplesPerFrame = opts.samplesPerFrame;
  }
  if (null == opts.endianness || endianness == opts.endianness) {
    // no "endianness" specified or explicit native endianness
    this.endianness = endianness;
  } else {
    // only native endianness is supported...
    this.emit('error', new Error('only native endianness ("' + endianness + '") is supported, got "' + opts.endianness + '"'));
  }
};

/**
 * `_write()` callback for the Writable base class.
 *
 * @param {Buffer} chunk
 * @param {String} encoding
 * @param {Function} done
 * @api private
 */

Speaker.prototype._write = function (chunk, encoding, done) {
  debug('_write() (%o bytes)', chunk.length);

  if (this._closed) {
    // close() has already been called. this should not be called
    return done(new Error('write() call after close() call'));
  }
  var b;
  var self = this;
  var left = chunk;
  var handle = this.audio_handle;
  if (!handle) {
    // this is the first time write() is being called; need to _open()
    try {
      handle = this._open();
    } catch (e) {
      return done(e);
    }
  }
  var chunkSize = this.blockAlign * this.samplesPerFrame;

  function write () {
    if (self._closed) {
      debug('aborting remainder of write() call (%o bytes), since speaker is `_closed`', left.length);
      return done();
    }
    b = left;
    if (b.length > chunkSize) {
      var t = b;
      b = t.slice(0, chunkSize);
      left = t.slice(chunkSize);
    } else {
      left = null;
    }
    debug('writing %o byte chunk', b.length);
    binding.write(handle, b, b.length, onwrite);
  }

  var THIS = this; //preserve "this" for onwrite call-back -DJ
  function onwrite (r) {
    debug('wrote %o bytes', r);
    if (isNaN(++THIS.numwr)) THIS.numwr = 1; //track #writes; is this == #frames? -DJ
    if (isNaN(THIS.wrtotal += r)) THIS.wrtotal = r; //track total data written -DJ
    THIS.emit('progress', {numwr: THIS.numwr, wrlen: r, wrtotal: THIS.wrtotal, buflen: (left || []).length}); //give caller some progress info -DJ
    if (r != b.length) {
      done(new Error('write() failed: ' + r));
    } else if (left) {
      debug('still %o bytes left in this chunk', left.length);
      write();
    } else {
      debug('done with this chunk');
      done();
    }
  }

  write();
};

/**
 * Called when this stream is pipe()d to from another readable stream.
 * If the "sampleRate", "channels", "bitDepth", and "signed" properties are
 * set, then they will be used over the currently set values.
 *
 * @api private
 */

Speaker.prototype._pipe = function (source) {
  debug('_pipe()');
  this._format(source);
  source.once('format', this._format);
};

/**
 * Called when this stream is pipe()d to from another readable stream.
 * If the "sampleRate", "channels", "bitDepth", and "signed" properties are
 * set, then they will be used over the currently set values.
 *
 * @api private
 */

Speaker.prototype._unpipe = function (source) {
  debug('_unpipe()');
  source.removeListener('format', this._format);
};

/**
 * Emits a "flush" event and then calls the `.close()` function on
 * this Speaker instance.
 *
 * @api private
 */

Speaker.prototype._flush = function () {
  debug('_flush()');
  this.emit('flush');
  this.close(false);
};

/**
 * Closes the audio backend. Normally this function will be called automatically
 * after the audio backend has finished playing the audio buffer through the
 * speakers.
 *
 * @param {Boolean} flush - if `false`, then don't call the `flush()` native binding call. Defaults to `true`.
 * @api public
 */

Speaker.prototype.close = function (flush) {
  debug('close(%o)', flush);
  if (this._closed) return debug('already closed...');

  if (this.audio_handle) {
    if (false !== flush) {
      // TODO: async most likely…
      debug('invoking flush() native binding');
      binding.flush(this.audio_handle);
    }

    // TODO: async maybe?
    debug('invoking close() native binding');
    binding.close(this.audio_handle);
    this.audio_handle = null;
  } else {
    debug('not invoking flush() or close() bindings since no `audio_handle`');
  }

  this._closed = true;
  this.emit('close');
};
