#!/usr/bin/env node
//de-glob a file spec

"use strict";
require("colors");
const glob = require("glob");
const pathlib = require("path");
//const {execSync} = require('child_process');


const deglob =
module.exports =
function deglob(pattern, maxm)
{
    if (!maxm) maxm = 1;
    if (!pattern) throw "deglob: no pattern specified".red_lt;
//    if (pattern.indexOf("*") == -1) return pattern;
    var files = glob.sync(pattern);
    if (!(files || []).length) throw `deglob: No files match '${pattern}'.`.red_lt;
    if (files.length > maxm) throw `deglob: only wanted ${maxm}, but found ${files.length} matching '${relpath(process.cwd(), pattern)}':\n`.red_lt + files.map(path => { return relpath(pattern, path); }).join("\n").yellow_lt;
    return files; //mult_okay? files: files[0];
}


function relpath(pattern, filepath)
{
    return pathlib.relative(pathlib.dirname(pattern), filepath);
}


//CLI for testing:
if (!module.parent)
{
    console.log("running CLI test".green_lt);
    var args = Array.from(process.argv).slice(2);
    var maxm = args.slice(-1)[0];
    if (isNaN(maxm)) maxm = 0;
    else args.pop();
    if (args.length > 1)
        console.log("Did you intend to put quotes around glob patterns?".yellow_lt);
//    var output = execSync("history");

    for (var i = 0; i < process.argv.length; ++i)
        console.log("arg[%d/%d]: '%s'".blue_lt, i, process.argv.length, process.argv[i]);

    var files = deglob((args.length > 1)? "{" + args.join(",") + "}": args[0], maxm);
    console.log("%d file(s) matching '%s':".green_lt, files.length, process.argv[2]);
    files.forEach((file, inx, all) => { console.log(`[${inx}/${all.length}]: '${file}'`.cyan_lt); });
}

//EOF
