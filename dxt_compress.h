// SPDX-FileCopyrightText: 2026 Nebulas Astra <https://github.com/nebulas-star>
// SPDX-License-Identifier: MIT
#include <stdint.h>
#include <stdlib.h>

// dxt块压缩
// https://github.com/nothings/stb/blob/master/stb_dxt.h
#define STB_DXT_IMPLEMENTATION
#include "lib/stb_dxt.h"

// dxt块解压
#include "lib/s3tc.h"

/**
 * 将RGBA数据压缩为DXT格式
 * 
 * @param rgba_data       输入RGBA数据，每个像素4字节（R,G,B,A），按行存储
 * @param width           图像宽度（像素）
 * @param height          图像高度（像素）
 * @param compress_to_dxt5 压缩模式：0=压缩为DXT1，1=压缩为DXT5
 * @param out_dxt_data    输出压缩后的DXT数据，需要调用者分配足够的内存
 * @return                0=成功，-1=参数错误
 */
int rgba_to_dxt(const uint8_t* rgba_data, int width, int height, int compress_to_dxt5, uint8_t* out_dxt_data) {
    // 参数检查
    if (!rgba_data || !out_dxt_data || width <= 0 || height <= 0)
        return -1;
    
    // 计算块数量
    int blocks_x = (width + 3) / 4;
    int blocks_y = (height + 3) / 4;
    
    // 每个块的大小：DXT1=8字节，DXT5=16字节
    int block_size = compress_to_dxt5 ? 16 : 8;
    
    // 压缩质量标志，可根据需要调整
    int mode = STB_DXT_HIGHQUAL;  // 高质量模式
    
    // 逐块压缩
    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            // 当前块的RGBA数据（4x4像素）
            uint8_t block_rgba[4 * 4 * 4]; // 16个像素 * 4通道
            
            // 提取当前块的16个像素
            for (int py = 0; py < 4; py++) {
                for (int px = 0; px < 4; px++) {
                    int img_x = bx * 4 + px;
                    int img_y = by * 4 + py;
                    
                    int src_idx;
                    if (img_x < width && img_y < height) {
                        // 在图像范围内，使用实际像素
                        src_idx = (img_y * width + img_x) * 4;
                    } else {
                        // 超出图像范围，填充透明黑色
                        src_idx = -1;
                    }
                    
                    int dst_idx = (py * 4 + px) * 4;
                    
                    if (src_idx >= 0) {
                        block_rgba[dst_idx + 0] = rgba_data[src_idx + 0];
                        block_rgba[dst_idx + 1] = rgba_data[src_idx + 1];
                        block_rgba[dst_idx + 2] = rgba_data[src_idx + 2];
                        block_rgba[dst_idx + 3] = rgba_data[src_idx + 3];
                    } else {
                        // 边界填充：使用透明黑色
                        block_rgba[dst_idx + 0] = 0;
                        block_rgba[dst_idx + 1] = 0;
                        block_rgba[dst_idx + 2] = 0;
                        block_rgba[dst_idx + 3] = 0;
                    }
                }
            }
            
            // 计算当前块在输出缓冲区中的位置
            int block_idx = (by * blocks_x + bx) * block_size;
            
            // 调用压缩函数 - 通过alpha参数区分DXT1和DXT5
            // compress_to_dxt5 = 1 时，alpha=1，压缩Alpha通道（DXT5）
            // compress_to_dxt5 = 0 时，alpha=0，忽略Alpha通道（DXT1）
            stb_compress_dxt_block(out_dxt_data + block_idx, block_rgba, compress_to_dxt5, mode);
        }
    }
    return 0;
}

/**
 * 计算压缩后数据的大小
 * 
 * @param width           图像宽度（像素）
 * @param height          图像高度（像素）
 * @param compress_to_dxt5 压缩模式：0=DXT1，1=DXT5
 * @return                压缩后数据的大小（字节）
 */
int get_dxt_buffer_size(int width, int height, int compress_to_dxt5) {
    int blocks_x = (width + 3) / 4;
    int blocks_y = (height + 3) / 4;
    int block_size = compress_to_dxt5 ? 16 : 8;
    return blocks_x * blocks_y * block_size;
}



static uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a;
}

// 解压DXT1块
static void decompress_dxt1_block(uint32_t x, uint32_t y, uint32_t width, 
                                   const uint8_t *block_storage, uint32_t *image)
{
    uint16_t color0 = *(const uint16_t *)(block_storage);
    uint16_t color1 = *(const uint16_t *)(block_storage + 2);
    uint32_t code = *(const uint32_t *)(block_storage + 4);
    
    uint32_t temp;
    uint8_t r0, g0, b0, r1, g1, b1;
    
    // 解压颜色0（RGB565 -> RGB888）
    temp = ((color0 >> 11) * 255 + 16);
    r0 = (uint8_t)((temp / 32 + temp) / 32);
    temp = (((color0 & 0x07E0) >> 5) * 255 + 32);
    g0 = (uint8_t)((temp / 64 + temp) / 64);
    temp = ((color0 & 0x001F) * 255 + 16);
    b0 = (uint8_t)((temp / 32 + temp) / 32);
    
    // 解压颜色1
    temp = ((color1 >> 11) * 255 + 16);
    r1 = (uint8_t)((temp / 32 + temp) / 32);
    temp = (((color1 & 0x07E0) >> 5) * 255 + 32);
    g1 = (uint8_t)((temp / 64 + temp) / 64);
    temp = ((color1 & 0x001F) * 255 + 16);
    b1 = (uint8_t)((temp / 32 + temp) / 32);
    
    // 解压4x4像素块
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            uint8_t position_code = (code >> (2 * (4 * j + i))) & 0x03;
            uint32_t final_color = 0;
            
            if (color0 > color1) {
                // 4色模式
                switch (position_code) {
                    case 0:
                        final_color = pack_rgba(r0, g0, b0, 255);
                        break;
                    case 1:
                        final_color = pack_rgba(r1, g1, b1, 255);
                        break;
                    case 2:
                        final_color = pack_rgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, 
                                                (2 * b0 + b1) / 3, 255);
                        break;
                    case 3:
                        final_color = pack_rgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, 
                                                (b0 + 2 * b1) / 3, 255);
                        break;
                }
            } else {
                // 3色模式（透明）
                switch (position_code) {
                    case 0:
                        final_color = pack_rgba(r0, g0, b0, 255);
                        break;
                    case 1:
                        final_color = pack_rgba(r1, g1, b1, 255);
                        break;
                    case 2:
                        final_color = pack_rgba((r0 + r1) / 2, (g0 + g1) / 2, 
                                                (b0 + b1) / 2, 255);
                        break;
                    case 3:
                        final_color = pack_rgba(0, 0, 0, 0);  // 完全透明
                        break;
                }
            }
            
            if (x + i < width) {
                image[(y + j) * width + (x + i)] = final_color;
            }
        }
    }
}

// 解压DXT5块
static void decompress_dxt5_block(uint32_t x, uint32_t y, uint32_t width,
                                   const uint8_t *block_storage, uint32_t *image)
{
    uint8_t alpha0 = block_storage[0];
    uint8_t alpha1 = block_storage[1];
    
    const uint8_t *bits = block_storage + 2;
    uint32_t alpha_code1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    uint16_t alpha_code2 = bits[0] | (bits[1] << 8);
    
    uint16_t color0 = *(const uint16_t *)(block_storage + 8);
    uint16_t color1 = *(const uint16_t *)(block_storage + 10);
    uint32_t code = *(const uint32_t *)(block_storage + 12);
    
    uint32_t temp;
    uint8_t r0, g0, b0, r1, g1, b1;
    
    // 解压颜色0
    temp = ((color0 >> 11) * 255 + 16);
    r0 = (uint8_t)((temp / 32 + temp) / 32);
    temp = (((color0 & 0x07E0) >> 5) * 255 + 32);
    g0 = (uint8_t)((temp / 64 + temp) / 64);
    temp = ((color0 & 0x001F) * 255 + 16);
    b0 = (uint8_t)((temp / 32 + temp) / 32);
    
    // 解压颜色1
    temp = ((color1 >> 11) * 255 + 16);
    r1 = (uint8_t)((temp / 32 + temp) / 32);
    temp = (((color1 & 0x07E0) >> 5) * 255 + 32);
    g1 = (uint8_t)((temp / 64 + temp) / 64);
    temp = ((color1 & 0x001F) * 255 + 16);
    b1 = (uint8_t)((temp / 32 + temp) / 32);
    
    // 解压4x4像素块
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int alpha_code_index = 3 * (4 * j + i);
            int alpha_code;
            
            // 提取alpha代码（3位）
            if (alpha_code_index <= 12) {
                alpha_code = (alpha_code2 >> alpha_code_index) & 0x07;
            } else if (alpha_code_index == 15) {
                alpha_code = (alpha_code2 >> 15) | ((alpha_code1 << 1) & 0x06);
            } else {
                alpha_code = (alpha_code1 >> (alpha_code_index - 16)) & 0x07;
            }
            
            // 解压alpha值
            uint8_t final_alpha;
            if (alpha_code == 0) {
                final_alpha = alpha0;
            } else if (alpha_code == 1) {
                final_alpha = alpha1;
            } else {
                if (alpha0 > alpha1) {
                    // 6个中间值线性插值
                    final_alpha = (uint8_t)(((8 - alpha_code) * alpha0 + 
                                             (alpha_code - 1) * alpha1) / 7);
                } else {
                    // 4个中间值线性插值，最后两个是0和255
                    if (alpha_code == 6)
                        final_alpha = 0;
                    else if (alpha_code == 7)
                        final_alpha = 255;
                    else
                        final_alpha = (uint8_t)(((6 - alpha_code) * alpha0 + 
                                                 (alpha_code - 1) * alpha1) / 5);
                }
            }
            
            // 解压颜色
            uint8_t color_code = (code >> (2 * (4 * j + i))) & 0x03;
            uint32_t final_color;
            
            switch (color_code) {
                case 0:
                    final_color = pack_rgba(r0, g0, b0, final_alpha);
                    break;
                case 1:
                    final_color = pack_rgba(r1, g1, b1, final_alpha);
                    break;
                case 2:
                    final_color = pack_rgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3,
                                            (2 * b0 + b1) / 3, final_alpha);
                    break;
                case 3:
                    final_color = pack_rgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3,
                                            (b0 + 2 * b1) / 3, final_alpha);
                    break;
                default:
                    final_color = pack_rgba(0, 0, 0, final_alpha);
                    break;
            }
            
            if (x + i < width) {
                image[(y + j) * width + (x + i)] = final_color;
            }
        }
    }
}

/**
 * 将DXT压缩数据解压为RGBA格式（始终输出4通道）
 * 
 * @param dxt_data   压缩的DXT数据
 * @param image_x    图像宽度（像素）
 * @param image_y    图像高度（像素）
 * @param image_channels 输入源格式（3表示DXT1，4表示DXT5）
 * @return uint8_t*  解压后的RGBA数据（4通道），调用者需要free()释放
 *                   失败返回NULL
 */
uint8_t* dxt_to_rgba(const uint8_t *dxt_data, int image_x, int image_y, int image_channels)
{
    if (!dxt_data || image_x <= 0 || image_y <= 0 || 
        (image_channels != 3 && image_channels != 4)) {
        return NULL;
    }
    
    // 分配输出缓冲区（始终是RGBA格式，4通道）
    size_t output_size = (size_t)image_x * image_y * 4;
    uint8_t *output = (uint8_t*)malloc(output_size);
    if (!output) {
        return NULL;
    }
    
    // 分配临时缓冲区用于存储32位RGBA像素
    uint32_t *temp_buffer = (uint32_t*)malloc((size_t)image_x * image_y * sizeof(uint32_t));
    if (!temp_buffer) {
        free(output);
        return NULL;
    }
    
    // 计算块数量
    uint32_t block_count_x = (image_x + 3) / 4;
    uint32_t block_count_y = (image_y + 3) / 4;
    
    // 根据输入源格式选择解压方法
    const uint8_t *block_ptr = dxt_data;
    
    if (image_channels == 4) {
        // 源数据是DXT5格式（带alpha通道）
        size_t block_size = 16;
        for (uint32_t j = 0; j < block_count_y; j++) {
            for (uint32_t i = 0; i < block_count_x; i++) {
                decompress_dxt5_block(i * 4, j * 4, image_x, block_ptr, temp_buffer);
                block_ptr += block_size;
            }
        }
    } else {
        // 源数据是DXT1格式（无alpha通道）
        size_t block_size = 8;
        for (uint32_t j = 0; j < block_count_y; j++) {
            for (uint32_t i = 0; i < block_count_x; i++) {
                decompress_dxt1_block(i * 4, j * 4, image_x, block_ptr, temp_buffer);
                block_ptr += block_size;
            }
        }
    }
    
    // 转换为RGBA格式（直接复制32位像素数据）
    memcpy(output, temp_buffer, output_size);
    
    free(temp_buffer);
    return output;
}