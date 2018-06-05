#!/usr/bin/env node
//One-by-one demo pattern: tests GPU config, GPIO pins, and WS281X connections
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes
//
//History:
//ver 0.9  DJ  10/3/16  initial version
//ver 0.95 DJ  3/15/17  cleaned up, refactored/rewritten for FriendsWithGpu article
//ver 1.0  DJ  3/20/17  finally got texture working on RPi
//ver 1.0a DJ  3/25/17  add getchar to allow single-step + color changes
//ver 1.0b DJ  11/22/17  add shim for non-OpenGL version of GpuCanvas
//ver 1.0c DJ  6/4/18  minor external cleanup

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const inherits = require('inherits');


/*
function formatCurrency(format)
{
  this.format = format;
}
const handler = 
{
  construct: function(target, args)
  {
    return new target("$" + args[0]);
  }
};
const proxy = new Proxy(formatCurrency, handler);
document.write(new proxy('100').format); // Result -> $100
*/


//https://stackoverflow.com/questions/37714787/can-i-extend-proxy-with-an-es2015-class
class Array2D
{
    constructor(opts)
    {
        console.log(`Array2D ctor: ${JSON.stringify(opts)}`.blue_lt);
//for proxy info see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
//new Proxy(function(){ this.isAry2D = true; },
        return new Proxy(this,
        {
            construct: function ary2d_jsctor(target, args, newTarget) //ctor args: width, height
            {
                const [opts] = args;
                const {w, h} = opts || {};
                const THIS = {w, h}; //wrapped obj
                THIS.dump = function() { console.log(`Array2D dump: w ${this.w}. h ${this.h}`.blue_lt); }
                THIS.func = function() { console.log("func".blue_lt); }
                console.log(`Array2D ctor: ${JSON.stringify(args)}`.cyan_lt);
                return THIS;
            },
    
    //for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
            get: function(target, key, rcvr)
            {
    //        if (!(propKey in target)) throw new ReferenceError('Unknown property: '+propKey);
    //        return Reflect.get(target, propKey, receiver);
//                console.log("get " + key);
    
                const value = Reflect.get(target, key, rcvr); //target[key]
                console.log(`ary2d: got ${key}? ${key in target}, type ${typeof value}, val ${value}`.blue_lt);
                if (typeof value == 'xfunction')
                {
                    return function (...args)
                    {
                        console.log(`ary2d: call '${key}' with ${args.length} args`);
    //                return Reflect[key](...args);
                        return value.apply(this, args);
                    }
                }
      //        console.log(`ary2d: get '${key}'`);
    //        return Reflect.get(target, propname, receiver);
                return value;
            },
            set: function(target, key, value, rcvr)
            {
                console.log(`ary2d: set[${key}] = ${value}`.blue_lt);
                return Reflect.set(target, key, value, rcvr);
            },
        });
    }
    dump()
    {
        console.log(`dump: ${JSON.stringify(this)}`.blue_lt);
    }
    func(val)
    {
        console.log(`func: ${typeof val} ${JSON.stringify(val)}`.blue_lt);
    }
//    if (!(this instanceof Array2D)) return new Array2D(opts);
//    Proxy.call(this, function(){ this.isAry2D = true; }, hooks);
}
//inherits(Array2D, Proxy);


//test:
const a = new Array2D({w: 2, h: 3});
a.dump();
console.log("x = " + a.x);
a.a = 2;
a[1] = 3;
a.func(1);
a[1, 2] = 4;

//eof