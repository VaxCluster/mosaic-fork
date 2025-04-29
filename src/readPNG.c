#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <X11/Xlib.h>

unsigned char *ReadPNG(FILE *infile, int *width, int *height, XColor *colrs) {
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep *row_pointers = NULL;
    png_bytep image_data;
    png_colorp palette;
    int num_palette;
    unsigned char *pixmap;
    int rowbytes;
    int i, j;

    // Check PNG signature
    unsigned char sig[8];
    if (fread(sig, 1, 8, infile) != 8 || png_sig_cmp(sig, 0, 8))
        return NULL;

    // Create read struct
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return NULL;

    // Create info struct
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    // Set error handling
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

    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    // Transformations
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    png_set_strip_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    image_data = (png_bytep) malloc(rowbytes * (*height));
    row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * (*height));

    for (i = 0; i < *height; i++)
        row_pointers[i] = image_data + i * rowbytes;

    png_read_image(png_ptr, row_pointers);

    // Assume 8-bit palettized image for compatibility
    if (png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette)) {
        for (i = 0; i < num_palette && i < 256; i++) {
            colrs[i].red = palette[i].red << 8;
            colrs[i].green = palette[i].green << 8;
            colrs[i].blue = palette[i].blue << 8;
            colrs[i].pixel = i;
            colrs[i].flags = DoRed | DoGreen | DoBlue;
        }
    }

    // Flatten image data to one byte per pixel (use first channel if RGB)
    pixmap = (unsigned char *) malloc((*width) * (*height));
    for (i = 0; i < *height; i++) {
        for (j = 0; j < *width; j++) {
            pixmap[i * (*width) + j] = row_pointers[i][j * 3];
        }
    }

    free(image_data);
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return pixmap;
}
