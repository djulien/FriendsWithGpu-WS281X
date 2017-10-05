#!/usr/bin/env node
//Private config settings for shell scripts

'use strict';

const CFG =
{
//TODO: put real values here, but don't check-in!
    RPi_user: "MY USERID HERE",
    RPi_pass: "MY PASSWORD HERE",
    RPi_addr: "IP ADDRESS HERE",
    RPi_folder: "DEST FOLDER HERE",
};

for (var item in CFG)
    console.log(item + "=" + CFG[item]);

//eof
