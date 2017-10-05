//RGB finder pattern:

const float SPEED = 0.5; //animation speed (sec); NOTE: must match caller

//custom GPU fx function:
//caller can pass in a float to influence fx gen
//(not used in this simple demo)
vec4 gpufx(float x, float fromcpu)
{
//    float x = hUNM.s; //[0..NUM_UNIV); universe#
    float y = hUNM.t; //UNIV_LEN - hUNM.t - 1.0; //[0..UNIV_LEN); node# within universe
    float byte = floor(x / 8.0); //[0..2]; which color byte (R, G, B) to pivot; A is at end
    float repeat = 9.0 - mod(x, 8.0);

    vec4 color = IF(byte == 0.0, RED) + IF(byte == 1.0, GREEN) + IF(byte == 2.0, BLUE);
    if (mod(y - elapsed, repeat) != 0.0) color = BLACK;
    return color;
}

//eof
