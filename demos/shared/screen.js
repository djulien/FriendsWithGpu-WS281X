//get RPi screen config info:
//query hdmi timing settings, device tree overlays

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output colors
const fs = require('fs');
const glob = require('glob');
const scrres = require('screenres');
const WebGL = require('node-webgl'); //NOTE: for RPi must be lukaaash/node-webg or djulien/node-webgl version, not mikeseven/node-webgl version
//const {SPEED, NUM_UNIV, UNIV_LEN} = require('./config');
//const cfg = require('./package.json');
const {debug} = require('./debug');

Math.round_even = function(val) { return this.round(val) & ~1; } //even values seem to work better for screen sizes

const Screen = 
module.exports.Screen = {};

//get actual display res:
//on RPi assume: screen res set as desired (running in full screen mode), ssh can use entire screen
var [wndw, wndh] = scrres.get();
if (!isRPi()) //non-RPi (dev mode only): leave a little space on screen for other stuff
{
//TODO: enable scroll bars and use full size
    wndh -= 64; //kludge: account for top and bottom bars in Linux (since we're not in full screen mode)
//try to preserve 4:3 aspect ratio on largest window that will fit on screen:
//include WS281X overscan in calculations
    wndh = Math.min(Math.round_even(wndw * 24/23.25 * 3/4), wndh);
    wndw = Math.min(Math.round_even(wndh * 4/3 * 23.25/24), wndw);
}

const document = WebGL.document(); //create window to simulate browser
//var canvas = {width: scrw, height: scrh};
const canvas = Screen.canvas = document.createElement("ws281x-canvas", wndw, wndh); //NOTE: name needs to include "canvas"
//gl = canvas.getContext("experimental-webgl");

//export values rather than callable functions (won't change without reboot):
//give caller drawable window size rather than full screen size
Screen.width = wndw; //canvas.width;
Screen.height = wndh; //canvas.height;
Screen.isRPi = isRPi();
Screen.gpio = toGPIO();
Object.assign(Screen, timing());
debug(10, `screen ${scrres.get().join("x")}, wnd ${[wndw, wndh].join("x")}, canvas ${[canvas.width, canvas.height].join("x")}, isRPi? ${Screen.isRPi}, gpio? ${Screen.gpio}`.blue_lt);


///////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//

//check if this is RPi:
function isRPi()
{
    if (isRPi.hasOwnProperty('cached')) return isRPi.cached;
//TODO: is there a better way to check?
    return isRPi.cached = fs.existsSync("/boot/config.sys");
}


//check if video is redirected to GPIO pins:
function toGPIO()
{
    if (toGPIO.hasOwnProperty('cached')) return toGPIO.cached;
//check if various device tree overlays are loaded:
//dpi24 or vga666 send video to GPIO pins
    var dpi24 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/dpi24*");
    if (dpi24 && dpi24.length) return toGPIO.cached = 8+8+8;
    var vga666 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga666*");
    if (vga666 && vga666.length) return toGPIO.cached = 6+6+6;
    var vga565 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga565*");
    if (vga565 && vga565.length) return toGPIO.cached = 5+6+5;
//console.log("dpi", dpi);
    return toGPIO.cached = false;
}


//get RPi display settings:
//TODO: use userland tvservice to read from memory
//for now, just read from config.txt
function timing(path)
{
    if (timing.hasOwnProperty('cached')) return timing.cached;
//console.log("re", hdmi_re.toString());
    if (!path) path = "/boot/config.txt";
    var cfg = fs.existsSync(path)? fs.readFileSync(path).toString(): "";
    if (!cfg)
    {
//create dummy config values to approximate desired RPi timing with current display settings:
//NOTE: use window size rather than screen size so it works with a windowed dev env
        var hblank =
        {
            total: Math.round(canvas.width * 24/23.25 / 48), ///~ 2% of total horizontal scan time
            get front() { return Math.floor(this.total / 4); }, ///front door = 25% of horiz blank
            get sync() { return this.total - this.front - this.back; }, //sync = 25% of horiz blank
            get back() { return Math.floor(this.total / 2); }, ///back door = 50% of horiz blank
        };
        var vblank =
        {
            total: Math.round(canvas.height * 24/23 / 24), ///~ 4% of total vertical scan time
            get front() { return Math.floor(this.total / 4); }, ///front door = 25% of vert blank
            get sync() { return this.total - this.front - this.back; }, //sync = 25% of vert blank
            get back() { return Math.floor(this.total / 2); }, ///back door = 50% of vert blank
        };
        debug("RPi cfg file '%s' not found, using fake values".yellow_lt, path);
        cfg =
        [
            "#hdmi_timings=  1 2 3 4  5     6 7  8  9 10 11 12 13 14 15 16   17",
//            "hdmi_timings=1488 0 12 12 24   1104 0 12 12 24    0 0 0 30 0 50000000 1",
//            "hdmi_timings=1504 0 8 8 16  1104 0 12 12 24  0 0 0 30 0 50000000 1",
            `hdmi_timings=${canvas.width} 0 ${hblank.front} ${hblank.sync} ${hblank.back}  ${canvas.height} 0 ${vblank.front} ${vblank.sync} ${vblank.back}  0 0 0 30 0 50000000 1`,
        ].join("\n"); //dummy values for non-RPi testing
//        cfg = cfg.replace(/#.*$/gm, ""); //strip comments
//        cfg = cfg.replace(/^(?=\n)$|^\s+|\s+$|\n\n+/gm, ""); //strip blank lines; https://stackoverflow.com/questions/31412765/regex-to-remove-white-spaces-blank-lines-and-final-line-break-in-javascript
//        debug(cfg./*replace(/000000\s+(\d+)\s*$/, "MHz $1").*/yellow_lt); //improve readability on clock freq?
    }
    cfg = cfg.replace(/\s*#.*$/gm, ""); //strip comments
//console.log("settings", settings);
//    var hdmi = settings.match(/\nhdmi_timings\s*=\s*(\d+)\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)\s+
    var hdmi, match;
    const hdmi_settings = new RegExp("^\s*hdmi_timings\\s*=\\s*" + new Array(17+1).join("(\\d+)\\s+").slice(0, -1) + "*$", "gm");
    while (match = hdmi_settings.exec(cfg))
    {
        hdmi = match; //remember latest one; not strictly correct (might have excluded sections), but close enough for now
        debug(match[0].blue_lt);
    }
//    console.log("found %j", hdmi);
    return timing.cached = {horiz: {disp: +hdmi[1], front: +hdmi[3], sync: +hdmi[4], back: +hdmi[5], res: +hdmi[1] + +hdmi[3] + +hdmi[4] + +hdmi[5]}, vert: {disp: +hdmi[6], front: +hdmi[8], sync: +hdmi[9], back: +hdmi[10], res: +hdmi[6] + +hdmi[8] + +hdmi[9] + +hdmi[10]}, fps: +hdmi[14], clock: +hdmi[16], intlv: +hdmi[17]};
}

//eof
