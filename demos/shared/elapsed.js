//elapsed time (sec):
//other processes will be synced (uses shared memory)
//TODO: use cluster instead?

'use strict'; //find bugs easier
//const cluster = require('cluster');
const shm = require('shm-typed-array');
//Date.now_usec = require("performance-now"); //override with hi-res version (usec)

//default values:
//caller can override these (lzay, but must be before first use)
//const HIGH = false; //true;
module.exports.hires = false;
module.exports.shmkey = 0xC10c4; //default shm key; caller can change it, but must coordinate with other processes

//var epoch = 
//module.exports.epoch = now_sec();
//module.exports.now = now_sec;

/*
const milli =
module.exports.milli =
function milli(sec)
{
//    return Math.floor(sec * 1000) / 1000;
    var str = Math.floor(sec * 1000).toString();
    if (str.length < 4) str = ("0000" + str).slice(-4);
    return str.replace(/(\d{3})$/, ".$1"); //.splice(-3, 0, ".");
}
*/


//get time since epoch:
const elapsed =
module.exports.elapsed =
function elapsed(new_epoch)
{
console.log("elapsed(%s)", new_epoch);
    if (!elapsed.shmbuf) //set up shared memory for sync across processes
        elapsed.shmbuf = module.exports.shmkey? shm.get(module.exports.shmkey, "Float64Array") || shm.create(1, "Float64Array", module.exports.shmkey): new FloatArray(1);
//        elapsed.shmbuf[0] = 0; //caller needs to do this
    if (arguments.length || !elapsed.shmbuf[0]) elapsed.shmbuf[0] = now_sec() - (new_epoch || 0); //set new epoch if passed; 0 = now; < 0 for past; > 0 for future
    return now_sec() - elapsed.shmbuf[0]; //(elapsed.epoch || 0);
}
process.nextTick(elapsed); //give caller a chance to set epoch before i do


//current time (sec):
const now_sec =
module.exports.now_sec =
function now_sec()
{
    if (!module.exports.hires) return Date.now() / 1e3;
    var parts = process.hrtime();
    return parts[0] + parts[1] / 1e9;
}

//eof
