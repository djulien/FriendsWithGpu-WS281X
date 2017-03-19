//functions to allow JavaScript synchronous coding style
//Copyright (c) 2016-2017 Don Julien
//Can be used for non-commercial purposes


//step thru generator function:
//allows synchronous (blocking) coding style
const blocking =
module.exports.blocking =
function blocking(gen)
{
	if (typeof gen == "function") //invoke generator if not already
    {
        setImmediate(function() { blocking(gen()); }); //avoid hoist errors
        return;
    }
//    process.stdin.pause(); //don't block process exit while not waiting for input
//    var retval;
	for (;;)
	{
		var status = gen.next(blocking.retval); //send previous value to generator
//		if (!status.done && (typeof status.value == "undefined")) continue; //cooperative multi-tasking
		if (typeof status.value == "function") //caller wants manual step
        {
            blocking.retval = status.value(gen); //remember latest ret val
            return; //caller wants to execute before next step
        }
        blocking.retval = status.value; //remember latest ret val, pass to next step
		if (status.done) return blocking.retval;
	}
}


//wake up from synchronous sleep:
const wait =
module.exports.wait =
function wait(delay)
{
    delay *= 1000; //sec -> msec
    if (delay > 1) return setTimeout.bind(null, blocking, delay);
}

//eof
