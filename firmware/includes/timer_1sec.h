////////////////////////////////////////////////////////////////////////////////
////
/// Virtual 1 second Timer; uses counter loop with Timer 1 50msec
//

#ifndef _TIMER_1SEC_H
#define _TIMER_1SEC_H

#include "timer_50msec.h"

//#warning CYAN_MSG "TODO: find why this is 192 instead of 20"
#define TMR1_LOOP_1sec  (ONE_SEC / U16FIXUP(50 msec)) //#Timer 1 ticks per second
#if TMR1_LOOP_1sec != 20 //paranoid
 #error RED_MSG "[ERROR] 1 sec loop preset wrong" TOSTR(TMP1_LOOP_1sec)
//kludge:
 #undef TMR1_LOOP_1sec
 #define TMR1_LOOP_1sec  20
#endif
BANK0 volatile uint8_t loop_1sec;


#ifdef debug
    volatile AT_NONBANKED(0) uint32_t tmrx_onesec_debug; //= ONE_SEC;
    volatile AT_NONBANKED(0) uint32_t tmrx_50msec_debug; //= 50 msec;
    volatile AT_NONBANKED(0) uint8_t tmrx_preset_debug; //= (ONE_SEC / (50 msec));
    volatile AT_NONBANKED(0) uint8_t tmrx_loop_debug; //= TMR1_LOOP_1sec;
 INLINE void tmr_1sec_debug(void)
 {
    debug(); //incl prev debug first
    tmrx_onesec_debug = ONE_SEC;
    tmrx_50msec_debug = 50 msec; //SDCC bad: did a sign extend
    tmrx_50msec_debug = U16FIXUP(50 msec);
    tmrx_preset_debug = (ONE_SEC / U16FIXUP(50 msec));
    tmrx_loop_debug = TMR1_LOOP_1sec;
 }
 #undef debug
 #define debug()  tmr_1sec_debug()
#endif


#ifndef init
 #define init() //initialize function chain
#endif

//;initialize timer 1:
INLINE void init_tmr_1sec(void)
{
	init(); //prev init first
    loop_1sec = TMR1_LOOP_1sec;
}
#undef init
#define init()  init_tmr_1sec() //function chain in lieu of static init


#ifndef on_tmr_1sec
 #define on_tmr_1sec() //initialize function chain
#endif

//NOTE: need macros here so "return" will exit caller
//    if (loop_1sec) return //SDCC generates extra instr :(
#define on_tmr_1sec_check()  \
{ \
    WREG = loop_1sec; \
    if (!ZERO) return; \
}

INLINE void on_1sec_tick(void)
{
	on_tmr_1sec(); //prev event handlers first
    LABDCL(0xE0);
    loop_1sec += TMR1_LOOP_1sec; //use += in case it changed again
//    WREG = TMR1_LOOP_1sec; //use += in case it changed again
//    loop_1sec += WREG;
}
#undef on_tmr_1sec
#define on_tmr_1sec()  on_1sec_tick() //event handler function chain


//#ifndef on_tmr_50msec
// #define on_tmr_50msec() //initialize function chain
//#endif

INLINE void on_tmr_1sec_tick(void)
{
	on_tmr_50msec(); //prev event handlers first
    --loop_1sec;
}
#undef on_tmr_50msec
#define on_tmr_50msec()  on_tmr_1sec_tick() //event handler function chain


#endif //ndef _TIMER1_H
//eof
