#!/usr/bin/env node
//Frame feeder
//
//History:
//ver 1.0  DJ  11/11/17  first version

'use strict'; //find bugs easier
require('colors'); //for console output
//const path = require('path');
const {blocking, wait} = require('blocking-style');

const {debug} = require('./shared/debug');
//const {Screen} = require('./shared/screen');
//const {GpuCanvas} = require('./shared/GpuCanvas');

//display settings:
const SPEED = 1; //animation speed (sec)
const DURATION = 5; //how long to run (sec)
//const UNIV_LEN = Screen.gpio? Screen.vert.disp: 30; //can't exceed #display lines; get rid of useless pixels when VGROUP != 1
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), using gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, Screen.gpio);
const DELAY = 5;

//show extra debug info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
//    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info
//    gpufx: "./pin-finder.glsl", //generate fx on GPU instead of CPU
};

//ARGB primary colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
//const YELLOW = 0xffffff00;
//const CYAN = 0xff00ffff;
//const MAGENTA = 0xffff00ff;
//const WHITE = 0xffffffff;
const BLACK = 0xff000000; //NOTE: alpha must be on to take effect

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//main logic:
//written with synchronous coding style to simplify logic structure
blocking(function*()
{
//    var canvas = new GpuCanvas("Pin Finder", NUM_UNIV, UNIV_LEN, OPTS);

    yield wait(DELAY);
//    debug("begin, run for %d sec".green_lt, DURATION);
    var started = now_sec();
//    canvas.elapsed = 0; //reset progress bar
//    canvas.duration = DURATION / SPEED; //set progress bar limit
    for (var t = 0; t <= DURATION / SPEED; ++t)
    {
//NOTE: pixel (0, 0) is upper left on screen
        if (true) console.log(t);
        else
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
            for (var x = 0; x < canvas.width; ++x)
                for (var y = 0; y < canvas.height; ++y)
                {
                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
                    var repeat = 9 - (x % 8);
                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
                }
        yield wait((t + 1) * SPEED - now_sec() + started); //canvas.elapsed); //avoid cumulative timing errors
//        ++canvas.elapsed; //= now_sec() - started; //update progress bar
    }
//    debug("end, pause 10 sec".green_lt);
    yield wait(DELAY); //pause at end so screen doesn't disappear too fast
});


//truncate after 3 dec places:
function milli(val)
{
    return Math.floor(val * 1e3) / 1e3;
}


//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}

//eof