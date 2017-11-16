////////////////////////////////////////////////////////////////////////////////
////
/// DataCanvas
//

//This is a Node.js add-on to display a rectangular grid of pixels (a texture) on screen using SDL2 and hardware acceleration (via GPU).
//In essence, RPi GPU provides a 24-bit high-speed parallel port with precision timing.
//Optionally, OpenGL and GLSL shaders can be used for generating effects.
//In dev mode, an SDL window is used.  In live mode, full screen is used (must be configured for desired resolution).
//Screen columns generate data signal for each bit of the 24-bit parallel port.
//Without external mux, there can be 24 "universes" of external LEDs to be controlled.  Screen height defines max universe length.

//Copyright (c) 2015, 2016, 2017 Don Julien, djulien@thejuliens.net


////////////////////////////////////////////////////////////////////////////////
////
/// Setup
//

//0. deps:
//0a. node + nvm
//1. software install:
//1a. git clone this repo
//1b. cd this folder
//1c. npm install
//1d. upgrade node-gyp >= 3.6.2
//     [sudo] npm explore npm -g -- npm install node-gyp@latest
//2. npm test (dev mode test, optional)
//3. config:
//3a. edit /boot/config.txt
//3b. give RPi GPU 256 MB RAM (optional)


//dependencies (pre-built, older):
//1. find latest + install using:
//  apt-cache search libsdl2    #2.0.0 as of 10/12/17
//  apt-get install libsdl2-dev
//??   sudo apt-get install freeglut3-dev
//reinstall: sudo apt-get install (pkg-name) --reinstall
//OR sudo apt-get remove (pkg)  then install again

//(from src, newer):

//troubleshooting:
//g++ name demangling:  c++filt -n  (mangled name)
//to recompile this file:  npm run rebuild   or   npm install


//advantages of SDL2 over webgl, glfw, etc:
//- can run from ssh console
//- fewer dependencies/easier to install

//perf: newtest maxed at 24 x 1024 = 89% idle (single core)

//TODO:
//- fix avg time used stats
//- add music
//- read Vix2 seq
//- map channels
//- WS281X-to-Renard
//- fix/add JS pivot props
//- fix JS Screen


////////////////////////////////////////////////////////////////////////////////
////
/// Headers, general macros, inline functions
//

//#include <stdio.h>
//#include <stdarg.h>
#include <string.h> //strlen
#include <limits.h> //INT_MAX
#include <stdint.h> //uint*_t types
//#include <math.h> //sqrt
//#include <sys/time.h>
#include <sys/stat.h>
//#include <iostream> //std::cin
//using std::cin;
//#include <cstring> //std::string
//using std::string;
//using std::getline;
//#include <sstream> //std::ostringstream
//using std::ostringstream;
//#include <type_traits> //std::conditional, std::enable_if, std::is_same, std::disjunction, etc
//#include <stdexcept> //std::runtime_error
//using std::runtime_error;
//#include <memory> //shared_ptr<>
#include <regex> //regex*, *match
//using std::regex;
//using std::cmatch;
//using std::smatch;
//using std::regex_search;
#include <algorithm> //std::min, std::max
//using std::min;


#define rdiv(n, d)  int(((n) + ((d) >> 1)) / (d))
#define divup(n, d)  int(((n) + (d) - 1) / (d))

#define SIZE(thing)  int(sizeof(thing) / sizeof((thing)[0]))

#define return_void(expr) { expr; return; } //kludge: avoid warnings about returning void value

//inline int toint(void* val) { return (int)(long)val; }
//#define toint(ptr)  reinterpret_cast<int>(ptr) //gives "loses precision" warning/error
#define toint(expr)  (int)(long)(expr)


//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//#define CONCAT(first, second)  CONCAT_NESTED(first, second)
//#define CONCAT_NESTED(first, second)  first ## second

#if 0
//template <typename Type>
inline const char* ifnull(const char* val, const char* defval, const char* fallback = 0)
{
    static const char* Empty = "";
    return val? val: defval? defval: fallback? fallback: Empty;
}
//#define ifnull(val, defval, fallback)  ((val)? val: (defval)? defval: fallback)
#endif
//#define ifnull(str, def)  ((str)? str: def)

//use inline functions instead of macros to avoid redundant param eval:

//int min(int a, int b) { return (a < b)? a: b; }
//int max(int a, int b) { return (a > b)? a: b; }

//placeholder stmt (to avoid warnings):
inline bool noop() { return false; }

#if 0
//get str len excluding trailing newline:
inline int strlinelen(const char* str)
{
    if (!str || !str[0]) return 0;
    int len = strlen(str);
    if (str[len - 1] == '\n') --len;
    return len;
}
#endif

//skip over first part of string if it matches:
inline const char* skip(const char* str, const char* prefix)
{
//    size_t preflen = strlen(prefix);
//    return (str && !strncmp(str, prefix, preflen))? str + preflen: str;
    if (str)
        while (*str == *prefix++) ++str;
    return str;
}

//text line out to stream:
inline void fputline(const char* buf, FILE* stm)
{
    fputs(buf, stm);
    fputc('\n', stm); 
    fflush(stm);
}


//debug/error messages:
#define WANT_LEVEL  99 //[0..9] for high-level, [10..19] for main logic, [20..29] for mid logic, [30..39] for lower level alloc/dealloc
#define myprintf(level, ...)  ((WANT_LEVEL >= (level))? printf(__VA_ARGS__): noop())

//#define eprintf(args…) fprintf (stderr, args)
//#define eprintf(format, …) fprintf (stderr, format, __VA_ARGS__)
//#define eprintf(format, …) fprintf (stderr, format, ##__VA_ARGS__)
#define NOERROR  -1
#define err(...)  errprintf(SDL_GetError(), __VA_ARGS__)
//TODO #define err(fmt, ...)  errprintf(SDL_GetError(), fmt, __VA_ARGS__)
//#define myerr(reason, ...)  errprintf(reason, __VA_ARGS__)
#define printf(...)  errprintf((const char*)NOERROR, __VA_ARGS__)
bool errprintf(const char* reason /*= 0*/, const char* fmt, ...); //fwd ref


////////////////////////////////////////////////////////////////////////////////
////
/// Color defs
//

#define LIMIT_BRIGHTNESS  (3*212) //limit R+G+B value; helps reduce power usage; 212/255 ~= 83% gives 50 mA per node instead of 60 mA

//ARGB primary colors:
#define BLACK  0xFF000000 //NOTE: need A
#define WHITE  0xFFFFFFFF
#define RED  0xFFFF0000
#define GREEN  0xFF00FF00
#define BLUE  0xFF0000FF
#define YELLOW  0xFFFFFF00
#define CYAN  0xFF00FFFF
#define MAGENTA  0xFFFF00FF
//other ARGB colors (debug):
//#define SALMON  0xFF8080

//const uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

//hard-coded ARGB format:
#pragma message("Compiled for ARGB color format (hard-coded)")
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
 #pragma message("Big endian")
// #define Rmask  0xFF000000
// #define Gmask  0x00FF0000
// #define Bmask  0x0000FF00
// #define Amask  0x000000FF
//#define Abits(a)  (clamp(a, 0, 0xFF) << 24)
//#define Rbits(a)  (clamp(r, 0, 0xFF) << 16)
//#define Gbits(a)  (clamp(g, 0, 0xFF) << 8)
//#define Bbits(a)  clamp(b, 0, 0xFF)
 #define Amask(color)  ((color) & 0xFF000000)
 #define Rmask(color)  ((color) & 0x00FF0000)
 #define Gmask(color)  ((color) & 0x0000FF00)
 #define Bmask(color)  ((color) & 0x000000FF)
 #define A(color)  (((color) >> 24) & 0xFF)
 #define R(color)  (((color) >> 16) & 0xFF)
 #define G(color)  (((color) >> 8) & 0xFF)
 #define B(color)  ((color) & 0xFF)
 #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))
#elif SDL_BYTEORDER == SDL_LITTLE_ENDIAN
 #pragma message("Little endian")
// #define Amask  0xFF000000
// #define Bmask  0x00FF0000
// #define Gmask  0x0000FF00
// #define Rmask  0x000000FF
 #define Amask(color)  ((color) & 0xFF000000)
 #define Rmask(color)  ((color) & 0x00FF0000)
 #define Gmask(color)  ((color) & 0x0000FF00)
 #define Bmask(color)  ((color) & 0x000000FF)
 #define A(color)  (((color) >> 24) & 0xFF)
 #define R(color)  (((color) >> 16) & 0xFF)
 #define G(color)  (((color) >> 8) & 0xFF)
 #define B(color)  ((color) & 0xFF)
 #define toARGB(a, r, g, b)  ((clamp(toint(a), 0, 255) << 24) | (clamp(toint(r), 0, 255) << 16) | (clamp(toint(g), 0, 255) << 8) | clamp(toint(b), 0, 255))
#else
 #error message("Unknown endian")
#endif
#define R_G_B_bytes(color)  R(color), G(color), B(color)
#define R_G_B_A_bytes(color)  R(color), G(color), B(color), A(color)
#define R_G_B_A_masks(color)  Rmask(color), Gmask(color), Bmask(color), Amask(color)


//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_LT  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_LT  ANSI_COLOR("1;32")
#define YELLOW_LT  ANSI_COLOR("1;33")
#define BLUE_LT  ANSI_COLOR("1;34")
#define MAGENTA_LT  ANSI_COLOR("1;35")
#define CYAN_LT  ANSI_COLOR("1;36")
#define GRAY_LT  ANSI_COLOR("0;37")
//#define ENDCOLOR  ANSI_COLOR("0")
//append the src line# to make debug easier:
#define ENDCOLOR_ATLINE(n)  " &" TOSTR(n) ANSI_COLOR("0") "\n"
#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(__LINE__)

const std::regex ANSICC_re("\x1b\\[([0-9;]+)m"); //find ANSI console color codes in a string


//simulate a few GLSL built-in functions (for debug/test patterns):
inline float _fract(float x) { return x - int(x); }
inline float _abs(float x) { return (x < 0)? -x: x; }
inline float _clamp(float x, float minVal, float maxVal) { return (x < minVal)? minVal: (x > maxVal)? maxVal: x; }
inline int _clamp(int x, int minVal, int maxVal) { return (x < minVal)? minVal: (x > maxVal)? maxVal: x; }
inline float _mix(float x, float y, float a) { return (1 - a) * x + a * y; }
//use macros so consts can be folded:
#define fract(x)  ((x) - int(x))
#define abs(x)  (((x) < 0)? -(x): x)
#define clamp(val, min, max)  ((val) < (min)? (min): (val) > (max)? (max): (val))
#define mix(x, y, a)  ((1 - (a)) * (x) + (a) * (y))


////////////////////////////////////////////////////////////////////////////////
////
/// SDL utilities, extensions
//

#include <SDL.h> //must include first before other SDL or GL header files
//#include <SDL_thread.h>
//#include <SDL2/SDL_image.h>
//#include <SDL_opengl.h>
//??#include <GL/GLU.h> //#include <GL/freeglut.h> #include <GL/gl.h> #include <GL/glu.h>

//SDL retval convention:
//0 == Success, < 0 == error, > 0 == data ptr (sometimes)
#define SDL_Success  0
#define OK(retval)  ((retval) >= 0)

//more meaningful names for SDL NULL params:
#define UNUSED  0
#define NORECT  NULL
#define FIRST_MATCH  -1
//#define THIS_THREAD  NULL


//allow libs to be treated like other allocated SDL objects:
typedef struct { /*int dummy;*/ } SDL_lib;
//typedef struct { int dummy; } IMG_lib;


//lib init shims to work with auto_ptr<> and OK macro:
inline SDL_lib* SDL_INIT(uint32_t flags, int where) // = 0)
{
    static SDL_lib ok;
    SDL_lib* retval = OK(SDL_Init(flags))? &ok: NULL;  //dummy non-NULL address for success
    if (where) myprintf(22, YELLOW_LT "SDL_Init(0x%x) = 0x%x ok? %d (from %d)" ENDCOLOR, flags, toint(retval), !!retval, where);
    return retval;
}
//NOTE: cpp avoids recursion so macro names can match actual function names here
//#define IMG_INIT(flags)  ((IMG_Init(flags) & (flags)) != (flags)) //0 == Success
#define SDL_INIT(flags)  SDL_INIT(flags, __LINE__)

//reduce verbosity:
#define SDL_PixelFormatShortName(fmt)  skip(SDL_GetPixelFormatName(fmt), "SDL_PIXELFORMAT_")

/*
inline const char* ThreadName(const SDL_Thread* thr = THIS_THREAD)
{
    static string name; //must be persistent after return
    name = SDL_GetThreadName(thr);
    if (!name.length()) name = "0x" + SDL_GetThreadID(thr);
    if (!name.length()) name = "(no name)";
    return name.c_str();
}
*/


////////////////////////////////////////////////////////////////////////////////
////
/// Safe pointer/wrapper (auto-dealloc resources when they go out of scope)
//

//make debug easier by tagging with name and location:
//#define TRACKED(thing)  thing.newwhere = __LINE__; thing.newname = TOSTR(thing); thing


//type-safe wrapper for SDL ptrs that cleans up when it goes out of scope:
//std::shared_ptr<> doesn't allow assignment or provide casting; this custom one does
template<typename DataType, typename = void>
class auto_ptr
{
//TODO: private:
public:
//CAUTION: do not initialize info here; it will overwrite results from nested member calls
    DataType* cast; //= NULL;
    static DataType* latest;
public:
//ctor/dtor:
    auto_ptr(DataType* that = NULL): cast(that) { if (that) latest = that; myprintf(22, YELLOW_LT "assign %s 0x%x" ENDCOLOR, TypeName(that), toint(that)); };
    ~auto_ptr() { this->release(); }
//cast to regular pointer:
//    operator const PtrType*() { return this->cast; }
//TODO; for now, just make ptr public
//conversion:
//NO; lval bypasses op=    operator /*const*/ DataType*&() { return this->cast; } //allow usage as l-value; TODO: would that preserve correct state?
    operator /*const*/ DataType*() { return this->cast; } //r-value only; l-value must use op=
//redundant?    operator bool() { return this->cast != NULL; }
//assignment:
//    PtrType*& operator=(/*const*/ PtrType* /*&*/ that)
    auto_ptr& operator=(/*const*/ DataType* /*&*/ that)
    {
        if (this->cast != that) //avoid self-assignment
        {
            this->release();
//set new state:
            myprintf(22, YELLOW_LT "assign %s 0x%x" ENDCOLOR, TypeName(that), toint(that));
            this->cast = that; //now i'm responsible for releasing new object (no ref counting)
            if (that) latest = that;
        }
        return *this; //chainable
    }
public:
    void release()
    {
        if (this->cast) Release(this->cast); //overloaded
        this->cast = NULL;
    }
    DataType* keep()
    {
        DataType* retval = this->cast;
        this->cast = NULL; //new owner is responsible for dealloc
        return retval;
    }
};
//#define auto_ptr(type)  auto_ptr<type, TOSTR(type)>
//#define auto_ptr  __SRCLINE__ auto_ptr_noline

template<>
SDL_lib* auto_ptr<SDL_lib>::latest = NULL;
template<>
SDL_Window* auto_ptr<SDL_Window>::latest = NULL;
template<>
SDL_Renderer* auto_ptr<SDL_Renderer>::latest = NULL;
template<>
SDL_Texture* auto_ptr<SDL_Texture>::latest = NULL;
template<>
SDL_Surface* auto_ptr<SDL_Surface>::latest = NULL;


//define type names (for debug or error messages):
inline const char* TypeName(const SDL_lib*) { return "SDL_lib"; }
//inline const char* TypeName(const IMG_lib*) { return "IMG_lib"; }
inline const char* TypeName(const SDL_Window*) { return "SDL_Window"; }
inline const char* TypeName(const SDL_Renderer*) { return "SDL_Renderer"; }
inline const char* TypeName(const SDL_Texture*) { return "SDL_Texture"; }
inline const char* TypeName(const SDL_Surface*) { return "SDL_Surface"; }
//inline const char* TypeName(const SDL_LockedTexture*) { return "SDL_LockedTexture"; }
//inline const char* TypeName(const SDL_mutex*) { return "SDL_mutex"; }
//inline const char* TypeName(const SDL_LockedMutex*) { return "SDL_LockedMutex"; }
//inline const char* TypeName(const SDL_cond*) { return "SDL_cond"; }
//inline const char* TypeName(const SDL_Thread*) { return "SDL_Thread"; }


//overloaded deallocator function for each type:
//avoids template specialization
//TODO: remove printf
#define debug(ptr)  myprintf(28, YELLOW_LT "dealloc %s 0x%x" ENDCOLOR, TypeName(ptr), toint(ptr))
inline int Release(SDL_lib* that) { debug(that); SDL_Quit(); return SDL_Success; }
//inline int Release(IMG_lib* that) { IMG_Quit(); return SDL_Success; }
inline int Release(SDL_Window* that) { debug(that); SDL_DestroyWindow(that); return SDL_Success; }
inline int Release(SDL_Renderer* that) { debug(that); SDL_DestroyRenderer(that); return SDL_Success; }
//inline int Release(SDL_GLContext* that) { debug(that); SDL_GL_DeleteContext(that); return SDL_Success; }
inline int Release(SDL_Texture* that) { debug(that); SDL_DestroyTexture(that); return SDL_Success; }
inline int Release(SDL_Surface* that) { debug(that); SDL_FreeSurface(that); return SDL_Success; }
//inline int Release(SDL_LockedTexture* that) { myprintf(28, YELLOW_LT "SDL_UnlockTexture" ENDCOLOR); SDL_UnlockTexture(that->txr); return SDL_Success; }
//inline int Release(SDL_mutex* that) { myprintf(28, YELLOW_LT "SDL_DestroyMutex" ENDCOLOR); SDL_DestroyMutex(that); return SDL_Success; }
//inline int Release(SDL_LockedMutex* that) { myprintf(28, YELLOW_LT "SDL_UnlockMutex" ENDCOLOR); return SDL_UnlockMutex(that->mutex); } //the only one that directly returns a value
//inline int Release(SDL_cond* that) { myprintf(28, YELLOW_LT "SDL_DestroyCond" ENDCOLOR); SDL_DestroyCond(that); return SDL_Success; }
//inline int Release(SDL_PendingCond* that) { Release(that->mutex); Release(that->cond); return SDL_Success; }
//inline int Release(SDL_Thread* that) { int exitval; SDL_WaitThread(that, &exitval); return exitval; } //this is one of the few dealloc funcs with a ret code, so might as well check it
#undef debug


////////////////////////////////////////////////////////////////////////////////
////
/// misc global defs
//

typedef struct WH { uint16_t w, h; } WH; //pack width, height into single word for easy return from functions

//auto_ptr<SDL_lib> sdl;
//auto_ptr<SDL_Window> window;
//auto_ptr<SDL_Renderer> renderer;
//auto_ptr<SDL_Texture> canvas;
//auto_ptr<SDL_Surface> pixels;

//fwd refs:
void debug_info(SDL_lib*);
void debug_info(SDL_Window*);
void debug_info(SDL_Renderer*);
void debug_info(SDL_Surface*);
WH Screen();
WH MaxFit();
uint32_t limit(uint32_t color);
uint32_t hsv2rgb(float h, float s, float v);
uint32_t ARGB2ABGR(uint32_t color);
const char* commas(int);
bool exists(const char* path);


////////////////////////////////////////////////////////////////////////////////
////
/// Main functions
//


//check for RPi:
//NOTE: results are cached (outcome won't change)
bool isRPi()
{
    static enum {No = false, Yes = true, Maybe} isrpi = Maybe;

//    myprintf(3, BLUE_LT "isRPi()" ENDCOLOR);
    if (isrpi == Maybe) isrpi = exists("/boot/config.txt")? Yes: No;
    return (isrpi == Yes);
}


//get screen width, height:
//wrapped in a function so it can be used as initializer (optional)
//screen height determines max universe size
//screen width should be configured according to desired data rate (DATA_BITS per node)
WH Screen()
{
    static WH wh = {0, 0};

    if (!wh.w || !wh.h)
    {
        auto_ptr<SDL_lib> sdl(SDL_INIT(SDL_INIT_VIDEO)); //for access to video info; do this in case not already done
        if (!SDL_WasInit(0)) err(RED_LT "ERROR: Tried to get screen before SDL_Init" ENDCOLOR);
        myprintf(22, BLUE_LT "%d display(s):" ENDCOLOR, SDL_GetNumVideoDisplays());
        for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
        {
            SDL_DisplayMode mode = {0};
            if (!OK(SDL_GetCurrentDisplayMode(i, &mode))) //NOTE: SDL_GetDesktopDisplayMode returns previous mode if full screen mode
                err(RED_LT "Can't get display mode[%d/%d]" ENDCOLOR, i, SDL_GetNumVideoDisplays());
            else myprintf(22, BLUE_LT "Display mode[%d/%d]: %d x %d px @%dHz, %i bbp %s" ENDCOLOR, i, SDL_GetNumVideoDisplays(), mode.w, mode.h, mode.refresh_rate, SDL_BITSPERPIXEL(mode.format), SDL_PixelFormatShortName(mode.format));
            if (!wh.w || !wh.h) { wh.w = mode.w; wh.h = mode.h; } //take first one, continue (for debug)
//            break; //TODO: take first one or last one?
        }
    }

//set reasonable values if can't get info:
    if (!wh.w || !wh.h)
    {
        wh.w = 1536;
        wh.h = wh.w * 3 / 4; //4:3 aspect ratio
        myprintf(22, YELLOW_LT "Using dummy display mode %dx%d" ENDCOLOR, wh.w, wh.h);
    }
    return wh;
}


#define WS281X_BITS  24 //each WS281X node has 24 data bits
#define TXR_WIDTH  (3 * WS281X_BITS - 1) //data signal is generated at 3x bit rate, last bit overlaps H blank

//window create and redraw:
class DataCanvas
{
private:
    auto_ptr<SDL_Window> window;
    auto_ptr<SDL_Renderer> renderer;
    auto_ptr<SDL_Texture> canvas;
    auto_ptr<SDL_Surface> pxbuf;
//shared data:
    static auto_ptr<SDL_lib> sdl;
    static int count;
//performance stats:
    uint64_t pivot_time, update_time, render_time, present_time, caller_time;
    uint64_t started, previous;
    uint32_t numfr;
    inline static uint64_t now() { return SDL_GetPerformanceCounter(); }
public:
    int num_univ, univ_len; //called-defined
    bool WantPivot;
public:
    DataCanvas(const char* title, int num_univ, int univ_len, bool want_pivot = true): num_univ(0), univ_len(0), WantPivot(want_pivot)
    {
        if (!count++) Init();
        if (!title) title = "DataCanvas";
        myprintf(3, BLUE_LT "Init(title '%s', #univ %d, univ len %d, pivot? %d)" ENDCOLOR, title, num_univ, univ_len, want_pivot);

#define IGNORED_X_Y_W_H  0, 0, 200, 100 //not used for full screen mode
        int wndw, wndh;
        window = isRPi()?
            SDL_CreateWindow(title, IGNORED_X_Y_W_H, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN): //| SDL_WINDOW_OPENGL): //don't use OpenGL; too slow
            SDL_CreateWindow(title, 10, 10, MaxFit().w, MaxFit().h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN); //| SDL_WINDOW_OPENGL);
        if (!window) return_void(err(RED_LT "Create window failed" ENDCOLOR));
        uint32_t fmt = SDL_GetWindowPixelFormat(window); //desktop OpenGL: 24 RGB8888, RPi: 32 ARGB8888
        if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_LT "Can't get window format" ENDCOLOR));
        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
        debug_info(window);

        this->num_univ = num_univ;
        this->univ_len = univ_len;
        this->WantPivot = want_pivot;
        if ((num_univ < 1) || (num_univ > WS281X_BITS)) return_void(err(RED_LT "Too many universes: %d (max %d)" ENDCOLOR, num_univ, WS281X_BITS));
        if ((univ_len < 1) || (univ_len > wndh)) return_void(err(RED_LT "Universe too big: %d (max %d)" ENDCOLOR, univ_len, wndh));

	    renderer = SDL_CreateRenderer(window, FIRST_MATCH, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) return_void(err(RED_LT "Create renderer failed" ENDCOLOR));
        debug_info(renderer);

#if 0 //NOTE: RPi does not like this; prevents texture creation below
//NOTE: SDL_GetWindowSurface calls SDL_CreateWindowFramebuffer which calls internal CreateWindowFramebuffer (SDL_CreateWindowTexture) in which SDL_Renderer is created.
        SDL_Surface* wnd_surf = SDL_GetWindowSurface(window); //NOTE: wnd will dealloc, so don't need auto_ptr here
        if (!wnd_surf) return_void(err(RED_LT "Can't get window surface" ENDCOLOR));
//NOTE: wnd_surf info is gone after SDL_CreateRenderer! (benign if info was saved already)
//    if (!wnd_surf->w || !wnd_surf->h) myprintf(12, YELLOW_LT "Window surface %dx%d (ignored)" ENDCOLOR, wnd_surf->w, wnd_surf->h);
        debug_info(wnd_surf);
//        SDL_Surface surf = {0}, *wnd_surf = &surf;
//        surf.h = Screen().h;
#endif

//        int wndw, wndh;
//        SDL_GL_GetDrawableSize(window, &wndw, &wndh);
//NOTE: surface + texture must always be 3 * WS281X_BITS - 1 pixels wide
//data signal is generated at 3x bit rate, last bit overlaps H blank
//surface + texture height determine max # WS281X nodes per universe
//SDL will stretch texture to fill window (V-grouping); OpenGL not needed for this
//use same fmt + depth as window; TODO: is this more efficient?
//        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, 8+8+8+8, SDL_PIXELFORMAT_ARGB8888);
        pxbuf = SDL_CreateRGBSurfaceWithFormat(UNUSED, TXR_WIDTH, univ_len, SDL_BITSPERPIXEL(fmt), fmt);
        if (!pxbuf) return_void(err(RED_LT "Can't alloc pixel buf" ENDCOLOR));
        if ((pxbuf.cast->w != TXR_WIDTH) || (pxbuf.cast->h != univ_len)) return_void(err(RED_LT "Pixel buf wrong size: got %d x %d, wanted %d x %d" ENDCOLOR, pxbuf.cast->w, pxbuf.cast->h, TXR_WIDTH, univ_len));
        if (toint(pxbuf.cast->pixels) & 3) return_void(err(RED_LT "Pixel buf not quad byte aligned" ENDCOLOR));
        if (pxbuf.cast->pitch != 4 * pxbuf.cast->w) return_void(err(RED_LT "Pixel buf pitch: got %d, expected %d" ENDCOLOR, pxbuf.cast->pitch, 4 * pxbuf.cast->w));
        debug_info(pxbuf);
//        if (!OK(SDL_FillRect(pxbuf, NORECT, BLACK))) return_void(err(RED_LT "Fill rect failed" ENDCOLOR));

        canvas = SDL_CreateTexture(renderer, pxbuf.cast->format->format, SDL_TEXTUREACCESS_STREAMING, pxbuf.cast->w, pxbuf.cast->h); //SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, wndw, wndh);
        if (!canvas) return_void(err(RED_LT "Can't create canvas texture" ENDCOLOR));

//        Paint(NULL); //start with all pixels dark
        pivot_time = update_time = render_time = present_time = caller_time = numfr = 0;
        started = previous = now();
//        return true;
    }
    ~DataCanvas()
    {
        stats();
        if (!--count) Quit();
        myprintf(22, YELLOW_LT "DataCanvas dtor" ENDCOLOR);
    };
public:
//repaint screen:
//NOTE: this will not return to caller until next video frame (v-synced at 60 Hz); TODO: is this bad for Node.js event loop?
//NOTE: caller owns pixel buf; leave it in caller's memory so it can be updated (need to copy here for pivot anyway)
//NOTE: pixel array assumed to be the correct size here (already checked by Javascript wrapper below)
    bool Paint(uint32_t* pixels, uint32_t fill = BLACK)
    {
        uint64_t delta;
        delta = now() - previous; caller_time += delta; previous += delta;
//        myprintf(6, BLUE_LT "Paint(pixels 0x%x), %d x %d = %s len (assumed), 0x%x 0x%x 0x%x ..." ENDCOLOR, pixels, this->num_univ, this->univ_len, commas(this->num_univ * this->univ_len), pixels? pixels[0]: fill, pixels? pixels[1]: fill, pixels? pixels[2]: fill);

        pivot(pixels, fill);
        delta = now() - previous; pivot_time += delta; previous += delta;

//NOTE: texture must be owned by render thread (according to various sources):
//this copies pixel data to GPU memory?
//TODO: wrap in locker template/class
#if 1
        SDL_Surface txr_info;
        if (!OK(SDL_LockTexture(canvas, NORECT, &txr_info.pixels, &txr_info.pitch)))
            return err(RED_LT "Can't lock texture" ENDCOLOR);
//NOTE: pixel data must be in correct (texture) format
//NOTE: SDL_UpdateTexture is reportedly slow; use Lock/Unlock and copy pixel data directly for better performance
        memcpy(txr_info.pixels, pxbuf.cast->pixels, pxbuf.cast->pitch * pxbuf.cast->h);
        SDL_UnlockTexture(canvas);
#else //slower; doesn't work with streaming texture?
        if (!OK(SDL_UpdateTexture(canvas, NORECT, pxbuf.cast->pixels, pxbuf.cast->pitch)))
            return err(RED_LT "Can't update texture" ENDCOLOR);
#endif
        delta = now() - previous; update_time += delta; previous += delta;

    	if (!OK(SDL_RenderCopy(renderer, canvas, NORECT, NORECT))) //&renderQuad, angle, center, flip ))
            return err(RED_LT "Unable to render to screen" ENDCOLOR);
        delta = now() - previous; render_time += delta; previous += delta;

//    if (!OK(SDL_GL_MakeCurrent(gpu.wnd, NULL)))
//        return err(RED_LT "Can't unbind current context" ENDCOLOR);
//    uint_least32_t time = now_usec();
//    xfr_time += ELAPSED;
//    render_times[2] += (time2 = now_usec()) - time1;
//    myprintf(22, RED_LT "TODO: delay music_time - now" ENDCOLOR);
//NOTE: RenderPresent() doesn't return until next frame if V-synced; this gives accurate 60 FPS (avoids jitter; O/S wait is +/- 10 msec)
//TODO: allow caller to resume in parallel
        SDL_RenderPresent(renderer); //update screen before delay (uploads to GPU)
        delta = now() - previous; present_time += delta; previous += delta;
        if (!(++numfr % (60 * 10))) stats(); //show stats every 10 sec
        return true;
    }
    void stats()
    {
        uint64_t elapsed = now() - started, freq = SDL_GetPerformanceFrequency(); //#ticks/second
        myprintf(12, YELLOW_LT "#fr %d, elapsed %2.1f sec, %2.1f fps, avg (msec): update %2.1f, render %2.1f, present %2.1f, caller %2.1f, %2.1f%% idle" ENDCOLOR, numfr, (double)elapsed / freq, (double)numfr / elapsed * freq, (double)update_time / freq / numfr, (double)render_time / freq / numfr, (double)present_time / freq / numfr, (double)caller_time / freq / numfr, 100. * (elapsed - update_time - render_time - caller_time) / elapsed);
    }
    bool Release()
    {
        num_univ = univ_len = 0;
        pxbuf.release();
        canvas.release();
        renderer.release();
        window.release();
        return true;
    }
private:
//24-bit pivot:
//CAUTION: expensive CPU loop here
//NOTE: need pixel-by-pixel copy for several reasons:
//- ARGB -> RGBA (desirable)
//- brightness limiting (recommended)
//- blending (optional)
//- 24-bit pivots (required, non-dev mode)
//        memset(pixels, 4 * this->w * this->h, 0);
//TODO: perf compare inner/outer swap
//TODO? locality of reference: keep nodes within a universe close to each other (favors caller)
    void pivot(uint32_t* pixels, uint32_t fill = BLACK)
    {
        uint32_t* pxbuf32 = reinterpret_cast<uint32_t*>(this->pxbuf.cast->pixels);
//myprintf(22, "paint pxbuf 0x%x pxbuf32 0x%x" ENDCOLOR, toint(this->pxbuf.cast->pixels), toint(pxbuf32));
#if 0 //test
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
            {
//if (!y || (y >= this->univ_len - 1) || !x3 || (x3 >= TXR_WIDTH - 3)) myprintf(22, "px[%d] @(%d/%d,%d/%d)" ENDCOLOR, yofs + x3, y, this->univ_len, x3, TXR_WIDTH);
                pxbuf32[yofs + x3 + 0] = RED;
                pxbuf32[yofs + x3 + 1] = GREEN;
                if (x3 < TXR_WIDTH - 1) pxbuf32[yofs + x3 + 2] = BLUE;
            }
        return;
#endif
#if 0 //NO; RAM is slower than CPU
        uint32_t rowbuf[TXR_WIDTH + 1], start_bits = this->WantPivot? WHITE: BLACK;
//set up row template with start bits, cleared data + stop bits:
        for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
        {
            rowbuf[x3 + 0] = start_bits; //WHITE; //start bits
            rowbuf[x3 + 1] = BLACK; //data bits (will be overwritten with pivoted color bits)
            rowbuf[x3 + 2] = BLACK; //stop bits (right-most overlaps H-blank)
        }
//            memcpy(&pxbuf32[yofs], rowbuf, TXR_WIDTH * sizeof(uint32_t)); //initialze start, data, stop bits
#endif
        for (int y = 0, yofs = 0; y < this->univ_len; ++y, yofs += TXR_WIDTH) //outer
        {
            for (int x3 = 0; x3 < TXR_WIDTH; x3 += 3) //inner; locality of reference favors destination
            {
                pxbuf32[yofs + x3 + 0] = WHITE; //start bits
                pxbuf32[yofs + x3 + 1] = BLACK; //data bits (will be overwritten with pivoted color bits)
                if (x3) pxbuf32[yofs + x3 - 1] = BLACK; //stop bits (right-most overlaps H-blank)
            }
#if 1
//fill with pivoted data bits:
            if (this->WantPivot)
            {
//NOTE: xmask loop assumes ARGB or ABGR fmt (A in upper byte)
                for (uint32_t x = 0, xofs = 0, xmask = 0x800000; x < (uint32_t)this->num_univ; ++x, xofs += this->univ_len, xmask >>= 1)
            	{
                    uint32_t color = pixels? pixels[xofs + y]: fill;
//                    if (!A(color) || (!R(color) && !G(color) && !B(color))) continue; //no data to pivot
                    color = limit(color); //limit brightness/power
//TODO                color = ARGB2ABGR(color);
                    for (int bit3 = 1; bit3 < TXR_WIDTH; bit3 += 3, color <<= 1)
                        if (color & 0x800000) pxbuf32[yofs + bit3] |= xmask; //set data bit
            	}
                continue; //next row
            }
#endif
//just copy pixels as-is (dev/debug only):
            for (int x = 0, x3 = 0, xofs = 0; x < this->num_univ; ++x, x3 += 3, xofs += this->univ_len)
                pxbuf32[yofs + x3 + 0] = pxbuf32[yofs + x3 + 1] = pixels? pixels[xofs + y]: fill;
        }
    }
public:
//initialize SDL:
    static bool Init()
    {
	    sdl = SDL_INIT(/*SDL_INIT_AUDIO |*/ SDL_INIT_VIDEO); //| SDL_INIT_EVENTS); //events, file, and threads are always inited, so they don't need to be flagged here; //| ??SDL_INIT_TIMER); //SDL_init(SDL_INIT_EVERYTHING);
        if (!OK(sdl)) return err(RED_LT "SDL init failed" ENDCOLOR);
        debug_info(sdl);

//NOTE: scaling *must* be set to nearest pixel sampling (0) because texture is stretched horizontally to fill screen
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0") != SDL_TRUE) //set texture filtering to linear; TODO: is this needed?
            err(YELLOW_LT "Warning: Linear texture filtering not enabled" ENDCOLOR);
//TODO??    SDL_bool SDL_SetHintWithPriority(const char*      name, const char*      value,SDL_HintPriority priority)
 
#if 0 //not needed
//use OpenGL 2.1:
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2)) || !OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1)))
            return err(RED_LT "Can't set GL version to 2.1" ENDCOLOR);
        if (!OK(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8)) || !OK(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8))) //|| !OK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8)))
            return err(RED_LT "Can't set GL R/G/B to 8 bits" ENDCOLOR);
//??SDL_GL_BUFFER_SIZE //the minimum number of bits for frame buffer size; defaults to 0
//??SDL_GL_DOUBLEBUFFER //whether the output is single or double buffered; defaults to double buffering on
//??SDL_GL_DEPTH_SIZE //the minimum number of bits in the depth buffer; defaults to 16
//??SDL_GL_STEREO //whether the output is stereo 3D; defaults to off
//??SDL_GL_MULTISAMPLEBUFFERS //the number of buffers used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_MULTISAMPLESAMPLES //the number of samples used around the current pixel used for multisample anti-aliasing; defaults to 0; see Remarks for details
//??SDL_GL_ACCELERATED_VISUAL //set to 1 to require hardware acceleration, set to 0 to force software rendering; defaults to allow either
        if (!OK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES))) //type of GL context (Core, Compatibility, ES)
            return err(RED_LT "Can't set GLES context profile" ENDCOLOR);
//NO    if (!OK(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1)))
//        return err(RED_LT "Can't set shared context attr" ENDCOLOR);
//??SDL_GL_SHARE_WITH_CURRENT_CONTEXT
//??SDL_GL_FRAMEBUFFER_SRGB_CAPABLE not needed
//??SDL_GL_CONTEXT_RELEASE_BEHAVIOR
#endif
       return true;
    }
//clean up SDL:
//nothing to do; auto_ptr does it all :)
    static bool Quit()
    {
        myprintf(3, BLUE_LT "Quit()" ENDCOLOR);
        return true;
    }
};
auto_ptr<SDL_lib> DataCanvas::sdl;
int DataCanvas::count = 0;


////////////////////////////////////////////////////////////////////////////////
////
/// Node.js interface
//

//see https://nodejs.org/docs/latest/api/addons.html
//https://stackoverflow.com/questions/3066833/how-do-you-expose-a-c-class-in-the-v8-javascript-engine-so-it-can-be-created-u

//refs:
//https://community.risingstack.com/using-buffers-node-js-c-plus-plus/
//https://nodeaddons.com/building-an-asynchronous-c-addon-for-node-js-using-nan/
//https://nodeaddons.com/c-processing-from-node-js-part-4-asynchronous-addons/
//example: https://github.com/Automattic/node-canvas/blob/b470ce81aabe2a78d7cdd53143de2bee46b966a7/src/CanvasRenderingContext2d.cc#L764
//https://github.com/nodejs/nan/blob/master/doc/v8_misc.md#api_nan_typedarray_contents
//http://bespin.cz/~ondras/html/classv8_1_1Object.html
#if 0
//see https://nodejs.org/api/addons.html
//and http://stackabuse.com/how-to-create-c-cpp-addons-in-node/
//parameter passing:
// http://www.puritys.me/docs-blog/article-286-How-to-pass-the-paramater-of-Node.js-or-io.js-into-native-C/C++-function..html
// http://luismreis.github.io/node-bindings-guide/docs/arguments.html
// https://github.com/nodejs/nan/blob/master/doc/methods.md
//build:
// npm install -g node-gyp
// npm install --save nan
// node-gyp configure
// node-gyp build
//#define NODEJS_ADDON //TODO: selected from gyp bindings?
#endif


#ifdef BUILDING_NODE_EXTENSION //set by node-gyp
 #pragma message "compiled as Node.js addon"
// #include <node.h>
 #include <nan.h> //includes v8 also
// #include <v8.h>

namespace NodeAddon //namespace wrapper for Node.js functions; TODO: is this needed?
{
//#define JS_STR(...) Nan::New<v8::String>(__VA_ARGS__).ToLocalChecked()
//#define JS_STR(iso, ...) v8::String::NewFromUtf8(iso, __VA_ARGS__)
//TODO: convert to Nan?
#define JS_STR(iso, val) v8::String::NewFromUtf8(iso, val)
//#define JS_INT(iso, val) Nan::New<v8::Integer>(iso, val)
#define JS_INT(iso, val) v8::Integer::New(iso, val)
//#define JS_FLOAT(iso, val) Nan::New<v8::Number>(iso, val)
//#define JS_BOOL(iso, val) Nan::New<v8::Boolean>(iso, val)
#define JS_BOOL(iso, val) v8::Boolean::New(iso, val)


//display/throw Javascript err msg:
void errjs(v8::Isolate* iso, const char* errfmt, ...)
{
	va_list params;
	char buf[BUFSIZ];
	va_start(params, errfmt);
	vsnprintf(buf, sizeof(buf), errfmt, params); //TODO: send to stderr?
	va_end(params);
//	printf("THROW: %s\n", buf);
//	Nan::ThrowTypeError(buf);
//    iso->ThrowException(v8::Exception::TypeError(v8::String::NewFromUtf8(iso, buf)));
    fputline(buf, stderr); //send to console in case not seen by Node.js; don't mix with stdout
    iso->ThrowException(v8::Exception::TypeError(JS_STR(iso, buf)));
//    return;    
}


//void isRPi_js(const Nan::FunctionCallbackInfo<v8::Value>& args)
//void isRPi_js(const v8::FunctionCallbackInfo<v8::Value>& args)
NAN_METHOD(isRPi_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "isRPi: expected 0 args, got %d", info.Length()));

//    v8::Local<v8::Boolean> retval = JS_BOOL(iso, isRPi()); //v8::Boolean::New(iso, isRPi());
//    myprintf(3, "isRPi? %d" ENDCOLOR, isRPi());
    info.GetReturnValue().Set(JS_BOOL(iso, isRPi()));
}


#if 0 //TODO: getter
void Screen_js(v8::Local<v8::String>& name, const Nan::PropertyCallbackInfo<v8::Value>& info)
{
#else
//int Screen_js() {}
//void Screen_js(v8::Local<const v8::FunctionCallbackInfo<v8::Value>& info)
NAN_METHOD(Screen_js) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "Screen: expected 0 args, got %d", info.Length()));

    WH wh = Screen();
//    struct { int w, h; } wh = {Screen().w, Screen().h};
//    v8::Local<v8::Object> retval = Nan::New<v8::Object>();
    v8::Local<v8::Object> retval = v8::Object::New(iso);
//    v8::Local<v8::String> w_name = Nan::New<v8::String>("width").ToLocalChecked();
//    v8::Local<v8::String> h_name = Nan::New<v8::String>("height").ToLocalChecked();
//    v8::Local<v8::Number> retval = v8::Number::New(iso, ScreenWidth());
//    retval->Set(v8::String::NewFromUtf8(iso, "width"), v8::Number::New(iso, wh.w));    
//    retval->Set(v8::String::NewFromUtf8(iso, "height"), v8::Number::New(iso, wh.h));
    retval->Set(JS_STR(iso, "width"), JS_INT(iso, wh.w));
    retval->Set(JS_STR(iso, "height"), JS_INT(iso, wh.h));
//    myprintf(3, "screen: %d x %d" ENDCOLOR, wh.w, wh.h);
//    Nan::Set(retval, w_name, Nan::New<v8::Number>(wh.w));
//    Nan::Set(retval, h_name, Nan::New<v8::Number>(wh.h));
    info.GetReturnValue().Set(retval);
}
#endif


//?? https://nodejs.org/docs/latest/api/addons.html#addons_wrapping_c_objects
class DataCanvas_js: public Nan::ObjectWrap
{
private:
    DataCanvas inner;
public:
//    static void Init(v8::Handle<v8::Object> exports);
    static void Init(v8::Local<v8::Object> exports);
    static void Quit(void* ignored); //signature reqd for AtExit

//protected:
private:
    explicit DataCanvas_js(const char* title, int w, int h, bool pivot): inner(title, w, h, pivot) {};
//    virtual ~DataCanvas_js();
    ~DataCanvas_js() {};

//    static NAN_METHOD(New);
    static void New(const v8::FunctionCallbackInfo<v8::Value>& info); //TODO: convert to Nan?
//    static NAN_GETTER(WidthGetter);
//    static NAN_GETTER(PitchGetter);
    static NAN_METHOD(paint);
    static NAN_PROPERTY_GETTER(get_pivot);
    static NAN_PROPERTY_SETTER(set_pivot);
    static NAN_METHOD(release);
//    static void paint(const Nan::FunctionCallbackInfo<v8::Value>& info);
//private:
    static Nan::Persistent<v8::Function> constructor; //v8:: //TODO: Nan macro?
//    void *data;
};
Nan::Persistent<v8::Function> DataCanvas_js::constructor;


//export class to Javascript:
//TODO: convert to Nan?
//void DataCanvas_js::Init(v8::Handle<v8::Object> exports)
void DataCanvas_js::Init(v8::Local<v8::Object> exports)
{
    v8::Isolate* iso = exports->GetIsolate(); //~vm heap
//??    Nan::HandleScope scope;

//ctor:
    v8::Local<v8::FunctionTemplate> ctor = /*Nan::New<*/v8::FunctionTemplate::New(iso, DataCanvas_js::New);
//    v8::Local<v8::FunctionTemplate> ctor = Nan::New<v8::FunctionTemplate>(DataCanvas_js::New);
    ctor->InstanceTemplate()->SetInternalFieldCount(1);
    ctor->SetClassName(JS_STR(iso, "DataCanvas"));
//    ctor->SetClassName(Nan::New("DataCanvas").ToLocalChecked());

//prototype:
//    Nan::SetPrototypeMethod(ctor, "paint", save);// NODE_SET_PROTOTYPE_METHOD(ctor, "save", save);
//    v8::Local<v8::ObjectTemplate> proto = ctor->PrototypeTemplate();
//    Nan::SetPrototypeMethod(proto, "paint", paint);
//    NODE_SET_PROTOTYPE_METHOD(ctor, "paint", DataCanvas_js::paint);
    Nan::SetPrototypeMethod(ctor, "paint", DataCanvas_js::paint);
    Nan::SetPrototypeMethod(ctor, "release", DataCanvas_js::release);
    Nan::SetPrototypeMethod(ctor, "release", DataCanvas_js::release);
//    Nan::SetAccessor(proto,JS_STR("width"), WidthGetter);
//    Nan::SetAccessor(proto,JS_STR("height"), HeightGetter);
//    Nan::SetAccessor(proto,JS_STR("pitch"), PitchGetter);
//    Nan::SetAccessor(proto,JS_STR("src"), SrcGetter, SrcSetter);
    constructor.Reset(/*iso,*/ ctor->GetFunction()); //?? v8::Isolate::GetCurrent(), ctor->GetFunction());
//??    Nan::Set(exports, JS_STR("DataCanvas"), ctor->GetFunction());
    exports->Set(JS_STR(iso, "DataCanvas"), ctor->GetFunction());
//    exports->Set(Nan::New("DataCanvas").ToLocalChecked(), ctor->GetFunction());

//  FreeImage_Initialise(true);
//NO    DataCanvas::Init(); //will be done by inner
    node::AtExit(DataCanvas_js::Quit);
}


//instantiate new instance for Javascript:
//TODO: convert to Nan?
void DataCanvas_js::New(const v8::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(DataCanvas_js::New) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
#if 0
//int inval = To<int>(info[0]).FromJust();
//v8::Local<v8::Array> results = New<v8::Array>(len);
//    registerImage(image);
#endif
    if (info.IsConstructCall()) //Invoked as constructor: `new MyObject(...)`
    {
//    	if (info.Length() != 3) return_void(errjs(iso, "DataCanvas.New: expected 3 params, got: %d", info.Length()));
        v8::String::Utf8Value title(!info[0]->IsUndefined()? info[0]->ToString(): JS_STR(iso, ""));
//        double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
	    int w = info[1]->Uint32Value(); //args[0]->IsUndefined()
        int h = info[2]->Uint32Value(); //args[0]->IsUndefined()
        bool pivot = !info[3]->IsUndefined()? info[3]->BooleanValue(): true;
        DataCanvas_js* canvas = new DataCanvas_js(*title? *title: NULL, w, h, pivot);
        canvas->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    }
    else //Invoked as plain function `MyObject(...)`, turn into construct call.
    {
        const int argc = 1;
        v8::Local<v8::Value> argv[argc] = { info[0]};
        v8::Local<v8::Context> context = iso->GetCurrentContext();
        v8::Local<v8::Function> consobj = v8::Local<v8::Function>::New(iso, constructor);
        v8::Local<v8::Object> result = consobj->NewInstance(context, argc, argv).ToLocalChecked();
        info.GetReturnValue().Set(result);
    }
}


//xfr/xfm Javascript array to GPU:
void DataCanvas_js::paint(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(DataCanvas_js::paint) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length() != 1) return_void(errjs(iso, "DataCanvas.paint: expected 1 param, got %d", info.Length()));
	if (!info.Length() || !info[0]->IsUint32Array()) return_void(errjs(iso, "DataCanvas.paint: missing uint32 array param"));
    DataCanvas_js* canvas = Nan::ObjectWrap::Unwrap<DataCanvas_js>(info.Holder()); //info.This());
//void* p = handle->GetAlignedPointerFromInternalField(0); 

//https://stackoverflow.com/questions/28585387/node-c-addon-how-do-i-access-a-typed-array-float32array-when-its-beenn-pa
#if 0
//    Local<Array> input = Local<Array>::Cast(args[0]);
//    unsigned int num_locations = input->Length();
//    for (unsigned int i = 0; i < num_locations; i++) {
//      locations.push_back(
//        unpack_location(isolate, Local<Object>::Cast(input->Get(i)))
  void *buffer = node::Buffer::Data(info[1]);
	v8::Local<v8::Array> new_pixels = v8::Local<v8::Array>::Cast(args[0]);
	if (new_pixels->Length() != pixels.cast->w * pixels.cast->h) return_void(noderr("Paint: pixel array wrong length: %d (should be %d)", new_pixels->Length(), pixels.cast->w * pixels.cast->h));
  Local<ArrayBuffer> buffer = ArrayBuffer::New(Isolate::GetCurrent(), size);
  Local<Uint8ClampedArray> clampedArray = Uint8ClampedArray::New(buffer, 0, size);
 void *ptr= JSTypedArrayGetDataPtr(array);
    size_t length = JSTypedArrayGetLength(array);
    glBufferData(GL_ARRAY_BUFFER, length, ptr, GL_STATIC_DRAW);    
#endif
//    v8::Local<v8::Uint32Array> ary = args[0].As<Uint32Array>();
//    Nan::TypedArrayContents<uint32_t> pixels(info[0].As<v8::Uint32Array>());
//https://github.com/casualjavascript/blog/issues/12
    v8::Local<v8::Uint32Array> aryp = info[0].As<v8::Uint32Array>();
    if (aryp->Length() != canvas->inner.num_univ * canvas->inner.univ_len) return_void(errjs(iso, "DataCanvas.paint: array param bad length: is %d, should be %d", aryp->Length(), canvas->inner.num_univ * canvas->inner.univ_len));
    void *data = aryp->Buffer()->GetContents().Data();
    uint32_t* pixels = static_cast<uint32_t*>(data);
//myprintf(33, "js pixels 0x%x 0x%x 0x%x ..." ENDCOLOR, pixels[0], pixels[1], pixels[2]);
    if (!canvas->inner.Paint(pixels)) return_void(errjs(iso, "DataCanvas.paint: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}


NAN_PROPERTY_GETTER(get_pivot)
//void get_pivot(v8::Local<v8::String> property, Nan::NAN_PROPERTY_GETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
}


NAN_PROPERTY_SETTER(set_pivot)
//void set_pivot(v8::Local<v8::String> property, v8::Local<v8::Value> value, Nan::NAN_PROPERTY_SETTER_ARGS_TYPE info)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
}


void DataCanvas_js::release(const Nan::FunctionCallbackInfo<v8::Value>& info)
//NAN_METHOD(DataCanvas_js::release) //defines "info"; implicit HandleScope (~ v8 stack frame)
{
    v8::Isolate* iso = info.GetIsolate(); //~vm heap
    if (info.Length()) return_void(errjs(iso, "DataCanvas.release: expected 0 args, got %d", info.Length()));
    DataCanvas_js* canvas = Nan::ObjectWrap::Unwrap<DataCanvas_js>(info.Holder()); //info.This());
    if (!canvas->inner.Release()) return_void(errjs(iso, "DataCanvas.release: failed"));
    info.GetReturnValue().Set(0); //TODO: what value to return?
}


void DataCanvas_js::Quit(void* ignored)
{
    DataCanvas::Quit();
}


//tell Node.js about my entry points:
NAN_MODULE_INIT(exports_js) //defines target
//void exports_js(v8::Local<v8::Object> exports, v8::Local<v8::Object> module)
{
    v8::Isolate* iso = target->GetIsolate(); //~vm heap
//    NODE_SET_METHOD(exports, "isRPi", isRPi_js);
//    NODE_SET_METHOD(exports, "Screen", Screen_js); //TODO: property instead of method
    Nan::Export(target, "isRPi", isRPi_js);
    Nan::Export(target, "Screen", Screen_js);
//    target->SetAccessor(JS_STR(iso, "Screen"), Screen_js);
    DataCanvas_js::Init(target);
//  Nan::SetAccessor(proto, Nan::New("fillColor").ToLocalChecked(), GetFillColor);
//    NAN_GETTER(Screen_js);
//    Nan::SetAccessor(exports, Nan::New("Screen").ToLocalChecked(), Screen_js);
//    NAN_PROPERTY_GETTER(getter_js);
//    NODE_SET_METHOD(exports, "DataCanvas", DataCanvas_js);
//https://github.com/Automattic/node-canvas/blob/b470ce81aabe2a78d7cdd53143de2bee46b966a7/src/CanvasRenderingContext2d.cc#L764
//    NODE_SET_METHOD(module, "exports", CreateObject);

//    Init();
//    node::AtExit(Quit_js);
}

} //namespace

NODE_MODULE(data_canvas, NodeAddon::exports_js) //tells Node.js how to find my entry points
//NODE_MODULE(NODE_GYP_MODULE_NAME, NodeAddon::exports_js)
//NOTE: can't use special chars in module name here, but bindings.gyp overrides it anyway?
#endif //def BUILDING_NODE_EXTENSION


////////////////////////////////////////////////////////////////////////////////
////
/// Standalone/test interface
//

#ifndef BUILDING_NODE_EXTENSION //not being compiled by node-gyp
 #pragma message "compiled as stand-alone"

bool eventh(int = INT_MAX); //fwd ref

uint32_t PALETTE[] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE};

#define NUM_UNIV  20
#define UNIV_LEN  20

int main(int argc, const char* argv[])
{
    myprintf(1, CYAN_LT "test/standalone routine" ENDCOLOR);
//{
    DataCanvas canvas(0, NUM_UNIV, UNIV_LEN, false);
    uint32_t pixels[NUM_UNIV * UNIV_LEN] = {0};
#if 0 //gradient fade
    static int hue = 0;
    uint32_t color = hsv2rgb((double)(hue++ % 360) / 360, 1, 1); //gradient fade on hue
    surf.FillRect(RECT_ALL, color);
#endif
#if 1 //one-by-one rainbow
    for (int xy = 0;; ++xy) //xy < 10 * 10; ++xy)
    {
        int x = (xy / UNIV_LEN) % NUM_UNIV, y = xy % UNIV_LEN; //cycle thru [0..9,0..9]
        if (eventh(1)) break; //user quit
        uint32_t color = PALETTE[(x + y + xy / SIZE(pixels)) % SIZE(PALETTE)]; //vary each cycle
//        myprintf(1, BLUE_LT "px[%d, %d] = 0x%x" ENDCOLOR, x, y, color);
        pixels[xy % SIZE(pixels)] = color;
        canvas.Paint(pixels); //60 FPS
//        SDL_Delay(1000);
    }
#endif
    myprintf(1, GREEN_LT "done, wait 5 sec" ENDCOLOR);
//}
//    canvas.Paint(pixels);
    SDL_Delay(5000);
    return 0;
}


//handle windowing system events:
//NOTE: SDL_Event handling loop must be in main() thread
//(mainly for debug)
bool eventh(int max /*= INT_MAX*/)
{
//    myprintf(14, BLUE_LT "evth max %d" ENDCOLOR, max);
    while (max)
    {
    	SDL_Event evt;
//        SDL_PumpEvents(void); //not needed if calling SDL_PollEvent or SDL_WaitEvent
//       if (SDL_WaitEvent(&evt)) //execution suspends here while waiting on an event
		if (SDL_PollEvent(&evt))
		{
            myprintf(14, BLUE_LT "evt type 0x%x" ENDCOLOR, evt.type);
			if (evt.type == SDL_QUIT) return true; //quit = true; //return;
			if (evt.type == SDL_KEYDOWN)
            {
				myprintf(14, CYAN_LT "got key down 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
			if (evt.type == SDL_PRESSED)
            {
				myprintf(14, CYAN_LT "got key press 0x%x" ENDCOLOR, evt.key.keysym.sym);
				if (evt.key.keysym.sym == SDLK_ESCAPE) return true; //quit = true; //return; //key codes defined in /usr/include/SDL2/SDL_keycode.h
			}
/*
            if (kbhit())
            {
                char ch = getch();
                myprintf(14, "char 0x%x pressed\n", ch);
            }
*/
		}
//        const Uint8* keyst = SDL_GetKeyboardState(NULL);
//        if (keyst[SDLK_RETURN]) { myprintf(8, CYAN_LT "Return key pressed." ENDCOLOR); return true; }
//        if (keyst[SDLK_ESCAPE]) { myprintf(8, CYAN_LT "Escape key pressed." ENDCOLOR); return true; }
        if (max != INT_MAX) --max;
//        myprintf(14, BLUE_LT "no evts" ENDCOLOR);
    }
    return false; //no evt
}
#endif //ndef BUILDING_NODE_EXTENSION


////////////////////////////////////////////////////////////////////////////////
////
/// Graphics info debug
//

//SDL lib info:
void debug_info(SDL_lib* ignored)
{
    myprintf(1, CYAN_LT "Debug detail level = %d" ENDCOLOR, WANT_LEVEL);

    SDL_version ver;
    SDL_GetVersion(&ver);
    myprintf(12, BLUE_LT "SDL version %d.%d.%d, platform: '%s', #cores %d, ram %s MB, isRPi? %d" ENDCOLOR, ver.major, ver.minor, ver.patch, SDL_GetPlatform(), SDL_GetCPUCount() /*std::thread::hardware_concurrency()*/, commas(SDL_GetSystemRAM()), isRPi());

    myprintf(12, BLUE_LT "%d video driver(s):" ENDCOLOR, SDL_GetNumVideoDrivers());
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
        myprintf(12, BLUE_LT "Video driver[%d/%d]: name '%s'" ENDCOLOR, i, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver(i));
}


//SDL window info:
void debug_info(SDL_Window* window)
{
#if 0
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    myprintf(20, BLUE_LT "GL_VERSION: %s" ENDCOLOR, glGetString(GL_VERSION));
    myprintf(20, BLUE_LT "GL_RENDERER: %s" ENDCOLOR, glGetString(GL_RENDERER));
    myprintf(20, BLUE_LT "GL_SHADING_LANGUAGE_VERSION: %s" ENDCOLOR, glGetString(GL_SHADING_LANGUAGE_VERSION));
    myprintf(20, "GL_EXTENSIONS: %s" ENDCOLOR, glGetString(GL_EXTENSIONS));
    SDL_GL_DeleteContext(gl_context);
#endif

    int wndw, wndh;
    SDL_GL_GetDrawableSize(window, &wndw, &wndh);
//        return err(RED_LT "Can't get drawable window size" ENDCOLOR);
    uint32_t fmt = SDL_GetWindowPixelFormat(window);
//    if (fmt == SDL_PIXELFORMAT_UNKNOWN) return_void(err(RED_LT "Can't get window format" ENDCOLOR));
    uint32_t flags = SDL_GetWindowFlags(window);
    std::ostringstream desc;
    if (flags & SDL_WINDOW_FULLSCREEN) desc << ";FULLSCR";
    if (flags & SDL_WINDOW_OPENGL) desc << ";OPENGL";
    if (flags & SDL_WINDOW_SHOWN) desc << ";SHOWN";
    if (flags & SDL_WINDOW_HIDDEN) desc << ";HIDDEN";
    if (flags & SDL_WINDOW_BORDERLESS) desc << ";BORDERLESS";
    if (flags & SDL_WINDOW_RESIZABLE) desc << ";RESIZABLE";
    if (flags & SDL_WINDOW_MINIMIZED) desc << ";MIN";
    if (flags & SDL_WINDOW_MAXIMIZED) desc << ";MAX";
    if (flags & SDL_WINDOW_INPUT_GRABBED) desc << ";GRABBED";
    if (flags & SDL_WINDOW_INPUT_FOCUS) desc << ";FOCUS";
    if (flags & SDL_WINDOW_MOUSE_FOCUS) desc << ";MOUSE";
    if (flags & SDL_WINDOW_FOREIGN) desc << ";FOREIGN";
    if (!desc.tellp()) desc << ";";
    myprintf(12, BLUE_LT "window %dx%d, fmt %i bpp %s %s" ENDCOLOR, wndw, wndh, SDL_BITSPERPIXEL(fmt), SDL_PixelFormatShortName(fmt), desc.str().c_str() + 1);

#if 0
    myprintf(22, BLUE_LT "SDL_WINDOW_FULLSCREEN    [%c]" ENDCOLOR, (flags & SDL_WINDOW_FULLSCREEN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_OPENGL        [%c]" ENDCOLOR, (flags & SDL_WINDOW_OPENGL) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_SHOWN         [%c]" ENDCOLOR, (flags & SDL_WINDOW_SHOWN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_HIDDEN        [%c]" ENDCOLOR, (flags & SDL_WINDOW_HIDDEN) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_BORDERLESS    [%c]" ENDCOLOR, (flags & SDL_WINDOW_BORDERLESS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_RESIZABLE     [%c]" ENDCOLOR, (flags & SDL_WINDOW_RESIZABLE) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MINIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MINIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MAXIMIZED     [%c]" ENDCOLOR, (flags & SDL_WINDOW_MAXIMIZED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_INPUT_GRABBED [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_GRABBED) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_INPUT_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_INPUT_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_MOUSE_FOCUS   [%c]" ENDCOLOR, (flags & SDL_WINDOW_MOUSE_FOCUS) ? 'X' : ' ');
    myprintf(22, BLUE_LT "SDL_WINDOW_FOREIGN       [%c]" ENDCOLOR, (flags & SDL_WINDOW_FOREIGN) ? 'X' : ' '); 
#endif

//NO
//    SDL_Surface* wnd_surf = SDL_GetWindowSurface(window); //NOTE: wnd will dealloc, so don't need auto_ptr here
//    if (!wnd_surf) return_void(err(RED_LT "Can't get window surface" ENDCOLOR));
//NOTE: wnd_surf info is gone after SDL_CreateRenderer! (benign if info was saved already)
//    debug_info(wnd_surf);
}


//SDL_Renderer info:
void debug_info(SDL_Renderer* renderer)
{
    myprintf(12, BLUE_LT "%d render driver(s):" ENDCOLOR, SDL_GetNumRenderDrivers());
    for (int i = 0; i <= SDL_GetNumRenderDrivers(); ++i)
    {
        SDL_RendererInfo info;
        std::ostringstream which, fmts, count, flags;
        if (!i) which << "active";
        else which << i << "/" << SDL_GetNumRenderDrivers();
        if (!OK(i? SDL_GetRenderDriverInfo(i - 1, &info): SDL_GetRendererInfo(renderer, &info))) { err(RED_LT "Can't get renderer[%s] info" ENDCOLOR, which.str().c_str()); continue; }
        if (info.flags & SDL_RENDERER_SOFTWARE) flags << ";SW";
        if (info.flags & SDL_RENDERER_ACCELERATED) flags << ";ACCEL";
        if (info.flags & SDL_RENDERER_PRESENTVSYNC) flags << ";VSYNC";
        if (info.flags & SDL_RENDERER_TARGETTEXTURE) flags << ";TOTXR";
        if (info.flags & ~(SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE)) flags << ";????";
        if (!flags.tellp()) flags << ";";
        for (unsigned int i = 0; i < info.num_texture_formats; ++i) fmts << ", " << SDL_BITSPERPIXEL(info.texture_formats[i]) << " bpp " << skip(SDL_GetPixelFormatName(info.texture_formats[i]), "SDL_PIXELFORMAT_");
        if (!info.num_texture_formats) { count << "no fmts"; fmts << "  "; }
        else if (info.num_texture_formats != 1) count << info.num_texture_formats << " fmts: ";
        myprintf(12, BLUE_LT "Renderer[%s]: '%s', flags 0x%x %s, max %dx%d, %s%s" ENDCOLOR, which.str().c_str(), info.name, info.flags, flags.str().c_str() + 1, info.max_texture_width, info.max_texture_height, count.str().c_str(), fmts.str().c_str() + 2);
    }
}


//SDL_Surface info:
void debug_info(SDL_Surface* surf)
{
    int numfmt = 0;
    std::ostringstream fmts, count;
    for (SDL_PixelFormat* fmtptr = surf->format; fmtptr; fmtptr = fmtptr->next, ++numfmt)
        fmts << ";" << SDL_BITSPERPIXEL(fmtptr->format) << " bpp " << SDL_PixelFormatShortName(fmtptr->format);
    if (!numfmt) { count << "no fmts"; fmts << ";"; }
    else if (numfmt != 1) count << numfmt << " fmts: ";
//    if (want_fmts && (numfmt != want_fmts)) err(RED_LT "Unexpected #formats: %d (wanted %d)" ENDCOLOR, numfmt, want_fmts);
    myprintf(18, BLUE_LT "Surface 0x%x: %d x %d, pitch %s, size %s, %s%s" ENDCOLOR, toint(surf), surf->w, surf->h, commas(surf->pitch), commas(surf->h * surf->pitch), count.str().c_str(), fmts.str().c_str() + 1);
}


////////////////////////////////////////////////////////////////////////////////
////
/// Graphics helpers
//

//get max window size that will fit on screen:
//try to maintain 4:3 aspect ratio
WH MaxFit()
{
    WH wh = Screen();
    wh.h = std::min(wh.h, (uint16_t)(wh.w * 3 * 24 / 4 / 23.25));
    wh.w = std::min(wh.w, (uint16_t)(wh.h * 4 / 3));
    return wh;
}


///////////////////////////////////////////////////////////////////////////////
////
/// color helpers
//

uint32_t limit(uint32_t color)
{
#ifdef LIMIT_BRIGHTNESS
 #pragma message "limiting R+G+B brightness to " TOSTR(LIMIT_BRIGHTNESS)
    unsigned int r = R(color), g = G(color), b = B(color);
    unsigned int sum = r + g + b; //max = 3 * 255 = 765
    if (sum > LIMIT_BRIGHTNESS) //reduce brightness, try to keep relative colors
    {
//GLuint sv = color;
        r = rdiv(r * LIMIT_BRIGHTNESS, sum);
        g = rdiv(g * LIMIT_BRIGHTNESS, sum);
        b = rdiv(b * LIMIT_BRIGHTNESS, sum);
        color = (color & Amask(0xFF)) | Rmask(r) | Gmask(g) | Bmask(b);
//printf("REDUCE: 0x%x, sum %d, R %d, G %d, B %d => r %d, g %d, b %d, 0x%x\n", sv, sum, R(sv), G(sv), B(sv), r, g, b, color);
    }
#endif //def LIMIT_BRIGHTNESS
    return color;
}


//based on http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl:
uint32_t hsv2rgb(float h, float s, float v)
{
//    static const float K[4] = {1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0};
    float p[3] = {_abs(_fract(h + 1.) * 6 - 3), _abs(_fract(h + 2/3.) * 6 - 3), _abs(_fract(h + 1/3.) * 6 - 3)};
    float rgb[3] = {v * _mix(h, _clamp(p[0] - 1, 0., 1.), s), v * _mix(h, _clamp(p[1] - 1, 0., 1.), s), v * _mix(1, _clamp(p[2] - 1, 0., 1.), s)};
    uint32_t color = toARGB(255, 255 * rgb[0], 255 * rgb[1], 255 * rgb[2]);
//    myprintf(33, BLUE_LT "hsv(%f, %f, %f) => argb 0x%x" ENDCOLOR, h, s, v, color);
    return color;
}


//TODO?
uint32_t ARGB2ABGR(uint32_t color)
{
    return color;
}


////////////////////////////////////////////////////////////////////////////////
////
/// Misc helper functions
//

//display a console message:
//only for dev/debug
#undef printf //want real printf from now on
bool errprintf(const char* reason /*= 0*/, const char* fmt, ...)
{
    bool want_popup = (reason != (const char*)NOERROR);
    std::string details; //need to save err msg in case error below changes it
    if (want_popup)
    {
        if (!reason) reason = SDL_GetError();
        if (!reason || !reason[0]) reason = "(no details)";
        details = ": "; details += reason; //need to save err msg in case MessageBox error below changes it
    }
    char fmtbuf[500];
    va_list args;
    va_start(args, fmt);
    size_t needlen = vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, args);
    va_end(args);
//NOTE: assumea all Escape chars are for colors codes
//find last color code:
    size_t lastcolor_ofs = strlen(fmtbuf); //0;
    for (const char* bp = fmtbuf; (bp = strchr(bp, *ANSI_COLOR("?"))); lastcolor_ofs = bp++ - fmtbuf);
//find insertion point for details:
    char srcline[4]; //too short, but doesn't matter
    strncpy(srcline, ENDCOLOR, sizeof(srcline));
    *strpbrk(srcline, "0123456789") = '\0'; //trim after numeric part
    size_t srcline_ofs = strstr(fmtbuf, srcline) - fmtbuf; //, srcline_len);
    if (!srcline_ofs || (srcline_ofs > lastcolor_ofs)) srcline_ofs = lastcolor_ofs;
//    if (!lastcolor_ofs) lastcolor_ofs = strlen(fmtbuf);
//    printf("err: '%.*s', ofs %zu, reason: '%s'\n", strlinelen(fmt), fmt, ofs, reason.c_str());
//show warning if fmtbuf too short:
    std::ostringstream tooshort;
    if (needlen >= sizeof(fmtbuf)) tooshort << " (fmt buf too short: needed " << needlen << " bytes) ";
//get abreviated thread name:
//    const char* thrname = ThreadName(THIS_THREAD);
//    const char* lastchar = thrname + strlen(thrname) - 1;
    char shortname[3] = "";
//    if (strstr(thrname, "no name")) strcpy(shortname, "M");
//    else if (strstr(thrname, "unknown")) strcpy(shortname, "?");
//    else { if (!isdigit(*lastchar)) ++lastchar; shortname[0] = thrname[0]; strcpy(shortname + 1, lastchar); }
//    bool locked = console.busy/*.cast*/ && OK(SDL_LockMutex(console.busy/*.cast*/)); //can't use auto_ptr<> (recursion); NOTE: mutex might not be created yet
//insert details, fmtbuf warning (if applicable), and abreviated thread name:
    printf("%.*s%s%.*s%s!%s%s", (int)srcline_ofs, fmtbuf, details.c_str(), (int)(lastcolor_ofs - srcline_ofs), fmtbuf + srcline_ofs, tooshort.str().c_str(), shortname, fmtbuf + lastcolor_ofs);
fflush(stdout);
    if (want_popup) //TODO: send to stderr instead
    {
//strip ANSI color codes from string:
        std::smatch match;
        std::string nocolor;
        std::string str(fmtbuf);
        while (std::regex_search(str, match, ANSICC_re))
        {
            nocolor += match.prefix().str();
            str = match.suffix().str();
        }
        nocolor += str;
        if (!OK(SDL_ShowSimpleMessageBox(0, nocolor.c_str(), details.c_str() + 2, auto_ptr<SDL_Window>::latest)))
            printf(RED_LT "Show msg failed: %s" ENDCOLOR, SDL_GetError());
    }
//    if (locked) locked = !OK(SDL_UnlockMutex(console.busy/*.cast*/)); //CAUTION: stays locked across ShowMessage(); okay if system-modal?
    return false; //probable caller ret val
}


//insert commas into a numeric string (for readability):
const char* commas(int val)
{
    static int ff;
    static char buf[2][24]; //allow 2 simultaneous calls
    const int LIMIT = 4; //max #commas to insert
    char* bufp = buf[ff++ % SIZE(buf)] + LIMIT; //don't overwrite other values within same printf, allow space for commas
    for (int grplen = std::min(sprintf(bufp, "%d", val), LIMIT * 3) - 3; grplen > 0; grplen -= 3)
    {
        memmove(bufp - 1, bufp, grplen);
        (--bufp)[grplen] = ',';
    }
    return bufp;
}


//check for file existence:
bool exists(const char* path)
{
    struct stat info;
    return !stat(path, &info); //file exists
}


//EOF