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
//const inherits = require('inherits');
const {debug} = require('./demos/shared/debug');


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
//CAUTION: class not hoisted
class Array2D
{
    constructor(opts)
    {
//        console.log(`Array2D ctor: ${JSON.stringify(opts)}`.blue_lt);
//for proxy info see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Proxy
//new Proxy(function(){ this.isAry2D = true; },
//        const {w, h} = opts || {};
//        const defaults = {w: 1, h: 1};
        Object.assign(this, /*defaults,*/ opts || {}); //{width, height, depth}
        this.ary = new Array((this.width || 1) * (this.height || 1) * (this.depth || 1)); //[];
        this.xyz = (isN(this.width) && isN(this.height) && isN(this.depth))? function(key) { var [x, y, z] = key.split(","); return (x * this.height + y) * this.depth + z; }:
            (isN(this.width) && isN(this.height))? function(key) { var [x, y] = key.split(","); return x * this.height + y; }:
            function(key) { return key; };
//        var [x, y, z] = key.split(","); //[[...]] in caller becomes comma-separated string; //.reduce((prev, cur, inx, all) => { return }, 0);
//        var inx = x;
//        if (!isNaN(x)? x: x * this.h + isNaN(y);
//        console.log(`xy(${typeof key} ${key}) => x ${typeof x} ${x}, y ${typeof y} ${y}, z ${typeof z} ${z} => inx ${typeof inx} ${inx}`.blue_lt);
//        return inx;
        return new Proxy(this, //return proxy object instead of default "this"; see https://stackoverflow.com/questions/40961778/returning-es6-proxy-from-the-es6-class-constructor?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        {
/*NO; ctor already happened (caller)
            construct: function(target, ctor_args, newTarget) //ctor args: [{width, height}]
            {
                const {w, h} = ctor_args[0] || {};
                const THIS = {pxy_w: w, pxy_h: h}; //wrapped obj
//                THIS.dump = function() { console.log(`Array2D dump: w ${this.w}. h ${this.h}`.blue_lt); }
//                THIS.func = function() { console.log("func".blue_lt); }
                console.log(`Array2D ctor trap: ${typeof target} => ${typeof newTarget}, ${JSON.stringify(THIS)} vs ${JSON.stringify(this)}`.cyan_lt);
                return THIS;
            },
*/    
    //for intercepting method calls, see http://2ality.com/2017/11/proxy-method-calls.html
            get: function(target, key, rcvr)
            {
    //        if (!(propKey in target)) throw new ReferenceError('Unknown property: '+propKey);
    //        return Reflect.get(target, propKey, receiver);
//                console.log("get " + key);
//                if (!(key in target)) target = target.ary; //if not at top level, delegate to array contents
                var inx;
                const value = target[key] || target.ary[inx = target.xy(key)]; //Reflect.get(target, key, rcvr);
                if (typeof value == 'function')
                {
                    return function (...args)
                    {
                        debug(`ary2d: call '${key}' with ${args.length} args`);
    //                return Reflect[key](...args);
                        return value.apply(this, args);
                    }
                }
                debug(`ary2d: got[${typeof key} ${key} => ${typeof inx} {inx}] = ${typeof value} ${value}`.blue_lt);
      //        console.log(`ary2d: get '${key}'`);
    //        return Reflect.get(target, propname, receiver);
                return value;
            },
            set: function(target, key, value, rcvr)
            {
                var inx = target.xy(key);
                debug(`ary2d: set[${typeof key} ${key} => ${typeof inx} {inx}] = ${value}`.blue_lt);
                return target.ary[inx] = value; //Reflect.set(target, key, value, rcvr);
            },
        });
    }

//    xy(key) //flatten 2D or 3D to 1D array
//    {
//        var [x, y, z] = key.split(","); //[[...]] in caller becomes comma-separated string; //.reduce((prev, cur, inx, all) => { return }, 0);
//        var inx = x;
//        if (!isNaN(x)? x: x * this.h + isNaN(y);
//        console.log(`xy(${typeof key} ${key}) => x ${typeof x} ${x}, y ${typeof y} ${y}, z ${typeof z} ${z} => inx ${typeof inx} ${inx}`.blue_lt);
//        return inx;
//    }

    dump(desc)
    {
        console.log(`dump ${desc || ""}: ${JSON.stringify(this.ary)}`.blue_lt);
    }

    static isN(val) { return !isNaN(val); }
//    func(val)
//    {
//        console.log(`func: ${typeof val} ${JSON.stringify(val)}`.blue_lt);
//    }
//    if (!(this instanceof Array2D)) return new Array2D(opts);
//    Proxy.call(this, function(){ this.isAry2D = true; }, hooks);
}
//inherits(Array2D, Proxy);


//test:
const a = new Array2D({w: 2, h: 3});
a.dump("after ctor");
console.log("x = " + a.x);
var x = 1, y = 2;
a[y] = 2;
a[1] = 3;
//a.func(a[[1,2]]);
a[[1,1]] = 4;
a[[x, y]] = 5;
a[['b', 0]] = 6;
a[[-1, 0]] = 7;
a.dump("updated");
console.log("a[[1,1]] = " + a[[1,1]]);

//eof