#!/usr/bin/env node

'use strict';
require('colors').enabled = true;
const os = require('os');
const cluster = require('cluster');
const Semaphore = require('posix-semaphore');
const shm = require('shm-typed-array');

const cpu = new Semaphore('mySema-cpu', { xdebug: true, xsilent: true, value: os.cpus().length - 1});
const idle = new Semaphore('mySema-idle', { xdebug: true, xsilent: true, value: os.cpus().length - 1});
if (cluster.isMaster) parentProcess();
else if (cluster.isWorker) childProcess();


function parentProcess()
{
//    sema.acquire();

    const bufParent = shm.create(4096);
console.log("parent hi, key %s, #cpu %d".green_lt, bufParent.key.toString(16), os.cpus().length);
    bufParent.write('hi there'.pink_lt);
//    setTimeout(function() { sema.release(); }, 5000);

    for (var w = 0; w < 6; ++w) cluster.fork({ SHM_KEY: bufParent.key, EPOCH: elapsed() }); //launch bkg wkers

//    setTimeout(() => { child.kill('SIGINT') }, 10000);
}


function childProcess()
{
    const EPOCH = process.env.EPOCH;
//    const sema = new Semaphore('mySemaphore', { xdebug: true, xsilent: true });
    const shmKey = parseInt(process.env.SHM_KEY);
    const bufChild = shm.get(shmKey);
console.log(`child '${process.pid}' start, elapsed ${elapsed(EPOCH)}`.green_lt);
  
    cpu.acquire();
console.log(`child '${process.pid}' acq @${elapsed(EPOCH)}`.cyan_lt);
    console.log(bufChild.toString());
setTimeout(function() { console.log(`child '${process.pid}' rel @${elapsed(EPOCH)}`.blue_lt); cpu.release(); process.exit(0); }, 3000);
}


//high-res elapsed time:
//NOTE: unknown epoch; used for relative time only, abs time not needed
function elapsed(epoch)
{
    var parts = process.hrtime();
    var now = parts[0] + parts[1] / 1e9; //sec, with nsec precision
    return now - (epoch || 0);
}


//eof
