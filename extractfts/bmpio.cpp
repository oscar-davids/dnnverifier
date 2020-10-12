#include <stdio.h>
#include <float.h>
#include "bmpio.h"

//#define _TO_REAL_DRIVE_ ('e')
int WriteBinaryBmp(char *bmpFileName, int width, int height, unsigned char *greyimg)
{

#if _DBG
	int i;
	int rwbytes;
	int delta;
	unsigned char temp[16], *pImage;
	BITMAPFILEHEADER bmpfh;
	BITMAPINFOHEADER bmpih;
	RGBQUAD bmpcolormap[256];
	FILE* bmpFile;
#ifdef _TO_REAL_DRIVE_
	char buf[256];
	memset(buf, 0x00, sizeof(buf));
	strcpy(buf, bmpFileName);
	buf[0] = _TO_REAL_DRIVE_;
	bmpFileName = buf;
#endif
	bmpFile = fopen(bmpFileName,"wb");
	if (bmpFile==NULL) return -1;

	bmpfh.bfType = 0x4D42; //"BM";
	bmpfh.bfSize = sizeof(BITMAPFILEHEADER);
	bmpfh.bfReserved1 = 0;
	bmpfh.bfReserved2 = 0;
	bmpfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*256;

	rwbytes = (int)((width*8+31)&(~31))/8;
	bmpih.biSize = sizeof(BITMAPINFOHEADER);
	bmpih.biWidth = width;
	bmpih.biHeight = height;
	bmpih.biPlanes = 1;
	bmpih.biBitCount = 8;
	bmpih.biCompression = 0;
	bmpih.biSizeImage = rwbytes*height;
	bmpih.biXPelsPerMeter = 0;
	bmpih.biYPelsPerMeter = 0;
	bmpih.biClrUsed = 0;
	bmpih.biClrImportant = 0;

	bmpcolormap[0].rgbBlue =
		bmpcolormap[0].rgbGreen =
		bmpcolormap[0].rgbRed = 0;
	bmpcolormap[0].rgbReserved = 0;
	
	for (i=1; i<256; i++)
	{
		bmpcolormap[i].rgbBlue =
			bmpcolormap[i].rgbGreen =
			bmpcolormap[i].rgbRed = 0xff;
		bmpcolormap[i].rgbReserved = 0;
	}

	fwrite(&bmpfh,sizeof(BITMAPFILEHEADER),1,bmpFile);
	fwrite(&bmpih,sizeof(BITMAPINFOHEADER),1,bmpFile);
	fwrite(bmpcolormap,sizeof(RGBQUAD)*256,1,bmpFile);


	pImage = greyimg + width*(height-1);

	delta = rwbytes - width;
	for (i=0; i<height; i++)
	{
		//		bmpFile.Write(pImage, width);
		fwrite(pImage,width,1,bmpFile);
		if (delta>0)
		{
			//			bmpFile.Write(temp, delta);
			fwrite(temp,delta,1,bmpFile);
		}
		pImage -= width;
	}

	//	bmpFile.Close();
	fclose(bmpFile);
#endif
	return 0;
}

int WriteGreyBmp(char *bmpFileName, int width, int height, unsigned char *greyimg)
{

#if _DBG
	int i;
	int rwbytes;
	int delta;
	unsigned char temp[16], *pImage;
	BITMAPFILEHEADER bmpfh;
	BITMAPINFOHEADER bmpih;
	RGBQUAD bmpcolormap[256];
	FILE* bmpFile;
#ifdef _TO_REAL_DRIVE_
	char buf[256];
	memset(buf, 0x00, sizeof(buf));
	strcpy(buf, bmpFileName);
	buf[0] = _TO_REAL_DRIVE_;
	bmpFileName = buf;
#endif

	bmpFile = fopen(bmpFileName,"wb");
	if (bmpFile==NULL) return -1;

	bmpfh.bfType = 0x4D42; //"BM";
	bmpfh.bfSize = sizeof(BITMAPFILEHEADER);
	bmpfh.bfReserved1 = 0;
	bmpfh.bfReserved2 = 0;
	bmpfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)*256;

	rwbytes = (int)((width*8+31)&(~31))/8;
	bmpih.biSize = sizeof(BITMAPINFOHEADER);
	bmpih.biWidth = width;
	bmpih.biHeight = height;
	bmpih.biPlanes = 1;
	bmpih.biBitCount = 8;
	bmpih.biCompression = 0;
	bmpih.biSizeImage = rwbytes*height;
	bmpih.biXPelsPerMeter = 0;
	bmpih.biYPelsPerMeter = 0;
	bmpih.biClrUsed = 0;
	bmpih.biClrImportant = 0;

	for (i=0; i<256; i++)
	{
		bmpcolormap[i].rgbBlue =
			bmpcolormap[i].rgbGreen =
				bmpcolormap[i].rgbRed = i;
		bmpcolormap[i].rgbReserved = 0;
	}

	fwrite(&bmpfh,sizeof(BITMAPFILEHEADER),1,bmpFile);
	fwrite(&bmpih,sizeof(BITMAPINFOHEADER),1,bmpFile);
	fwrite(bmpcolormap,sizeof(RGBQUAD)*256,1,bmpFile);


	pImage = greyimg + width*(height-1);

	delta = rwbytes - width;
	for (i=0; i<height; i++)
	{
//		bmpFile.Write(pImage, width);
		fwrite(pImage,width,1,bmpFile);
		if (delta>0)
		{
//			bmpFile.Write(temp, delta);
			fwrite(temp,delta,1,bmpFile);
		}
		pImage -= width;
	}

//	bmpFile.Close();
	fclose(bmpFile);
#endif
	return 0;
}

int WriteColorBmp(char *bmpFileName, int width, int height, unsigned char *colorimg)
{

#if _DBG
	int i;
	int rwbytes;
	int delta;
	unsigned char temp[16], *pImage;
	BITMAPFILEHEADER bmpfh;
	BITMAPINFOHEADER bmpih;
	FILE* bmpFile;
#ifdef _TO_REAL_DRIVE_
	char buf[256];
	memset(buf, 0x00, sizeof(buf));
	strcpy(buf, bmpFileName);
	buf[0] = _TO_REAL_DRIVE_;
	bmpFileName = buf;
#endif

//return 0;
	bmpFile = fopen(bmpFileName,"wb");
	if (bmpFile==NULL) return -1;

	bmpfh.bfType = 0x4D42; //"BM";
	bmpfh.bfSize = sizeof(BITMAPFILEHEADER);
	bmpfh.bfReserved1 = 0;
	bmpfh.bfReserved2 = 0;
	bmpfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	rwbytes = (int)((width*24+31)&(~31))/8;
	bmpih.biSize = sizeof(BITMAPINFOHEADER);
	bmpih.biWidth = width;
	bmpih.biHeight = height;
	bmpih.biPlanes = 1;
	bmpih.biBitCount = 24;
	bmpih.biCompression = 0;
	bmpih.biSizeImage = rwbytes*height;
	bmpih.biXPelsPerMeter = 0;
	bmpih.biYPelsPerMeter = 0;
	bmpih.biClrUsed = 0;
	bmpih.biClrImportant = 0;

	fwrite(&bmpfh,sizeof(BITMAPFILEHEADER),1,bmpFile);
	fwrite(&bmpih,sizeof(BITMAPINFOHEADER),1,bmpFile);

	pImage = colorimg + width*(height-1)*3;

	delta = rwbytes - width*3;
	for (i=0; i<height; i++)
	{
		//bmpFile.Write(pImage, width);
		fwrite(pImage,width*3,1,bmpFile);
		if (delta>0)
		{
			//bmpFile.Write(temp, delta);
			fwrite(temp,delta,1,bmpFile);
		}
		pImage -= (width*3);
	}

	//	bmpFile.Close();
	fclose(bmpFile);
#endif

	return 0;
}
int WriteFloatBmp(const char* bmpFileName, int width, int height, float *greyimg)
{
	int y, x;
	unsigned char *greybuff = (unsigned char *)malloc(width*height);
	float fmax = FLT_MIN;// -1000000.0;
	float fmin = FLT_MAX;// 1000000.0;
	for (y = 0; y < width * height; y++)
	{
		if (greyimg[y] > fmax) fmax = greyimg[y];
		if (greyimg[y] < fmin) fmin = greyimg[y];
	}

	float fscale = 255.0 / (fmax - fmin);
	for (y = 0; y < width * height; y++)
	{
		greybuff[y] = (unsigned char)(((float)(greyimg[y] - fmin))* fscale);
	}

	WriteGreyBmp((char*)bmpFileName, width, height, greybuff);

	if (greybuff) free(greybuff);

	return 0;
}

extern int WriteShortBmp(char* bmpFileName, int width, int height, short *greyimg)
{
	int y, x;
	unsigned char *greybuff = (unsigned char *)malloc(width*height);
	short fmax = -32767;
	short fmin = 32767;
	for (y = 0; y < width * height; y++)
	{
		if (greyimg[y] > fmax) fmax = greyimg[y];
		if (greyimg[y] < fmin) fmin = greyimg[y];
	}

	float fscale = 255.0 / (fmax - fmin);
	for (y = 0; y < width * height; y++)
	{
		greybuff[y] = (unsigned char)((float)(greyimg[y] - fmin) * fscale);
	}

	WriteGreyBmp(bmpFileName, width, height, greybuff);

	if (greybuff) free(greybuff);

	return 0;
}