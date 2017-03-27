//elapsed time (sec):

'use strict'; //find bugs easier
const HIGH = false; //true;

var epoch = 
module.exports.epoch = now_sec();
module.exports.now = now_sec;
module.exports.milli =
function milli(sec)
{
//    return Math.floor(sec * 1000) / 1000;
    return Math.floor(sec * 1000).toString().replace(/(\d{3})$/, ".$1"); //.splice(-3, 0, ".");
}


//get time since epoch:
module.exports.elapsed =
function elapsed(new_epoch)
{
    if (arguments.length) epoch = new_epoch;
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
