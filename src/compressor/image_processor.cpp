#include "utils/utils.h"
#include "compressor/image_processor.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <jpeglib.h>
#include <libexif/exif-data.h>
#include <png.h>
#include <libheif/heif.h>

namespace media_handler::compressor {

    ImageProcessor::ImageProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger)
		: config(cfg), logger(std::move(logger)) { //TODO std::move() to avoid ref-count increment
    }

    static bool file_exists_and_readable(const std::filesystem::path& path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
    }

    ProcessResult ImageProcessor::fallback_copy(const std::filesystem::path& input, const std::filesystem::path& output) {
        try {
            std::ifstream src(input, std::ios::binary);
            if (!src) return ProcessResult::Error("Failed to open input for copy");

            std::ofstream dst(output, std::ios::binary);
            if (!dst) return ProcessResult::Error("Failed to open output for copy");

            dst << src.rdbuf();
            return ProcessResult::OK();
        }
        catch (const std::filesystem::filesystem_error& e) {
            return ProcessResult::Error(std::format("Filesystem error during copy: {}", e.what()));
        }
    }

    ProcessResult ImageProcessor::compress_jpeg(const std::filesystem::path& input, const std::filesystem::path& output) {
        FILE* infile = fopen(input.string().c_str(), "rb");
        if (!infile) {
            return ProcessResult::Error("Failed to open input JPEG");
        }

        FILE* outfile = fopen(output.string().c_str(), "wb");
        if (!outfile) {
            fclose(infile);
            return ProcessResult::Error("Failed to open output JPEG");
        }

        struct jpeg_decompress_struct srcinfo;
        struct jpeg_compress_struct dstinfo;
        struct jpeg_error_mgr jerr;

        srcinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&srcinfo);
        jpeg_stdio_src(&srcinfo, infile);
        if (jpeg_read_header(&srcinfo, TRUE) != JPEG_HEADER_OK) {
            jpeg_destroy_decompress(&srcinfo);
            fclose(infile);
            fclose(outfile);
            return ProcessResult::Error("Invalid JPEG header");
        }

        jpeg_start_decompress(&srcinfo);
        dstinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&dstinfo);
        jpeg_stdio_dest(&dstinfo, outfile);

        dstinfo.image_width = srcinfo.output_width;
        dstinfo.image_height = srcinfo.output_height;
        dstinfo.input_components = srcinfo.output_components;
        dstinfo.in_color_space = srcinfo.out_color_space;

        jpeg_set_defaults(&dstinfo);
        jpeg_set_quality(&dstinfo, PHOTO_QUALITY, TRUE);
        jpeg_start_compress(&dstinfo, TRUE);

        // Try to copy EXIF if present
        ExifData* exif = exif_data_new_from_file(input.string().c_str());
        if (exif) {
            unsigned char* exif_buf = nullptr;
            unsigned int exif_len = 0;
            exif_data_save_data(exif, &exif_buf, &exif_len);
            if (exif_buf && exif_len > 6 && memcmp(exif_buf, "Exif\0\0", 6) == 0) {
                jpeg_write_marker(&dstinfo, JPEG_APP0 + 1, exif_buf, exif_len);
            }
            if (exif_buf) free(exif_buf);
            exif_data_unref(exif);
        }

        // allocate buffer for one scanline
        const int row_stride = srcinfo.output_width * srcinfo.output_components;
        JSAMPARRAY buffer = (*srcinfo.mem->alloc_sarray)((j_common_ptr)&srcinfo, JPOOL_IMAGE, row_stride, 1);
        while (srcinfo.output_scanline < srcinfo.output_height) {
            jpeg_read_scanlines(&srcinfo, buffer, 1);
            jpeg_write_scanlines(&dstinfo, buffer, 1);
        }

        jpeg_finish_compress(&dstinfo);
        jpeg_finish_decompress(&srcinfo);
        jpeg_destroy_compress(&dstinfo);
        jpeg_destroy_decompress(&srcinfo);
        fclose(infile);
        fclose(outfile);

        return ProcessResult::OK();
    }

    ProcessResult ImageProcessor::compress_heic(const std::filesystem::path& input, const std::filesystem::path& output) {
        //Signature check
        FILE* f = fopen(input.string().c_str(), "rb");
        if (!f) return ProcessResult::Error("Failed to open HEIC for signature check");
        unsigned char header[12] = { 0 };
        size_t r = fread(header, 1, sizeof(header), f);
        fclose(f);
        if (r < 12 || memcmp(&header[4], "ftypheic", 8) != 0) {
            return ProcessResult::Error("Not a HEIC file (signature mismatch)");
        }

        heif_context* ctx = heif_context_alloc();
        if (!ctx) return ProcessResult::Error("Failed to allocate heif context");

        heif_error err = heif_context_read_from_file(ctx, input.string().c_str(), nullptr);
        if (err.code != heif_error_Ok) {
            heif_context_free(ctx);
            return ProcessResult::Error(std::string("heif read error: ") + (err.message ? err.message : "unknown"));
        }

        heif_image_handle* handle = nullptr;
        err = heif_context_get_primary_image_handle(ctx, &handle);
        if (err.code != heif_error_Ok || !handle) {
            heif_context_free(ctx);
            return ProcessResult::Error("Failed to get primary image handle");
        }

        heif_image* image = nullptr;
        err = heif_decode_image(handle, &image, heif_colorspace_RGB, heif_chroma_interleaved_RGB, nullptr);
        if (err.code != heif_error_Ok || !image) {
            heif_image_handle_release(handle);
            heif_context_free(ctx);
            return ProcessResult::Error("Failed to decode HEIC image");
        }

        heif_encoder* encoder = nullptr;
        err = heif_context_get_encoder_for_format(ctx, heif_compression_AV1, &encoder);
        if (err.code != heif_error_Ok || !encoder) {
            heif_image_release(image);
            heif_image_handle_release(handle);
            heif_context_free(ctx);
            return ProcessResult::Error("Failed to get AV1 encoder");
        }

        heif_encoder_set_lossy_quality(encoder, PHOTO_QUALITY);
        err = heif_context_encode_image(ctx, image, encoder, nullptr, nullptr);
        if (err.code != heif_error_Ok) {
            heif_encoder_release(encoder);
            heif_image_release(image);
            heif_image_handle_release(handle);
            heif_context_free(ctx);
            return ProcessResult::Error("Failed to encode HEIC image");
        }

        err = heif_context_write_to_file(ctx, output.string().c_str());
        heif_encoder_release(encoder);
        heif_image_release(image);
        heif_image_handle_release(handle);
        heif_context_free(ctx);

        if (err.code != heif_error_Ok) {
            return ProcessResult::Error(std::string("Failed to write HEIC: ") + (err.message ? err.message : "unknown"));
        }

        return ProcessResult::OK();
    }

    ProcessResult ImageProcessor::compress_png(const std::filesystem::path& input, const std::filesystem::path& output) {
        png_structp png_read_ptr = NULL;
        png_infop read_info_ptr = NULL;
        png_structp png_write_ptr = NULL;
        png_infop write_info_ptr = NULL;
        unsigned char* image_data = NULL;
        FILE* in_file = NULL;
        FILE* out_file = NULL;

        // Open input file
        in_file = fopen(input.string().c_str(), "rb");
        if (!in_file) {
            return ProcessResult::Error(std::format("Failed to open input file {}: {}", input.string(), strerror(errno)));
        }

        // Check PNG signature
        unsigned char sig[8];
        if (fread(sig, 1, 8, in_file) != 8 || memcmp(sig, "\x89PNG\r\n\x1A\n", 8) != 0) {
            fclose(in_file);
            return ProcessResult::Error(std::format("File {} is not a valid PNG (missing PNG signature)", input.string()));
        }
        rewind(in_file); // Reset file pointer

        // Initialize libpng for reading
        png_read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_read_ptr) {
            fclose(in_file);
            return ProcessResult::Error(std::format("Failed to create read structure for file {}", input.string()));
        }

        read_info_ptr = png_create_info_struct(png_read_ptr);
        if (!read_info_ptr) {
            png_destroy_read_struct(&png_read_ptr, NULL, NULL);
            fclose(in_file);
            return ProcessResult::Error(std::format("Failed to create png read info struct for file {}", input.string()));
        }

        // Error handling for reading
        if (setjmp(png_jmpbuf(png_read_ptr))) {
            png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL);
            if (image_data) free(image_data);
            fclose(in_file);
            return ProcessResult::Error(std::format("Error reading png file {}", input.string()));
        }

        png_init_io(png_read_ptr, in_file);
        png_read_info(png_read_ptr, read_info_ptr);

        // Get image info
        png_uint_32 width = png_get_image_width(png_read_ptr, read_info_ptr);
        png_uint_32 height = png_get_image_height(png_read_ptr, read_info_ptr);
        int color_type = png_get_color_type(png_read_ptr, read_info_ptr);
        int bit_depth = png_get_bit_depth(png_read_ptr, read_info_ptr);

        // Convert to RGB if necessary
        if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_read_ptr);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_read_ptr);
        if (png_get_valid(png_read_ptr, read_info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_read_ptr);
        if (bit_depth == 16) png_set_strip_16(png_read_ptr);

        // Update info after transformations
        png_read_update_info(png_read_ptr, read_info_ptr);
        color_type = png_get_color_type(png_read_ptr, read_info_ptr);
        bit_depth = png_get_bit_depth(png_read_ptr, read_info_ptr);

        // Allocate image data
        image_data = (unsigned char*)malloc(height * png_get_rowbytes(png_read_ptr, read_info_ptr));
        if (!image_data) {
            png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL);
            fclose(in_file);
            return ProcessResult::Error(std::format("Failed to allocate memory for png data in {}", input.string()));
        }

        png_bytep* row_pointers = (png_bytep*)malloc(height * sizeof(png_bytep));
        if (!row_pointers) {
            free(image_data);
            png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL);
            fclose(in_file);
            return ProcessResult::Error(std::format("Failed to allocate row pointer for {}", input.string()));
        }

        // Initialize row pointers
        for (png_uint_32 y = 0; y < height; y++) {
            row_pointers[y] = image_data + y * png_get_rowbytes(png_read_ptr, read_info_ptr);
        }

        // Read the image
        png_read_image(png_read_ptr, row_pointers);
        png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL);
        fclose(in_file);

        // Resize output dimensions
        png_uint_32 out_width = width > PHOTO_TRIM_WIDTH ? PHOTO_TRIM_WIDTH : width;
        png_uint_32 out_height = height > PHOTO_TRIM_HEIGHT ? PHOTO_TRIM_HEIGHT : height;

        // Open output file
        out_file = fopen(output.string().c_str(), "wb");
        if (!out_file) {
            free(image_data);
            free(row_pointers);
            return ProcessResult::Error(std::format("Failed to open output file {}: {}", output.string(), strerror(errno)));
        }

        // Initialize libpng for writing
        png_write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_write_ptr) {
            free(image_data);
            free(row_pointers);
            fclose(out_file);
            return ProcessResult::Error(std::format("Failed to create png write struct for {}", output.string()));
        }

        write_info_ptr = png_create_info_struct(png_write_ptr);
        if (!write_info_ptr) {
            png_destroy_write_struct(&png_write_ptr, NULL);
            free(image_data);
            free(row_pointers);
            fclose(out_file);
            return ProcessResult::Error(std::format("Failed to create png write info struct for {}", output.string()));
        }

        if (setjmp(png_jmpbuf(png_write_ptr))) {
            png_destroy_write_struct(&png_write_ptr, &write_info_ptr);
            free(image_data);
            free(row_pointers);
            fclose(out_file);
            return ProcessResult::Error(std::format("Error writing png file {}", output.string()));
        }

        png_init_io(png_write_ptr, out_file);
        png_set_IHDR(png_write_ptr, write_info_ptr, out_width, out_height, bit_depth,
            color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);
        png_set_compression_level(png_write_ptr, 9); // Maximum compression
        png_write_info(png_write_ptr, write_info_ptr);

        // Write scaled rows (simple nearest-neighbor scaling)
        for (png_uint_32 y = 0; y < out_height; y++) {
            png_uint_32 src_y = (png_uint_32)((double)y * height / out_height);
            png_write_row(png_write_ptr, row_pointers[src_y]);
        }

        // Finish writing
        png_write_end(png_write_ptr, write_info_ptr);
        png_destroy_write_struct(&png_write_ptr, &write_info_ptr);
        free(image_data);
        free(row_pointers);
        fclose(out_file);

        return ProcessResult::OK();
    }

    ProcessResult ImageProcessor::compress(const std::filesystem::path& input, const std::filesystem::path& output) {
        try {
            if (!file_exists_and_readable(input)) return ProcessResult::Error("Input file missing");
            auto ext = input.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext == ".jpg" || ext == ".jpeg") {
                return compress_jpeg(input, output);
            }
            else if (ext == ".png") {
                return compress_png(input, output);
            }
            else if (ext == ".heic" || ext == ".heif") {
                return compress_heic(input, output);
            }
            else {
                return fallback_copy(input, output);
            }
        }
        catch (const std::exception& e) {
            return ProcessResult::Error(std::string("Exception in process: ") + e.what());
        }
        catch (...) {
            return ProcessResult::Error("Unknown exception in process");
        }
    }

} // namespace media_handler::compressor