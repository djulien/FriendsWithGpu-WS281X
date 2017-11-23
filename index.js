#!/usr/bin/env node
//GpuCanvas encoder, decoder, and stream


/*
TODO:
//newtest.js -> gpu-test-stream.js
mp3player.js(lame + speaker) + GpuStream.js(index.js) + latency balancer -> gpu-player.js
vix2loader.js + vix2models.js = vix2stream.js
vix2stream.js + GpuPlayer.js = show
*/

"use strict"; //find bugs easier
//export everything from Gpu components:
Object.assign(module.exports,
//TODO    require('./src/GpuEncoder'), //GpuEncoder converts pixel frame data into ndjson stream
//TODO    require('./src/GpuDecoder'), //GpuDecoder converts ndjson stream to frames of pixel data
    require('./src/GpuStream'), //GpuStream accepts pixel data frames and displays them on the screen to drive the LEDs
//    require("./src/GpuTestStream"), //GpuTestStream generates a test pattern
);

//display modes:
module.exports.Modes =
{
    LIVE: true,
    DEV: false,
};

//default config settings (caller can override):
//        NUM_UNIV: 24,
//        get UNIV_LEN() { return this.Screen.height; },
//        FPS: 60,
//        LATENCY: 1.4, //sec
//        get MODE() { return this.DEV; },

//EOF
