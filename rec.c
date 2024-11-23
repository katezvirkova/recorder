#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>

void record_video(int duration, const char *output_file, const char *video_device) {
    avdevice_register_all();

    AVFormatContext *input_ctx = NULL;
    const AVInputFormat *input_format = av_find_input_format("avfoundation");
    AVDictionary *options = NULL;
    av_dict_set(&options, "framerate", "30", 0);  // Підтримувана частота кадрів
    av_dict_set(&options, "video_size", "640x480", 0);  // Підтримуваний розмір
    av_dict_set(&options, "pixel_format", "yuyv422", 0); // Підтримуваний формат

    if (avformat_open_input(&input_ctx, video_device, input_format, &options) < 0) {
        fprintf(stderr, "Cannot open input device: %s\n", video_device);
        return;
    }

    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Cannot find stream info for input device.\n");
        avformat_close_input(&input_ctx);
        return;
    }

    AVFormatContext *output_ctx = NULL;
    avformat_alloc_output_context2(&output_ctx, NULL, "mov", output_file);
    if (!output_ctx) {
        fprintf(stderr, "Could not create output format context.\n");
        avformat_close_input(&input_ctx);
        return;
    }

    AVStream *output_stream = avformat_new_stream(output_ctx, NULL);
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Cannot find H.264 codec.\n");
        return;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->width = 640;
    codec_ctx->height = 480;
    codec_ctx->time_base = (AVRational){1, 30};  // Відповідно до fps
    codec_ctx->framerate = (AVRational){30, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 800000;

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Cannot open codec.\n");
        return;
    }
    avcodec_parameters_from_context(output_stream->codecpar, codec_ctx);

    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Cannot open output file.\n");
            return;
        }
    }

    avformat_write_header(output_ctx, NULL);

    struct SwsContext *sws_ctx = sws_getContext(
        640, 480, AV_PIX_FMT_YUYV422,
        640, 480, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, NULL, NULL, NULL);

    AVFrame *frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    av_frame_get_buffer(frame, 32);

    int64_t start_time = av_gettime_relative();
    int64_t duration_us = duration * 1000000;
    int64_t pts = 0;

    while (av_gettime_relative() - start_time < duration_us) {
        AVPacket packet = {0};
        if (av_read_frame(input_ctx, &packet) >= 0) {
            AVFrame *input_frame = av_frame_alloc();
            input_frame->format = AV_PIX_FMT_YUYV422;
            input_frame->width = 640;
            input_frame->height = 480;
            av_frame_get_buffer(input_frame, 32);

            sws_scale(sws_ctx,
                      (const uint8_t *const *)input_frame->data, input_frame->linesize,
                      0, input_frame->height,
                      frame->data, frame->linesize);

            frame->pts = pts++;
            avcodec_send_frame(codec_ctx, frame);
            avcodec_receive_packet(codec_ctx, &packet);

            packet.stream_index = output_stream->index;
            packet.pts = av_rescale_q(frame->pts, codec_ctx->time_base, output_stream->time_base);
            packet.dts = packet.pts;
            packet.duration = av_rescale_q(1, codec_ctx->time_base, output_stream->time_base);

            av_interleaved_write_frame(output_ctx, &packet);
            av_packet_unref(&packet);
            av_frame_free(&input_frame);
        }
    }

    av_write_trailer(output_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&input_ctx);
    avio_closep(&output_ctx->pb);
    avformat_free_context(output_ctx);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <duration> <output_file> <device>\n", argv[0]);
        return -1;
    }

    int duration = atoi(argv[1]);
    const char *output_file = argv[2];
    const char *video_device = argv[3];

    record_video(duration, output_file, video_device);
    return 0;
}
