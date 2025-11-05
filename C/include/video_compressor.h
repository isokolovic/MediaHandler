#ifndef VIDEO_COMPRESSOR
#define VIDEO_COMPRESSOR

#ifdef __cplusplus
extern "C" {
#endif

#include "file_handler.h"
#include "logger.h"
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

typedef struct {
    AVFormatContext* in_ctx;
    AVFormatContext* out_ctx;
    AVPacket* pkt;
    AVFrame* frame;
    AVFrame* converted_frame;
    int* stream_mapping;
    AVCodecContext** dec_contexts;
    AVCodecContext** enc_contexts;
    struct SwsContext** sws_contexts;
    double* last_progress;
    int64_t total_duration;
    unsigned int nb_streams;
    int use_carriage_return;
} VideoCompressionContext;

/// @brief Compress video file main function
/// @param file Input video
/// @param output_file Compressed video
/// @return True if compression successful
bool compress_video(const char* file, const char* output_file);

/// @brief Releases all resources associated with a VideoCompressionContext
/// @param ctx Pointer to the VideoCompressionContext structure
void cleanup_video(VideoCompressionContext* ctx);

/// @brief Writes a packet to an output format context, rescaling timestamps and stream indices
/// @param out_ctx Pointer to the output AVFormatContext where the packet will be written
/// @param pkt Pointer to the AVPacket to be written (its timestamps and stream index will be updated)
/// @param in_stream Pointer to the input AVStream (used for timestamp rescaling)
/// @param out_stream Pointer to the output AVStream (used for timestamp rescaling)
/// @param stream_index Packet stream index
/// @return True if the packet was written successfully
bool write_packet(AVFormatContext* out_ctx, AVPacket* pkt, AVStream* in_stream, AVStream* out_stream, int stream_index);

/// @brief Processes a video frame using the specified compression context and input file.
/// @param ctx Pointer to the video compression context
/// @param filename Input file
/// @param stream_index Index of the video stream
/// @return True if the frame was processed successfully
bool process_video_frame(VideoCompressionContext* ctx, const char* filename, int stream_index);

#ifdef __cplusplus
}
#endif

#endif //VIDEO_COMPRESSOR