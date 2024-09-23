#ifndef Vita_Swizzle
#define Vita_Swizzle

// Unswizzle
// Code by Committee of Zero
//https://github.com/CommitteeOfZero/impacto

inline int Uint32Log2(uint32_t v) {
    unsigned int const b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    unsigned int const S[] = {1, 2, 4, 8, 16};
    int i;

    unsigned int r = 0;      // result of log2(v) will go here
    for (i = 4; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i]) {
            v >>= S[i];
            r |= S[i];
        }
    }
    return r;
}

uint32_t Compact1By1(uint32_t x) {
    x &= 0x55555555;                 // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    x = (x ^ (x >> 1)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x >> 2)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x >> 4)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x >> 8)) & 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    return x;
}
uint32_t DecodeMorton2X(uint32_t code) { return Compact1By1(code >> 0); }
uint32_t DecodeMorton2Y(uint32_t code) { return Compact1By1(code >> 1); }

void VitaUnswizzle(int *x, int *y, int width, int height) {

    int origX = *x, origY = *y;

    int i = (origY * width) + origX;
    int min = width < height ? width : height;
    int k = Uint32Log2(min);

    if (height < width) {   // XXXyxyxyx -> XXXxxxyyy
        int j = i >> (2 * k) << (2 * k) | (DecodeMorton2Y(i) & (min - 1)) << k | (DecodeMorton2X(i) & (min - 1)) << 0;
        *x = j / height;
        *y = j % height;
    } else {                // YYYyxyxyx -> YYYyyyxxx
        int j = i >> (2 * k) << (2 * k) | (DecodeMorton2X(i) & (min - 1)) << k | (DecodeMorton2Y(i) & (min - 1)) << 0;
        *x = j % width;
        *y = j / width;
    }
}

// Swizzle
// Code by nebulas

uint32_t BreakByte(uint32_t x) {                    // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = ((x & 0x0000FF00) << 8) ^ (x & 0x000000FF); // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = ((x & 0x00F000F0) << 4) ^ (x & 0x000F000F); // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = ((x & 0x0C0C0C0C) << 2) ^ (x & 0x03030303); // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = ((x & 0x22222222) << 1) ^ (x & 0x11111111); // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    return x;
}
uint32_t EncodeMorton2X(uint32_t code) { return (BreakByte(code) << 0); }
uint32_t EncodeMorton2Y(uint32_t code) { return (BreakByte(code) << 1); }

void VitaSwizzle(int *x, int *y, int width, int height) {

    int origX = *x, origY = *y;

    int min = width < height ? width : height;
    int k = Uint32Log2(min);

    if (height < width) {   // XXXyyyxxx -> XXXxyxyxy
        int i = (origX * height) + origY;
        int j = i >> (2 * k) << (2 * k) | EncodeMorton2Y(( i >> k ) & (min - 1)) | EncodeMorton2X(( i >> 0 ) & (min - 1));
        *x = j % width;
        *y = j / width;
    } else {                // YYYxxxyyy -> YYYxyxyxy
        int i = (origY * width) + origX;
        int j = i >> (2 * k) << (2 * k) | EncodeMorton2X(( i >> k ) & (min - 1)) | EncodeMorton2Y(( i >> 0 ) & (min - 1));
        *x = j % width;
        *y = j / width;
    }
}


#endif // Vita_Swizzle