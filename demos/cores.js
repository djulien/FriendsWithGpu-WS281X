#!/usr/bin/env node

'use strict'; //find bugs easier
require('colors').enabled = true; //for console output (incl bkg threads)
//const {Worker} = require('webworker-threads');
const os = require('os');
const cluster = require('cluster');
const JSON = require('circular-json'); //CAUTION: replace std JSON with circular-safe version
const {debug} = require('./shared/debug');
const {AtomicAdd, shmbuf} = require('gpu-friends-ws281x');

const SHMKEY = 0xbeef; //make value easy to find (for debug)
var shm = new Uint32Array(shmbuf(SHMKEY, 4 * Uint32Array.BYTES_PER_ELEMENT));

const NUM_CPU = os.cpus().length;
const AutoRestart = false; //true;

if (cluster.isMaster)
{
    shm.fill(0);
    for (var w = 0; w < 6; ++w) cluster.fork(); //launch bkg wkers
    console.log(`master: atomic was ${AtomicAdd(shm, 0, 12)}`.yellow_lt);
    console.log(`master: atomic is now ${AtomicAdd(shm, 0, 0)}`.yellow_lt);

    cluster.on('online', (wker) =>
    {
        debug(`worker# ${wker.id} pid '${wker.process.pid}' started`.green_lt);
    });
    cluster.on('disconnect', (wker) =>
    {
        debug(`worker# ${wker.id} pid '${wker.process.pid}' disconnected`.cyan_lt);
    });
    cluster.on('message', (wker, msg, handle) =>
    {
//??        if (arguments.length == 2) { handle = msg; msg = wker; wker = undefined; } //?? shown in example at https://nodejs.org/api/cluster.html#cluster_event_message_1
//        var pid = wker.process._handle.pid; //NOTE: pid is in different place during this callback
debug(`got reply from wker pid '${wker.process._handle.pid}'`.blue_lt);
        wker.send({quit: true});
    });
    cluster.on('exit', (wker, code, signal) =>
    {
//        delete this.wkers[wker.process.pid];
//    var want_restart = (Object.keys(wkers).length < numWkers);
        debug(`worker pid '${wker.process.pid}' died (${code || signal}), restart? ${AutoRestart}`.red_lt);
        if (AutoRestart) cluster.fork(); //{UNIV_UNIV, UNIV_LEN}); //preserve settings
    });
//    debug(JSON.stringify(cluster.workers));
    for (var w in cluster.workers)
    {
        var wker = cluster.workers[w];
        wker.send({fibo: 30});
    }
}
else
{
    console.log(`child pid '${process.pid}': atomic was ${AtomicAdd(shm, 0, -1)}`.yellow_lt);
    console.log(`child pid '${process.pid}': atomic is now ${AtomicAdd(shm, 0, 0)}`.yellow_lt);

    process.on('message', (msg) =>
    {
        debug(`got msg ${JSON.stringify(msg)} for bkg wker pid'${process.pid}'`.blue_lt);
        if (msg.fibo) { process.send({reply: fibo(msg.fibo)}); debug(`pid '${process.pid}' fibo reply`.blue_lt); return; }
        if (msg.quit) { process.send({quit: true}); debug(`pid '${process.pid}' quit reply`.blue_lt); process.exit(0); return; }
        throw `pid '${process.pid}' unhandled msg ${JSON.stringify(msg)}`.red_lt;
    });
}


function fibo(n) { return (n < 2)? n: fibo(n - 1) + fibo(n - 2); }

//eof
