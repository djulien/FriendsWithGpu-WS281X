//Butterfly effect generator:

//custom GPU fx function:
//caller can pass in a 24-bit value to control the fx gen
//(not used in this simple demo)
vec4 gpufx(vec3 selectfx)
{
//hw (univ#, node#, model#):
//these are ints, but used in floating arithmetic so just leave them as floats
//    float hw_univ = hUNM.s;
//    float hw_node = hUNM.t;
//    float hw_model = hUNM.p;
    float x = hUNM.s;
    float y = hUNM.t;
    float ofs = elapsed * 4.0; // / 0.01;

//axis fixes: fix the colors for pixels at (0,1) and (1,0)
    if (EQ(x, 0.0) && EQ(y, 1.0)) y += 1.0;
    if (EQ(x, 1.0) && EQ(y, 0.0)) x += 1.0;
//based on http://mathworld.wolfram.com/ButterflyFunction.html
    float num = abs((x * x - y * y) * sin(ofs + ((x + y) * PI * 2.0 / (UNIV_LEN + NUM_UNIV))));
    float den = x * x + y * y;

    float hue = IIF(GT(den, 0.001), num / den, 0.0);
    vec3 color = hsv2rgb(vec3(hue, 1.0, 1.0));
    return vec4(color, 1.0); //set A so RGB value takes effect
}

//eof
