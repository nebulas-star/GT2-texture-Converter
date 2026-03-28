// SPDX-FileCopyrightText: 2026 Nebulas Astra <https://github.com/nebulas-star>
// SPDX-License-Identifier: MIT
#ifndef __GT2_CONVERTER__
#define __GT2_CONVERTER__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h> 
#include "vita_swizzle.h"

#ifndef BUILDER 
#define BUILDER " GT2 Texture File for VITA/Compatible/Build: GT2textureConverter "
#endif

uint64_t fsize(FILE *fp){
    uint64_t n;
    fpos_t fpos;
    fgetpos(fp, &fpos);     //保存原位
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fsetpos(fp, &fpos);      //复位
    return n;
}

uint8_t *fcache_full(const char *filename){
    FILE *input = fopen(filename, "rb");
    uint64_t size = fsize(input);
    uint8_t *ptr = (uint8_t *)malloc(size);
    fread(ptr, 1, size, input);
    fclose(input);
    return ptr;
}

char* get_filename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    
    // 取两个分隔符中较后的一个
    const char* filename_start = path;
    if (last_slash != NULL && last_slash >= filename_start) 
        filename_start = last_slash + 1;
    if (last_backslash != NULL && last_backslash >= filename_start) 
        filename_start = last_backslash + 1;
    const char* last_dot = strrchr(filename_start, '.');

    size_t name_len;
    if (last_dot != NULL && last_dot > filename_start) 
        name_len = last_dot - filename_start;
    else 
        name_len = strlen(filename_start);
    char* result = (char*)malloc(name_len + 1);
    strncpy(result, filename_start, name_len);
    result[name_len] = '\0';
    
    return result;
}

uint8_t SingleChar[4];
uint8_t *membuild_32LE(uint32_t FourChar){
    SingleChar[0] = (FourChar >> 0) & 0xFF;
    SingleChar[1] = (FourChar >> 8) & 0xFF;
    SingleChar[2] = (FourChar >> 16) & 0xFF;
    SingleChar[3] = (FourChar >> 24) & 0xFF;
    return SingleChar;
}

uint8_t *gt2_load(char const *filename, int *image_x, int *image_y, char *vitaFourCC){
    uint8_t *gt2_data = fcache_full(filename);
    
    uint32_t gt2_header_size = ((gt2_data[7] << 24) | (gt2_data[6] << 16) | (gt2_data[5] << 8) | gt2_data[4]);
    uint32_t offset = gt2_header_size + 0x44;
    uint32_t filename_padding_length = ((gt2_data[offset + 3] << 24) | (gt2_data[offset + 2] << 16) | (gt2_data[offset + 1] << 8) | gt2_data[offset + 0]);
    uint32_t gt2_metadata_size = 0x44 + filename_padding_length;

    offset = gt2_header_size + 0x18;
    *image_x = ((gt2_data[offset + 3] << 24) | (gt2_data[offset + 2] << 16) | (gt2_data[offset + 1] << 8) | gt2_data[offset + 0]); 
    *image_y = ((gt2_data[offset + 7] << 24) | (gt2_data[offset + 6] << 16) | (gt2_data[offset + 5] << 8) | gt2_data[offset + 4]); 
    memcpy(vitaFourCC, (gt2_data + gt2_header_size + 0x28), 4);

    gt2_data = (gt2_data + gt2_header_size + gt2_metadata_size);
    return gt2_data;
}

uint8_t gt2_header[0x78] = {
    'G',  'T',  'X', 0x01,   0x78, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,
    0x68, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,
    0x68, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x41, 0x00, 0x00, 0x00,   0x42, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00,   // [0x34] = char [0x44]
};

uint8_t gt2_metadata[0x58] = {
    0x00, 0x05, 0x07, 0x00,   0x0F, 0x00, 0x00, 0x00,   0x10, 0x00, 0x00, 0x00,   0x3C, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00,   0x01, 0x00, 0x04, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,  // [0x18] = texture_width, [0x1C] = texture_height
    0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,  // [0x20] = texture_width * 4, [0x28] = vita_texture_FourCC, [0x2C] = byte_per_pixel * 16
    0x01, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,   0x14, 0x00, 0x00, 0x00,                                                      // [0x40] = texture_length, [0x48] = char[0x10]
};

void gt2_header_build(void){
    memcpy(gt2_header + 0x34, BUILDER, 0x42);
    gt2_header[0x77] = 0;
}

void gt2_metadata_build(uint32_t texture_width, uint32_t texture_height, float byte_per_pixel, char *FourCC, char *file_name){
    uint8_t *width     = membuild_32LE(texture_width);
    memcpy(gt2_metadata + 0x18, width, 4);
    uint8_t *height    = membuild_32LE(texture_height);
    memcpy(gt2_metadata + 0x1C, height, 4);
    uint8_t *Qwidth    = membuild_32LE(texture_width * 4);
    memcpy(gt2_metadata + 0x20, Qwidth, 4);
    memcpy(gt2_metadata + 0x28, FourCC, 4);
    uint8_t *blocksize = membuild_32LE(byte_per_pixel * 16);
    memcpy(gt2_metadata + 0x2C, blocksize, 4);
    uint8_t *length    = membuild_32LE(texture_width * texture_height * byte_per_pixel);
    memcpy(gt2_metadata + 0x40, length, 4);
    memcpy(gt2_metadata + 0x48, file_name, 0x10);
}

/*
uint8_t DDS_header[0x80] = {
    0x44, 0x44, 0x53, 0x20, 0x7c, 0x00, 0x00, 0x00, 0x07, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x40, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void DDS_header_build(uint32_t texture_width, uint32_t texture_height, char *dwFourCC){
    uint8_t *height = membuild_32LE(texture_height);
    memcpy(DDS_header + 0x0C, height, 4);
    uint8_t *width  = membuild_32LE(texture_width);
    memcpy(DDS_header + 0x10, width, 4);
    memcpy(DDS_header + 0x54, dwFourCC, 4);
}
*/

void texture_swizzle(uint32_t width, uint32_t height, float Bppx, uint8_t *blockStorage, uint8_t *image, bool flag)
{
    int blockSize = 16 * Bppx;
    uint32_t blockCountX = (width + 3) / 4;
    uint32_t blockCountY = (height + 3) / 4;
    
    for (uint32_t j = 0; j < blockCountY; j++)
    {
        for (uint32_t i = 0; i < blockCountX; i++)
        {
            int x = i, y = j;
            if (flag == 0)
                VitaSwizzle(&x, &y, blockCountX, blockCountY);
            if (flag == 1)
                VitaUnswizzle(&x, &y, blockCountX, blockCountY);
            uint32_t newBlock = y * blockCountX + x;
            uint32_t oldBlock = j * blockCountX + i;
            for (int z = 0; z < blockSize; z++)
                image[newBlock * blockSize + z] = blockStorage[oldBlock * blockSize + z];
        }
    }
}

#endif