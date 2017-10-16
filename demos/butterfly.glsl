//Butterfly effect generator:

const float speed = 1.0; //10; //must match caller; TODO: use a uniform?

//custom GPU fx function:
//caller can pass in a float to influence fx gen
//(not used in this simple demo)
vec4 gpufx(float x, float fromcpu)
{
//hw (univ#, node#, model#):
//these are ints, but used in floating arithmetic so just leave them as floats
//    float hw_univ = hUNM.s;
//    float hw_node = hUNM.t;
//    float hw_model = hUNM.p;
//    float x = hUNM.s; //[0..NUM_UNIV); universe#
    float y = hUNM.t; //UNIV_LEN - hUNM.t - 1.0; //[0..UNIV_LEN); node# within universe
    y = UNIV_LEN - y - 1.0; //flip y axis; (0,0) is top of screen
    float ofs = elapsed * speed; //generate ofs from progress info

//axis fixes: fix the colors for pixels at (0,1) and (1,0)
    if (EQ(x, 0.0) && EQ(y, 1.0)) y += 1.0;
    if (EQ(x, 1.0) && EQ(y, 0.0)) x += 1.0;
//based on http://mathworld.wolfram.com/ButterflyFunction.html
//    float num = abs((x * x - y * y) * sin(ofs + ((x + y) * PI * 2.0 / (UNIV_LEN + NUM_UNIV))));
    float num = abs((x * x - y * y) * sin(((ofs + x + y) * PI * 2.0 / (UNIV_LEN + NUM_UNIV))));
    float den = x * x + y * y;

    float hue = IIF(GT(den, 0.001), num / den, 0.0);
//hue = elapsed / duration; //debug
    vec3 color = hsv2rgb(vec3(hue, 1.0, 1.0));
    return vec4(color, 1.0); //set A so RGB value takes effect
}

//eof
