#!/usr/bin/env node

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
Date.now_usec = require("performance-now"); //override with hi-res version (usec)
const bindings = require('bindings');
const {isRPi, Screen, DataCanvas} = fixup(bindings('data-canvas.node')); //require("../build/release/gpu-canvas");
//console.log(typeof Screen);
//console.dir(Screen);
//console.log(JSON.stringify(Screen()));
console.log("screen %dx%d, is RPi? %s".cyan_lt, Screen.width, Screen.height, isRPi());

const NUM_UNIV = 24; //10; //max 24;
const UNIV_LEN = Screen.height; //20 //must be == screen height for live show (else vgrouping)
//const pixels = new Buffer(Uint32Array(NUM_UNIV * UNIV_LEN);
//const pixels = new ArrayBuffer(NUM_UNIV * UNIV_LEN * Uint32Array.BYTES_PER_ELEMENT);
//this.pixel_bytes = new Uint8Array(this.pixel_buf);
const pixels = new Uint32Array(NUM_UNIV * UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
//this.pixel_view32 = new DataView(this.pixel_buf); //for faster access (quad bytes); //caller-selectable byte order
const canvas = new DataCanvas("Friends2017", NUM_UNIV, UNIV_LEN, true);
//TODO canvas.Type = 1; //Chipiplexed SSRs

console.log("pivot = %d", canvas.pivot);
//canvas.pivot = false;
//console.log("pivot = %d", canvas.pivot);
//canvas.pivot = true;
//console.log("pivot = %d", canvas.pivot);

const BLACK = 0xff000000; //NOTE: need A set for color to take effect
const RED = 0xffff0000;
const GREEN = 0xff00ff00;
const BLUE = 0xff0000ff;
const YELLOW = 0xffffff00;
const CYAN = 0xff00ffff;
const MAGENTA = 0xffff00ff;
const WHITE = 0xffffffff;

const PALETTE = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE];

//burn some CPU cycles:
function fibo(n) { return (n < 3)? n: fibo(n - 1) + fibo(n - 2); }

var timer; //= setInterval(function() { /*console.log("toggle")*/; canvas.pivot = !canvas.pivot; }, 100);

const duration = 10; //sec
var retval = 
step(function*()
{
    console.log("loop for %d sec (%d frames) ...".green_lt, duration, duration * 60);
    for (var xy = 0; xy < duration * 60; ++xy) //11sec @60 FPS
    {
        var x = Math.floor(xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
//    if (eventh(1)) break; //user quit
        var color = PALETTE[(x + y + Math.floor(xy / pixels.length)) % PALETTE.length]; //vary each cycle
        fibo(25); //simulate some CPU work
        pixels[xy % pixels.length] = color;
        canvas.paint(pixels); //, cb, {fr: xy, started: Date.now_usec()}); //60 FPS
        yield xy; //process events
//        SDL_Delay(1000);
    }
//console.log("done=%d, wait 5 sec".green_lt, retval);
//SDL_Delay(5000);
    return 12;
});
console.log("done, retval=%d, wait %d sec".green_lt, retval, duration + 1);
setTimeout(function() { if (timer) clearInterval(timer); }, (duration + 1) * 1000);
//clearInterval(timer);
//SDL_Delay(5000);


//function cb(data)
//{
//    console.log("paint[%d]: now - started = %d usec", data.fr, Data.now_usec() - data.started);
//}


//step thru generator function:
function step(gen)
{
    if (typeof gen == "function") gen = gen();
    var {value, done} = gen.next();
//console.log("step got: {val %d, done %d}", value, done);
    if (!done) setImmediate(step, gen); //do another step *after* processing events (process.nextTick would step *before* other events)
    return value;
}

//pixels.fill(0xffff00ff);
//canvas.paint(pixels);
//setTimeout(function() { pixels.fill(0xff00ffff); canvas.paint(pixels); }, 2000);
//setTimeout(function() { canvas.release(); }, 5000);

//temp work-arounds:
function fixup(imports)
{
    var retval = {};
    retval.isRPi = imports.isRPi; //(); //TODO: function => value?
    retval.Screen = imports.Screen_tofix(); //TODO: function => object
    retval.DataCanvas = imports.DataCanvas;
    Object.defineProperty(retval.DataCanvas.prototype, "pivot", //TODO: methods => setters, getters
    {        
        get() { return this.pivot_tofix(); },
        set(newval) { this.pivot_tofix(newval); },
    });
    return retval;
}

//EOF
