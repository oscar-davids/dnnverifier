#ifndef _BMPIO
#define _BMPIO

#define _DBG 1

#if defined(_MSC_VER)
#include <windows.h>
#else

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
#define MAX_PATH			256


#pragma pack(1)
typedef struct tagBITMAPFILEHEADER {
	WORD    bfType;
	DWORD   bfSize;
	WORD    bfReserved1;
	WORD    bfReserved2;
	DWORD   bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
	DWORD      biSize;
	long       biWidth;
	long       biHeight;
	WORD       biPlanes;
	WORD       biBitCount;
	DWORD      biCompression;
	DWORD      biSizeImage;
	long       biXPelsPerMeter;
	long       biYPelsPerMeter;
	DWORD      biClrUsed;
	DWORD      biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;
typedef struct tagRGBQUAD {
	BYTE    rgbBlue;
	BYTE    rgbGreen;
	BYTE    rgbRed;
	BYTE    rgbReserved;
} RGBQUAD;
#pragma pack()
#endif

extern int WriteBinaryBmp(char* bmpFileName, int width, int height, unsigned char *greyimg);
extern int WriteGreyBmp(char* bmpFileName, int width, int height, unsigned char *greyimg);
extern int WriteColorBmp(char *bmpFileName, int width, int height, unsigned char *colorimg);
extern int WriteFloatBmp(const char* bmpFileName, int width, int height, float *greyimg);
extern int WriteShortBmp(char* bmpFileName, int width, int height, short *greyimg);

#endif