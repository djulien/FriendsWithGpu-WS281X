#!/usr/bin/env node

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const bindings = require('bindings');
const {isRPi, Screen, DataCanvas} = fixup(bindings('data-canvas.node')); //require("../build/release/gpu-canvas");
//console.log(typeof Screen);
//console.dir(Screen);
//console.log(JSON.stringify(Screen()));
console.log("screen %dx%d, is RPi? %s".cyan_lt, Screen.width, Screen.height, isRPi());

const NUM_UNIV = 24; //10; //max 24;
const UNIV_LEN = 20; //Screen.height; //must be == screen height for live show (else vgrouping)
//const pixels = new Buffer(Uint32Array(NUM_UNIV * UNIV_LEN);
//const pixels = new ArrayBuffer(NUM_UNIV * UNIV_LEN * Uint32Array.BYTES_PER_ELEMENT);
//this.pixel_bytes = new Uint8Array(this.pixel_buf);
const pixels = new Uint32Array(NUM_UNIV * UNIV_LEN); //this.pixel_buf); //NOTE: platform byte order
//this.pixel_view32 = new DataView(this.pixel_buf); //for faster access (quad bytes); //caller-selectable byte order
const canvas = new DataCanvas("Friends2017", NUM_UNIV, UNIV_LEN, true);
//TODO canvas.Type = 1; //Chipiplexed SSRs

console.log("pivot = %d", canvas.pivot);
canvas.pivot = false;
console.log("pivot = %d", canvas.pivot);
canvas.pivot = true;
console.log("pivot = %d", canvas.pivot);

var timer = setInterval(function() { console.log("toggle"); canvas.pivot = !canvas.pivot; }, 1000);

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


var retval = 
0; //step(function*()
//{
for (var xy = 0; xy < 11*60; ++xy) //11sec @60 FPS
{
    var x = Math.floor(xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
//    if (eventh(1)) break; //user quit
    var color = PALETTE[(x + y + Math.floor(xy / pixels.length)) % PALETTE.length]; //vary each cycle
    pixels[xy % pixels.length] = color;
    canvas.paint(pixels); //60 FPS
    fibo(25);
//    yield; //process events
//        SDL_Delay(1000);
}
//console.log("done=%d, wait 5 sec".green_lt, retval);
//SDL_Delay(5000);
//return 12;
//});
console.log("done=%d, wait 12 sec".green_lt, retval);
setTimeout(function() {}, 12000);
clearInterval(timer);
//SDL_Delay(5000);


//step thru generator function:
function step(gen)
{
    if (typeof gen == "function") gen = gen();
    tick();

    function tick()
    {
        var {value, done} = gen.next();
        if (done) return value;
        else setImmediate(tick); //process.nextTick(tick);
    }
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
