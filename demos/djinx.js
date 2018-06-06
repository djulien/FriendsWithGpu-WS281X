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
const {debug} = require('./shared/debug');


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
        const want_debug = true; //reduce xyz() overhead
        Object.assign(this, /*defaults,*/ opts || {}); //{width, height, depth}
//store 3D dimensions for closure use; also protects against caller changing dimensions after array is allocated:
        const width = this.w || this.width || 1, height = this.h || this.height || 1, depth = this.d || this.depth || 1;
        const ary /*this.ary*/ = new Array(width * height * depth); //[]; //contiguous for better xfr perf to GPU
//        const xyz /*this.xy = this.xyz*/ = /*(isNaN(this.w || this.width) || isNaN(this.h || this.height))*/(height * depth == 1)? inx1D: /*isNaN(this.d || this.depth)*/(depth == 1)? inx2D: inx3D;
//        var [x, y, z] = key.split(","); //[[...]] in caller becomes comma-separated string; //.reduce((prev, cur, inx, all) => { return }, 0);
        ary.fill(0);
        debug(`after ctor: ${JSON.stringify(this)}, ary len ${ary.length}`.blue_lt);
        this.dump = function(desc) { console.log(`dump '${desc || ""}': ${ary.length} ${JSON.stringify(ary)}`.blue_lt); }
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
//most accesses will be for pixel array, so extra overhead for non-array props is not significant
            get: function(target, key, rcvr)
            {
    //        if (!(propKey in target)) throw new ReferenceError('Unknown property: '+propKey);
    //        return Reflect.get(target, propKey, receiver);
//                console.log("get " + key);
//                if (!(key in target)) target = target.ary; //if not at top level, delegate to array contents
                const inx = xyz(key), istop = isNaN(inx);
                const value = istop? target[key]: (inx >= 0)? ary[inx]: null;
                if (typeof value == 'function') return function (...args) { return value.apply(this, args); } //Reflect[key](...args);
                debug(shorten(`ary2d: got-${istop? "obj": (inx < 0)? "noop": "ary"}[${typeof key}:${key} => ${typeof inx}:${inx}] = ${typeof value}:${value}`.blue_lt));
      //        console.log(`ary2d: get '${key}'`);
    //        return Reflect.get(target, propname, receiver);
                return value;
            },
            set: function(target, key, newval, rcvr)
            {
                const inx = xyz(key), istop = isNaN(inx);
                debug(shorten(`ary2d: set-${istop? "obj": (inx < 0)? "noop": "ary"}[${typeof key}:${key} => ${typeof inx}:${inx}] = ${newval}`.blue_lt));
                return istop? target[key] = newval: (inx >= 0)? /*target.*/ary[inx] = newval: newval; //Reflect.set(target, key, value, rcvr);
            },
        });

//multi-dim array indexing:
//        function inx1D(key)
//        {
//            if (isNaN(key)) return key; //top level prop name
//            return (+key < ary.length)? +key: -1; //out of range check
//        }
//        function inx2D(key)
//        {
//            const [x, y] = key.split(","); //[[x,y]] in caller becomes comma-separated string
//            const inx = (typeof y == "undefined")? (isNaN(x)? x: (+x < ary.length)? +x: -1): //1D; range check
//                (isNaN(x) || isNaN(y))? key: //prop name; not array index
//                (/*(+x >= 0) &&*/ (+x < width) && (+y >= 0) && (+y < height))? +x * height + +y: -1; //2D; range check
//            if (want_debug) debug(`xy '${key}' => x ${typeof x}:${x}, y ${typeof y}:${y} => inx ${typeof inx}:${inx}`.blue_lt);
//            return inx;
//        }
        function xyz(key)
        {
            const [x, y, z] = key.split(","); //[[x,y,z]] in caller becomes comma-separated string
            const inx = (typeof y == "undefined")? (isNaN(x)? x: (+x < ary.length)? +x: -1): //1D; range check or as-is
                            (isNaN(x) || isNaN(y))? key: //prop name; not array index
                            (typeof z == "undefined")? //2D
                                (/*(+x >= 0) &&*/ (+x < width) && (+y >= 0) && (+y < height))? +x * height + +y: -1: //range check
                            isNaN(z)? key: //prop name; not array index
                                (/*(+x >= 0) &&*/ (+x < width) && (+y >= 0) && (+y < height) && (+z >= 0) && (+z < depth))? (+x * height + +y) * depth + +z: -1; //range check
            if (want_debug && isNaN(++(xyz.seen || {})[key])) //reduce redundant debug info
            {
                debug(shorten(`xyz '${key}' => x ${typeof x}:${x}, y ${typeof y}:${y}, z ${typeof z}:${z} => inx ${typeof inx}:${inx}`.blue_lt));
                if (!xyz.seen) xyz.seen = {};
                xyz.seen[key] = 1;
            }
            return inx;
        }
//cut down on verbosity:
        function shorten(str)
        {
            return (str || "").replace(/undefined(:undefined)?|string:|number:|object:/g, str => { return str.slice(0, 3) + ((str.slice(-1) == ":")? ":": ""); });
        }
    }

//    xy(key) //flatten 2D or 3D to 1D array
//    {
//        var [x, y, z] = key.split(","); //[[...]] in caller becomes comma-separated string; //.reduce((prev, cur, inx, all) => { return }, 0);
//        var inx = x;
//        if (!isNaN(x)? x: x * this.h + isNaN(y);
//        console.log(`xy(${typeof key} ${key}) => x ${typeof x} ${x}, y ${typeof y} ${y}, z ${typeof z} ${z} => inx ${typeof inx} ${inx}`.blue_lt);
//        return inx;
//    }

//    dump(desc)
//    {
//        console.log(`dump ${desc || ""}: ${JSON.stringify(ary)}`.blue_lt);
//    }

//    static isN(val) { return !isNaN(val); }
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
a.x = "hi";
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
console.log("a[1] = " + a[1]);
a[45] = 45;
console.log("a[45] = " + a[45]);
a.dump("ovfl");
console.log(JSON.stringify(a));

//eof