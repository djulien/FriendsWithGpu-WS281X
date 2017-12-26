#!/usr/bin/env node
//wrapper for MP3 player; can be run in fg or bkg

const WANT_DEBUG = true; //false;

"use strict"; //find bugs easier
const fs = require("fs");
//const pathlib = require('path');
const lame = require('lame');
const Speaker = require('speaker');
const cluster = require('cluster');
const JSON = require('circular-json'); //CAUTION: replace std JSON with circular-safe version
const deglob = require('./deglob');
const mp3len = require('./mp3len');
const {debug} = require('./debug');


//get default audio file:
//finds first mp3 file in current folder or from cmd line
//lazy-load to avoid extraneous processing
Object.defineProperty(module.exports, "DefaultAudioFile",
{
    get: function() //define read-only prop
    {
//const {caller} = require('./demos/shared/caller');
//        console.log("mod exp", caller(1));
        var filename = Array.from(process.argv).slice(2).reduce((previous, arg) => { return (arg[0] != "-")? arg: previous; }, null);
//console.log(typeof filename, filename)
        return FindAudio(filename || __dirname + "/*.mp3"); //cmdline or current folder
    },
    configurable: true, //allow caller to modify/delete
});


const FindAudio =
module.exports.FindAudio =
function FindAudio(pattern, maxm)
{
//console.log(typeof pattern, pattern);
    return deglob(pattern, maxm)[0]; //choose first match; enforce uniqueness (optional param from caller)
}

module.exports.mp3len = mp3len; //let caller have this one also


////////////////////////////////////////////////////////////////////////////////
////
/// MP3 playback
//

//initiate mp3 playback:
//can run in fg or bkg
//finishes asynchronously
//optional callbacks for audio data cued (ready)/done and progress/events
const mp3pb =
module.exports.mp3playback =
function mp3playback(filename, bkg, done_callercb, progress_callercb)
{
    if (!filename) return;
    if (bkg)
    {
        if (!cluster.isMaster) throw "mp3playback only supported from main process".red_lt;
        mp3pb.wker = cluster.fork({MP3FILE: filename});
        debug(`mp3 wker '${mp3pb.wker.process.pid}' forked`.green_lt, JSON.stringify(mp3pb.wker));

        mp3pb.wker.on('online', () =>
        {
            debug(`bkg mp3 wker started`.green_lt);
//            wkers[wker.process.pid] = wker;
        });
        mp3pb.wker.on('disconnect', () =>
        {
            debug(`bkg mp3 wker disconnected`.cyan_lt);
        });
        mp3pb.wker.on('message', (msg) =>
        {
            debug(`got msg ${JSON.stringify(msg)} from bkg wker`.blue_lt);
//send msgs back to caller:
            if (msg.mp3done) done_cb.apply(null, msg.mp3done);
            if (msg.mp3progess) progress_cb.apply(null, msg.mp3progress);
        });
//handle this one in caller so wker can be restarted:
        cluster.on('exit', (wker, code, signal) =>
        {
            if (wker.process.id != mp3pb.wker.process.id) return; //ignore; not mine
            debug(`mp3 wker '${wker.process.id}' died (${code || signal}); NO restart.`.red_lt);
//            mp3pb.wker = cluster.fork(); //this won't help; audio stream has been interrupted
        });
//        process.on('message', (msg) =>
//        {
//            if (msg.env) { wkers[pid].process.env = msg.env; delete msg.env; } //save in case caller wants access to wker's env
//            step.retval = {msg, wker: wkers[pid]}; //{process: {pid, env: wkers[pid].env}}}; //, narg: arguments.length};
//console.log("got reply", JSON.stringify(step.retval).slice(0, 800));
//            if (msg.mp3quit) { console.log("rcv quit"); return mp3playback.done = true; } //just set flag; don't interfere with step
//            step(); //wake up caller
//}catch(exc) {console.log(`cluster onmsg EXC: ${exc}`.red_lt); }
//        });

//    if (mp3playback.bkg = bkg) return bkg_send(filename);
//        return mp3pb.wker.send({audio: filename}); //return yield;
        return;
    }

    debug(`mp3playback: '${filename}'`.blue_lt);
    fs.createReadStream(mp3pb.filename = filename) //process.argv[2]) //specify mp3 file on command line
        .pipe(mp3pb.pipe = new lame.Decoder())
        .on('format', function(fmt)
        {
            mp3pb.persec = fmt.sampleRate * fmt.channels * fmt.bitDepth / 8; //#bytes per sec
            debug(`mp3 format ${JSON.stringify(fmt)}`.blue_lt); //.sampleRate, format.bitDepth);
            if (done_cb) done_cb(false); //cued
////////////////////setTimeout(mp3cancel, 10  * 1000); //just run it for 10 sec
////////////////////            if (cluster.isMaster) step();
//////////////////////            else process.send({mp3ack: true}); //tell fg thread to start fx playback
//            if (want_play) 
            this.pipe(new Speaker(fmt))
                .on('open', function() { progress_cb("audio open".blue_lt); }) //pbstart = elapsed(); /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
                .on('progress', function(data) { progress_cb("audio progress".blue_lt, data); }) //pbstart = elapsed(); /*pbtimer = setInterval(position, 1000)*/; console.log("[%s] speaker opened".yellow_light, timescale(pbstart - started)); })
                .on('flush', function() { progress_cb("audio flush".blue_lt); done_cb(true); }) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker end-flushed".yellow_light, timescale(elapsed() - started)); })
                .on('close', function() { progress_cb("audio close".blue_lt); done_cb(true); }) // /*clearInterval(pbtimer)*/; console.log("[%s] speaker closed".yellow_light, timescale(elapsed() - started)); });
//            elapsed(0);
            progress_cb("audio start".green_lt);
        })
        .on('end', function() { progress_cb("audio done".green_lt); }) //console.log("[%s] decode done!".yellow_light, timescale(elapsed() - started)); })
        .on('error', function(err) { progress_cb(`audio ERROR: ${err}`.red_lt, err); done_cb(); }); //console.log("[%s] decode ERROR %s".red_light, timescale(elapsed() - started), err); });

    function done_cb(state)
    {
console.log(JSON.stringify(arguments));
        debug("mp3 done? %s".cyan_lt, (state === true)? "yes": (state === false)? "cued": (state === undefined)? "error": "unknown");
        if (done_callercb) done_callercb.apply(null, arguments);
    };

    function progress_cb(desc, data)
    {
//console.log(JSON.stringify(arguments));
//        if (!WANT_DEBUG) return;
        if (data)
        {
            var duration = mp3pb.duration || (mp3pb.duration = mp3len(mp3pb.filename));
            if (data.numwr % 100) return; //don't report progress too often
            data["%done"] = tenths(100 * data.wrtotal / (mp3pb.duration * mp3pb.persec)); //estimated based on data written so far vs. total size
//don't report these:
            delete data.wrlen;
            delete data.buflen;
        }
        debug(`mp3 ${desc}`.pink_lt, data? JSON.stringify(data).pink_lt: "");
        if (progress_callercb) progress_callercb.apply(null, arguments);
    }
}


if (cluster.isWorker)
{
    mp3pb(process.env.MP3FILE, false, done_cb, progress_cb);

    function done_cb(state) { process.send({mp3done: arguments}); }
    function progress_cb(args) { process.send({mp3progess: arguments}); }
}


//truncate after 1 dec place:
function tenths(val)
{
//    return Math.floor(val * 10) / 10;
    return (10 * val | 0) / 10; //https://stackoverflow.com/questions/596467/how-do-i-convert-a-float-number-to-a-whole-number-in-javascript
}


//cancel playback:
//TODO: make it more accurate/graceful
const mp3cancel =
module.exports.mp3cancel =
function mp3cancel()
{
    mp3pb.done = true;
    debug("cancel %s".yellow_lt, mp3pb.wker? "bkg quit": "local pipe");
    if (mp3pb.wker) mp3pb.wker.send({mp3quit: true});
    if (mp3pb.pipe) mp3pb.pipe.end(); //push(null); //https://stackoverflow.com/questions/19277094/how-to-close-a-readable-stream-before-end
}


////////////////////////////////////////////////////////////////////////////////
////
/// unit test:
//

//CLI for testing:
if (!module.parent && cluster.isMaster)
setImmediate(function()
{
    console.log("running CLI test".green_lt);
    console.log("default file: '%s'".cyan_lt, module.exports.DefaultAudioFile);
//console.log(JSON.stringify(process.argv));
    var want_bkg = Array.from(process.argv).some((arg) => { return (arg.substr(0, 2).toLowerCase() == "-b"); });
    module.exports.mp3playback(module.exports.DefaultAudioFile, want_bkg, done_cb);
    console.log("CLI test continues asynchronously".green_lt);

    function done_cb(state)
    {
//        console.log("cancel in 10 sec".blue_lt);
        if (state === false) setTimeout(module.exports.mp3cancel, 10  * 1000); //just run it for 10 sec
    }
});


//eof
