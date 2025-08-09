
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

/******************************************************************************/

typedef   signed int   s32;
typedef   signed short s16;
typedef   signed char  s8;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;


typedef struct {
	int unicode;

	/* Glyph bitmap infomation */
	int bw, bh;
	int bx, by;
	int advance;
	u8 *bitmap;
	int bsize;
} GLYPH;


int clist[65536];
GLYPH *ctab[65536];
int total_chars;

int pixel_size;
int font_ascent;
int base_adj = 0;
int font_size = 14;

u8 font_buf[0x1000000];
int font_buf_size;

/******************************************************************************/


int ucs2utf8(u16 ucs, char *p)
{
	if(ucs<=0x7f){
		p[0] = ucs;
		return 1;
	}else if(ucs<=0x7ff){
		p[0] = 0xc0 | ((ucs>>6)&0x1f);
		p[1] = 0x80 | ((ucs>>0)&0x3f);
		return 2;
	}else if(ucs<=0xffff){
		p[0] = 0xe0 | ((ucs>>12)&0x0f);
		p[1] = 0x80 | ((ucs>>6)&0x3f);
		p[2] = 0x80 | ((ucs>>0)&0x3f);
		return 3;
	}

	return 0;
}


/******************************************************************************/

#if 0
GLYPH *bdf_load_char(FILE *fp)
{
	int bmp, lsize, x;
	char lbuf[128];
	GLYPH *pg;

	pg = NULL;
	bmp = -1;
	while(1){
		if(fgets(lbuf, 128, fp)==NULL)
			break;

		if(strncmp(lbuf, "STARTCHAR", 9)==0){
			pg = (GLYPH*)malloc(sizeof(GLYPH));
			memset(pg, 0, sizeof(GLYPH));
		}else if(strncmp(lbuf, "ENCODING", 8)==0){
			pg->unicode = atoi(lbuf+9);
		}else if(strncmp(lbuf, "DWIDTH", 6)==0){
			pg->advance = atoi(lbuf+7);
		}else if(strncmp(lbuf, "BBX", 3)==0){
			sscanf(lbuf+4, "%d %d %d %d", &pg->bw, &pg->bh, &pg->bx, &pg->by);
#if 1
			int ny = (font_ascent+base_adj-1) - (pg->bh - 1 + pg->by);
			if(ny<0)
				ny = 0;
			pg->by = ny;
#endif
			if(pg->bx<0){
				pg->advance -= pg->bx;
				pg->bx = 0;
			}
			lsize = (pg->bw+7)/8;
			pg->bsize = lsize*pg->bh;
			pg->bitmap = malloc(pg->bsize);
		}else if(strncmp(lbuf, "BITMAP", 6)==0){
			bmp = 0;
		}else if(strncmp(lbuf, "ENDCHAR", 7)==0){
			break;
		}else if(bmp>=0){
			int len = strlen(lbuf);
			if(lbuf[len-1]=='\n' || lbuf[len-1]=='\r') len -= 1;
			if(lbuf[len-1]=='\n' || lbuf[len-1]=='\r') len -= 1;
			x = 0;
			for(int i=0; i<len; i+=2){
				char tmp[4];
				tmp[0] = lbuf[i+0];
				tmp[1] = lbuf[i+1];
				tmp[2] = 0;
				if(x<lsize){
					pg->bitmap[bmp] = strtoul(tmp, NULL, 16);
					bmp += 1;
					x += 1;
				}
			}
		}

	}

	return pg;
}


int load_bdf(char *bdf_name)
{
	FILE *fp;
	GLYPH *pg;
	char lbuf[128];

	total_chars = 0;
	memset(ctab, 0, sizeof(ctab));

	fp = fopen(bdf_name, "r");
	if(fp==NULL)
		return -1;

	fgets(lbuf, 128, fp);
	if(strncmp(lbuf, "STARTFONT", 9)){
		printf("STARTFONT not found!\n");
		return -1;
	}
	printf("\nLoad BDF: %s\n", bdf_name);

	pixel_size = 0;
	font_ascent = 0;
	while(1){
		if(fgets(lbuf, 128, fp)==NULL)
			break;
		if(strncmp(lbuf, "PIXEL_SIZE", 10)==0){
			pixel_size = atoi(lbuf+11);
			printf("  PIXEL_SIZE: %d\n", pixel_size);
		}else if(strncmp(lbuf, "FONT_ASCENT", 11)==0){
			font_ascent = atoi(lbuf+12);
			printf("  FONT_ASCENT: %d\n", font_ascent);
		}else if(strncmp(lbuf, "CHARS ", 6)==0){
			break;
		}
	}
	if(pixel_size==0){
		printf("Wrong PIXEL_SIZE: %d\n", pixel_size);
		return -1;
	}


	while(1){
		pg = bdf_load_char(fp);
		if(pg==NULL)
			break;

		if(clist[pg->unicode]==0){
			free(pg->bitmap);
			free(pg);
			continue;
		}
		ctab[pg->unicode] = pg;
		total_chars += 1;
	}
	printf("  Chars load: %d\n", total_chars);

	fclose(fp);
	return 0;
}

#endif

/******************************************************************************/

FT_Library ft_lib;
FT_Face ft_face;


GLYPH *ttf_load_char(int ucs)
{
	FT_Glyph glyph;
	int index, retv;

	index = FT_Get_Char_Index(ft_face, ucs);
	if(index==0){
		//printf("FT_Get_Char_Index(%04x): not found!\n", ucs);
		return NULL;
	}

	//retv = FT_Load_Glyph(tt->face, index, FT_LOAD_DEFAULT|FT_LOAD_NO_HINTING);
	retv = FT_Load_Glyph(ft_face, index, FT_LOAD_DEFAULT);
	if(retv){
		printf("FT_Load_Gluph(%04x): error=%08x\n", ucs, retv);
		return NULL;
	}

	FT_Get_Glyph(ft_face->glyph, &glyph);

	if(ft_face->glyph->format!=FT_GLYPH_FORMAT_BITMAP){
		retv = FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_MONO, 0, 1);
		if(retv){
			printf("FT_Render_Glyph(%04x): error=%08x\n", ucs, retv);
			return NULL;
		}
	}

	FT_BitmapGlyph bit = (FT_BitmapGlyph)glyph;
	FT_Bitmap bm = bit->bitmap;

	GLYPH *pg = (GLYPH*)malloc(sizeof(GLYPH));
	memset(pg, 0, sizeof(GLYPH));

	pg->unicode = ucs;
	pg->bw = bm.width;
	pg->bh = bm.rows;
	pg->bx = bit->left;
	pg->by = font_size-bit->top;
	pg->advance = (glyph->advance.x+0x00008000)>>16;

	int lsize = (pg->bw+7)/8;
	pg->bsize = lsize*pg->bh;
	pg->bitmap = malloc(pg->bsize);

	for(int y=0; y<bm.rows; y++){
		memcpy(pg->bitmap+y*lsize, bm.buffer+y*bm.pitch, lsize);
	}

	return pg;
}



int load_ttf(char *ttf_name)
{
	int retv, i;

	retv = FT_Init_FreeType(&ft_lib);
	if(retv){
		printf("FT_Init_Freetype failed! error=%08x\n", retv);
		return retv;
	}

	retv = FT_New_Face(ft_lib, ttf_name, 0, &ft_face);
	if(retv){
		printf("FT_New_Face(%s): error=%08x\n", ttf_name, retv);
		return retv;
	}

#if 1
	printf("FACE: num_fixed_sizes: %d\n", ft_face->num_fixed_sizes);
	for(i=0; i<ft_face->num_fixed_sizes; i++){
		printf("%2d: %2ld %2dx%2d\n", i, ft_face->available_sizes[i].size/64,
			ft_face->available_sizes[i].width,
			ft_face->available_sizes[i].height);
	}
#endif

	retv = FT_Set_Pixel_Sizes(ft_face, font_size, font_size);
	if(retv){
		printf("FT_Set_Pixel_Size: error=%08x\n", retv);
		//return retv;
	}

	total_chars = 0;

	for(i=0; i<65536; i++){
		if(clist[i]==0)
			continue;
		ctab[i] = ttf_load_char(i);
		if(ctab[i]){
			total_chars += 1;
		}else{
			//printf("load char %04x failed!\n", i);
		}
	}


	FT_Done_Face(ft_face);
	FT_Done_FreeType(ft_lib);

	return 0;
}

/******************************************************************************/



void load_list(char *name)
{
	FILE *fp;
	int n, ucs;

	memset(clist, 1, sizeof(clist));

	if(name==NULL)
		return;

	fp = fopen(name, "rb");
	if(fp==NULL)
		return;

	memset(clist, 0, sizeof(clist));

	fread(&ucs, 2, 1, fp);
	if(ucs!=0xfeff){
		printf("文件[%s]不是UTF16-LE格式!\n", name);
		exit(-1);
	}

	while(1){
		if(fread(&ucs, 2, 1, fp)<1)
			break;
		if(ucs==0x0d || ucs==0x0a)
			continue;

		if(clist[ucs]==0){
			clist[ucs] = 1;
			n += 1;
		}
	}

	printf("Load list form %s: %d\n", name, n);
}



/******************************************************************************/


void print_glyph(GLYPH *pg)
{
	int x, y;
	char ubuf[4];

	*(u32*)(ubuf) = 0;
	ucs2utf8(pg->unicode, ubuf);

	printf("\nchar %04x [%s]\n", pg->unicode, ubuf);
	printf("  bw=%-2d bh=%-2d bx=%-2d by=%-2d  adv=%d\n", pg->bw, pg->bh, pg->bx, pg->by, pg->advance);

	int lsize = (pg->bw+7)/8;
	for(y=0; y<pg->bh; y++){
		printf("    ");
		for(x=0; x<pg->bw; x++){
			if(pg->bitmap[y*lsize+(x>>3)]&(0x80>>(x%8)))
				printf("*");
			else
				printf(".");
		}
		printf("\n");
	}

}

void dump_glyph(void)
{
	int i;

	for(i=0; i<65536; i++){
		if(ctab[i]){
			print_glyph(ctab[i]);
		}
	}
}


void dump_list(void)
{
	int i;
	char ubuf[4];

	for(i=0; i<65536; i++){
		if(ctab[i]){
			*(u32*)(ubuf) = 0;
			ucs2utf8(i, ubuf);
			printf("%04x=%s\n", i, ubuf);
		}
	}

}


/******************************************************************************/


void write_be16(u8 *p, int val)
{
	p[0] = val&0xff;
	p[1] = (val>>8)&0xff;
}


void build_font(void)
{
	int i, p, offset;

	write_be16(font_buf, total_chars);
	offset = total_chars*4+2;

	p = 2;
	for(i=0; i<65536; i++){
		if(ctab[i]){
			GLYPH *pg = ctab[i];
			write_be16(font_buf+p, i);
			write_be16(font_buf+p+2, offset);
			p += 4;
			offset += pg->bsize+5;
		}
	}

	for(i=0; i<65536; i++){
		if(ctab[i]){
			GLYPH *pg = ctab[i];
			font_buf[p+0] = pg->advance;
			font_buf[p+1] = pg->bw;
			font_buf[p+2] = pg->bh;
			font_buf[p+3] = pg->bx;
			font_buf[p+4] = pg->by;
			p += 5;
			memcpy(font_buf+p, pg->bitmap, pg->bsize);
			p += pg->bsize;
		}
	}

	font_buf_size = p;
}


/******************************************************************************/


int bin2c(u8 *buffer, int size, char *file_name, char *label_name)
{
	FILE *dest;
	int i, unit=1;

	if((dest = fopen(file_name, "w")) == NULL) {
		printf("Failed to open/create %s.\n", file_name);
		return 1;
	}

	fprintf(dest, "#ifndef __%s__\n", label_name);
	fprintf(dest, "#define __%s__\n\n", label_name);
	fprintf(dest, "#define %s_size  %d\n", label_name, size);
	fprintf(dest, "static const unsigned char %s[] __attribute__((aligned(4))) = {", label_name);

	for(i=0; i<size; i+=unit) {
		if((i % (16/unit)) == 0){
			fprintf(dest, "\n\t");
		}else{
			fprintf(dest, " ");
		}
		if(unit==1){
			fprintf(dest, "0x%02x,", buffer[i]);
		}else if(unit==2){
			fprintf(dest, "0x%04x,", buffer[i]);
		}else{
			fprintf(dest, "0x%08x,", buffer[i]);
		}
	}

	fprintf(dest, "\n};\n\n#endif\n");

	fclose(dest);

	return 0;
}


/******************************************************************************/


int main(int argc, char *argv[])
{
	int retv, i;
	int flags = 0;
	char *bdf_name = NULL;
	char *font_name = NULL;
	char *list_name = NULL;
	char *label_name = NULL;

	if(argc==1){
		printf("\nUsage:\n");
		printf("\tbdfont [-dg] [-dl] [-l list] [-adj base] [-s size] [-c name] [-h name label] font_file\n");
		printf("\t\t-dg          dump glyph\n");
		printf("\t\t-dl          dump char list\n");
		printf("\t\t-l   list    load char list\n");
		printf("\t\t-adj base    adjust base line\n");
		printf("\t\t-s   size    set font size(for ttf)\n");
		printf("\t\t-c   name\n");
		printf("\t\t             output binary font\n");
		printf("\t\t-h   name label\n");
		printf("\t\t             output c header font\n");
		return 0;
	}

	for(i=1; i<argc; i++){
		if(strcmp(argv[i], "-dg")==0){
			flags |= 1;
		}else if(strcmp(argv[i], "-dl")==0){
			flags |= 2;
		}else if(strcmp(argv[i], "-l")==0){
			list_name = argv[i+1];
			i += 1;
		}else if(strcmp(argv[i], "-adj")==0){
			base_adj = atoi(argv[i+1]);
			i += 1;
		}else if(strcmp(argv[i], "-s")==0){
			font_size = atoi(argv[i+1]);
			i += 1;
		}else if(strcmp(argv[i], "-c")==0){
			flags |= 4;
			font_name = argv[i+1];
			i += 1;
		}else if(strcmp(argv[i], "-h")==0){
			flags |= 8;
			font_name = argv[i+1];
			label_name = argv[i+2];
			i += 2;
		}else if(bdf_name==NULL){
			bdf_name = argv[i];
		}
	}

	load_list(list_name);

	//retv = load_bdf(bdf_name);
	retv = load_ttf(bdf_name);
	if(retv<0)
		return -1;

	if(flags&1){
		dump_glyph();
	}else if(flags&2){
		dump_list();
	}else if(flags&4){
		build_font();
		FILE *fp = fopen(font_name, "wb");
		fwrite(font_buf, 1, font_buf_size, fp);
		fclose(fp);
	}else if(flags&8){
		build_font();
		bin2c(font_buf, font_buf_size, font_name, label_name);
	}

	return 0;
}

