#include "../common/frame_info.h"
#include "string.h"

#include "frame.h"

extern unsigned char kernel_hankaku[];
extern unsigned char kernel_hankaku_len;

FrameInfo *frame_info;

unsigned char *getFont(char c) {
	if(c*16 < kernel_hankaku_len) return NULL;
	return kernel_hankaku + c*16;
}

// RGB
void WritePixel(unsigned int x, unsigned int y, const Color *c) {
	frame_info->frame_base[(frame_info->pixel_per_scanline * y + x) * 4] = c->blue;
	frame_info->frame_base[(frame_info->pixel_per_scanline * y + x) * 4 + 1] = c->green;
	frame_info->frame_base[(frame_info->pixel_per_scanline * y + x) * 4 + 2] = c->red;
	frame_info->frame_base[(frame_info->pixel_per_scanline * y + x) * 4 + 3] = c->reserved;
}

void WriteAscii(char ch, unsigned int x, unsigned int y, const Color *c) {
	unsigned char *font = getFont(ch);
	for(int i = 0; i < 16; i++) {
		for(int j = 0; j < 8; j++) {
			if(((font[i] >> (8-j))&0x01) == 1) {
				WritePixel(x+j, y+i, c);
			}
		}
	}
}

void WriteString(char *str, unsigned int x, unsigned int y, Color *c) {
	for(int i = 0; i < strlen(str); i++) {
		WriteAscii(str[i], x+i*8, y, c);
	}
}

void WriteSquare(unsigned int x1, unsigned int y1, unsigned x2, unsigned y2, Color *c) {
	for(int j = y1; j <= y2; j++) {
		for(int i = x1; i <= x2; i++) {
			WritePixel(i,j,c);
		}
	}
}

void ClearScreen() {
	for(unsigned int i = 0; i < frame_info->frame_size; i++) {
		frame_info->frame_base[i] = 0;
	}
}

/*
void Print(char *str) {
	int len = strlen(str);
*/
