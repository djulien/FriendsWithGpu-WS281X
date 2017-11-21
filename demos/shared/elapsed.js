//elapsed time (sec):

'use strict'; //find bugs easier
const HIGH = false; //true;
//Date.now_usec = require("performance-now"); //override with hi-res version (usec)

var epoch = 
module.exports.epoch = now_sec();

module.exports.now = now_sec;

module.exports.milli =
function milli(sec)
{
//    return Math.floor(sec * 1000) / 1000;
    var str = Math.floor(sec * 1000).toString();
    if (str.length < 4) str = ("0000" + str).slice(-4);
    return str.replace(/(\d{3})$/, ".$1"); //.splice(-3, 0, ".");
}


//get time since epoch:
module.exports.elapsed =
function elapsed(new_epoch)
{
    if (arguments.length) epoch = now_sec() - new_epoch; //set new epoch if passed; 0 == now; < 0 == past; > 0 == future
    return now_sec() - epoch;
}


//current time (sec):
function now_sec()
{
    if (!HIGH) return Date.now() / 1e3;
    var parts = process.hrtime();
    return parts[0] + parts[1] / 1e9;
}

//eof
