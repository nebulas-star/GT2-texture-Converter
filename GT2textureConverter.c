#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "GT2textureConverter.h"

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

void fwrite32LE(uint32_t FourChar, FILE *out_file)
{
    uint8_t SingleChar[4];
    SingleChar[0] = (FourChar >> 0) & 0xFF;
    SingleChar[1] = (FourChar >> 8) & 0xFF;
    SingleChar[2] = (FourChar >> 16) & 0xFF;
    SingleChar[3] = (FourChar >> 24) & 0xFF;
    fwrite(SingleChar, 1, 4, out_file);
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
        memcpy(vitaFourCC, "UBC1", 4);
    }
    else if (DDShead[0x54] == 'D' && DDShead[0x55] == 'X' && DDShead[0x56] == 'T' && DDShead[0x57] == '5')
    {
        bytePerPixel = 1;
        memcpy(vitaFourCC, "UBC3", 4);
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
    SwizzleCtrl(textureWidth, textureHeight, bytePerPixel, textureData, textureOutput, 0);

    char outputTextureFileName[0x80];
    int cpylen = strcspn(argv, ".");
    memcpy(outputTextureFileName, argv, cpylen);
    memcpy(outputTextureFileName + cpylen, ".gtx", 5);
    FILE *outputTextureFile = fopen_or_exit(outputTextureFileName, "wb");

    //GXT FIle Header
    char    *filename = get_filename(argv);
    GT2_header_build();
    GT2_metadata_build(textureWidth, textureHeight, bytePerPixel, vitaFourCC, filename);
    fwrite(GT2_header, 1, 0x78, outputTextureFile);
    fwrite(GT2_metadata, 1, 0x58, outputTextureFile);

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
        memcpy(dwFourCC, "DXT1", 4);
    }
    else if (GTXhead[headerMessageSize + 0x28] == 'U' && GTXhead[headerMessageSize + 0x29] == 'B' && GTXhead[headerMessageSize + 0x2A] == 'C' && GTXhead[headerMessageSize + 0x2B] == '3')
    {
        bytePerPixel = 1;
        memcpy(dwFourCC, "DXT5", 4);
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
    SwizzleCtrl(textureWidth, textureHeight, bytePerPixel, textureData, textureOutput, 1);

    char outputTextureFileName[0x80];
    int cpylen = strcspn(argv, ".");
    memcpy(outputTextureFileName, argv, cpylen);
    memcpy(outputTextureFileName + cpylen, ".DDS", 5);

    FILE *outputTextureFile = fopen_or_exit(outputTextureFileName, "wb");

    DDS_header_build(textureWidth, textureHeight, dwFourCC);
    fwrite(DDS_header, 1, 0x80, outputTextureFile);

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