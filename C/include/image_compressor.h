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

/// @brief Compress image file
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_image(const char* file, const char* output_file, const char* extension);

/// @brief Libheif compression used for .heic files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_heic(const char* file, const char* output_file);

/// @brief Libjpeg-turbo compression used for .jpg/.jpeg files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_jpeg(const char* file, const char* output_file);

/// @brief Libpng compression used for .png files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_png(const char* file, const char* output_file);

#ifdef __cplusplus
}
#endif

#endif //IMAGE_COMPRESSOR