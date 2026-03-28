// SPDX-FileCopyrightText: 2026 Nebulas Astra <https://github.com/nebulas-star>
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
//#define STBI_ONLY_PNG
#include "lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "lib/stb_image_write.h"

#include "dxt_compress.h"
#include "gt2_convert.h"

void png_to_gt2(char *input_dir, char *output_dir){
    int image_x, image_y, image_channels;
    uint8_t *image_data = stbi_load(input_dir, &image_x, &image_y, &image_channels, 0);

    bool have_alpha;
    float byte_per_pixel;
    char vitaFourCC[4];
    if (image_channels == 4){
        have_alpha = 1;
        byte_per_pixel = 1;
        memcpy(vitaFourCC, "UBC3", 4);
    }
    else if (image_channels == 3){
        have_alpha = 0;
        byte_per_pixel = 0.5;
        memcpy(vitaFourCC, "UBC1", 4);
    }
    else {
        printf("[Warning] Input image isn't a color image.");
        image_data = stbi_load(input_dir, &image_x, &image_y, &image_channels, 3);
        have_alpha = 0;
        byte_per_pixel = 0.5;
        memcpy(vitaFourCC, "UBC1", 4);
    }
    
    int dxt_size = get_dxt_buffer_size(image_x, image_y, have_alpha);
    uint8_t *dxt_data = (uint8_t *)malloc(dxt_size);
    rgba_to_dxt(image_data, image_x, image_y, have_alpha, dxt_data);
    stbi_image_free(image_data);

    char    *filename = get_filename(input_dir);
    gt2_header_build();
    gt2_metadata_build(image_x, image_y, byte_per_pixel, vitaFourCC, filename);
    uint8_t *gxt_data = (uint8_t *)malloc(image_x * image_y * byte_per_pixel);
    texture_swizzle(image_x, image_y, byte_per_pixel, dxt_data, gxt_data, 0);
    free(dxt_data);

    FILE *output_file = fopen(output_dir, "wb");
    fwrite(gt2_header, 1, 0x78, output_file);
    fwrite(gt2_metadata, 1, 0x58, output_file);
    fwrite(gxt_data, 1, image_x * image_y * byte_per_pixel, output_file);
    fclose(output_file);
    free(gxt_data);
}

void gt2_to_png(char *input_dir, char *output_dir){
    int image_x;
    int image_y;
    char vitaFourCC[4];
    uint8_t *gxt_data = gt2_load(input_dir, &image_x, &image_y, vitaFourCC);

    int image_channels;
    float byte_per_pixel;
    if (!memcmp(vitaFourCC, "UBC1", 4)){
        byte_per_pixel = 0.5;
        image_channels = 3;
    } 
    else if (!memcmp(vitaFourCC, "UBC3", 4)){
        byte_per_pixel = 1;
        image_channels = 4;
    }
    else {
        printf("[Error] Unsupported Texture Format:%c%c%c%c.", vitaFourCC[0], vitaFourCC[1], vitaFourCC[2], vitaFourCC[3]);
        exit(-1);
    }

    uint8_t *dxt_data = (uint8_t *)malloc(image_x * image_y * byte_per_pixel);
    texture_swizzle(image_x, image_y, byte_per_pixel, gxt_data, dxt_data, 1);
    uint8_t *rgba_data = dxt_to_rgba(dxt_data, image_x, image_y, image_channels);
    stbi_write_png(output_dir, image_x, image_y, 4, rgba_data, 0);
}

uint8_t gt2_magic[4] = {'G', 'T', 'X', 0x01};
uint8_t png_magic[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};


int main(int argc, char* argv[]){

    if (argc != 3){
        printf("Usage: GT2TextureConverter input output");
        exit(-1);
    }

    FILE *input = fopen(argv[1], "rb");
    uint8_t magic[8];
    fread(magic, 1, 8, input);
    fclose(input);

    if      (!memcmp(magic, gt2_magic, 4))
        gt2_to_png(argv[1], argv[2]);
    else if (!memcmp(magic, png_magic, 8))
        png_to_gt2(argv[1], argv[2]);
    else
        printf("[ERROR] Unsupported file format.");

    return 0;
}