//functions to allow JavaScript synchronous coding style
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes

'use strict'; //find bugs easier
require('colors'); //for console output
const keypress = require('keypress');
//const {caller} = require("./caller");
//const {debug} = require('./debug');


//register callbacks when synchronous main ends:
const atexit =
module.exports.atexit =
function atexit(new_cb)
{
    if (!atexit._registered) atexit._registered = [];
    if (!new_cb) //call the functions
    {
//        debug("call %d exit cbs".cyan_lt, atexit._registered.length, caller());
        atexit._registered.forEach(cb => { cb(); }); //setImmediate(cb); }); //call after return to caller
    }
    else atexit._registered.push(new_cb);
}


//step thru generator function:
//allows synchronous (blocking) coding style
const blocking =
module.exports.blocking =
function blocking(gen)
{
	if (typeof gen == "function") //invoke generator if not already
    {
        setImmediate(function() { blocking(gen()); }); //avoid hoist errors
        return;
    }
//    process.stdin.pause(); //don't block process exit while not waiting for input
//    var retval;
	for (;;)
	{
		var status = gen.next(blocking.retval); //send previous value to generator
//		if (!status.done && (typeof status.value == "undefined")) continue; //cooperative multi-tasking
		if (typeof status.value == "function") //caller wants manual step
        {
            blocking.retval = status.value(gen); //remember latest ret val
            return; //caller wants to execute before next step
        }
        blocking.retval = status.value; //remember latest ret val, pass to next step
		if (!status.done) continue;
        atexit();
        return blocking.retval;
	}
}


//wake up from synchronous sleep:
const wait =
module.exports.wait =
function wait(delay)
{
    delay *= 1000; //sec -> msec
    if (delay > 1) return setTimeout.bind(null, blocking, delay);
}


//pause for synchronous keyboard input (string):
//useful info:
// https://github.com/TooTallNate/keypress
// https://docs.nodejitsu.com/articles/command-line/how-to-prompt-for-command-line-input/
const prompt =
module.exports.prompt =
function prompt(args)
{
    if (!prompt.init)
    {
        process.stdin.setEncoding('utf8');
//        process.stdin.setRawMode(true); //get each char
        process.stdin.on('data', function(text)
        {
            process.stdin.pause();
//        process.exit();
//        process.stdin.pause();
            blocking.retval = text.toString(); //give entered text back to caller
            blocking(prompt.gen);
        });
        prompt.init = true;
    }
    process.stdin.resume();
    console.log.apply(console, args? arguments: ["Enter to continue ..."]);
    return function(gen) { prompt.gen = gen; } //save generator for text wakeup later
}


//pause for synchronous keyboard input (single char):
const getchar =
module.exports.getchar =
function getchar(args)
{
    if (!getchar.init)
    {
        keypress(process.stdin); //ask for keypress events
        process.stdin.setRawMode(true);
        process.stdin.on('keypress', function(ch, key)
        {
//console.log("getchar gen", getchar.gen);
            process.stdin.pause();
//        console.log('got "keypress"', key);
//        if (key && key.ctrl && key.name == 'c') process.stdin.pause();
            blocking.retval = (key || {}).name;
            blocking(getchar.gen);
        });
        getchar.init = true;
    }
    process.stdin.resume();
    if (console.log._previous) console.log._previous = null; //kludge: defeat custom console dedup
    console.log.apply(console, args? arguments: ["Any key to continue ..."]);
    return function(gen) { /*debug("save gen", gen)*/; getchar.gen = gen; } //save generator for text wakeup later
}

//eof
