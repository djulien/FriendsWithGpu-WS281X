#!/usr/bin/env node
//RGB finder pattern: tests GPU config, GPIO pins, and WS281X connections
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture working on RPi

'use strict'; //find bugs easier
require('colors'); //for console output
const {debug} = require('./js-shared/debug');
const {Screen} = require('./js-shared/screen');
const {Canvas} = require('./js-shared/canvas');
const {blocking, wait} = require('./js-shared/blocking');

//display settings:
const SPEED = 0.5; //animation speed (sec)
const DURATION = 60; //how long to run (sec)
const VGROUP = !Screen.gpio? Screen.height / 24: 1; //node grouping; used to increase effective pixel size or reduce resolution for demo/debug
const UNIV_LEN = Math.ceil(Screen.height / VGROUP); //can't exceed #display lines; get rid of useless pixels when VGROUP is set
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
debug("screen %d x %d, video cfg %d x %d, vgroup %d, gpio? %s, speed %d".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, milli(VGROUP), Screen.gpio, SPEED);

//show extra debug info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const DEBUG_OPTS =
{
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
//    WS281X_FMT: true, //force WS281X formatting on screen
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
//const GPUFX = 0; //tranparent value allowing GPU fx to show thru

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//main logic:
//written with synchronous coding style to simplify timing logic
blocking(function*()
{
    var canvas = new Canvas("RGB Finder", NUM_UNIV, UNIV_LEN, DEBUG_OPTS);

    debug("begin, run for %d sec".green_lt, DURATION);
    var started = now_sec();
    canvas.duration = DURATION; //progress bar limit
//    for (var started = now_sec(), a = 0; now_sec() < started + canvas.duration; ++a)
    for (var t = 0; t <= DURATION / SPEED; ++t)
    {
        canvas.elapsed = now_sec() - started; //update progress bar
//        canvas.fill(BLACK);
        for (var x = 0; x < canvas.width; ++x)
            for (var y = 0; y < canvas.height; ++y)
            {
                var color = [RED, GREEN, BLUE][Math.floor(x / 8)];
                var repeat = 9 - (x % 8);
                canvas.pixel(x, y, ((y - t) % repeat)? BLACK: color);
//    canvas.pixel(x, y, color);
//console.log("[%d,%d] => 0x%s", x, y, (color >>> 0).toString(16));
//                if ((x < 3) && (y < 3)) canvas.pixel(x, y, GPUFX + 0x33);
            }
//canvas.fill(GPUFX);
        yield wait((t + 1) * SPEED - canvas.elapsed); //avoid cumulative timing errors
    }
    debug("end, pause 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too fast
    canvas.destroy();
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
