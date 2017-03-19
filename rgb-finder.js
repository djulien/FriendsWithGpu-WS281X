#!/usr/bin/env node
//RGB finder pattern: tests GPU config, GPIO pins, and WS281X connections
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 1.0  DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article

'use strict'; //find bugs easier
require('colors'); //for console output
const {debug} = require('./js-shared/debug');
const {Screen} = require('./js-shared/screen');
const {Canvas} = require('./js-shared/canvas');
const {blocking, wait} = require('./js-shared/blocking');

//display settings:
const SPEED = 0.5; //animation speed
const VGROUP = !Screen.gpio? Screen.height / 24: 1; //node grouping; used to increase effective pixel size or reduce resolution for demo/debug
const UNIV_LEN = Math.ceil(Screen.height / VGROUP); //vert.disp; //can't exceed #display lines; get rid of useless pixels when VGROUP is set
const NUM_UNIV = 24; //can't exceed #VGA output pins unless external mux used
debug("screen %d x %d, video cfg %d x %d, vgroup %d, gpio? %s, speed %d".blue_lt, Screen.width, Screen.height, Screen.horiz.disp, Screen.vert.disp, VGROUP, Screen.gpio, SPEED);

//RGBA primary colors:
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const CYAN = 0xff00ffff;
const MAGENTA = 0xffff00ff;
const WHITE = 0xffffffff;
const BLACK = 0xff000000; //NOTE: needs alpha to take effect

const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA];


//main logic:
var canvas;
blocking(function*()
{
    /*var*/ canvas = new Canvas("RGB Finder", NUM_UNIV, UNIV_LEN); //, VGROUP);
    const pixel_delay = 0.1; // 0.1 sec * 24 * 24 ~= 1 minute

    debug("begin".green_lt);
    canvas.elapsed = -1;
//if (false)
    for (var i = 0; i < PALETTE.length; ++i)
    {
        canvas.fill(PALETTE[i]);
        yield wait(2);
    }

//if (false)
    for (var i = 0; i < 4; ++i)
    {
        /*yield*/ canvas.load("demo/" + ["one.png", "two.png", "three.png", "four.png"][i]);
//        draw(2);
        canvas.elapsed = i - 4;
        yield wait(10);
    }

    canvas.fill(BLACK);
    var started = now_sec();
    debug(2, "turn on %d x %d = %d pixels 1 by 1 ...".blue_lt, canvas.width, canvas.height, canvas.width * canvas.height);
//    canvas.interval = .01; // 1 / pixel_delay; //just show %
    canvas.duration = canvas.width * canvas.height; // / VGROUP; // * pixel_delay;
    for (var x = 0, t = 0; x < canvas.width; ++x)
        for (var y = 0; y < canvas.height; ++y, ++t) // / VGROUP; ++y, ++t)
        {
//if (t > 40) break;
            canvas.elapsed = t; // * pixel_delay;
            canvas.pixel(x, y, PALETTE[t % PALETTE.length]); //TODO: line y+VGROUP
//console.log("rAF @%s, progress %d%%", t * 1/4, t / (NUM_UNIV * UNIV_LEN));
//            var progress = t / (canvas.width * canvas.height); //[0..1)
//TODO: figure out why +0.002 is needed
//            draw(2 + progress - 2); // + 0.002); //, now_sec() - started); //, "t " + t + ", x " + x + "/" + txtr.width + ", y " + y + "/" + txtr.height));
            yield wait(t * pixel_delay + started - now_sec()); //adjust to avoid cumulative timing error
        }
    yield wait(5);
    /*yield*/ canvas.load("demo/" + "./texture.png");
//    draw(2);
    canvas.elapsed = canvas.duration;
    yield wait(5); //started + 45 - now_sec()); //wait 5 sec more
    debug("end".green_lt);
    canvas.destroy();
});


/*
function draw(progress) //, flush) //, msec) //not_first)
{
    canvas.progress = progress; //drives animation
    var timeslot = Math.floor(100 * progress) / 100; //elapsed / duration) / 100;
    if (timeslot == draw.prior) return;
    debug(2, "progress %d".blue_lt, timeslot);
    draw.prior = timeslot;
}
*/


//current time in seconds:
function now_sec()
{
    return Date.now() / 1000;
}


//eof
