#include <stdio.h> //printf
#include <stdint.h>
#include <stdlib.h> //rand

//kludge: need nested macros to stringize correctly:
//https://stackoverflow.com/questions/2849832/c-c-line-number
#define TOSTR(str)  TOSTR_NESTED(str)
#define TOSTR_NESTED(str)  #str

//ANSI color codes (for console output):
//https://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_COLOR(code)  "\x1b[" code "m"
#define RED_LT  ANSI_COLOR("1;31") //too dark: "0;31"
#define GREEN_LT  ANSI_COLOR("1;32")
#define YELLOW_LT  ANSI_COLOR("1;33")
#define BLUE_LT  ANSI_COLOR("1;34")
#define MAGENTA_LT  ANSI_COLOR("1;35")
#define PINK_LT  MAGENTA_LT
#define CYAN_LT  ANSI_COLOR("1;36")
#define GRAY_LT  ANSI_COLOR("0;37")
//#define ENDCOLOR  ANSI_COLOR("0")
//append the src line# to make debug easier:
#define ENDCOLOR_ATLINE(n)  " &" TOSTR(n) ANSI_COLOR("0") "\n"
#define ENDCOLOR_MYLINE  ENDCOLOR_ATLINE(%d) //NOTE: requires extra param
#define ENDCOLOR  ENDCOLOR_ATLINE(__LINE__)


#if 0
struct
{
    uint8_t delay, row, cols;
} list[56]; //168 bytes
struct
{
    uint8_t numrc;
    uint8_t rowmap;
    uint8_t columns[8];
} rcinfo[56];
uint8_t nument = 0;
uint8_t rowmap[256];

struct
{
    uint8_t rowmap, cols;
} list[256];
#endif


#if 1 //BINARY SEARCH
#define NUM_SSR  (1 << CHPLEX_RCSIZE)
#define CHPLEX_RCSIZE  3
#define CHPLEX_RCMASK  (NUM_SSR - 1)
#define ROW(rc)  ((rc) >> CHPLEX_RCSIZE)
#define ROW_bits(rc)  ((rc) & ~CHPLEX_RCMASK)
#define COL(rc)  ((rc) & CHPLEX_RCMASK)
#define ISDIAG(rc)  (COL(rc) == ROW(rc))
#define HASDIAG(rc)  (COL(rc) > ROW(rc))

#define xchg(a, b) { int wreg = a ^ b; a ^= wreg; b ^= wreg; }

//sorted indices:
//in order to reduce memory shuffling, only indices are sorted
uint8_t sorted[NUM_SSR * (NUM_SSR - 1)]; //56 bytes
//statically allocated delay info:
//never moves after allocation, but could be updated
struct DimRowEntry
{
    uint8_t delay; //brightness
//    uint8_t rownum_numcols; //#col upper, row# lower; holds up to 16 each [0..15]
//    uint8_t colmap; //bitmap of columns for this row
//   uint8_t rowmap; //bitmap of rows
    uint8_t numrows;
    uint8_t colmaps[8]; //bitmap of columns for each row
} DimRowList[NUM_SSR * (NUM_SSR - 1)]; //3*56 = 168 bytes    //10*56 = 560 bytes (616 bytes total used during sort, max 169 needed for final list)
uint8_t total_rows = 0;
//uint8_t numrows[NUM_SSR * (NUM_SSR - 1)];
uint8_t count = 0; //#dim slots allocated
uint8_t rcinx = 0; //raw (row, col) address; ch*plex diagonal address will be skipped

void write(int val)
{
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
    if (count >= NUM_SSR * (NUM_SSR - 1)) { printf(RED_LT "overflow" ENDCOLOR); return; }
    struct DimRowEntry* ptr = &DimRowList[count];
//    ptr->rownum_numcols = ROW_bits(rcinx) | 1;
//    ptr->colmap = 0x80 >> COL(rcinx);
    ptr->delay = val;
//    ptr->rowmap = 0x80 >> ROW(rcinx);
    for (int i = 0; i < 8; ++i) ptr->colmaps[i] = 0; //TODO: better to do this 1x at start, or incrementally as needed?
    ptr->colmaps[ROW(rcinx)] = 0x80 >> COL(rcinx);
    ptr->numrows = 1;
    ++total_rows;
    ++count;
    ++rcinx;
}

void init_list()
{
//    count = 0;
//    write(0); //create a reusable null entry
//    DimRowList[0].delay = DimRowList[0].rownum_numcols = DimRowList[0].cols = 0; //create reusable null entry
    total_rows = 0;
#if 0
    struct DimRowEntry* ptr = &DimRowList[0];
//    ptr->delay = ptr->rownum_numcols = 0; //ptr->rowmap = 0;
    ptr->delay = 0;
//    for (int i = 0; i < 8; ++i) ptr->colmaps[i] = 0;
    ptr->numrows = 0;
    sorted[0] = 0;
    count = 1;
#else
    count = 0;
#endif
    rcinx = 0;
}

void showkeys(int newvalue)
{
    printf(BLUE_LT "keys now (%d): ", newvalue);
    for (int i = 0; i < count; ++i)
        printf("%s[%d] %d, ", (!i || (DimRowList[sorted[i - 1]].delay > DimRowList[sorted[i]].delay))? GREEN_LT: RED_LT, i, DimRowList[sorted[i]].delay);
    printf(ENDCOLOR);
}

void show_list(const char* desc)
{
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
//    showkeys(-233);
    printf(CYAN_LT "%s %d entries (%d total rows):" ENDCOLOR, desc, count, total_rows);
    for (int i = 0; i < count; ++i)
    {
//        if (ISDIAG(i)) continue; //skip diagonal ch*plex row/col address
//        int ii = i - ROW(i) - HASDIAG(i); //skip diagonal ch*plex row/col address
        struct DimRowEntry* ptr = &DimRowList[sorted[i]];
//        printf(PINK_LT "[%d/%d=%d]: delay %d, row# %d, #cols %d, cols 0x%x" ENDCOLOR, i, count, sorted[i], ptr->delay, ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), ptr->colmap);
        printf(PINK_LT "[%d/%d=%d]: delay %d, #rows %d, cols 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x" ENDCOLOR, i, count, sorted[i], ptr->delay, ptr->numrows, ptr->colmaps[0], ptr->colmaps[1], ptr->colmaps[2], ptr->colmaps[3], ptr->colmaps[4], ptr->colmaps[5], ptr->colmaps[6], ptr->colmaps[7]);
//        if (!ptr->delay) break; //eof
    }
}

void insert(int newvalue)
{
    printf(BLUE_LT "INS[%d] %d [r %d,c %d]: ", count, newvalue, ROW(rcinx), COL(rcinx));
    if (ISDIAG(rcinx)) { ++rcinx; printf(BLUE_LT "skip diagonal" ENDCOLOR); return; }
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
    if (!newvalue) { /*sorted[count - 1] = 0*/; ++rcinx; printf(BLUE_LT "skip null" ENDCOLOR); return; } //don't need to store this one
    if (rcinx >= NUM_SSR * NUM_SSR) { printf(RED_LT "overflow" ENDCOLOR); return; }
//    if (newvalue == 233) showkeys(newvalue);
    int start, end;
//check if entry already exists using binary search:
    for (start = 0, end = count; start < end;)
    {
        int mid = (start + end) / 2;
        struct DimRowEntry* ptr = &DimRowList[sorted[mid]];
        int cmpto = ptr->delay;
//        if (newvalue == 233) printf("cmp start %d, end %d, mid %d val %d\n", start, end, mid, ptr->delay);
//NOTE: sort in descending order
        if (newvalue > ptr->delay) { end = mid; continue; } //search first half
        if (newvalue < ptr->delay) { start = mid + 1; continue; } //search second half
//printf("new val[%d] %d?, row %d, col %d, vs row %d, cols %d 0x%x, ofs %d" ENDCOLOR, mid, newvalue, ROW(rcinx), COL(rcinx), ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), ptr->colmap, mid);
printf("new val[%d] %d?, row %d, col %d, vs cols %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x %s0x%x, ofs %d" ENDCOLOR, mid, newvalue, ROW(rcinx), COL(rcinx), "*" + (ROW(rcinx) != 0), ptr->colmaps[0], "*" + (ROW(rcinx) != 1), ptr->colmaps[1], "*" + (ROW(rcinx) != 2), ptr->colmaps[2], "*" + (ROW(rcinx) != 3), ptr->colmaps[3], "*" + (ROW(rcinx) != 4), ptr->colmaps[4], "*" + (ROW(rcinx) != 5), ptr->colmaps[5], "*" + (ROW(rcinx) != 6), ptr->colmaps[6], "*" + (ROW(rcinx) != 7), ptr->colmaps[7], mid);
//        if (ROW_bits(rcinx) > ROW_bits(ptr->rownum_numcols)) { end = mid; continue; }
//        if (ROW_bits(rcinx) < ROW_bits(ptr->rownum_numcols)) { start = mid + 1; continue; }
//collision:
//        if (!(ptr->rowmap & (0x80 >> ROW(rcinx)))) ptr->colmaps[COL(rcinx)] = 0; //TODO: better to do this incrementally, or 1x when entry first created?
        if (!ptr->colmaps[ROW(rcinx)]) { ++ptr->numrows; ++total_rows; }
        ptr->colmaps[ROW(rcinx)] |= 0x80 >> COL(rcinx);
//        ptr->rowmap |= 0x80 >> ROW(rcinx);
//printf(YELLOW_LT "found: add col, new count: %d" ENDCOLOR, COL(ptr->rownum_numcols + 1));
//        if (!COL(ptr->rownum_numcols + 1)) printf(RED_LT "#col wrap" ENDCOLOR);
//        ++ptr->rownum_numcols;
//        ptr->colmap |= 0x80 >> COL(rcinx);
//fill in list tail:
//        sorted[count] = count;
//        write(0); //null (off) entry
        ++rcinx;
        return;
    }
//create a new entry, insert into correct position:
printf(BLUE_LT "ins new val %d at %d, shift %d entries" ENDCOLOR, newvalue, start, count - start);
    for (int i = count; i > start; --i) sorted[i] = sorted[i - 1];
    sorted[start] = count;
    write(newvalue);
//    if (newvalue == 233) showkeys(newvalue);
}

void insert(uint8_t* list56)
{
    init_list();
    for (int i = 0; i < 56; ++i) insert(list56[i]);
//    for (int i = 0; i < 56; ++i) new_insert(list56[i]);
//    for (int i = 0; i < delay_count; ++i) printf("delay[%d/%d]: %d, # %d\n", i, delay_count, dim_list[i].delay, dim_list[i].numrows);
}

int Log2(int val)
{
    for (int i = 0; i < 8; ++i)
        if (val >= 0x80 >> i) return 7 - i;
    return -1;
}

//resolve delay conflicts (multiple rows competing for same dimming slot):
//assign dimming slots to each row
void resolve_conflicts()
{
//    int count = rcinx - ROW(rcinx) - HASDIAG(rcinx); //skip diagonal ch*plex row/col address
    printf(BLUE_LT "resolve dups %d -> %d" ENDCOLOR, rcinx, count);
#if 0
    for (int i = 0+1+1; i < rcinx; ++i) //skip diag + first ent (need 2 to compare)
    {
        if (ISDIAG(i)) continue; //skip diagonal ch*plex row/col address
        int ii = i - ROW(i) - HASDIAG(i); //skip diagonal ch*plex row/col address
        ListEntry* prev = &DispList[sorted[ii - 1]];
        ListEntry* ptr = &DispList[sorted[ii]];
        if (ptr->delay != prev->delay) continue;
        if (COL(ptr->rownum_numcols) >= COL(prev->rownum_numcols)) continue; //put "more important" rows first
        printf(YELLOW_LT "swap needed at [%d] row %d col %d" ENDCOLOR, ii, ROW(i), COL(i));
    }
#endif
//first pass: favor rows with more columns
//    for (int i = 0+1; i < count; ++i) //skip first entry; nothing to compare it to
//    {
//        if (ISDIAG(i)) continue; //skip diagonal ch*plex row/col address
//        int ii = i - ROW(i) - HASDIAG(i); //skip diagonal ch*plex row/col address
//do insertion sort into second sort list:
//assume #row conflicts is small and just use linear search
//        for (int j = i; j > 0; --j)
//    uint8_t newsort[NUM_SSR * (NUM_SSR - 1)]; //56 bytes
#if 0 //TODO: is this worth doing?
//    uint8_t numrows[NUM_SSR * (NUM_SSR - 1)];
    for (int i = 0; i < count; ++i) //re-order entries; put busier rows first
    {
//        int save = sorted[i];
        int next = i;
        DimRowEntry* ptr = &DimRowList[sorted[next]];
        if (!ptr->delay) break; //eof
//        numrows[count] = 1;
        for (int j = i + 1; j < count; ++j) //find next (max) for this dim level
        {
            DimRowEntry* other = &DimRowList[sorted[j]];
            if (other->delay != ptr->delay) break; //{ numrows[count] = other - ptr; break; }
            if (COL(other->rownum_numcols) <= COL(ptr->rownum_numcols)) continue;
//            sorted[j - 1] = sorted[j];
            next = j; //this one should come next
            ptr = &DimRowList[sorted[next]];
        }
        if (next == i) continue;
//        if (i == j) continue;
//        sorted[j] = save;
        DimRowEntry* thiss = &DimRowList[sorted[i]];
        printf(BLUE_LT "xchg [%d] row# %d, #cols %d 0x%x <-> [%d] row# %d, #cols %d 0x%x" ENDCOLOR, i, ROW(thiss->rownum_numcols), COL(thiss->rownum_numcols), thiss->colmap, next, ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), ptr->colmap);
        xchg(sorted[i], sorted[next]);
//        --i; //repeat
    }
//return;
    show_list("reorder dups");
#endif
#if 0
//second pass: reassign slots as needed
    uint8_8 prevdelay;
    for (int i = 0; i < count; ++i) //adjust delays
    {
        DimRowEntry* ptr = &DimRowList[sorted[i]];
        int min = count - i - 1, max = 255 - i;
        if (ptr->delay < min) { printf(BLUE_LT "Too low[%d]: inc %d [%d,%d] to %d" ENDCOLOR, i, ptr->delay, ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), min); ptr->delay = min; continue; }
        if (ptr->delay > max) { printf(BLUE_LT "Too high[%d]: dec %d [%d,%d] to %d" ENDCOLOR, i, ptr->delay, ROW(ptr->rownum_numcols), COL(ptr->rownum_numcols), max); ptr->delay = max; continue; }
        if (!i) continue;
        DimRowEntry* prev = &DimRowList[sorted[i - 1]];
        prevdelay = ptr->delay;
        if (prev->delay != ptr->delay) continue; //no conflict
//        if (prev->delay < 255) { printf(BLUE_LT "inc prev delay[%d] %d" ENDCOLOR, sorted[i - 1], ptr->delay); ++prev->delay; continue; }

    }
#endif
//    struct
//    {
//        uint8_t delay, row, colmap;
//    } slots[56];
//    uint8_t numslots = 0;
//    uint8_t delay[8][7+1];
    struct
    {
        uint8_t delay, rowmap, colmap;
    } DispList[56+1]; //3*56+1 == 169 bytes
    int disp_count = 0;
    uint8_t max = 255, min = total_rows;
    for (int i = 0; i < count; ++i)
    {
//this entry uses dimming slots [delay + (numrows - 1) / 2 .. delay - numrows / 2]
//#define UPSHIFT(ptr)  ((ptr->numrows - 1) / 2)
//#define DOWNSHIFT(ptr)  (ptr->numrows / 2)
//#define FIRST_SLOT(ptr)  (ptr->delay + UPSHIFT(ptr))
//#define LAST_SLOT(ptr)  (ptr->delay - DOWNSHIFT(ptr))
//        struct DimRowEntry* prev = i? &DimRowList[sorted[i - 1]]: 0;
        struct DimRowEntry* ptr = &DimRowList[sorted[i]];
//        struct DimRowEntry* next = (i < count - 1)? &DimRowList[sorted[i + 1]]: 0;
#if 0
        if (prev && (FIRST_SLOT(ptr) >= LAST_SLOT(prev)) //need to shift later
            if (next && (FIRST_SLOT(next) >= LAST_SLOT(ptr)) //need
            if (
        if (ptr->delay + ptr->numrows / 2 > 255) ptr->delay += ptr->numrows / 2;
#endif
        min -= ptr->numrows; //update min for this group
        int adjust = (ptr->numrows - 1) / 2; //UPSHIFT(ptr);
        if (ptr->delay + adjust > max) { adjust = max - ptr->delay; printf(YELLOW_LT "can only upshift delay[%d/%d] %d by <= %d" ENDCOLOR, i, count, ptr->delay, adjust); }
        else if (ptr->delay + adjust - ptr->numrows < min) { adjust = min - (ptr->delay - ptr->numrows); printf(YELLOW_LT "must upshift delay[%d/%d] %d by >= %d" ENDCOLOR, i, count, ptr->delay, adjust); }
        else if (adjust) printf(BLUE_LT "upshifted delay[%d/%d] %d by +%d" ENDCOLOR, i, count, ptr->delay, adjust);
        ptr->delay += adjust;
//TODO: order rows according to #cols?
        bool firstrow = true;
        for (int r = 0; r < 8; ++r)
        {
            if (!ptr->colmaps[r]) continue;
            DispList[disp_count].delay = firstrow? max - ptr->delay + 1: 1; //ptr->delay--;
            DispList[disp_count].rowmap = 0x80 >> r;
            DispList[disp_count].colmap = ptr->colmaps[r];
            firstrow = false;
            ++disp_count;
        }
//        ptr->delay += ptr->numrows; //restore for debug display
        max = ptr->delay - ptr->numrows; //update max for next group
    }
    DispList[disp_count].delay = 0;
//return;
    printf("disp list %d ents:\n", disp_count);
    int dim = 256;
    for (int i = 0; i <= disp_count; ++i)
        printf("disp[%d/%d]: delay %d (dim %d), rowmap 0x%x (row %d), colmap 0x%x\n", i, disp_count, DispList[i].delay, dim -= DispList[i].delay, DispList[i].rowmap, Log2(DispList[i].rowmap), DispList[i].colmap);
}
#endif


#if 0
//new way:
struct DimEntry
{
    uint8_t delay; //brightness
    uint8_t numrows;
} dim_list[56];
uint8_t delay_count = 0;

void new_insert(uint8_t newvalue)
{
    if (!newvalue) return; //don't need to store this one
    int start, end;
//check if entry already exists using binary search:
    for (start = 0, end = delay_count; start < end;)
    {
        int mid = (start + end) / 2;
//        ListEntry* ptr = &DispList[sorted[mid]];
        struct DimEntry* ptr = &dim_list[mid];
//NOTE: sort in descending order
        if (newvalue > ptr->delay) { end = mid; continue; } //search first half
        if (newvalue < ptr->delay) { start = mid + 1; continue; } //search second half
//printf("new val[%d] %d?, row %d, col %d, vs row %d, cols 0x%x, ofs %d" ENDCOLOR, mid, newvalue, ROW(rcinx), COL(rcinx), ROW(ptr->rownum_numcols), ptr->cols, mid);
//        if (ROW_bits(rcinx) > ROW_bits(ptr->rownum_numcols)) { end = mid; continue; }
//        if (ROW_bits(rcinx) < ROW_bits(ptr->rownum_numcols)) { start = mid + 1; continue; }
//collision:
//printf(YELLOW_LT "found: add col, new count: %d" ENDCOLOR, COL(ptr->rownum_numcols + 1));
//        if (!COL(ptr->rownum_numcols + 1)) printf(RED_LT "#col wrap" ENDCOLOR);
//        ++ptr->rownum_numcols;
//        ptr->cols |= 0x80 >> COL(rcinx);
//fill in list tail:
//        sorted[count] = count;
//        write(0); //null (off) entry
//        ++rcinx;
        ++ptr->numrows;
        return;
    }
//create a new entry, insert into correct position:
//printf(BLUE_LT "ins new val %d at %d, shift %d entries" ENDCOLOR, newvalue, start, count - start);
    for (int i = delay_count; i > start; --i) dim_list[i] = dim_list[i - 1];
//    sorted[start] = count;
//    write(newvalue);
    dim_list[start].delay = newvalue;
    dim_list[start].numrows = 1;
    ++delay_count;
}
#endif



#if 0 //LINEAR SEARCH
#define NUM_SSR  (1 << CHPLEX_RCSIZE)
#define CHPLEX_RCSIZE  3
#define CHPLEX_RCMASK  (NUM_SSR - 1)

uint8_t sorted[NUM_SSR * NUM_SSR];
typedef struct
{
    uint8_t delay; //brightness
    uint8_t rownum_numcol; //#col upper, row# lower; holds up to 16 each [0..15]
    uint8_t cols; //col bitmask
} ListEntry;
ListEntry DispList[NUM_SSR * NUM_SSR];
uint8_t count = 0;

void init_list()
{
    count = 0;
}
void write(int val)
{
    DispList[count].delay = val;
    DispList[count].rownum_numcol = (count & ~CHPLEX_RCMASK) | 1;
    DispList[count].cols = 0x80 >> (count & CHPLEX_RCMASK);
    ++count;
}
void insert(int newvalue)
{
    for (int i = 0; i < count; ++i)
    {
        ListEntry* ptr = &DispList[sorted[i]];
        if (ptr->delay < newvalue) continue; //keep searching
        if (ptr->delay == newvalue)
        {
            if (ptr->rownum_numcol < count & ~CHPLEX_RCMASK) continue;
            if (!((ptr->rownum_numcol ^ count) & ~CHPLEX_RCMASK)) //collision
            {
                ++ptr->rownum_numcol;
                ptr->cols |= 0x80 >> (count & CHPLEX_RCMASK);
                return;
            }
        }
//insert here:
        for (int j = count; j > i; --j) sorted[j] = sorted[j - 1];
        sorted[i] = count;
        write(newvalue);
        return;
    }
    sorted[count] = count;
    write(newvalue);
}
void show_list()
{
    printf("current list %d entries: \n", count);
    for (int i = 0; i < count; ++i)
    {
        ListEntry* ptr = &DispList[sorted[i]];
        printf("[%d/%d=%d]: delay %d, row# %d, #col %d, cols 0x%x\n", i, count, sorted[i], ptr->delay, ptr->rownum_numcol >> CHPLEX_RCSIZE, ptr->rownum_numcol & CHPLEX_RCMASK, ptr->cols);
    }
}
#endif


#if 0
struct
{
    int value;
//    struct Node* next;
    int next;
} Nodes[1+56]; //use first entry as head ptr for simpler addressing logic
//Node* head = NULL;
int newinx = 1;

void init_list()
{
    for (int i = 0; i < 1+56; ++i) //fill with junk
    {
        Nodes[i].value = -1;
        Nodes[i].next = -1;
    }
    Nodes[0].next = 0; //head ptr
}

void insert(int newvalue)
{
    int ptr;
    for (ptr = 0; Nodes[ptr].next; ptr = Nodes[ptr].next)
        if (newvalue < Nodes[ptr].value) break; //insert here
    Nodes[newinx].value = value;
    Nodes[newinx].next = Nodes[ptr].next;
    Nodes[ptr].next = newinx;
    ++newinx;
}

void show_list()
{
    printf("current list %d entries: ", newinx);
    for (int next = headinx; next != newinx; next = Nodes[next].next)
        printf("%d [%d], ", Nodes[next].value, Nodes[next].next);
    printf("\n");
}
#endif

#define REPEAT(n, val)  REPEAT##n(val)
#define REPEAT1(val)  val
#define REPEAT2(val)  REPEAT1(val), val
#define REPEAT3(val)  REPEAT2(val), val
#define REPEAT4(val)  REPEAT3(val), val
#define REPEAT5(val)  REPEAT4(val), val
#define REPEAT6(val)  REPEAT5(val), val
#define REPEAT7(val)  REPEAT6(val), val
#define REPEAT8(val)  REPEAT7(val), val
#define REPEAT9(val)  REPEAT8(val), val
#define REPEAT10(val)  REPEAT9(val), val
#define REPEAT11(val)  REPEAT10(val), val
#define REPEAT12(val)  REPEAT11(val), val
#define REPEAT13(val)  REPEAT12(val), val
#define REPEAT14(val)  REPEAT13(val), val
#define REPEAT15(val)  REPEAT14(val), val
#define REPEAT16(val)  REPEAT15(val), val


int main()
{
#if 0
    init_list();
    show_list("empty");

    uint8_t list1[56] = {0, 3};
    init_list();
    insert(list1);
    show_list("singleton");

    uint8_t list2[56] = {3, 10, 2, 10, 20, 19, 1, REPEAT(4, 0), 10, REPEAT(16, 0), 10};
    init_list();
    insert(list2);
    show_list("test list");
#endif

//    insert(3); //(0, 0)
    uint8_t list1[56] = {0, 3};
    insert(list1);
    show_list("singleton");

#if 0
    insert(10); //(0, 1)
    insert(2); //(0, 2)
    insert(10); //(0, 3)
    insert(20); //(0, 4)
    insert(19); //(0, 5)
    insert(1); //(0, 6)
    for (int i = 0; i < 4; ++i) insert(0); //(0, 7), (1, 0), (1, 1), (1, 2)
    insert(10); //(1, 3)
    for (int i = 0; i < 2*8; ++i) insert(0); //(1, 4) .. (3, 3)
    insert(10); //(3, 4)
#else
    uint8_t list2[56] = {3, 10, 2, 10, 20, 19, 1, REPEAT(4, 0), 10, REPEAT(16, 0), 10};
    insert(list2);
#endif
    show_list("test list2");
    resolve_conflicts();
    show_list("resolve conflicts");

    uint8_t list3[56] = {REPEAT(4, 255)};
    insert(list3);
    show_list("test list3");
    resolve_conflicts();
    show_list("resolve conflicts");

    init_list();
//    srand(time(NULL)); //random seed
    uint8_t common[4+4];
    for (int i = 0; i < 4+4; ++i) common[i] = rand() % 256;
    for (int i = 0; i <= NUM_SSR * NUM_SSR; ++i)
    {
        int which = rand();
        insert(!(which & 1)? common[(which / 2) % 4]: !(which & 8)? common[4 + (which / 4) % 4]: which & 0xff);
    }
    printf(BLUE_LT "common: %d %d %d %d, unc: %d %d %d %d" ENDCOLOR, common[0], common[1], common[2], common[3], common[4], common[5], common[6], common[7]);
    show_list("random list");
    resolve_conflicts();
    show_list("resolve conflicts");
}


#if 0
typedef struct Node
{
    int value;
    struct Node* next;
} Node;

//from https://codereview.stackexchange.com/questions/8521/insert-sort-on-a-linked-list
/*public static*/ Node insertNode(Node node) {
//    if (node == null)
//        return null;

    // Make the first node the start of the sorted list.
    Node sortedList = node;
    node = node.next;
    sortedList.next = null;

    while(node != null) {
        // Advance the nodes.
        final Node current = node;
        node = node.next;

        // Find where current belongs.
        if (current.value < sortedList.value) {
            // Current is new sorted head.
            current.next = sortedList;
            sortedList = current;
        } else {
            // Search list for correct position of current.
            Node search = sortedList;
            while(search.next != null && current.value > search.next.value)
                search = search.next;

            // current goes after search.
            current.next = search.next;
            search.next = current;
        }
    }

    return sortedList;
}
#endif


#if 0
//from http://www.geeksforgeeks.org/insertion-sort-for-singly-linked-list/
/* C program for insertion sort on a linked list */
#include<stdio.h>
#include<stdlib.h>

/* Link list node */
struct Node
{
    int data;
    struct Node* next;
};

// Function to insert a given node in a sorted linked list
void sortedInsert(struct Node**, struct Node*);

// function to sort a singly linked list using insertion sort
void insertionSort(struct Node **head_ref)
{
    // Initialize sorted linked list
    struct Node *sorted = NULL;

    // Traverse the given linked list and insert every
    // node to sorted
    struct Node *current = *head_ref;
    while (current != NULL)
    {
        // Store next for next iteration
        struct Node *next = current->next;

        // insert current in sorted linked list
        sortedInsert(&sorted, current);

        // Update current
        current = next;
    }

    // Update head_ref to point to sorted linked list
    *head_ref = sorted;
}

/* function to insert a new_node in a list. Note that this
  function expects a pointer to head_ref as this can modify the
  head of the input linked list (similar to push())*/
void sortedInsert(struct Node** head_ref, struct Node* new_node)
{
    struct Node* current;
    /* Special case for the head end */
    if (*head_ref == NULL || (*head_ref)->data >= new_node->data)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }
    else
    {
        /* Locate the node before the point of insertion */
        current = *head_ref;
        while (current->next!=NULL &&
               current->next->data < new_node->data)
        {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

/* BELOW FUNCTIONS ARE JUST UTILITY TO TEST sortedInsert */

/* Function to print linked list */
void printList(struct Node *head)
{
    struct Node *temp = head;
    while(temp != NULL)
    {
        printf("%d  ", temp->data);
        temp = temp->next;
    }
}

/* A utility function to insert a node at the beginning of linked list */
void push(struct Node** head_ref, int new_data)
{
    /* allocate node */
    struct Node* new_node = new Node;

    /* put in the data  */
    new_node->data  = new_data;

    /* link the old list off the new node */
    new_node->next = (*head_ref);

    /* move the head to point to the new node */
    (*head_ref)    = new_node;
}


// Driver program to test above functions
int main()
{
    struct Node *a = NULL;
    push(&a, 5);
    push(&a, 20);
    push(&a, 4);
    push(&a, 3);
    push(&a, 30);

    printf("Linked List before sorting \n");
    printList(a);

    insertionSort(&a);

    printf("\nLinked List after sorting \n");
    printList(a);

    return 0;
}
#endif

//eof
