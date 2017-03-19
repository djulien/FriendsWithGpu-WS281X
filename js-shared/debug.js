//debug logger:
//patterned after https://github.com/visionmedia/debug (uses DEBUG env var for control),
// but auto-defines name, uses indents, and allows caller to specify color and detail level
//turn debug on with:
// DEBUG=* node caller.js
// DEBUG=thing1,thing2 node caller.js
// DEBUG=thing1=level,thing2=level node caller.js

'use strict'; //find bugs easier
const path = require('path');
const {elapsed, mmm} = require('./elapsed');

const ColorCodes = /\x1b\[\d+(;\d+)?m/;
//console.log("parent", module.parent);
const my_parent = path.basename(module.parent.filename, path.extname(module.parent.filename));
delete require.cache[__filename]; //kludge: get fresh parent info each time; see http://stackoverflow.com/questions/13651945/what-is-the-use-of-module-parent-in-node-js-how-can-i-refer-to-the-requireing
//const parent_name = module.parent.filename;


module.exports.debug =
function debug(args)
{
    var detail = 0;
    args = Array.from(arguments); //turn into real array
    if ((args.length >= 1) && (typeof args[0] == "number")) { detail = args[0]; args.shift(); }
    if ((args.length < 1) || (typeof args[0] != "string")) args.unshift("%j"); //placeholder for fmt

//TODO: filter by detail level, DEBUG modules
    var fmt = args[0];
//    ColorCodes.lastIndex = -1; //clear previous search (persistent)
    var match = ColorCodes.exec(fmt);
//console.log("found last", match, ColorCodes);
    ColorCodes.lastIndex = 0; //reset for next time (persistent)
//    var svcolor = [];
//    fmt = fmt.replace(ColorCodes, function(str) { svcolor.push(str); return ''; }); //strip color codes
//    if (!svcolor.length) svcolor.push('');
    var ofs = (match && !match.index)? match[0].length: 0; //don't split leading color code
    args[0] = fmt.substr(0, ofs) + my_parent + "[@" + mmm(elapsed()) + "] " + fmt.substr(ofs);

    return console.log.apply(console, args);
}

//eof
