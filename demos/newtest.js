#!/usr/bin/env node

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const bindings = require('bindings');
var /*const*/ {isRPi, Screen, DataCanvas} = bindings('data-canvas.node'); //require("../build/release/gpu-canvas");
Screen = Screen(); //TODO
//console.log(typeof Screen);
//console.dir(Screen);
//console.log(JSON.stringify(Screen()));
console.log("screen %dx%d, is RPi? %s".blue_lt, Screen.width, Screen.height, isRPi());

//class DataCanvas
//{
//    constructor(title, w, h) { console.log("data-canvas()"); }
//    release() { console.log("released"); }
//    paint(pixels) { console.log("paint"); }
//};

const NUM_UNIV = 24; //10; //max 24;
const UNIV_LEN = 20; //Screen.height; //must be == screen height for live show (else vgrouping)
//const pixels = new Buffer(Uint32Array(NUM_UNIV * UNIV_LEN);
//const pixels = new ArrayBuffer(NUM_UNIV * UNIV_LEN * Uint32Array.BYTES_PER_ELEMENT);
//this.pixel_bytes = new Uint8Array(this.pixel_buf);
const pixels = new Uint32Array(NUM_UNIV * UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
//this.pixel_view32 = new DataView(this.pixel_buf); //for faster access (quad bytes); //caller-selectable byte order
const canvas = new DataCanvas("Friends2017", NUM_UNIV, UNIV_LEN, false);

const BLACK = 0xff000000; //NOTE: need A set for color to take effect
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const CYAN = 0xff00ffff;
const MAGENTA = 0xffff00ff;
const WHITE = 0xffffffff;

const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE];

for (var xy = 0;; ++xy)
{
    var x = Math.floor(xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
//    if (eventh(1)) break; //user quit
    var color = PALETTE[(x + y + Math.floor(xy / pixels.length)) % PALETTE.length]; //vary each cycle
    pixels[xy % pixels.length] = color;
    canvas.paint(pixels); //60 FPS
//        SDL_Delay(1000);
}
console.log("done, wait 5 sec".green_lt);
//SDL_Delay(5000);


//pixels.fill(0xffff00ff);
//canvas.paint(pixels);
//setTimeout(function() { pixels.fill(0xff00ffff); canvas.paint(pixels); }, 2000);
//setTimeout(function() { canvas.release(); }, 5000);


//EOF
