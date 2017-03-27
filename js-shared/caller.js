//return name/location of caller
'use strict';

const path = require('path');
const util = require('util');
const callsite = require('callsite');
// /*var sprintf =*/ require('sprintf.js'); //.sprintf;
//var shortname = require('my-plugins/utils/shortname');
//var stack = require('stack-trace');

/*
module.exports.stackx = function(depth)
{
    return callsite()[depth].getFileName();
}

//use exclude if stack depth might be variable (ie, ctors with optional "new" will use an extra level of stack)
module.exports.stack = function(depth, exclude)
{
    var trace = stack.get();
//    console.log("stack trace %j", trace);
    for (;;)
    {
        var retval = trace[depth].getFileName();
        if (retval == exclude) { ++depth; continue; }
        return retval;
    }
}
*/


const caller =
module.exports.caller =
function caller(depth)
{
    var retval = '';
    var want_all = !depth;
    var abbreviated = (depth < 0);
    if (abbreviated) depth = -depth;
    callsite().every(function(stack, inx)
    {
        if (!want_all)
        {
            if (stack.getFileName().indexOf("node_modules") != -1) return true;
            if (stack.getFileName() == __filename) return true;
            if (depth--) return true;
        }
//        console.log('stk[%d]: %s@%s:%d'.blue, inx, stack.getFunctionName() || 'anonymous', relpath(stack.getFileName()), stack.getLineNumber());
        var caller = util.format("@%s:%d", shortname(stack.getFileName()), stack.getLineNumber());
        if (!abbreviated) caller = util.format('stk[%d]: %s', inx, stack.getFunctionName() || '(anonymous)') + caller;
        if (!want_all) retval = caller;
        else retval += '\n' + caller;
        return want_all; //false;
    });
    return retval || '??';
}


const shortname =
module.exports.shortname = 
function shortname(filename)
{
    for (;;)
    {
        var retval = path.basename(filename, path.extname(filename));
        if (retval != "index") return retval;
        filename = path.dirname(filename); //use parent folder name; basename was not descriptive enough
    }
}

//eof
