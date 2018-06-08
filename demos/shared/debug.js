//debug logger:
//patterned after https://github.com/visionmedia/debug (uses DEBUG env var for control),
// but auto-defines name, uses indents, and allows caller to specify color and detail level or adjust stack
//turn debug on with:
// DEBUG=* node caller.js
// DEBUG=thing1,thing2 node caller.js
// DEBUG=thing1=level,thing2=level node caller.js
//turn off or exclude with:
// DEBUG=-thing

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const util = require('util');
//const pathlib = require('path');
const {elapsed/*, milli*/} = require('./elapsed');
const {caller/*, calledfrom, shortname*/} = require("./caller");

const ColorCodes = /\x1b\[\d+(;\d+)?m/; //ANSI color escape codes

//console.log("parent", module.parent);
//const my_parent = path.basename(module.parent.filename, path.extname(module.parent.filename));
//delete require.cache[__filename]; //kludge: get fresh parent info each time; see http://stackoverflow.com/questions/13651945/what-is-the-use-of-module-parent-in-node-js-how-can-i-refer-to-the-requireing
//const parent_name = module.parent.filename;

var want_debug = {};
(process.env.DEBUG || "").split(/\s*,\s*/).forEach((part, inx) =>
{
    if (!part) return; //continue;
    var parsed = part.match(/^([+-])?([^\s=]+|\*)(\s*=\s*(\d+))?$/);
    if (!parsed) { console.error(`ignoring unrecognized DEBUG option[${inx}]: '${part}'`.red_lt); return; }
    var [, remove, name,, level] = parsed;
//    console.log("part: \"%s\", +/- '%s', name '%s', level '%s'", part, remove, name, level);
    if (name == "*") //all on/off; clear previous options
        want_debug = (remove == "-")? {}: {'*': level || Number.MAX_SAFE_INTEGER}; //remove/enable all, set default level if missing
    else //named module
        want_debug[name] = (remove == "-")? -1: level || Number.MAX_SAFE_INTEGER;
});


module.exports.elapsed = elapsed; //give caller access to my timebase

const debug =
module.exports.debug =
function debug(args)
{
//console.error("debug:" + JSON.stringify(arguments));
    var detail = 0;
    args = Array.from(arguments); //turn into real array
    if (/*(args.length >= 1) &&*/ (typeof args[0] == "number") && (args[1].toString().indexOf("%") != -1)) { detail = args[0]; args.shift(); }
//    if ((args.length < 1) || (typeof args[0] != "string")) args.unshift("%j"); //placeholder for fmt
//??    else if (args[0].toString().indexOf("%") == -1) args.unshift("%s"); //placeholder for fmt
    var my_parent = caller(-1 - debug.nested); debug.nested = 0; //reset it for next time
    var want_detail = want_debug[my_parent.replace(/^@|:.*$/g, "")] || want_debug['*'] || -1;
//console.log("enabled: %j, parent %s", Object.keys(want_debug), my_parent.replace(/^@|:.*$/g, ""));
//console.log("DEBUG '%s': want %d vs current %d, discard? %d, options %j", my_parent, want_detail, detail, detail >= want_detail, want_debug);
    if (detail >= want_detail) return; //too much detail; user doesn't want it

//    if (typeof args[0] == "string")
//    if (args[0].toString().indexOf("%") == -1)
//    if (args.length > 1)
//    {
//        var fmt = args[0];
    var fmt = util.format.apply(util, args);
//    ColorCodes.lastIndex = -1; //clear previous search (persistent)
    var match = ColorCodes.exec(fmt);
//console.log("found last", match, ColorCodes);
    ColorCodes.lastIndex = 0; //reset for next time (regex state is persistent)
//    var svcolor = [];
//    fmt = fmt.replace(ColorCodes, function(str) { svcolor.push(str); return ''; }); //strip color codes
//    if (!svcolor.length) svcolor.push('');
    var ofs = (match && !match.index)? match[0].length: 0; //don't split leading color code
    fmt = fmt.substr(0, ofs) + `[${my_parent.slice(1)} @${trunc(elapsed(), 1e3)}] ` + fmt.substr(ofs);
//    }
//    return console.error.apply(console, fmt); //args); //send to stderr in case stdout is piped
    return console.error(fmt);
}
debug.nested = 0; //allow caller to adjust stack level


function trunc(val, digits) 
{
    return Math.round(val * digits) / digits;
}

//eof
