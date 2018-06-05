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
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas
//ver 1.0c DJ  6/4/18  minor external cleanup

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const {debug} = require('./shared/debug');
const {blocking, wait, getchar} = require('blocking-style');
const {Screen, GpuCanvas, UnivTypes} = require('gpu-friends-ws281x');
//const {Screen} = require('./shared/screen');
//const {GpuCanvas/*, rAF*/} = require('./shared/GpuCanvas');

//display settings:
const FPS = 60; //how fast to run in auto mode (for performance testing)
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
const UNIV_LEN = 60; //24/6; //Screen.height; //Screen.gpio? Screen.height: Math.round(Screen.height / Math.round(Screen.scanw / 24)); ///can't exceed #display lines; for dev try to use ~ square pixels (on-screen only, for debug)
debug("Screen %d x %d, is RPi? %d, vgaIO? %d".cyan_lt, Screen.width, Screen.height, Screen.isRPi, Screen.vgaIO);
//debug("window %d x %d, video cfg %d x %d vis (%d x %d total), vgroup %d, gpio? %s".cyan_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, Screen.horiz.res, Screen.vert.res, milli(VGROUP), Screen.gpio);

//show extra debug info:
//NOTE: these only apply when dpi24 overlay is *not* loaded (otherwise interferes with WS281X timing)
const OPTS =
{
    SHOW_INTRO: 10, //how long to show intro (on screen only, in seconds)
//    SHOW_SHSRC: true, //show shader source code
//    SHOW_VERTEX: true, //show vertex info (corners)
//    SHOW_LIMITS: true, //show various GLES/GLSL limits
    SHOW_PROGRESS: true, //show progress bar at bottom of screen
//    UNIV_TYPE: UnivTypes.CHPLEX_SSR, //set default univ type
//    WS281X_FMT: true, //force WS281X formatting on screen
//    WS281X_DEBUG: true, //show timing debug info; NOTE: debug display will slow RPi down from 60 FPS to 20 FPS
    NUM_WKERS: 1, //whole-house bg render
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
//console.log("canvas", Object.keys(canvas)); //, Object.keys(canvas.prototype));
//    canvas.UnivType(5, UnivTypes.WS281X);
//    canvas.UnivType(7, UnivTypes.PLAIN_SSR);
    canvas.UnivType[1] = UnivTypes.CHPLEX_SSR; //| UnivTypes.CHECKSUM | UnivTypes.ACTIVE_HIGH);
    canvas.DumpFile = "dump.log";
//    canvas.UnivType(1, UnivTypes.CHPLEX_SSR); //| UnivTypes.CHECKSUM | UnivTypes.ACTIVE_HIGH);
//console.log("hello");
//var buf = [];
//    for (var i = 0; i < canvas.width; ++i) buf.push(canvas.UnivType(i));
//console.log("univ types: ", buf.join(","));

    if (OPTS.SHOW_INTRO && !Screen.vgaIO) //show title image for 10 sec
    {
//        canvas.fill(MAGENTA);
        canvas.load(__dirname + "/images/one-by-one24x24-5x5.png");
        canvas.duration = OPTS.SHOW_INTRO; //set progress bar limit
        canvas.WS281X_FMT.push(false); //turn off formatting while showing bitmap (debug)
        for (canvas.elapsed = 0; canvas.elapsed <= canvas.duration; ++canvas.elapsed) //update progress bar while waiting
        {
//if (!(canvas.elapsed % 4)) canvas.SHOW_PROGRESS = !canvas.SHOW_PROGRESS;
            canvas.paint(); //this only updates progress bar
            if (canvas.elapsed < canvas.duration) yield wait(1); //don't need to wait final time, just paint
        }
//        canvas.fill(BLACK);
        canvas.WS281X_FMT.pop(); //restore previous value
    }

    console.error(`begin, turn on ${canvas.width} x ${canvas.height} = ${canvas.width * canvas.height} pixels 1 by 1`.green_lt);
    console.error(`Enter ${"q".bold.cyan_lt} to quit, ${"a".bold.cyan_lt} for auto, ${"f".bold.cyan_lt} toggles formatting, 1 of ${"rgbycmw".bold.cyan_lt} for color, other for next pixel`);
    canvas.elapsed = 0; //reset progress bar
    canvas.duration = canvas.width * canvas.height; //set progress bar limit
    canvas.fill(BLACK); //start with all pixels off
//var pxbuf = canvas.txr.pixel_bytes;

    var started; //timing for auto mode
    var color = 'r'; //start with red initially selected
//canvas.pixel(0, 0, WHITE);
//canvas.pixel(0, 1, CYAN);
//canvas.pixel(0, 10, RED);
//canvas.pixel(0, 20, GREEN);
//canvas.pixel(0, 30, BLUE);
//canvas.pixel(0, 40, WHITE);
//canvas.pixel(0, 50, RED);
//canvas.pixel(0, 679, WHITE);
//canvas.pixel(0, 695, RED);
//canvas.pixel(0, 696, GREEN);
//canvas.pixel(0, 697, BLUE);
    for (var x = 0; x < canvas.width; ++x)
        for (var y = 0; y < canvas.height; ++y, ++canvas.elapsed)
        {
            if (started) yield wait(started + canvas.elapsed / FPS - now_sec()); ///use cumulative time to avoid drift
            else for (;;)
            {
                var cmd = yield getchar(`Next pixel (${x}, ${y}), ${color} ${canvas.WS281X_FMT? "": "no-"}fmt?`.pink_lt);
                if (cmd == "q") { console.error("quit".green_lt); return; }
                if (cmd == "f") { canvas.WS281X_FMT.toggle(); continue; } //toggle formatting
                if (cmd == "a") { canvas.render_stats(true); started = now_sec() - canvas.elapsed / FPS; break; } ///back-dated to compensate for pixels that are already set
                if (cmd in PALETTE) { color = cmd; continue; } //change color
                break; //advance to next pixel
            }
            canvas.pixel[x, y] = PALETTE[color];
            canvas.paint();
        }
    if (started) debug("auto fps: target %d, actual %d, #render/sec %d".cyan_lt, FPS, Math.round(10 * canvas.elapsed / (now_sec() - started)) / 10, Math.round(10 * canvas.render_stats()) / 10);
    canvas.stats(); //show perf stats before delay to avoid skewing stats
    console.error("end, pause for 10 sec".green_lt);
    yield wait(10); //pause at end so screen doesn't disappear too soon
    canvas.StatsAdjust = -10; //exclude pause in final stats
});


//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}

//eof