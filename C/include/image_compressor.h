#ifndef IMAGE_COMPRESSOR
#define IMAGE_COMPRESSOR

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

//Set photo quality for image compression: 0 (worst), 50 (medium), 90 (near-lossless)
#define PHOTO_QUALITY 50 
//Ptoho trim dimensions
#define PHOTO_TRIM_WIDTH 3840
#define PHOTO_TRIM_HEIGHT 2160

bool compress_image(const char* file, const char* output_file, const char* extension);
bool compress_heic(const char* file, const char* output_file);
bool compress_jpeg(const char* file, const char* output_file);
bool compress_png(const char* file, const char* output_file);

#ifdef __cplusplus
}
#endif

#endif //IMAGE_COMPRESSOR