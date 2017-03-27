//debug logger:
//patterned after https://github.com/visionmedia/debug (uses DEBUG env var for control),
// but auto-defines name, uses indents, and allows caller to specify color and detail level
//turn debug on with:
// DEBUG=* node caller.js
// DEBUG=thing1,thing2 node caller.js
// DEBUG=thing1=level,thing2=level node caller.js
//turn off or exclude with:
// DEBUG=-thing

'use strict'; //find bugs easier
require('colors'); //for console output
const path = require('path');
const {elapsed, milli} = require('./elapsed');

const ColorCodes = /\x1b\[\d+(;\d+)?m/;
//console.log("parent", module.parent);
const my_parent = path.basename(module.parent.filename, path.extname(module.parent.filename));
delete require.cache[__filename]; //kludge: get fresh parent info each time; see http://stackoverflow.com/questions/13651945/what-is-the-use-of-module-parent-in-node-js-how-can-i-refer-to-the-requireing
//const parent_name = module.parent.filename;

var enabled = {};
(process.env.DEBUG || "").split(/\s*,\s*/).forEach((part, inx) =>
{
    if (!part) return;
    var parsed = part.match(/^([+-])?([^\s=]+|\*)(\s*=\s*(\d+))?$/);
    if (!parsed) { console.log("ignoring unrecognized DEBUG option[%d]: '%s'".red_lt, inx, part); return; }
    var [, remove, name,, level] = parsed;
//    console.log("part: \"%s\", +/- '%s', name '%s', level '%s'", part, remove, name, level);
    if (name == "*") //all on/off; clear previous options
        enabled = (remove == "-")? {}: {'*': level || Number.MAX_SAFE_INTEGER}; //remove/enable all, set default level if missing
    else //named module
        enabled[name] = (remove == "-")? -1: level || Number.MAX_SAFE_INTEGER;
});


const debug =
module.exports.debug =
function debug(args)
{
    var detail = 0;
    args = Array.from(arguments); //turn into real array
    if ((args.length >= 1) && (typeof args[0] == "number")) { detail = args[0]; args.shift(); }
    if ((args.length < 1) || (typeof args[0] != "string")) args.unshift("%j"); //placeholder for fmt

    var want_detail = enabled[my_parent] || enabled['*'] || -1;
//console.log("DEBUG '%s': want %d vs current %d, discard? %d, options %j", my_parent, want_detail, detail, detail >= want_detail, enabled);
    if (detail >= want_detail) return; //too much detail; user doesn't want it
    var fmt = args[0];
//    ColorCodes.lastIndex = -1; //clear previous search (persistent)
    var match = ColorCodes.exec(fmt);
//console.log("found last", match, ColorCodes);
    ColorCodes.lastIndex = 0; //reset for next time (persistent)
//    var svcolor = [];
//    fmt = fmt.replace(ColorCodes, function(str) { svcolor.push(str); return ''; }); //strip color codes
//    if (!svcolor.length) svcolor.push('');
    var ofs = (match && !match.index)? match[0].length: 0; //don't split leading color code
    args[0] = fmt.substr(0, ofs) + my_parent + "[@" + milli(elapsed()) + "] " + fmt.substr(ofs);

    return console.log.apply(console, args);
}

//eof
