//hacked up/cleaned up code from http://glslsandbox.com/e#42784.0 and others

//#ifdef GL_ES
//precision highp float;
//#endif


//uniform float time;
//uniform vec2 mouse;
//uniform vec2 resolution;

#define WANT_GRID
//#define DOWN_SCALE 4.0 //2.0
const float DOWN_SCALE = 4.0; //2.0 //how big to make the pixels

//#define CHAR_SIZE  vec2(6, 7)
//#define CHAR_SPACING  vec2(6, 9)
const vec2 CHAR_SIZE = vec2(6, 7);
const vec2 CHAR_SPACING = vec2(6, 9);

const vec2 resolution = vec2(WND_WIDTH,  WND_HEIGHT); //shim
const vec2 res = resolution.xy / DOWN_SCALE;
//vec2 start_pos = vec2(0);
vec2 print_pos = vec2(0);
vec2 print_pos_pre_move = vec2(0);
//vec3 text_color = vec3(1);
//float hash(float x);
//float linenum = 0.0;

/* 
Top left pixel is the most significant bit.
Bottom right pixel is the least significant bit.

 ███  |
█   █ |
█   █ |  
█   █ |
█████ |
█   █ |
█   █ |

011110 
100010
100010  
100010
111110
100010
100010

011100 (upper 21 bits)
100010 -> 011100 100010 100010 100 -> 935188
100010  
100
   010 (lower 21 bits)
111110 -> 010 111110 100010 100010 -> 780450
100010
100010

vec2(935188.0, 780450.0)
 */

//Text coloring
#define HEX(i) /*text_color =*/ mod(vec3(i / 65536, i / 256, i), vec3(256.0)) / 255.0
#define RGB(red, green, blue) /*text_color =*/ vec3(red, green, blue) //NOTE: had to change "r" var to "red" because preprocessor was matching it within "color"

#define STRWIDTH(c) (c * CHAR_SPACING.x)
#define STRHEIGHT(c) (c * CHAR_SPACING.y)
#define BEGIN_TEXT(x, y) print_pos = floor(vec2(x, y)); start_pos = floor(vec2(x, y)); linenum = 0.;


//glyphs:
//Automatically generated from the sprite sheet here: http://uzebox.org/wiki/index.php?title=File:Font6x8.png
//TODO: create node.js script to regen glyphs
//removed hash animation
//delayed applying text_color until the end
#define _  px_color += char(vec2(0.0, 0.0), uv); // * text_color; //added * text_color for consistency
#define _spc  px_color += char(vec2(0.0, 0.0), uv); // * text_color;
#define _exc  px_color += char(vec2(276705.0, 32776.0), uv); // * text_color;
#define _quo  px_color += char(vec2(1797408.0, 0.0), uv); // * text_color;
#define _hsh  px_color += char(vec2(10738.0, 21134484.0; // * text_color; //hash(time)), uv) * text_color;
#define _dol  px_color += char(vec2(538883.0, 19976.0), uv); // * text_color;
#define _pct  px_color += char(vec2(1664033.0, 68006.0), uv); // * text_color;
#define _amp  px_color += char(vec2(545090.0, 174362.0), uv); // * text_color;
#define _apo  px_color += char(vec2(798848.0, 0.0), uv); // * text_color;
#define _lbr  px_color += char(vec2(270466.0, 66568.0), uv); // * text_color;
#define _rbr  px_color += char(vec2(528449.0, 33296.0), uv); // * text_color;
#define _ast  px_color += char(vec2(10471.0, 1688832.0), uv); // * text_color;
#define _crs  px_color += char(vec2(4167.0, 1606144.0), uv); // * text_color;
#define _per  px_color += char(vec2(0.0, 1560.0), uv); // * text_color;
#define _dsh  px_color += char(vec2(7.0, 1572864.0), uv); // * text_color;
#define _com  px_color += char(vec2(0.0, 1544.0), uv); // * text_color;
#define _lsl  px_color += char(vec2(1057.0, 67584.0), uv); // * text_color;
#define _0  px_color += char(vec2(935221.0, 731292.0), uv); // * text_color;
#define _1  px_color += char(vec2(274497.0, 33308.0), uv); // * text_color;
#define _2  px_color += char(vec2(934929.0, 1116222.0), uv); // * text_color;
#define _3  px_color += char(vec2(934931.0, 1058972.0), uv); // * text_color;
#define _4  px_color += char(vec2(137380.0, 1302788.0), uv); // * text_color;
#define _5  px_color += char(vec2(2048263.0, 1058972.0), uv); // * text_color;
#define _6  px_color += char(vec2(401671.0, 1190044.0), uv); // * text_color;
#define _7  px_color += char(vec2(2032673.0, 66576.0), uv); // * text_color;
#define _8  px_color += char(vec2(935187.0, 1190044.0), uv); // * text_color;
#define _9  px_color += char(vec2(935187.0, 1581336.0), uv); // * text_color;
#define _col  px_color += char(vec2(195.0, 1560.0), uv); // * text_color;
#define _scl  px_color += char(vec2(195.0, 1544.0), uv); // * text_color;
#define _les  px_color += char(vec2(135300.0, 66052.0), uv); // * text_color;
#define _equ  px_color += char(vec2(496.0, 3968.0), uv); // * text_color;
#define _grt  px_color += char(vec2(528416.0, 541200.0), uv); // * text_color;
#define _que  px_color += char(vec2(934929.0, 1081352.0), uv); // * text_color;
#define _ats  px_color += char(vec2(935285.0, 714780.0), uv); // * text_color;
#define _A  px_color += char(vec2(935188.0, 780450.0), uv); // * text_color;
#define _B  px_color += char(vec2(1983767.0, 1190076.0), uv); // * text_color;
#define _C  px_color += char(vec2(935172.0, 133276.0), uv); // * text_color;
#define _D  px_color += char(vec2(1983764.0, 665788.0), uv); // * text_color;
#define _E  px_color += char(vec2(2048263.0, 1181758.0), uv); // * text_color;
#define _F  px_color += char(vec2(2048263.0, 1181728.0), uv); // * text_color;
#define _G  px_color += char(vec2(935173.0, 1714334.0), uv); // * text_color;
#define _H  px_color += char(vec2(1131799.0, 1714338.0), uv); // * text_color;
#define _I  px_color += char(vec2(921665.0, 33308.0), uv); // * text_color;
#define _J  px_color += char(vec2(66576.0, 665756.0), uv); // * text_color;
#define _K  px_color += char(vec2(1132870.0, 166178.0), uv); // * text_color;
#define _L  px_color += char(vec2(1065220.0, 133182.0), uv); // * text_color;
#define _M  px_color += char(vec2(1142100.0, 665762.0), uv); // * text_color;
#define _N  px_color += char(vec2(1140052.0, 1714338.0), uv); // * text_color;
#define _O  px_color += char(vec2(935188.0, 665756.0), uv); // * text_color;
#define _P  px_color += char(vec2(1983767.0, 1181728.0), uv); // * text_color;
#define _Q  px_color += char(vec2(935188.0, 698650.0), uv); // * text_color;
#define _R  px_color += char(vec2(1983767.0, 1198242.0), uv); // * text_color;
#define _S  px_color += char(vec2(935171.0, 1058972.0), uv); // * text_color;
#define _T  px_color += char(vec2(2035777.0, 33288.0), uv); // * text_color;
#define _U  px_color += char(vec2(1131796.0, 665756.0), uv); // * text_color;
#define _V  px_color += char(vec2(1131796.0, 664840.0), uv); // * text_color;
#define _W  px_color += char(vec2(1131861.0, 699028.0), uv); // * text_color;
#define _X  px_color += char(vec2(1131681.0, 84130.0), uv); // * text_color;
#define _Y  px_color += char(vec2(1131794.0, 1081864.0), uv); // * text_color;
#define _Z  px_color += char(vec2(1968194.0, 133180.0), uv); // * text_color;
#define _lsb  px_color += char(vec2(925826.0, 66588.0), uv); // * text_color;
#define _rsl  px_color += char(vec2(16513.0, 16512.0), uv); // * text_color;
#define _rsb  px_color += char(vec2(919584.0, 1065244.0), uv); // * text_color;
#define _pow  px_color += char(vec2(272656.0, 0.0), uv); // * text_color;
#define _usc  px_color += char(vec2(0.0, 62.0), uv); // * text_color;
#define _a  px_color += char(vec2(224.0, 649374.0), uv); // * text_color;
#define _b  px_color += char(vec2(1065444.0, 665788.0), uv); // * text_color;
#define _c  px_color += char(vec2(228.0, 657564.0), uv); // * text_color;
#define _d  px_color += char(vec2(66804.0, 665758.0), uv); // * text_color;
#define _e  px_color += char(vec2(228.0, 772124.0), uv); // * text_color;
#define _f  px_color += char(vec2(401543.0, 1115152.0), uv); // * text_color;
#define _g  px_color += char(vec2(244.0, 665474.0), uv); // * text_color;
#define _h  px_color += char(vec2(1065444.0, 665762.0), uv); // * text_color;
#define _i  px_color += char(vec2(262209.0, 33292.0), uv); // * text_color;
#define _j  px_color += char(vec2(131168.0, 1066252.0), uv); // * text_color;
#define _k  px_color += char(vec2(1065253.0, 199204.0), uv); // * text_color;
#define _l  px_color += char(vec2(266305.0, 33292.0), uv); // * text_color;
#define _m  px_color += char(vec2(421.0, 698530.0), uv); // * text_color;
#define _n  px_color += char(vec2(452.0, 1198372.0), uv); // * text_color;
#define _o  px_color += char(vec2(228.0, 665756.0), uv); // * text_color;
#define _p  px_color += char(vec2(484.0, 667424.0), uv); // * text_color;
#define _q  px_color += char(vec2(244.0, 665474.0), uv); // * text_color;
#define _r  px_color += char(vec2(354.0, 590904.0), uv); // * text_color;
#define _s  px_color += char(vec2(228.0, 114844.0), uv); // * text_color;
#define _t  px_color += char(vec2(8674.0, 66824.0), uv); // * text_color;
#define _u  px_color += char(vec2(292.0, 1198868.0), uv); // * text_color;
#define _v  px_color += char(vec2(276.0, 664840.0), uv); // * text_color;
#define _w  px_color += char(vec2(276.0, 700308.0), uv); // * text_color;
#define _x  px_color += char(vec2(292.0, 1149220.0), uv); // * text_color;
#define _y  px_color += char(vec2(292.0, 1163824.0), uv); // * text_color;
#define _z  px_color += char(vec2(480.0, 1148988.0), uv); // * text_color;
#define _lpa  px_color += char(vec2(401542.0, 66572.0), uv); // * text_color;
#define _bar  px_color += char(vec2(266304.0, 33288.0), uv); // * text_color;
#define _rpa  px_color += char(vec2(788512.0, 1589528.0), uv); // * text_color;
#define _tid  px_color += char(vec2(675840.0, 0.0), uv); // * text_color;
#define _lar  px_color += char(vec2(8387.0, 1147904.0), uv); // * text_color;
#define _gay  px_color += char(vec2(133120.0, 0.0), uv); // * text_color;
#define _nl  print_pos = start_pos - (++linenum) * vec2(0, CHAR_SPACING.y);


//Extracts bit b from the given number.
float extract_bit(float n, float b)
{
	b = clamp(b, -1.0, 22.0);
	return floor(mod(floor(n / pow(2.0, floor(b))), 2.0));   
}


//Returns the pixel at uv in the given bit-packed sprite.
float sprite(vec2 spr, /*vec2 size,*/ vec2 uv)
{
    vec2 size = CHAR_SIZE;
	uv = floor(uv);
	float bit = (size.x - uv.x - 1.0) + uv.y * size.x;  
	bool bounds = all(greaterThanEqual(uv, vec2(0))) && all(lessThan(uv, size)); 
//	return bounds ? extract_bit(spr.x, bit - 21.0) + extract_bit(spr.y, bit) : 0.0;
	return IIF(bounds, extract_bit(spr.x, bit - 21.0) + extract_bit(spr.y, bit), 0.0);
}


//Prints a character and moves the print position forward by 1 character width.
vec3 char(vec2 ch, vec2 uv)
{
	float px = sprite(ch, /*CHAR_SIZE,*/ uv - print_pos);
	print_pos.x += CHAR_SPACING.x;
	return vec3(px);
}


//example textout function:
//moved some vars in here so it can be called multiple times
vec4 Textout_example() //vec2 uv)
{
    vec2 start_pos = vec2(0);
	vec2 uv = floor(gl_FragCoord.xy / DOWN_SCALE);
//    vec2 resolution = vec2(WND_WIDTH, WND_HEIGHT); //shim
//    vec2 res = resolution.xy / DOWN_SCALE;
	vec3 px_color = vec3(0.0);
    vec3 text_color = vec3(1);
    float linenum = 0.0;

	vec2 center_pos = vec2(res.x/2.0 - STRWIDTH(20.0)/2.0, res.y/1.5 - STRHEIGHT(1.0)/2.0);
   	BEGIN_TEXT(center_pos.x, center_pos.y)
	print_pos += vec2(20.0, 0.0); //vec2(20.+cos(time * 6.) * 7., sin(time * 3.) * 7.);
	
//	text_color = RGB(sin(2. * time+uv.x * 0.03)/2. +0.5,  -sin(4. * time-uv.y * 0.03)/2. +0.5,  cos(5. * time+dot(cos(uv), sin(uv))/3.)/2. +0.5)
    text_color = RGB(0.8, 0.3, 0.8);
    _R _P _i _ _G _P _U _exc

    px_color *= text_color; //just apply color once at end
//I know MAD instructions are efficient, but there is no need for it on individual chars above
//also might be a code space savings by consolidating into 1 multiply
//OTOH, if GLSL compiled inlines functions anyway, then maybe not

    if (px_color != vec3(0.0))
    {
#ifdef WANT_GRID
//show a dot matrix in background (looks nicer):
	    uv = gl_FragCoord.xy / DOWN_SCALE;
	    px_color = px_color * 0.9 + 0.1;
	    px_color *= (1. - distance(mod(uv, vec2(1.0)), vec2(0.65))) * 1.2;
#endif
    }
		
	return vec4(px_color, 1.0);
}

//float hash(float x)
//{
//	return fract(fract(x) * 1234.56789 * fract(fract(-x) * 1234.56789));	
//}


/*
//show a dot matrix in background:
vec3 TextOnGrid()
{
	vec2 uv = gl_FragCoord.xy / DOWN_SCALE;
//	vec2 duv = floor(gl_FragCoord.xy / DOWN_SCALE);
    
	vec3 pixel = Text(); //duv);
    
//show pixel grid:
	vec3 color = pixel * 0.9 + 0.1;
	color *= (1. - distance(mod(uv, vec2(1.0)), vec2(0.65))) * 1.2;

    return color;
}
*/


/*
void printf_main( void )
{
//	vec2 uv = gl_FragCoord.xy / DOWN_SCALE;
//	vec2 duv = floor(gl_FragCoord.xy / DOWN_SCALE);
    
//	vec3 pixel = Text(duv);
    
//	vec3 color = pixel * 0.9 + 0.1;
//	color *= (1. - distance(mod(uv, vec2(1.0)), vec2(0.65))) * 1.2;
//    vec3 color = pixel;
    vec3 color = Text();
	gl_FragColor = vec4(vec3(color),  1.0);
}
*/


vec4 printf(vec2 start_pos, vec3 text_color, float val)
{
//    vec2 start_pos = vec2(0);
	vec2 uv = floor(gl_FragCoord.xy / DOWN_SCALE);
    vec3 px_color = vec3(0.0);
//    vec3 text_color = vec3(1);
    float linenum = 0.0;
    start_pos *= resolution;

	vec2 center_pos = vec2(res.x/2.0 - STRWIDTH(20.0)/2.0, res.y/1.5 - STRHEIGHT(1.0)/2.0);
    start_pos = center_pos;
   	BEGIN_TEXT(start_pos.x, start_pos.y)
	print_pos += vec2(20.0, 0.0); //vec2(20.+cos(time * 6.) * 7., sin(time * 3.) * 7.);
	
//	text_color = RGB(sin(2. * time+uv.x * 0.03)/2. +0.5,  -sin(4. * time-uv.y * 0.03)/2. +0.5,  cos(5. * time+dot(cos(uv), sin(uv))/3.)/2. +0.5)
//    text_color = RGB(0.8, 0.3, 0.8);
    float digit = 0;
    _R _P _i _ _G _P _U _exc

    px_color *= text_color; //just apply color once at end
//I know MAD instructions are efficient, but there is no need for it on individual chars above
//also might be a code space savings by consolidating into 1 multiply
//OTOH, if GLSL compiled inlines functions anyway, then maybe not

    if (px_color != vec3(0.0))
    {
#ifdef WANT_GRID
//show a dot matrix in background (looks nicer):
        uv = gl_FragCoord.xy / DOWN_SCALE;
        px_color = px_color * 0.9 + 0.1;
        px_color *= (1. - distance(mod(uv, vec2(1.0)), vec2(0.65))) * 1.2;
#endif
    }
		
	return vec4(px_color, 1.0);
}


//eof
