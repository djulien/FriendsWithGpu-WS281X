#!/usr/bin/env node
//Pin finder RGB patterns: tests GPU config, GPIO pins, and WS281X connections
//Sends out a different pattern on each GPIO pin to make them easier to identify.
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes.
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture re-write working on RPi
//ver 1.0a DJ  9/24/17  minor clean up
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
//const path = require('path');
const {debug} = require('./shared/debug');
const {blocking, wait} = require('blocking-style');
const {Screen, GpuCanvas} = require('gpu-friends-ws281x');

//display settings:
//const SPEED = 0.5; //animation step speed (sec)
const FPS = 2+3; //animation step speed
const DURATION = 60; //how long to run (sec)
const NUM_UNIV = 24; //can't exceed #VGA output pins (24) unless external mux used
const UNIV_LEN = Screen.gpio? Screen.height: 30; //can't exceed #display lines; show larger pixels in dev mode
debug("Screen %d x %d, is RPi? %d, GPIO? %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.gpio);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//show extra debug info:
//NOTE: these only apply when GPIO pins are *not* presenting VGA signals (otherwise interferes with WS281X timing)
const OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
//    SHOW_PROGRESS: true, //show progress bar at bottom of screen
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
    var canvas = new GpuCanvas("Pin Finder", NUM_UNIV, UNIV_LEN, OPTS);

    debug("begin, run for %d sec @%d fps".green_lt, DURATION, FPS);
    var started = now_sec();
    canvas.duration = DURATION * FPS; //set progress bar limit
    for (var t = 0; t <= DURATION * FPS; ++t)
    {
//NOTE: pixel (0, 0) is upper left on screen
        if (!OPTS.gpufx) //generate fx on CPU instead of GPU
            for (var x = 0; x < canvas.width; ++x)
                for (var y = 0; y < canvas.height; ++y)
                {
                    var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
                    var repeat = 9 - (x % 8);
                    canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
                }
        canvas.paint();
        yield wait(started + (t + 1) / FPS - now_sec()); //avoid cumulative timing errors
        ++canvas.elapsed; //update progress bar
    }
    debug("end, pause 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too fast
    canvas.StatsAdjust = -10; //exclude pause in final stats
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
