#!/usr/bin/env node
//mp3 playback:
'use strict';

require('colors'); //var colors = require('colors/safe'); //https://www.npmjs.com/package/colors; http://stackoverflow.com/questions/9781218/how-to-change-node-jss-console-font-color
const fs = require('fs');
const lame = require('lame');
const pathlib = require('path');
const deglob = require("./shared/deglob");
const {debug} = require('./shared/debug');
const {now, elapsed, milli} = require('./shared/elapsed');
const Speaker = require('speaker'); //('yalp/my-speaker');

//const mp3len = require('yalp/utils/mp3len');
//const relpath = require('yalp/utils/relpath');
//const {elapsed} = require('yalp/utils/elapsed');
//const {timescale} = require('yalp/utils/timefmt');

Date.now_usec = function()
{
    var parts = process.hrtime();
//    return parts[0] + parts[1] / 1e9; //sec
    return parts[0] * 1e6 + parts[1] / 1e3; ///usec
}

//display commas for easier debug:
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return val.toLocaleString();
}


debug("spkr api %s, name '%s', rev %s\n desc '%s'".blue_lt, Speaker.api_version, Speaker.name, Speaker.revision, Speaker.description);
var x = process.hrtime();
console.log(x);

//need status info:
//https://www.mpg123.de/api/group__mpg123__seek.shtml

//example mp3 player from https://gist.github.com/TooTallNate/3947591
//more info: https://jwarren.co.uk/blog/audio-on-the-raspberry-pi-with-node-js/
//this is impressively awesome - 6 lines of portable code!
//https://github.com/TooTallNate/node-lame
var started;
const mp3player =
module.exports =
function mp3player(filename, cb)
{
    if (!cb) cb = debug;
//console.log("mp3player: type", typeof filename, filename);
//    filename = filename.replace(/[()]/, "."); //kludge: () treated as special chars
//    var duration = mp3len(filename);
//    var decode = new Elapsed();
//    const want_play = true;
//    var started = elapsed();

//console.log("playback %s %s".yellow_light, duration, filename);
//    var decoder, pbstart, pbtimer;
    elapsed(0); //set time base
    started = Date.now_usec();
//console.log("started %d", Date.now_usec());
    fs.createReadStream(filename) //process.argv[2]) //specify mp3 file on command line
        .pipe(new lame.Decoder())
        .on('format', function(format)
        {          
            debug("mp3 '%s', format %s".blue_lt, filename, JSON.stringify(format)); //.sampleRate, format.bitDepth);
//            if (want_play) 
            this.pipe(new Speaker(format))
                .on('open', function() { cb("audio open".blue_lt); }) //pbstart = elapsed(); /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
                .on('flush', function() { cb("audio flush".blue_lt); }) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
                .on('close', function() { cb("audio close".blue_lt); }) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker closed".yellow_light, timescale(elapsed() - started)); });
//            elapsed(0);
            cb("audio start".green_lt);
        })
        .on('end', function() { cb("audio done".green_lt); }) //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
        .on('error', function(err) { cb(`audio ERROR: ${err}`.red_lt); }); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });
//    return started - 0.25; //kludge: compensate for latency
}


//CLI for testing:
//avoid hoisting errors
if (!module.parent)
setImmediate(function()
{
    console.error("running CLI test".green_lt);
    var files = deglob(process.argv[2] || pathlib.resolve(__dirname, "*.mp3")); //process.cwd(), "*.mp3"));
    console.log("found files %j".blue_lt, files);
    mp3player(files[0], function(msg) { console.log("[%s] %s", milli(elapsed()), msg); }); //commas(Math.floor(Date.now_usec() - started + 0.5)), msg); });
    console.error("CLI test continues asynchronously".green_lt);
});


//eof
