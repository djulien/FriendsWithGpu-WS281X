#!/usr/bin/env node
//color spaces

'use strict';
require('colors').enabled = true; //console output colors; allow bkg threads to use it also

const hsv2rgb_360_100_100 =
module.exports.hsv2rgb_360_100_100 =
function hsv2rgb_360_100_100(h, s, v)
{
    return hsv2rgb(h / 360, s / 100, v / 100);
}


//convert color space:
//HSV is convenient for color selection during fx gen
//display hardware requires RGB
const hsv2rgb =
module.exports.hsv2rgb =
function hsv2rgb(h, s, v)
//based on sample code from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
{
    h *= 6; //[0..6]
    const segment = h >>> 0; //(long)hh; //convert to int
    const angle = (segment & 1)? h - segment: 1 - (h - segment); //fractional part
//NOTE: it's faster to do the *0xff >>> 0 in here than in toargb
    const p = ((v * (1.0 - s)) * 0xff) >>> 0;
    const qt = ((v * (1.0 - (s * angle))) * 0xff) >>> 0;
//redundant    var t = (v * (1.0 - (s * (1.0 - angle))) * 0xff) >>> 0;
    v = (v * 0xff) >>> 0;

    switch (segment)
    {
        default: //h >= 1 comes in here also
        case 0: return toargb(v, qt, p); //[v, t, p];
        case 1: return toargb(qt, v, p); //[q, v, p];
        case 2: return toargb(p, v, qt); //[p, v, t];
        case 3: return toargb(p, qt, v); //[p, q, v];
        case 4: return toargb(qt, p, v); //[t, p, v];
        case 5: return toargb(v, p, qt); //[v, p, q];
    }
}


//convert (r, g, b) to 32-bit ARGB color:
const toargb =
module.exports.toargb =
function toargb(r, g, b)
{
    return (0xff000000 | (r << 16) | (g << 8) | b) >>> 0; //force convert to uint32
}


//CLI unit test:
if (!module.parent)
{
    console.log("running CLI test".green_lt);
    [hsv(0), hsv(120), hsv(240), hsv(360), hsv(120, 100, 0), hsv(240, 0, 100), hsv(99, 50, 50)].forEach((hsv) =>
    {
        console.log(`hsv(${JSON.stringify(hsv)}) = rgb(0x${hsv2rgb_360_100_100(hsv[0], hsv[1], hsv[2]).toString(16)})`.blue_lt);
    });
    console.log("done".green_lt);

    function hsv(h, s, v) { return [ifdef(h, 0), ifdef(s, 100), ifdef(v, 100)]; }
    function ifdef(val, def) { return (typeof val != "undefined")? val: def; }
}

//eof
