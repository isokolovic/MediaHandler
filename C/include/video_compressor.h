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

bool compress_video(const char* file, const char* output_file);
void cleanup_video(VideoCompressionContext* ctx);
bool write_packet(AVFormatContext* out_ctx, AVPacket* pkt, AVStream* in_stream, AVStream* out_stream, int stream_index);
bool process_video_frame(VideoCompressionContext* ctx, const char* filename, int stream_index);

#ifdef __cplusplus
}
#endif

#endif //VIDEO_COMPRESSOR