//get screen config info:
//query hdmi timing settings, device tree overlays

'use strict'; //find bugs easier
require('colors'); //for console output
const fs = require('fs');
const glob = require('glob');
const WebGL = require('node-webgl'); //NOTE: for RPi must be lukaaash/node-webg or djulien/node-webgl version, not mikeseven/node-webgl version
const document = WebGL.document();
//const {SPEED, NUM_UNIV, UNIV_LEN} = require('./config');
//const cfg = require('./package.json');

const Screen =
module.exports.Screen = timing();

Screen.gpio = toGPIO();

//get OpenGL display info:
//set actual display res
var canvas = document.createElement("ws281x-canvas"); //name needs to include "canvas"
//gl = canvas.getContext("experimental-webgl");
Screen.width = canvas.width;
Screen.height = canvas.height;


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//


//check if video sent to GPIO pins:
function toGPIO()
{
    if (toGPIO.hasOwnProperty('cached')) return toGPIO.cached;
//check if various device tree overlays are loaded:
//dpi24 or vga666 send video to GPIO pins
    var dpi24 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/dpi24*");
    if (dpi24 && dpi24.length) return toGPIO.cached = 24;
    var vga666 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga666*");
    if (vga666 && vga666.length) return toGPIO.cached = 18;
//TODO: is there a vga565 also?
//console.log("dpi", dpi);
    return false;
}


//get RPi display settings:
//TODO: use userland tvservice to read from memory
//for now, read from config.txt
//hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1
function timing(path)
{
    if (timing.hasOwnProperty('cached')) return timing.cached;
    const hdmi_re = new RegExp("^hdmi_timings\\s*=\\s*" + new Array(16+1).join("(\\d+)\\s+") + "(\\d+)\\s*$", "gm");
//console.log("re", hdmi_re.toString());
    if (!path) path = "/boot/config.txt";
    var cfg = fs.existsSync(path)? fs.readFileSync(path).toString(): "";
    if (!cfg)
    {
        console.log("'%s' not found, using dummy values".red_lt, path);
        cfg =
        [
            "hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1",
            "#hdmi_timings=1 2 3 4 5   6 7 8 9 10    11 12 13 14 15 16 17",
        ].join("\n"); //dummy file for testing
    }
//console.log("settings", settings);
//    var hdmi = settings.match(/\nhdmi_timings\s*=\s*(\d+)\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)\s+
    var hdmi, match;
    while (match = hdmi_re.exec(cfg)) hdmi = match; //remember latest one; not strictly correct, but close enough
//    console.log("found %j", hdmi);
    return timing.cached = {horiz: {disp: hdmi[1], front: hdmi[3], sync: hdmi[4], back: hdmi[5], res: 1 * hdmi[1] + 1 * hdmi[3] + 1 * hdmi[4] + 1 * hdmi[5]}, vert: {disp: hdmi[6], front: hdmi[8], sync: hdmi[9], back: hdmi[10], res: 1 * hdmi[6] + 1 * hdmi[8] + 1 * hdmi[9] + 1 * hdmi[10]}, fps: hdmi[14], clock: hdmi[16], intlv: hdmi[17]};
}

//eof
