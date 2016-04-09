#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_SIZE 1024

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;

u8* LZLikeDecode(u8 *out, u8 *data, u8 *end)
{
	u8 window[WINDOW_SIZE];
	u16 head=0;
	while (data<end)
	{
		u8 a=*(data++);
		u8 b=*(data++);
		if (a==0)
		{
			window[(head++)&(WINDOW_SIZE-1)]=b;
			*(out++)=b;
		}
		else
		{
			u16 idx=head-1-(b+((a>>6)<<8))+WINDOW_SIZE;
			a&=0x3F;
			for (int i=0; i<=a; i++)
			{
				u8 v=window[(idx+i)&(WINDOW_SIZE-1)];
				window[(head++)&(WINDOW_SIZE-1)]=v;
				*(out++)=v;
			}
		}
	}
	return out;
}

u8* LZLikeEncode(u8 *out, u8 *data, u8 *end)
{
	int eidx=(int)(end-data);
	int start=0;
	while (start<eidx)
	{
		int bestOffset=data[start];
		int bestLen=1;
		for (int o=0; o<WINDOW_SIZE; o++)
		{
			int k=start;
			if (k-1-o>=0)
			{
				int len=0;
				while (data[k]==data[k-1-o] && len<64 && k<eidx)
				{
					len++;
					k++;
				}
				if (len>bestLen)
				{
					bestLen=len;
					bestOffset=o;
				}
			}
		}
		*(out++)=(bestLen-1)|((bestOffset>>8)<<6);
		*(out++)=bestOffset;
		start+=bestLen;
	}
	return out;
}

int main(int argc, char **argv)
{
	if (argc!=3)
	{
		printf("Usage: c64convert source.tap dest.tpz\n");
		return 1;
	}
	FILE *f=fopen(argv[1], "r");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		long size=ftell(f);
		printf("Loaded %s (%d bytes)\n", argv[1], (int)size);
		fseek(f, 0, SEEK_SET);
		u8 *data=(u8*)malloc(size);
		fread(data, 1, size, f);
		fclose(f);

		printf("Read done\n");
		u8 *out=(u8*)malloc(2*size); // worst case
		u8 *eout=LZLikeEncode(out, data, data+size);
		printf("Compressed:%d/%d\n", (int)(eout-out), (int)size);
	
		FILE *g=fopen(argv[2], "w");
		if (g)
		{
			fwrite(out, 1, eout-out, g);
			fclose(g);
		}
		else
		{
			printf("Couldn't open '%s' for write\n", argv[2]);
			return 1;
		}

		u8 *dest=(u8*)malloc(size);
		u8 *edest=LZLikeDecode(dest, out, eout);
		if (memcmp(dest, data, size)!=0)
		{
			printf("Decompress error %d/%d\n", (int)(edest-dest), (int)size);
			return 1;
		}
	}
	else
	{
		printf("Couldn't open '%s' for read\n", argv[1]);
		return 1;
	}
	return 0;
}

