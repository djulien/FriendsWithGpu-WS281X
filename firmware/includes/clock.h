////////////////////////////////////////////////////////////////////////////////
////
/// Clock defs
//

#ifndef _CLOCK_H
#define _CLOCK_H

//convenience macros (for readability):
//CAUTION: might need parentheses, depending on usage
#define usec
#define msec  *1000
#define sec  *1000000
#define MHz  *1000000
//#define K  *1024 //needed sooner


//instruction timing:
//;NOTE: these require 32-bit arithmetic.
//Calculations are exact for small numbers, but might wrap or round for larger numbers (needs 32-bit arithmetic at compile time).
//;misc timing consts:
#define ONE_MSEC  (1 msec) //1000
#define ONE_SEC  (1 sec) //1000000
//#define MHz(clock)  ((clock)/ONE_SEC)
#define INSTR_CYCLES  4 //#clock cycles per instr


//define ext clock, int osc freq:
#ifdef PIC16X //extended instr set (faster PICs)
 #define MAXINTOSC_FREQ  (32 MHz) //uses 4x PLL
#else
 #define MAXINTOSC_FREQ  (8 MHz)
#endif

#ifdef CLOCK_FREQ //ext clock will be present
// #define UseIntOsc  (CLOCK_FREQ == MAXINTOSC_FREQ)
#else //use int osc @max freq if ext clock not present
// #define UseIntOsc  TRUE
 #define CLOCK_FREQ  MAXINTOSC_FREQ
#endif
//#define CLOCK_FREQ  (eval(DEVICE)##_FREQ)
//#define CLOCK_FREQ  CLOCK_FREQ_eval(DEVICE) //(eval(DEVICE)##_FREQ)
//#define CLOCK_FREQ_eval(device)  eval(device)##_FREQ
//#define uSec2Instr(usec, clock)  ((usec) * InstrPerUsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
#define uSec2Instr(usec, clock)  ((usec) * InstrPerMsec(clock) / ONE_MSEC) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
//use this one for larger values:
#define mSec2Instr(msec, clock)  ((msec) * InstrPerMsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
//#define Instr2uSec(instr, clock)  ((instr) / InstrPerUsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
//NOTE: this only seems to work up to 32 usec:
#define Instr2uSec(instr, clock)  ((instr) * ONE_MSEC / InstrPerMsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
#define Instr2mSec(instr, clock)  ((instr) / InstrPerMsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors

#define InstrPerSec(clock)  ((clock) / INSTR_CYCLES)  //;#instructions/sec at specified clock speed; clock freq is always a multiple of INSTR_CYCLES
#define InstrPerMsec(clock)  (InstrPerSec(clock) / ONE_MSEC)  //;#instructions/msec at specified clock speed; not rounded but clock speeds are high enough that this doesn't matter
#define InstrPerUsec(clock)  (InstrPerSec(clock) / ONE_SEC) //CAUTION: usually this one is fractional, so it's not accurate with integer arithmetic

//#if CLOCK_FREQ == 32 MHz
// #if Instr2uSec(120, CLOCK_FREQ) != 15
//  #warning YELLOW_MSG "Instr2uSec bad arithmetic; taking shortcut"
//  #undef Instr2uSec
//  #define Instr2uSec(usec, clock)  ((usec) / 8)
// #else
//  #warning YELLOW_MSG "is okay?"
// #endif
//#endif


//Int osc configuration:
//Prescalar and preset values are defined below based on desired clock speed
#ifdef PIC16X //extended/faster PIC
 #define PLL  IIF(CLOCK_FREQ > 8 MHz, 4, 1) //need PLL
//BoostC doesn't like IIF in #pragma config
// #if CLOCK_FREQ > (8 MHz)
//  #define PLL  4 //need PLL for faster speeds
// #else
//  #define PLL  1
// #endif
 #define IntOsc_PrescalarOffset  (8-1) //kludge: 2 values for LFINTOSC; skip 1
 #define IntOsc_MaxPrescalar  15
// #define UseIntOsc  (CLOCK_FREQ <= 8 MHz * PLL) //PIX16X //int osc is fast enough (with PLL)
#else //non-extended (slower) processors
 #define PLL  1 //no PLL available
 #define IntOsc_PrescalarOffset  0
 #define IntOsc_MaxPrescalar  7
#endif
//#define PLL  IIF(CLOCK_FREQ > 8 MHz, 4, 1) //need PLL; BoostC doesn't like IIF in #pragma config
//#if CLOCK_FREQ > (8 MHz)
// #define PLL  4
//#else
// #define PLL  1
//#endif
#define UseIntOsc  (CLOCK_FREQ <= MAXINTOSC_FREQ) //8 MHz * PLL) //PIX16X //int osc is fast enough (with PLL)
//#if CLOCK_FREQ > 8 MHz * PLL
// #define UseIntOsc  FALSE //too fast for int osc
//#else
// #define UseIntOsc  TRUE
//#endif
//#define SCS_INTOSC  1 //0 //use config, which will be set to int osc (PLL requires 0 here)

#if PLL != 1
 #define IFPLL(stmt)  stmt
#else
 #define IFPLL(stmt)
#endif


#if 0
//kludge: #warning doesn't reduce macro values, so force it here (mainly for debug/readability):
#if CLOCK_FREQ == 20 MHz
 #if UseIntOsc
  #define CLOCK_FREQ_tostr  "20 MHz (int)"
 #else
  #define CLOCK_FREQ_tostr  "20 MHz (ext)"
 #endif
#elif CLOCK_FREQ == 32 MHz
 #if UseIntOsc
  #define CLOCK_FREQ_tostr  "32 MHz (int, with PLL)"
 #else
  #define CLOCK_FREQ_tostr  "32 MHz (ext, with PLL)"
 #endif
#elif CLOCK_FREQ == 18432000
 #if UseIntOsc
  #define CLOCK_FREQ_tostr  "18.432 MHz (int)"
 #else
  #define CLOCK_FREQ_tostr  "18.432 MHz (ext)"
 #endif
#else
 #define CLOCK_FREQ_tostr  CLOCK_FREQ UseIntOsc
#endif
//#endif
//#endif
//#endif
#endif


#ifdef PIC16X //extended instr set
 #warning BLUE_MSG "[INFO] Compiled for " TOSTR(DEVICE) " running at " TOSTR(CLOCK_FREQ) " with extended instruction set."
// #define GPRAM_LINEAR_START  0x2000
// #warning BLUE_MSG "device "DEVICE" has ext instr set, "GPRAM_LINEAR_SIZE" ram"
// #define RAM(device)  device##_RAM
/////// #define _GPRAM_LINEAR_SIZE(device)  (concat(device, _RAM) - 16)
//#define GPRAM_LINEAR_SIZE  ((4 * 0x50) + 0x30) //384 excludes 16 bytes non-banked GP RAM
//#define GPRAM_LINEAR_SIZE  ((12 * 0x50) + 0x30) //1024 excludes 16 bytes non-banked GP RAM
//#define GPRAM_LINEAR_SIZE  ((1 * 0x50) + 0x20) //128 excludes 16 bytes non-banked GP RAM
//#define GPRAM_LINEAR_SIZE  ((3 * 0x50)) //256 excludes 16 bytes non-banked GP RAM
#else
 #warning BLUE_MSG "[INFO] Compiled for " TOSTR(DEVICE) " running at " TOSTR(CLOCK_FREQ) " with NON-extended instruction set."
#endif


#define TimerPreset(duration, overhead, which, clock)  (0 - rdiv(uSec2Instr(duration, clock) + overhead, which##_Prescalar))

#ifndef LFINTOSC_FREQ
 #define LFINTOSC_FREQ  31250 //31.25 KHz LFINTOSC
#endif

//#define IntOsc_MinPrescalarSize  0 //smallest configurable Int Osc prescalar bit size; smallest prescalar is 1:1; 1:1 is actually a special case (LFINTOSC)
//#define IntOsc_MaxPrescalarSize  7 //largest configurable Int Osc prescalar bit size; largest prescalar is 1:128
//#define Timer0_PrescalarSize(range, clock)  NumBits(uSec2Instr(range, clock))-8) //;bit size of prescalar
//#define Timer0_Prescalar(clock)  Prescalar(Timer0_Range, clock) //;gives a Timer0 range of Timer0_Range at the specified clock frequency
//#define IntOsc_Prescalar(clock)  (NumBits8(InstrPerSec(clock) / LFINTOSC_FREQ) + IntOsc_PrescalarOffset) //;Int osc prescalar
#define IntOsc_Prescalar(clock)  MIN(NumBits8(InstrPerSec(clock) / LFINTOSC_FREQ) + IntOsc_PrescalarOffset,  IntOsc_MaxPrescalar) //;Int osc prescalar

//OSCCON config:
//Sets internal oscillator frequency and source.
#define MY_OSCCON(clock)  \
(0 \
	| (IntOsc_Prescalar((clock) / PLL) /*<< IRCF0*/ * _IRCF0) /*set clock speed; bump up to max*/ \
	| IIF(UseIntOsc /*&& (PLL == 1)*/, 0, /*1 << SCS0*/ _SCS0) /*;use CONFIG clock source (ext clock), else internal osc; NOTE: always use int osc; if there's an ext clock then it failed*/ \
)


#ifdef debug
    volatile AT_NONBANKED(0) uint32_t clock_freq_debug; //= CLOCK_FREQ;
    volatile AT_NONBANKED(0) uint32_t max_intosc_debug; //= MAXINTOSC_FREQ;
    volatile AT_NONBANKED(0) uint8_t intosc_prescalar_debug; //= IntOsc_Prescalar(CLOCK_FREQ);
    volatile AT_NONBANKED(0) uint8_t use_intosc_debug; //= UseIntOsc;
    volatile AT_NONBANKED(0) uint8_t pll_debug; //= PLL;
    volatile AT_NONBANKED(0) uint8_t osccon_debug; //= MY_OSCCON(CLOCK_FREQ);
    volatile AT_NONBANKED(0) uint32_t instrpersec_debug;
    volatile AT_NONBANKED(0) uint32_t instrpermsec_debug;
    volatile AT_NONBANKED(0) uint32_t instrperusec_debug;
    volatile AT_NONBANKED(0) uint32_t usec2instr_debug;
    volatile AT_NONBANKED(0) uint32_t msec2instr_debug;
    volatile AT_NONBANKED(0) uint32_t instr2usec_debug;
    volatile AT_NONBANKED(0) uint32_t instr2msec_debug;
 INLINE void clock_debug(void)
 {
    debug(); //incl prev debug
    clock_freq_debug = CLOCK_FREQ;
    max_intosc_debug = MAXINTOSC_FREQ;
    intosc_prescalar_debug = IntOsc_Prescalar(CLOCK_FREQ);
    use_intosc_debug = UseIntOsc;
    pll_debug = PLL;
    osccon_debug = MY_OSCCON(CLOCK_FREQ);
    instrpersec_debug = InstrPerSec(CLOCK_FREQ); //8M @ 8 MIPS
    instrpermsec_debug = InstrPerMsec(CLOCK_FREQ); //8K @ 8 MIPS
    instrperusec_debug = InstrPerUsec(CLOCK_FREQ); //8 @ 8 MIPS
    usec2instr_debug = uSec2Instr(128, CLOCK_FREQ); //should be 1K @ 8 MIPS
    usec2instr_debug = uSec2Instr(U16FIXUP(50 msec), CLOCK_FREQ); //should be 400K @ 8 MIPS
    msec2instr_debug = mSec2Instr(50, CLOCK_FREQ); //should be ~ 400K @ 8 MIPS
    usec2instr_debug = uSec2Instr(120 msec, CLOCK_FREQ); //SDCC: BROKEN; should be 1M @ 8 MIPS
    msec2instr_debug = mSec2Instr(120, CLOCK_FREQ); //should be ~ 1M @ 8 MIPS
    instr2usec_debug = Instr2uSec(8, CLOCK_FREQ); //should be 1 @ 8 MIPS
    instr2usec_debug = Instr2uSec(32, CLOCK_FREQ); //should be 4 @ 8 MIPS
    instr2usec_debug = Instr2uSec(40, CLOCK_FREQ); //should be 5 @ 8 MIPS; SDCC: BROKEN
//    instr2usec_debug = Instr2uSec(56, CLOCK_FREQ); //should be 7 @ 8 MIPS; SDCC: BROKEN
//    instr2usec_debug = Instr2uSec(72, CLOCK_FREQ); //should be 9 @ 8 MIPS; SDCC: BROKEN
    instr2usec_debug = Instr2uSec(120, CLOCK_FREQ); //should be 15 @ 8 MIPS; SDCC: BROKEN
//#define Instr2uSec(instr, clock)  ((instr) * ONE_MSEC / InstrPerMsec(clock)) //CAUTION: scaling down CLOCK_FREQ avoids arith overflow, but causes rounding errors
    instr2usec_debug = Instr2uSec(64000, CLOCK_FREQ); //should be 8K @ 8 MIPS
    instr2usec_debug = Instr2uSec(128000, CLOCK_FREQ); //should be 16K @ 8 MIPS
    instr2usec_debug = Instr2uSec(320000, CLOCK_FREQ); //should be 40K @ 8 MIPS
    instr2usec_debug = Instr2uSec(65535 * 8, CLOCK_FREQ); //should be ~ 64K @ 8 MIPS
    instr2usec_debug = Instr2uSec(65536 * 8, CLOCK_FREQ); //should be 64K @ 8 MIPS
    instr2usec_debug = Instr2uSec(65537 * 8, CLOCK_FREQ); //should be ~ 64K @ 8 MIPS
    instr2msec_debug = Instr2mSec(64000, CLOCK_FREQ); //should be 8 @ 8 MIPS
    instr2msec_debug = Instr2mSec(320000, CLOCK_FREQ); //should be 40 @ 8 MIPS
//    instr2usec_debug = 2 * U16FIXUP(60 * ONE_MSEC);
//    instr2usec_debug = InstrPerMsec(CLOCK_FREQ);
//    instr2usec_debug = (120 * ONE_MSEC / InstrPerMsec(CLOCK_FREQ));
 }
 #undef debug
 #define debug()  clock_debug()
#endif

#if 0
#warning CYAN_MSG "[DEBUG] instr/sec " TOSTR(InstrPerSec(CLOCK_FREQ))
#warning CYAN_MSG "[DEBUG] #bits " TOSTR(NumBits8(InstrPerSec(CLOCK_FREQ) / LFINTOSC_FREQ))
#warning CYAN_MSG "[DEBUG] presc " TOSTR(IntOsc_Prescalar(CLOCK_FREQ))
#warning CYAN_MSG "[DEBUG] osccon " TOSTR(IntOsc_Prescalar(CLOCK_FREQ / PLL) * _IRCF0)
INLINE void debug(void)
{
    volatile uint8_t val;
    val = InstrPerSec(CLOCK_FREQ);
    val = NumBits8(InstrPerSec(CLOCK_FREQ) / LFINTOSC_FREQ);
    val = IntOsc_Prescalar(CLOCK_FREQ);
    val = IntOsc_Prescalar(CLOCK_FREQ / PLL) * _IRCF0;
}
#endif

#ifndef init
 #define init() //initialize function chain
#endif

//;initialize clock:
//NOTE: power-up default speed for PIC16F688 is 1 MIPS (4 MHz)
INLINE void init_clock(void)
{
	init(); //prev init first
    LABDCL(0xCF);
//    debug();
//??	if (!osccon.OSTS) //running from int osc
//	if (UseIntOsc || ExtClockFailed) /*can only change int osc freq*/
//	{
//		if (CLOCK_FREQ == MAXINTOSC_FREQ)
//#define UseIntOsc  (CLOCK_FREQ <= 8 MHz * PLL) //PIX16X //int osc is fast enough (with PLL)
#if CLOCK_FREQ > MAXINTOSC_FREQ
 #error RED_MSG "[ERROR] Clock setting " TOSTR(CLOCK_FREQ) " exceeds int osc limit"
//		if (ExtClockFailed) osccon = MY_OSCCON(MAXINTOSC_FREQ); //run as fast as we can on internal oscillator
//		else
#endif
//		osccon = 0x70; //8 MHz or 32 MHz (2 MIPS or 8 MIPS); 8 MIPS ==> 125 nsec/instr, 2 MIPS ==> 500 nsec/instr
		OSCCON = MY_OSCCON(CLOCK_FREQ); //| IIF(!UseIntOsc, 1 << SCS, 0); /*should be 0x70 for 32MHz (8 MHz + PLL) 16F1827 or 8MHz 16F688*/;NOTE: FCM does not set SCS, so set it manually (page 31); serial port seems to be messed up without this!
#if 0 //is this really needed? (initialization code takes a while anyway); easier IDE debug without it
		while (!oscstat.HFIOFS); /*wait for new osc freq to stabilize (affects other timing)*/
		IFPLL(while (!oscstat.PLLR)); /*wait for PLL to stabilize*/
#endif
//	}
}
#undef init
#define init()  init_clock() //function chain in lieu of static init


#endif //ndef _CLOCK_H
//eof
