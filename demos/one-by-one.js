#!/usr/bin/env node
//One-by-one demo pattern: tests GPU config, GPIO pins, and WS281X connections
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture working on RPi
//ver 1.0a DJ  3/25/17  add getchar to allow single-step + color changes

'use strict'; //find bugs easier
require('colors'); //for console output
const {debug} = require('../js-shared/debug');
const {Screen} = require('../js-shared/screen');
const {GpuCanvas} = require('../js-shared/GpuCanvas');
const {blocking, wait, getchar} = require('../js-shared/blocking');

//display settings:
const VGROUP = 4; //!Screen.gpio? Screen.height / 24: 1; //node grouping; used to increase effective pixel size or reduce resolution for demo/debug
const UNIV_LEN = Screen.height / VGROUP; //can't exceed #display lines; get rid of useless pixels when VGROUP != 1
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
debug("screen %d x %d, video cfg %d x %d (%d x %d), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//show extra debug info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const OPTS =
{
//    SHOW_INTRO: 10, //how long to show intro screen (on screen only)
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
//    WS281X_FMT: true, //force WS281X formatting on screen
    WS281X_DEBUG: true, //show timing debug info
};

//ARGB primary colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const CYAN = 0xff00ffff;
const MAGENTA = 0xffff00ff;
const WHITE = 0xffffffff;
//const WHITE = 0xFFE7DBA5; //turn a few bits off to make debug easier
const BLACK = 0xff000000; //NOTE: alpha must be on to take effect

//const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE];
const PALETTE = {r: RED, g: GREEN, b: BLUE, y: YELLOW, c: CYAN, m: MAGENTA, w: WHITE};


//main logic:
//written with synchronous coding style to simplify logic flow
blocking(function*()
{
    var canvas = new GpuCanvas("One-by-one", NUM_UNIV, UNIV_LEN, OPTS);

    if (OPTS.SHOW_INTRO && !Screen.gpio) //show title screen for 10 sec
    {
        canvas.load("images/one-by-one24x24-5x5.png");
        canvas.duration = OPTS.SHOW_INTRO; //set progress bar limit
        canvas.push.WS281X_FMT(false); //turn off formatting while showing bitmap (debug)
        for (canvas.elapsed = 0; canvas.elapsed < canvas.duration; ++canvas.elapsed)
            yield wait(1);
        canvas.pop.WS281X_FMT();
    }

    debug("begin, turn on %d x %d = %d pixels 1 by 1".green_lt, canvas.width, canvas.height, canvas.width * canvas.height);
    console.log("Enter %s to quit, %s toggles formatting, 1 of %s for color, other for next pixel", "q".bold.cyan_lt, "f".bold.cyan_lt, "rgbycmw".bold.cyan_lt);
    canvas.elapsed = 0; //reset progress bar
    canvas.duration = canvas.width * canvas.height; //set progress bar limit
    canvas.fill(BLACK); //start with all pixels off
    var color = 'r';
canvas.pixel(0, 0, WHITE);
canvas.pixel(0, 10, RED);
canvas.pixel(0, 20, GREEN);
canvas.pixel(0, 30, BLUE);
canvas.pixel(0, 40, WHITE);
canvas.pixel(0, 50, RED);
canvas.pixel(0, 679, WHITE);
canvas.pixel(0, 695, RED);
canvas.pixel(0, 696, GREEN);
canvas.pixel(0, 697, BLUE);
    for (var x = 0; x < canvas.width; ++x)
        for (var y = 0; y < canvas.height; ++y, ++canvas.elapsed)
        {
            for (;;)
            {
                var ch = yield getchar("Next pixel (%d, %d) %s %s?".pink_lt, x, y, color, canvas.WS281X_FMT? "fmt": "no-fmt");
                if (ch == "q") { debug("quit".green_lt); return; }
                if (ch == "f") { canvas.WS281X_FMT = !canvas.WS281X_FMT; continue; } //toggle formatting
                if (ch in PALETTE) { color = ch; continue; } //change color
                break; //advance to next pixel
            }
            canvas.pixel(x, y, PALETTE[color]);
        }
    debug("end, pause for 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear
});


//truncate after 3 dec places:
function milli(val)
{
    return Math.floor(val * 1e3) / 1e3;
}

//eof
