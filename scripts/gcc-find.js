#!/usr/bin/env node

"use strict";
//require("colors").enabled = true; //NOTE: assumes global install
const fs = require("fs");
const {exec} = require("child_process");

//ANSI color codes:
//https://en.wikipedia.org/wiki/ANSI_escape_code
const Colors =
{
    RED_LT: "\x1b[1;31m", //too dark: "\e[0;31m"
    GREEN_LT: "\x1b[1;32m",
    YELLOW_LT: "\x1b[1;33m",
    BLUE_LT: "\x1b[1;34m",
    MAGENTA_LT: "\x1b[1;35m",
    CYAN_LT: "\x1b[1;36m",
    GRAY: "\x1b[0;37m",
    NORMAL: "\x1b[0m",
};
for (const [key, val] of Object.entries(Colors))
    Object.defineProperty(String.prototype, key,
    {
        get() { return val + this + Colors.NORMAL; },
    });


var lookfor = Array.from(process.argv).slice(2);
//console.log(`looking for: ${lookfor}`.GREEN_LT);

//ask compiler for list of folders:
//https://stackoverflow.com/questions/344317/where-does-gcc-look-for-c-and-c-header-files
//exec("echo | g++ -E -Wp,-v -", (err, stdout, stderr) =>
exec("echo | `gcc -print-prog-name=cc1plus` -v", (err, stdout, stderr) =>
{
    if (err) return console.error(`exec error: ${err}`.RED_LT);
    console.log(`stdout: ${stdout}`.BLUE_LT);
    console.log(`stderr: ${stderr}`.CYAN_LT);
    var dirs = stderr./*replace(/^\s*#/gm, "").*/split(/\s*\n\s*/);
//console.log(`look in dirs ${dirs}`.YELLOW_LT);
    lookfor.forEach(file =>
    {
//console.log(`looking for '${file}' ...`.BLUE_LT);
        var found = false;
        dirs.forEach(dir =>
        {
            if (dir.indexOf('/') == -1) return; //continue; //probably not a directory
//console.log(`  looking in '${dir}' ...`.BLUE_LT);
            var path = dir.replace(/^\s+|\s+$/g, "") + "/" + file;
            if (!fs.existsSync(path)) return; //continue;
            console.log(path.GREEN_LT);
            found = true;
        })
        if (!found) console.log(`'${file}' not found`.RED_LT);
    });
});

//EOF
