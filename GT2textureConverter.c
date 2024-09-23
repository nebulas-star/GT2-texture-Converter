#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>
#include "VitaSwizzle.h"

#define BUILDER " GT2 Texture File for VITA/Compatible/Programmed by lipsum, nebulas and Xiyan "

void fwrite32LE(uint32_t FourChar, FILE *out_file)
{
    uint8_t SingleChar[4];
    SingleChar[0] = (FourChar >> 0) & 0xFF;
    SingleChar[1] = (FourChar >> 8) & 0xFF;
    SingleChar[2] = (FourChar >> 16) & 0xFF;
    SingleChar[3] = (FourChar >> 24) & 0xFF;
    fwrite(SingleChar, 1, 4, out_file);
}

FILE *fopen_or_exit(const char *filename, const char *mode)
{
    FILE *ret = fopen(filename, mode);

    if (ret == NULL)
    {
        int err = errno;
        fprintf(stderr, "Failed to open file [%s].\nError: [%d]%s\n", filename, err, strerror(err));
        exit(err);
    }

    return ret;
}

void Swizzle(uint32_t width, uint32_t height, float Bppx, uint8_t *blockStorage, uint8_t *image, _Bool flag)
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

void DDS_to_GTX(char *argv)
{
    FILE *DDSfile = fopen_or_exit(argv, "rb");
    uint32_t headerSize = 0x80;
    uint8_t DDShead[0x80];
    fread(DDShead, 1, headerSize, DDSfile);

    uint32_t textureWidth = ((DDShead[0x13] << 24) | (DDShead[0x12] << 16) | (DDShead[0x11] << 8) | DDShead[0x10]);
    uint32_t textureHeight = ((DDShead[0x0F] << 24) | (DDShead[0x0E] << 16) | (DDShead[0x0D] << 8) | DDShead[0x0C]);

    float bytePerPixel;
    char vitaFourCC[4];
    if (DDShead[0x54] == 'D' && DDShead[0x55] == 'X' && DDShead[0x56] == 'T' && DDShead[0x57] == '1')
    {
        bytePerPixel = 0.5;
        strcpy(vitaFourCC, "UBC1");
    }
    else if (DDShead[0x54] == 'D' && DDShead[0x55] == 'X' && DDShead[0x56] == 'T' && DDShead[0x57] == '5')
    {
        bytePerPixel = 1;
        strcpy(vitaFourCC, "UBC3");
    }
    else
    {
        printf("Error: Unsupported DDS Texture Format:%c%c%c%c.", DDShead[0x54], DDShead[0x55], DDShead[0x56], DDShead[0x57]);
        exit(-1);
    }

    uint8_t *textureData = (uint8_t *)malloc((textureWidth * textureHeight * bytePerPixel));

    fseek(DDSfile, headerSize, SEEK_SET);
    fread(textureData, 1, textureWidth * textureHeight * bytePerPixel, DDSfile);
    fclose(DDSfile);

    uint8_t *textureOutput = (uint8_t *)malloc((textureWidth * textureHeight * bytePerPixel));
    Swizzle(textureWidth, textureHeight, bytePerPixel, textureData, textureOutput, 0);

    char outputTextureFileName[0x80];
    int cpylen = strcspn(argv, ".");
    memcpy(outputTextureFileName, argv, cpylen);
    memcpy(outputTextureFileName + cpylen, ".gtx", 5);
    FILE *outputTextureFile = fopen_or_exit(outputTextureFileName, "wb");

    //GXT FIle Header
    char     builder_message[] = BUILDER;
    uint32_t builder_message_legnth = strlen(builder_message);
    uint32_t builder_message_padding_legnth = ((builder_message_legnth / 4) + 1) * 4;
    char     filename[0x100] = { 0 };
        cpylen = strcspn(argv, ".");
        memcpy(filename, argv, cpylen);
        memcpy(filename + cpylen, "", 1);
    uint32_t filename_legnth = strlen(filename);
    uint32_t filename_padding_legnth = ((filename_legnth / 4) + 1) * 4;

    uint32_t texture_length = textureWidth * textureHeight * bytePerPixel;
    uint8_t  nodata[0x20] = { 0 };

    uint8_t  magi[4] = { 0x47, 0x54, 0x58, 0x01 };
    uint32_t header_length = 0x34 + builder_message_padding_legnth;
    uint32_t texture_length_with_metadata = texture_length + 0x48 + filename_padding_legnth;
    uint32_t building_data_length = builder_message_legnth + 0x24;
    uint8_t  unknown1[0x04] = { 0x04, 0x00, 0x00, 0x00 };

    uint8_t  metadata_magi[4] = { 0x00, 0x05, 0x07, 0x00 };
    uint8_t  unknown2[0x0C] = { 0x3C, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00 };
    uint8_t  unknown3[0x04] = { 0x01, 0x00, 0x00, 0x00 };
    uint32_t unknown4 = bytePerPixel * 16;
    uint8_t  unknown5[0x10] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    fwrite(magi, 1, 4, outputTextureFile);
    fwrite32LE(header_length, outputTextureFile);
    fwrite(nodata, 1, 4, outputTextureFile);
    fwrite32LE(texture_length_with_metadata, outputTextureFile);
    fwrite32LE(building_data_length, outputTextureFile);
    fwrite(nodata, 1, 0x14, outputTextureFile);
    fwrite32LE(builder_message_legnth, outputTextureFile);
    fwrite32LE(builder_message_legnth + 1, outputTextureFile);
    fwrite(unknown1, 1, 4, outputTextureFile);
    fwrite(builder_message, 1, builder_message_legnth, outputTextureFile);
    fwrite(nodata, 1, (builder_message_padding_legnth - builder_message_legnth), outputTextureFile);

    fwrite(metadata_magi, 1, 4, outputTextureFile);
    fwrite32LE(filename_legnth, outputTextureFile);
    fwrite32LE(filename_legnth + 1, outputTextureFile);
    fwrite(unknown2, 1, 0x0C, outputTextureFile);
    fwrite32LE(textureWidth, outputTextureFile);
    fwrite32LE(textureHeight, outputTextureFile);
    fwrite32LE(textureWidth * 4, outputTextureFile);
    fwrite(unknown3, 1, 0x04, outputTextureFile);
    fwrite(vitaFourCC, 1, 0x04, outputTextureFile);
    fwrite32LE(unknown4, outputTextureFile);
    fwrite(unknown5, 1, 0x10, outputTextureFile);
    fwrite32LE(texture_length, outputTextureFile);
    fwrite32LE(filename_padding_legnth + 4, outputTextureFile);
    fwrite(filename, 1, filename_legnth, outputTextureFile);
    fwrite(nodata, 1, (filename_padding_legnth - filename_legnth), outputTextureFile);
    //GXT FIle Header End
    fwrite(textureOutput, 1, textureWidth * textureHeight * bytePerPixel, outputTextureFile);
    fclose(outputTextureFile);
}


void GTX_to_DDS(char *argv)
{
    FILE *GTXfile = fopen_or_exit(argv, "rb");
    fseek(GTXfile, 0x04, SEEK_SET);
    uint8_t crash[4];
    fread(crash, 4, 1, GTXfile);
    uint32_t headerMessageSize = ((crash[3] << 24) | (crash[2] << 16) | (crash[1] << 8) | crash[0]);
    fseek(GTXfile, headerMessageSize + 0x44, SEEK_SET);
    fread(crash, 4, 1, GTXfile);
    uint32_t filenamePaddingLegnth = ((crash[3] << 24) | (crash[2] << 16) | (crash[1] << 8) | crash[0]);
    uint32_t headerSize = headerMessageSize + 0x44 + filenamePaddingLegnth;
    fseek(GTXfile, 0, SEEK_SET);
    uint8_t GTXhead[0x200];
    fread(GTXhead, 1, headerSize, GTXfile);

    uint32_t textureSizeOffset = headerMessageSize + 0x18;
    uint32_t textureWidth  = ((GTXhead[textureSizeOffset + 3] << 24) | (GTXhead[textureSizeOffset + 2] << 16) | (GTXhead[textureSizeOffset + 1] << 8) | GTXhead[textureSizeOffset + 0]);
    uint32_t textureHeight = ((GTXhead[textureSizeOffset + 7] << 24) | (GTXhead[textureSizeOffset + 6] << 16) | (GTXhead[textureSizeOffset + 5] << 8) | GTXhead[textureSizeOffset + 4]);

    float bytePerPixel;
    char dwFourCC[4];
    if       (GTXhead[headerMessageSize + 0x28] == 'U' && GTXhead[headerMessageSize + 0x29] == 'B' && GTXhead[headerMessageSize + 0x2A] == 'C' && GTXhead[headerMessageSize + 0x2B] == '1')
    {
        bytePerPixel = 0.5;
        strcpy(dwFourCC, "DXT1");
    }
    else if (GTXhead[headerMessageSize + 0x28] == 'U' && GTXhead[headerMessageSize + 0x29] == 'B' && GTXhead[headerMessageSize + 0x2A] == 'C' && GTXhead[headerMessageSize + 0x2B] == '3')
    {
        bytePerPixel = 1;
        strcpy(dwFourCC, "DXT5");
    }
    else
    {
        printf("Error: Unsupported Texture Format:%c%c%c%c.", GTXhead[0xA0], GTXhead[0xA1], GTXhead[0xA2], GTXhead[0xA3]);
        exit(-1);
    }

    uint8_t *textureData = (uint8_t *)malloc((textureWidth * textureHeight * bytePerPixel));
    fseek(GTXfile, headerSize, SEEK_SET);
    fread(textureData, 1, textureWidth * textureHeight * bytePerPixel, GTXfile);
    fclose(GTXfile);

    uint8_t *textureOutput = (uint8_t *)malloc((textureWidth * textureHeight * bytePerPixel));
    Swizzle(textureWidth, textureHeight, bytePerPixel, textureData, textureOutput, 1);

    char outputTextureFileName[0x80];
    int cpylen = strcspn(argv, ".");
    memcpy(outputTextureFileName, argv, cpylen);
    memcpy(outputTextureFileName + cpylen, ".DDS", 5);

    FILE *outputTextureFile = fopen_or_exit(outputTextureFileName, "wb");

    uint8_t DDSheader[0x80] = {
        0x44, 0x44, 0x53, 0x20, 0x7c, 0x00, 0x00, 0x00, 0x07, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x40, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    DDSheader[0x0C] = GTXhead[textureSizeOffset + 4]; DDSheader[0x0D] = GTXhead[textureSizeOffset + 5]; DDSheader[0x0E] = GTXhead[textureSizeOffset + 6]; DDSheader[0x0F] = GTXhead[textureSizeOffset + 7];
    DDSheader[0x10] = GTXhead[textureSizeOffset + 0]; DDSheader[0x11] = GTXhead[textureSizeOffset + 1]; DDSheader[0x12] = GTXhead[textureSizeOffset + 2]; DDSheader[0x13] = GTXhead[textureSizeOffset + 3];
    DDSheader[0x54] = dwFourCC[0]; DDSheader[0x55] = dwFourCC[1]; DDSheader[0x56] = dwFourCC[2]; DDSheader[0x57] = dwFourCC[3];
    fwrite(DDSheader, 1, 0x80, outputTextureFile);

    fwrite(textureOutput, 1, textureWidth * textureHeight * bytePerPixel, outputTextureFile);
    fclose(outputTextureFile);
}


int main(int argc, char *argv[])
{
    fputs("GT2 Texture Converter Programmed by lipsum, nebulas and Xiyan\n", stderr);
    if (argc != 2)
    {
        fprintf(stderr, "Usage: GT2TextureConverter <filename.dds/filename.gtx>\n");
        exit(-1);
    }

    FILE *inputFile = fopen_or_exit(argv[1], "rb");

    char magic[4];
    fread(magic, 4, 1, inputFile);
    fclose(inputFile);
    uint32_t magicNumber = (magic[0] << 24) | (magic[1] << 16) | (magic[2] << 8) | magic[3];

    if (magicNumber == 0x47545801)
        GTX_to_DDS(argv[1]);
    else if (magicNumber == 0x44445320)
        DDS_to_GTX(argv[1]);
    else
        printf("Error: Unsupported file format. Please chack your file.");

    return 0;
}