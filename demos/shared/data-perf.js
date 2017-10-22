#!/usr/bin/env node
//from https://jsperf.com/uint8array-vs-dataview3/3

const callerid = require("caller-id");

const LOOP = 1;


////////////////////////////////////////////////////////////////////////////////
////
/// Prep
//

  var native = new Int8Array(new Int16Array([1]).buffer)[0] == 1;

  function CustomView(buffer) {
    this.buffer = buffer;
    this.u8 = new Uint8Array(buffer);
    this.u32 = new Uint32Array(buffer);
  }

  CustomView.prototype.getUint32 = function (index) {
    return this.u32[(index/4)|0];
  }

  CustomView.prototype.setUint32 = function (index, value) {
    this.u32[(index/4)|0] = value;
  }

  CustomView.prototype.getUint32_2 = function (i, le) {
    if (le) {
      return (this.u8[i+3] << 24) | (this.u8[i+2] << 16) | (this.u8[i+1] << 8) | this.u8[i];
    } else {
      return (this.u8[i] << 24) | (this.u8[i+1] << 16) | (this.u8[i+2] << 8) | this.u8[i+3];
    }
  }

  CustomView.prototype.getUint32_3 = function (i, le) {
    if ((le == native) && (i & 7)) {
      return this.u32[i >> 2];
    } else if (le) {
      return (this.u8[i+3] << 24) | (this.u8[i+2] << 16) | (this.u8[i+1] << 8) | this.u8[i];
    } else {
      return (this.u8[i] << 24) | (this.u8[i+1] << 16) | (this.u8[i+2] << 8) | this.u8[i+3];
    }
  }

  CustomView.prototype.setUint32_2 = function (index, value) {
    this.u8[index] = (value) & 0xff;
    this.u8[index+1] = (value >> 8) & 0xff;
    this.u8[index+2] = (value >> 16) & 0xff;
    this.u8[index+3] = (value >> 24) &0xff;
  }

  var len = 1024* 20;
  var len2 = len/4;
  var a = new ArrayBuffer(len);
  var u32 = new Uint32Array(a);
  var dv = new DataView(a);
  var cv = new CustomView(a);


////////////////////////////////////////////////////////////////////////////////
////
/// Tests
//


function DataView_write()
{
    ops();
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      dv.setUint32(ii, ii);
    }
    ops(LOOP * len / 4);
}


function Uint8_write()
{
    ops();
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len2; ++ii) {
      u32[ii] = ii;
    }
    ops(LOOP * len2);
}


function DataView_read()
{
    ops();
    var sum=0;
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      sum += dv.getUint32(ii);
    }
    ops(LOOP * len / 4);
}


function Uint8_read()
{
    ops();	
    var sum=0;
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len2; ++ii) {
      sum += u32[ii];
    }
    ops(LOOP * len2);
}


function CustomView_write()
{
    ops();
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      cv.setUint32(ii, ii);
    }
    ops(LOOP * len / 4);
}


function CustomView_read()
{
    ops();	
    var sum=0;
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      sum += cv.getUint32(ii);
    }
    ops(LOOP * len / 4);
}


function CustomView_write2()
{
    ops();
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      cv.setUint32_2(ii, ii);
    }
    ops(LOOP * len / 4);
}


function CustomView_read2()
{
    ops();
    var sum=0;
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      sum += cv.getUint32_2(ii, true);
    }
    ops(LOOP * len / 4);
}


function CustomView_read3()
{	
    ops();
    var sum=0;
for (var repeat = 0; repeat < LOOP; ++repeat)
    for (var ii = 0; ii < len; ii+=4) {
      sum += cv.getUint32_3(ii, true);
    }
    ops(LOOP * len / 4);
}


function mult16()
{
    ops();
    var sum = 0;
    for (var i = 0; i < 1000000; ++i)
        sum += i * 16;
    ops(1000000);
}
function lsh4()
{
    ops();
    var sum = 0;
    for (var i = 0; i < 1000000; ++i)
        sum += i << 4;
    ops(1000000);
}


function test()
{
    ops();
    setTimeout(function tester() { ops(10); }, 1000);
}


DataView_write();
//Uint8_write();
DataView_read();
Uint8_write();
Uint8_read();
CustomView_write();
CustomView_read();
CustomView_write2();
CustomView_read2();
CustomView_read3();
mult16();
lsh4();
test(); //do this last since ops() is single-threaded


////////////////////////////////////////////////////////////////////////////////
////
/// Helpers
//

function elapsed_sec(sttime)
{
    const NS_PER_SEC = 1e9;
    var diff = process.hrtime(sttime);
//    return diff[0] * NS_PER_SEC + diff[1];
    return diff[0] + diff[1] / NS_PER_SEC;
}

function ops(count)
{
    if (!count) return ops.sttime = process.hrtime();
    var persec = count / elapsed_sec(ops.sttime);
    console.log("%s: %s ops/sec", callerid.getData().functionName, commas(persec));
}

//display commas for easier debug:
//NOTE: need to use string, otherwise JSON.stringify will remove commas again
function commas(val)
{
//number.toLocaleString('en-US', {minimumFractionDigits: 2})
    return val.toLocaleString();
}


//EOF
