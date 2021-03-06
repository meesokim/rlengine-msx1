/*
 * RetroDeLuxe Engine for MSX
 *
 * Copyright (C) 2017 Enric Martin Geijo (retrodeluxemsx@gmail.com)
 *
 * RDLEngine is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <getopt.h>
#include <libgen.h>
#include <string.h>

#define PALSIZE 16
#define MAX_SCR2_SIZE   6144

struct rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct scr2 {
    uint8_t patrn;
    uint8_t color;
};

struct fbit {
    uint8_t color;
};

struct rgb tms9918_pal[PALSIZE]= {
    { 0  ,0  ,0  },             /* 0 Transparent                   */
    { 0  ,0  ,0  },             /* 1 Black           0    0    0   */
    { 33 ,200,66 },             /* 2 Medium green   33  200   66   */
    { 94 ,220,120},             /* 3 Light green    94  220  120   */
    { 84 ,85 ,237},             /* 4 Dark blue      84   85  237   */
    { 125,118,252},             /* 5 Light blue    125  118  252   */
    { 212,82 ,77 },             /* 6 Dark red      212   82   77   */
    { 66 ,235,245},             /* 7 Cyan           66  235  245   */
    { 252,85 ,84 },             /* 8 Medium red    252   85   84   */
    { 255,121,120},             /* 9 Light red     255  121  120   */
    { 212,193,84 },             /* A Dark yellow   212  193   84   */
    { 230,206,128},             /* B Light yellow  230  206  128   */
    { 33 ,176,59 },             /* C Dark green     33  176   59   */
    { 201,91 ,186},             /* D Magenta       201   91  186   */
    { 204,204,204},             /* E Gray          204  204  204   */
    { 255,255,255}              /* F White         255  255  255   */
};

struct tga_header
{
    uint8_t identsize;          /* size of ID field that follows 18 byte header */
    uint8_t colourmaptype;      /* type of colour map 0=none, 1=has palette     */
    uint8_t imagetype;          /* type of image 0=none,1=indexed,2=rgb,3=grey, */
                                /*               +8=rle packed                  */
    int16_t colourmapstart;     /* first colour map entry in palette            */
    int16_t colourmaplength;    /* number of colours in palette                 */
    int16_t xstart;             /* image x origin                               */
    int16_t ystart;             /* image y origin                               */
    int16_t width;              /* image width in pixels                        */
    int16_t height;             /* image height in pixels                       */
    uint8_t bits;               /* image bits per pixel 8,16,24,32              */
    uint8_t descriptor;         /* image descriptor bits (00vhaaaa)             */
                                /*     (v : vertical flip, h : horizontal flip, */
                                /*                              a: alpha bits)  */
};

#define IMAGE_SIZE_B  (tga.width * tga.height / 8)

struct rgb *image;              /* input image rgb data          */
struct tga_header tga;          /* input image tga header        */
struct rgb palette[PALSIZE];    /* target palette                */
struct scr2 *image_out_scr2;    /* image out in scr2 format      */
struct fbit *image_out_4bit;    /* image out in 4bit pal format  */

char *input_file;               /* name of input file used to name data */

int rle_encode = 0;

uint16_t rgb_square_error(uint8_t clr, uint16_t x, uint16_t y)
{
    int16_t u0,u1,u2;
    uint8_t col = image_out_4bit[ x + y * tga.width].color;

    /* confusing , but works */
    u0 = (palette[col].r - palette[clr].r);
    u1 = (palette[col].g - palette[clr].g);
    u2 = (palette[col].b - palette[clr].b);
    return u0 * u0 + u1 * u1 + u2 * u2;
}


struct scr2 match_line(uint16_t x,  uint16_t y)
{
    int i;
    uint8_t c1, c2;
    uint16_t xx;
    uint8_t bp = 0, bc = 0;
    uint16_t cp, cs, bs = SHRT_MAX;
    uint16_t mc1, mc2;

    struct scr2 match;

    for (c1 = 1; c1 < 16; c1++) {
        for (c2 = c1 + 1; c2 < 16; c2++) {
            cs = 0; cp = 0;
            for (i = 0; i < 8; i++) {
                xx = ((x << 3) | i);

                mc1 = rgb_square_error(c1, xx, y);
                mc2 = rgb_square_error(c2, xx, y);

                cp = (cp << 1) | (mc1 > mc2);
                cs += (mc1 > mc2) ? mc2 : mc1;

                if (cs > bs) break;   /* error too big already */
            }
            if (cs < bs) {
                bs = cs;              /* best square error */
                bp = cp;              /* best pattern */
                bc = c2 * 16 + c1;    /* best color */
            }
        }
    }

    match.patrn = bp;
    match.color = bc;

    return match;
}

/**
 * XXX: we don't actually do floyd
 */
int tga2msx_scr2_tiles()
{
    uint16_t x, y, j;
    uint16_t yy, qe;
    struct scr2 *dst = image_out_scr2;

    for (y = 0; y < (tga.height + 7) >> 3; y++) {
        for (x = 0; x < (tga.width + 7 ) >> 3; x++) {
            /* process 8x8 pixel block */
            for (j = 0; j < 8; j++) {
                yy = ((y << 3) | j);

                *dst++ = match_line(x, yy);
            }
        }
    }
    return 0;
}


uint8_t find_min_sqerr_color(struct rgb *col, struct rgb *pal)
{
    uint8_t i, best = PALSIZE;
    double er, eg, eb, se, least = INT_MAX;

    // This requires fine tuning, is not accurate enough to
    // match properly different shades of the same color.

    for (i = 0; i < PALSIZE; i++) {
        er = (col->b - pal[i].r) * 0.4;
        eg = (col->g - pal[i].g) * 0.75;
        eb = (col->r - pal[i].b) * 0.8;
        se = er * er + eg * eg + eb * eb;

        if (se < least) {
            best = i;
            least = se;
        }
    }

    return best;
}

void dump_4bitimage()
{
    uint16_t i,j;

    for (j = 0; j < tga.height; j++) {
        for(i = 0; i < tga.width; i++) {
            printf("0x%2.2X,",(image_out_4bit + i + j * tga.width)->color);
        }
        printf("\n");
    }
}

int tga2msx_palette()
{
    struct rgb *src = image;
    struct fbit *dst = image_out_4bit;

    do {
        (dst++)->color = find_min_sqerr_color(src++, palette);
    } while (src < image + (tga.width * tga.height) - 1);

    return 0;
}

int verify_header(struct tga_header *header)
{

    if ((header->imagetype != 2)) {
        return 1;
    }
    return 0;
}

void dump_header(struct tga_header *header)
{
    printf("--- tga header ---\n");
    printf("idsize  \t: %d\n",header->identsize);

    if (header->colourmaptype)
        printf("pallette\t: yes\n");
    else
        printf("pallette\t: no\n");

    if ((header->imagetype & 0x07) == 0)
        printf("color     \t: none\n");

    if ((header->imagetype & 0x07) == 1)
        printf("color     \t: indexed\n");

    if ((header->imagetype & 0x07) == 2)
        printf("color     \t: rgb\n");

    if ((header->imagetype & 0x07) == 3)
        printf("color     \t: greyscale\n");

    if (header->imagetype & 8)
        printf("rle compr\t: yes\n");
    else
        printf("rle compr\t: no\n");

    printf("offset x \t: %d\n",header->xstart);
    printf("offset y \t: %d\n",header->ystart);
    printf("width  px\t: %d\n",header->width);
    printf("heigth px\t: %d\n",header->height);
    printf("bpp      \t: %d\n",header->bits);

    if (header->descriptor & 0x20)
        printf("vert flip\t: yes\n");
    else
        printf("vert flip\t: no\n");

    if (header->descriptor & 0x10)
        printf("horz flip\t: yes\n");
    else
        printf("horz flip\t: no\n");

    printf("alpha\t: %d\n", header->descriptor & 0x0f);

    printf(" --- --- --- ---\n");
}

void usage(void)
{
    int i;

    printf("Usage: tga2header [OPTION]... [file.tga]\n"
           "Transform tga graphic file into MSX C header\n"
           " -h, --help           Print this help.\n"
           " -f, --full (DEFAULT) full tga to scr2 format conversion,\n"
           " -p, --palette        convert to msx1 palette only,\n"
           " -t, --type=TYPE      output format,\n"
           "                          TILE    : scr2 pattern & color C source,\n"
           "                          TILEH   : scr2 pattern & color H source (no data),\n"
           "                          SPRITE  : msx1 16x16 sprite planes C source,\n"
           "                          SPRITEH : msx1 16x16 sprite planes H source (no data),\n"
           " -o, --output=FILE    output file,\n"
           "\n");
}


/* -------------------------------------- */


static int load_image(int fileidx, int argc, char **argv)
{
    FILE *file;
    uint16_t i, tgasize, nr;
    struct rgb *imidx;


    if (argc - fileidx <= 0) {
        fprintf(stderr, "No input file specified\n");
        usage();
        return -1;
    }

    input_file = argv[fileidx];

    file = fopen(input_file, "rb");
    if (file == NULL) {
        fprintf(stderr, "Cannot open %s file!\n",argv[1]);
        return -1;
    }

    nr = fread(&tga, sizeof(struct tga_header), 1, file);
    if (nr != 1) {
        fprintf(stderr, "Unable to read file header.\n");
        return -1;
    }

    if(verify_header(&tga)) {
        fprintf(stderr, "Unsupported file format\n");
        dump_header(&tga);
        return -1;
    }

    tgasize = tga.width * tga.height;

    image = malloc(tgasize * sizeof(struct rgb));

    // FIXME: these shouldn't be here
    image_out_4bit = malloc(tgasize * sizeof(struct fbit));
    image_out_scr2 = malloc(MAX_SCR2_SIZE * sizeof(struct scr2));

    if (image == NULL) {
        fprintf(stderr, "Unable to allocate memory\n");
        return -1;
    }

    nr = fread(image, sizeof(struct rgb), tgasize, file);
    if (nr != tgasize) {
        fprintf(stderr, "Unable to read file data.\n");
        return -1;
    }

    fclose(file);

    return 0;
}


void dump_sprite_8x8_block(FILE *fd, struct fbit *base, uint8_t color)
{
    uint8_t i,j, enabled;
    uint8_t byte = 0;

    for (j = 0; j < 8; j++) {
        for (i = 0; i < 8; i++) {
            enabled = (base + i + j * tga.width)->color == color ? 1 : 0;
            byte = byte << 1 | enabled;
        }
        fprintf(fd, "0x%2.2X,",byte);
    }
    fprintf(fd, "\n");
}

int block_8x8_has_color(struct fbit *base, uint8_t color)
{
    uint8_t i,j;

    for(j=0;j<8;j++) {
        for(i=0;i<8;i++) {
            if ((base + i + j * tga.width)->color == color)
                return 1;
        }
    }
    return 0;
}

int pattern_has_color(struct fbit *idx, uint8_t color)
{
    return block_8x8_has_color(idx,color) |
           block_8x8_has_color(idx + 8 ,color) |
           block_8x8_has_color(idx + tga.width * 8,color) |
           block_8x8_has_color(idx + 8 + tga.width * 8 ,color);
}

void dump_sprite_file(FILE *fd, int only_header)
{
    struct fbit *idx = image_out_4bit;
    uint8_t color, colcnt = 0, np = 0;
    char *dataname, *filename, *path;
    uint8_t used_colors[16], used_cnt = 0;

    path = strdup(input_file);
    filename = basename(path);
    dataname = strdup(filename);
    dataname[strlen(dataname)-4]='\0';

    fprintf(fd, "#ifndef _GENERATED_SPRITES_H_%s\n", dataname);
    fprintf(fd, "#define _GENERATED_SPRITES_H_%s\n", dataname);

    if (only_header) {
            fprintf(fd, "extern const unsigned char %s_color[];", dataname);
            fprintf(fd, "extern const unsigned char %s[];\n", dataname);
            fprintf(fd, "#endif\n");
            return;
    }

    fprintf(fd, "const unsigned char %s_color[] = { ", dataname);

    do {
        for (color = 1; color < 16; color++) {
            if (pattern_has_color(idx, color)) {
                fprintf(fd, "%d,", color);
            }
        }
        if (++colcnt > (tga.width / 16) - 1) {
            colcnt = 0;
            idx += tga.width * 15 + 16;
        } else {
            idx += 16;
        }
        np++;
    } while (idx < image_out_4bit + (tga.width * tga.height) - 1);

    fprintf(fd, "0 };\n");

    colcnt = 0; idx = image_out_4bit; np = 0;

    fprintf(fd, "const unsigned char %s[] = {\n", dataname);

    do {
        for (color = 1; color < 16; color++) {
            if (pattern_has_color(idx, color)) {

                fprintf(fd, "/* ---- pattern: %d color: %d ---- */\n", np, color);

                dump_sprite_8x8_block(fd, idx, color);
                dump_sprite_8x8_block(fd, idx + tga.width * 8, color);
                dump_sprite_8x8_block(fd, idx + 8, color);
                dump_sprite_8x8_block(fd, idx + 8 + tga.width * 8, color);
            }
        }
        /* move to the next block */
        if (++colcnt > (tga.width / 16) - 1) {
            colcnt = 0;
            idx += tga.width * 15 + 16;
        } else {
            idx += 16;
        }
        np++;
    } while (idx < image_out_4bit + (tga.width * tga.height) - 1);

    fprintf(fd, "0x00};\n");
    fprintf(fd, "#endif\n");

}


void dump_buffer_rle(struct scr2 *buffer, FILE *file, int type)
{
        int16_t curr_byte, prev_byte, cnt = 0;
        uint8_t run_cnt = 0;
        struct scr2 *p;

        prev_byte = -1;

        p = buffer;
        while (cnt < IMAGE_SIZE_B) {
                if (type == 0)
                        curr_byte = (p++)->patrn;
                else
                        curr_byte = (p++)->color;
                cnt++;
                if (cnt == IMAGE_SIZE_B)
                        fprintf(file,"0x%2.2X};\n\n", curr_byte);
                else
                        fprintf(file,"0x%2.2X,", curr_byte);
                if (curr_byte == prev_byte) {
                        run_cnt = 0;
                        while (cnt < IMAGE_SIZE_B) {
                                if (type == 0)
                                        curr_byte = (p++)->patrn;
                                else
                                        curr_byte = (p++)->color;
                                cnt++;
                                if (curr_byte == prev_byte) {
                                        run_cnt++;
                                        if (run_cnt == 255) {
                                                fprintf(file,"0x%2.2X,", run_cnt);
                                                prev_byte = -1;
                                                break;
                                        }
                                } else {
                                        fprintf(file,"0x%2.2X,", run_cnt);
                                        if (cnt == IMAGE_SIZE_B)
                                                fprintf(file,"0x%2.2X};\n\n", curr_byte);
                                        else
                                                fprintf(file,"0x%2.2X,", curr_byte);
                                        prev_byte = curr_byte;
                                        run_cnt = 0;
                                        break;
                                }

                        }
                } else {
                        prev_byte = curr_byte;
                }
                if (cnt == IMAGE_SIZE_B) {
                        if (run_cnt != 0)
                                fprintf(file,"0x%2.2X};\n\n", run_cnt);
                        break;
                }
        }
}

void dump_tiles(struct scr2 *buffer, FILE *file, int only_header)
{
    struct scr2 *p;
    int bytectr=0, cnt;
    char *dataname, *filename, *path;

    path = strdup(input_file);
    filename = basename(path);
    dataname = strdup(filename);
    dataname[strlen(dataname)-4]='\0';

    fprintf(file,"#ifndef _GENERATED_TILESET_H_%s\n", dataname);
    fprintf(file,"#define _GENERATED_TILESET_H_%s\n", dataname);

    if (only_header) {
            fprintf(file,"extern const unsigned char %s_tile_w;\n", dataname);
            fprintf(file,"extern const unsigned char %s_tile_h;\n", dataname);
            fprintf(file,"extern const unsigned char %s_tile[];\n", dataname);
            fprintf(file,"extern const unsigned char %s_tile_color[];\n",dataname);
            return;
    }

    fprintf(file,"const unsigned char %s_tile_w = %d;\n", dataname, tga.width / 8);
    fprintf(file,"const unsigned char %s_tile_h = %d;\n", dataname, tga.height / 8);
    fprintf(file,"const unsigned char %s_tile[]={\n",dataname);

    if (rle_encode)
        dump_buffer_rle(buffer, file, 0);
    else {
            p = buffer;
            for (cnt = 0; cnt < (tga.width * tga.height / 8) ; p++, cnt++) {
                if(bytectr++ > 6) {
                    fprintf(file,"0x%2.2X,\n",p->patrn);
                    bytectr=0;
                } else {
                    fprintf(file,"0x%2.2X,",p->patrn);
                }
            }
            fprintf(file,"0x%2.2X};\n\n",p->patrn);
    }
    fprintf(file,"const unsigned char %s_tile_color[]={\n",dataname);

    if (rle_encode)
        dump_buffer_rle(buffer, file, 1);
    else {
            p = buffer;
            for (cnt = 0; cnt < (tga.width * tga.height / 8); p++, cnt++) {
                if(bytectr++ > 6) {
                    fprintf(file,"0x%2.2X,\n",p->color);
                    bytectr=0;
                } else {
                    fprintf(file,"0x%2.2X,",p->color);
                }
            }
            fprintf(file,"0x%2.2X};\n\n",p->color);
    }
}

void dump_tile_file(FILE *fd, int only_header)
{

    dump_tiles(image_out_scr2, fd, only_header);

    fprintf(fd,"#endif\n");
}

int generate_header(char *outfile, char *type)
{

    int do_tile = 0;
    int do_sprite = 0;
    int do_only_header = 0;
    FILE *file;

    if (!strcmp(type, "TILE")) {
        do_tile = 1;
    } else if (!strcmp(type, "SPRITE")) {
        do_sprite = 1;
    } else if (!strcmp(type, "TILEH")) {
        do_tile = 1;
        do_only_header = 1;
    } else if (!strcmp(type, "SPRITEH")) {
        do_sprite = 1;
        do_only_header = 1;
    } else {
        printf("Unsupported output type.\n");
        usage();
        return -1;
    }

    file = fopen(outfile,"w");
    if (file == NULL) {
        fprintf(stderr, "Cannot open %s file for writing\n",outfile);
        return -1;
    }

    if (do_sprite)
        dump_sprite_file(file, do_only_header);
    else if (do_tile)
        dump_tile_file(file, do_only_header);

    fclose(file);
    return 0;
}

int main(int argc, char **argv)
{
    struct rgb *imidx;

    int do_full = 1;
    int do_palette = 0;
    int fileidx;
    int result = 0;
    char *type = 0;
    char *outfile = 0;
    int opt;

#define OPT_HELP    'h'
#define OPT_FULL    'f'
#define OPT_PALETTE 'p'
#define OPT_TYPE    't'
#define OPT_OUTPUT  'o'
#define OPT_RLE 'z'


#define ALL_OPTIONS \
    { "help",   0, 0, OPT_HELP }, \
    { "full",   0, 0, OPT_FULL }, \
    { "palette", 0, 0, OPT_PALETTE }, \
    { "type",   1, 0, OPT_TYPE }, \
    { "output", 1, 0, OPT_OUTPUT }, \
    { "rle", 0, 0, OPT_RLE }, \

#define OPT_STR "hfpt:o:z"

    static const struct option options[] = {
        ALL_OPTIONS
        { 0, 0, 0, 0},
    };
    static const char short_options[] = OPT_STR;

    while ((opt = getopt_long(argc, argv, short_options,
        options, 0)) != -1) {
        switch(opt) {
            case '?':
            case OPT_HELP:
                usage();
                return 0;
            case OPT_FULL:
                do_full = 1;
                break;
            case OPT_PALETTE:
                do_full = 0;
                do_palette = 1;
                break;
            case OPT_TYPE:
                type = optarg;
                break;
            case OPT_OUTPUT:
                outfile = optarg;
                break;
            case OPT_RLE:
                rle_encode = 1;
                break;
        default:
            break;
        }
    }

    fileidx = optind;

    if (type == 0 || outfile == 0) {
        fprintf(stderr, "Need to specify output filename and type\n");
        usage();
        return -1;
    }

    if (!do_palette && !strcmp(type,"SPRITE")) {
        fprintf(stderr, "output type SPRITE requires palette only processing\n");
        usage();
        return -1;
    }

    int i;
    for (i=0; i<PALSIZE; i++) {
        palette[i].r = tms9918_pal[i].r;
        palette[i].g = tms9918_pal[i].g;
        palette[i].b = tms9918_pal[i].b;
    }

    result = load_image(fileidx, argc, argv);

    // handle this...

    if (!strcmp(type,"TILE") && ((tga.width / 8 * tga.height > 2048) ||
                (tga.width % 8 != 0 || tga.height % 8 != 0))) {
         fprintf(stderr, "When generating TILE output, \
                        input file size must have width and heigth multiple of 8 and be smaller than 256x64 pixels (16Kb).\n");
        return -1;
    }


    if ((result == 0) && do_full) {
        result = tga2msx_palette();
        //dump_4bitimage();
        result = tga2msx_scr2_tiles();
    }

    if ((result == 0) && do_palette) {
        result = tga2msx_palette();
    }

    if (result == 0) {
        result = generate_header(outfile, type);
    }


    fflush(stdout);
    fflush(stderr);
    return result;
}
