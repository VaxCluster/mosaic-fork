/****************************************************************************
 * NCSA Mosaic for the X Window System                                      *
 * Software Development Group                                               *
 * National Center for Supercomputing Applications                          *
 * University of Illinois at Urbana-Champaign                               *
 * 605 E. Springfield, Champaign IL 61820                                   *
 * mosaic@ncsa.uiuc.edu                                                     *
 *                                                                          *
 * Copyright (C) 1993, Board of Trustees of the University of Illinois      *
 *                                                                          *
 * NCSA Mosaic software, both binary and source (hereafter, Software) is    *
 * copyrighted by The Board of Trustees of the University of Illinois       *
 * (UI), and ownership remains with the UI.                                 *
 *                                                                          *
 * The UI grants you (hereafter, Licensee) a license to use the Software    *
 * for academic, research and internal business purposes only, without a    *
 * fee.  Licensee may distribute the binary and source code (if released)   *
 * to third parties provided that the copyright notice and this statement   *
 * appears on all copies and that no charge is associated with such         *
 * copies.                                                                  *
 *                                                                          *
 * Licensee may make derivative works.  However, if Licensee distributes    *
 * any derivative work based on or derived from the Software, then          *
 * Licensee will (1) notify NCSA regarding its distribution of the          *
 * derivative work, and (2) clearly notify users that such derivative       *
 * work is a modified version and not the original NCSA Mosaic              *
 * distributed by the UI.                                                   *
 *                                                                          *
 * Any Licensee wishing to make commercial use of the Software should       *
 * contact the UI, c/o NCSA, to negotiate an appropriate license for such   *
 * commercial use.  Commercial use includes (1) integration of all or       *
 * part of the source code into a product for sale or license by or on      *
 * behalf of Licensee to third parties, or (2) distribution of the binary   *
 * code or source code to third parties that need it to utilize a           *
 * commercial product sold or licensed by or on behalf of Licensee.         *
 *                                                                          *
 * UI MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR   *
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED          *
 * WARRANTY.  THE UI SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY THE    *
 * USERS OF THIS SOFTWARE.                                                  *
 *                                                                          *
 * By using or copying this Software, Licensee agrees to abide by the       *
 * copyright law and all other applicable laws of the U.S. including, but   *
 * not limited to, export control laws, and the terms of this license.      *
 * UI shall have the right to terminate this license immediately by         *
 * written notice upon Licensee's breach of, or non-compliance with, any    *
 * of its terms.  Licensee may be held legally responsible for any          *
 * copyright infringement that is caused or encouraged by Licensee's        *
 * failure to abide by the terms of this license.                           *
 *                                                                          *
 * Comments and questions are welcome and can be sent to                    *
 * mosaic-x@ncsa.uiuc.edu.                                                  *
 ****************************************************************************/

/* Author: DXP

 A lot of this is copied from the PNGLIB file example.c

 Modified:

    August   1995 - Glenn Randers-Pehrson <glennrp@arl.mil>
                    Changed dithering to use a 6x6x6 color cube.

    March 21 1996 - DXP
                    Fixed some interlacing problems.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <X11/Xlib.h>
#include <limits.h>

// RGB hint thresholds
#define RED_HINT   0xD0, 0x20, 0x20
#define GREEN_HINT 0x20, 0xD0, 0x20
#define BLUE_HINT  0x20, 0x20, 0xD0

static int color_exists(XColor *colrs, int count, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < count; i++) {
        if ((colrs[i].red >> 8) == r &&
            (colrs[i].green >> 8) == g &&
            (colrs[i].blue >> 8) == b) {
            return 1;
        }
    }
    return 0;
}

static void reserve_color(XColor *colrs, int *num_colors, unsigned char r, unsigned char g, unsigned char b) {
    if (*num_colors < 256 && !color_exists(colrs, *num_colors, r, g, b)) {
        colrs[*num_colors].red = r << 8;
        colrs[*num_colors].green = g << 8;
        colrs[*num_colors].blue = b << 8;
        colrs[*num_colors].pixel = *num_colors;
        colrs[*num_colors].flags = DoRed | DoGreen | DoBlue;
        (*num_colors)++;
    }
}

static int closest_color(XColor *colrs, int num_colors, unsigned char r, unsigned char g, unsigned char b) {
    int best = 0;
    long best_dist = LONG_MAX;
    for (int i = 0; i < num_colors; i++) {
        long dr = r - (colrs[i].red >> 8);
        long dg = g - (colrs[i].green >> 8);
        long db = b - (colrs[i].blue >> 8);
        long dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

static int find_or_add_color(XColor *colrs, int *num_colors, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < *num_colors; i++) {
        if ((colrs[i].red >> 8) == r &&
            (colrs[i].green >> 8) == g &&
            (colrs[i].blue >> 8) == b) {
            return i;
        }
    }

    if (*num_colors < 256) {
        colrs[*num_colors].red = r << 8;
        colrs[*num_colors].green = g << 8;
        colrs[*num_colors].blue = b << 8;
        colrs[*num_colors].pixel = *num_colors;
        colrs[*num_colors].flags = DoRed | DoGreen | DoBlue;
        return (*num_colors)++;
    }

    return closest_color(colrs, *num_colors, r, g, b);
}

unsigned char *ReadPNG(FILE *infile, int *width, int *height, XColor *colrs) {
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers = NULL;
    png_bytep image_data;
    unsigned char *pixmap;
    int rowbytes, channels;
    int i, j, num_colors = 0;

    unsigned char sig[8];
    if (fread(sig, 1, 8, infile) != 8 || png_sig_cmp(sig, 0, 8))
        return NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        if (row_pointers) free(row_pointers);
        return NULL;
    }

    png_init_io(png_ptr, infile);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);

    if (png_get_bit_depth(png_ptr, info_ptr) == 16)
        png_set_strip_16(png_ptr);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY ||
        png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY_ALPHA) {
#ifdef PNG_READ_EXPAND_SUPPORTED
        png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
        png_set_expand(png_ptr);
#endif
    }

#ifdef PNG_READ_tRNS_SUPPORTED
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
#endif

    png_set_strip_alpha(png_ptr);

    if (png_get_interlace_type(png_ptr, info_ptr) == PNG_INTERLACE_ADAM7)
        png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    channels = png_get_channels(png_ptr, info_ptr);

    image_data = (png_bytep) malloc(rowbytes * (*height));
    if (!image_data) return NULL;

    row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * (*height));
    if (!row_pointers) {
        free(image_data);
        return NULL;
    }

    for (i = 0; i < *height; i++)
        row_pointers[i] = image_data + i * rowbytes;

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    // Reserve base colors
    reserve_color(colrs, &num_colors, RED_HINT);
    reserve_color(colrs, &num_colors, GREEN_HINT);
    reserve_color(colrs, &num_colors, BLUE_HINT);

    // Output indexed pixmap
    pixmap = (unsigned char *) malloc((*width) * (*height));
    if (!pixmap) {
        free(image_data);
        free(row_pointers);
        return NULL;
    }

    for (i = 0; i < *height; i++) {
        for (j = 0; j < *width; j++) {
            int index = i * (*width) + j;
            int r = row_pointers[i][j * channels + 0];
            int g = row_pointers[i][j * channels + 1];
            int b = row_pointers[i][j * channels + 2];
            pixmap[index] = find_or_add_color(colrs, &num_colors, r, g, b);
        }
    }

    free(image_data);
    free(row_pointers);
    return pixmap;
}
