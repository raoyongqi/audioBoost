#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <limits>
#include <cstdint>
#include "Read.h"

void adjust_audio_gain(AVFrame* frame, double gain_factor) {
    int num_samples = frame->nb_samples;
    int num_channels = frame->ch_layout.nb_channels;

    // 检查 frame->data 是否有效
    if (frame->data == nullptr) {
        std::cerr << "Frame data is null!" << std::endl;
        return;
    }

    // 遍历每个通道进行增益调整
    for (int channel = 0; channel < num_channels; ++channel) {
        // 获取当前通道的数据指针
        int16_t* data = reinterpret_cast<int16_t*>(frame->data[channel]);
        for (int i = 0; i < num_samples; ++i) {
            int32_t adjusted_sample = static_cast<int32_t>(data[i]) * gain_factor;

            // 对调整后的样本进行范围检查
            if (adjusted_sample > INT16_MAX) {
                data[i] = INT16_MAX;
            }
            else if (adjusted_sample < INT16_MIN) {
                data[i] = INT16_MIN;
            }
            else {
                data[i] = static_cast<int16_t>(adjusted_sample);
            }
        }
    }
}


int main() {
    const std::string input_file = "C:/Users/r/Desktop/url/BV1mS4y1C7ta_508165150.m4a";
    const std::string output_folder = "C:/Users/r/Desktop/adjusted_audio";

    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    int audio_stream_index = -1;

    if (!frame || !packet) {
        std::cerr << "Failed to allocate AVFrame or AVPacket." << std::endl;
        return -1;
    }

    avformat_network_init();

    if (avformat_open_input(&format_ctx, input_file.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file." << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        return -1;
    }

    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        std::cerr << "Audio stream not found." << std::endl;
        return -1;
    }

    const AVCodec* codec = avcodec_find_decoder(format_ctx->streams[audio_stream_index]->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Codec not found." << std::endl;
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[audio_stream_index]->codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters." << std::endl;
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Could not open codec." << std::endl;
        return -1;
    }

    double gain_factor = 1.5;

    std::filesystem::create_directory(output_folder);
    const std::string output_file = output_folder + "/adjusted_audio.m4a";

    AVFormatContext* output_format_ctx = nullptr;
    if (avformat_alloc_output_context2(&output_format_ctx, nullptr, "mp4", output_file.c_str()) < 0) {
        std::cerr << "Could not create output format context." << std::endl;
        return -1;
    }

    AVStream* out_stream = avformat_new_stream(output_format_ctx, nullptr);
    if (!out_stream) {
        std::cerr << "Failed to create output stream." << std::endl;
        return -1;
    }

    const AVCodec* output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!output_codec) {
        std::cerr << "Output codec not found." << std::endl;
        return -1;
    }

    AVCodecContext* output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx) {
        std::cerr << "Failed to allocate output codec context." << std::endl;
        return -1;
    }

    AVCodecParameters* input_codecpar = format_ctx->streams[audio_stream_index]->codecpar;
    output_codec_ctx->ch_layout = input_codecpar->ch_layout;
    output_codec_ctx->sample_rate = input_codecpar->sample_rate;
    output_codec_ctx->sample_fmt = output_codec->sample_fmts[0];
    output_codec_ctx->bit_rate = 64000;

    if (avcodec_open2(output_codec_ctx, output_codec, nullptr) < 0) {
        std::cerr << "Could not open output codec." << std::endl;
        return -1;
    }

    if (avcodec_parameters_from_context(out_stream->codecpar, output_codec_ctx) < 0) {
        std::cerr << "Failed to copy codec parameters to output stream." << std::endl;
        return -1;
    }

    if (!(output_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_format_ctx->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file." << std::endl;
            return -1;
        }
    }

    if (avformat_write_header(output_format_ctx, nullptr) < 0) {
        std::cerr << "Error writing header to output file." << std::endl;
        return -1;
    }
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == audio_stream_index) {
            // 发送包进行解码
            int ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                std::cerr << "Error sending packet for decoding: "
                   << std::endl;
                av_packet_unref(packet);
                break; // 如果发生错误，则跳出整个外部 while 循环
            }

            // 接收解码后的帧
            while ((ret = avcodec_receive_frame(codec_ctx, frame)) >= 0) {
                adjust_audio_gain(frame, gain_factor);

                // 发送帧进行编码
                ret = avcodec_send_frame(output_codec_ctx, frame);
                if (ret < 0) {
                    std::cerr << "Error sending frame for encoding: "
                        << std::endl;
                    av_frame_unref(frame);
                    av_packet_unref(packet);
                    break; // 跳出当前的内层 while 循环
                }

                // 接收编码后的数据包并写入文件
                while ((ret = avcodec_receive_frame(codec_ctx, frame)) >= 0) {
                    adjust_audio_gain(frame, gain_factor);

                    // 发送帧进行编码
                    ret = avcodec_send_frame(output_codec_ctx, frame);
                    if (ret < 0) {
                        std::cerr << "ret: " << ret << std::endl;
                        av_frame_unref(frame);
                        av_packet_unref(packet);
                        break; // 跳出当前的内层 while 循环
                    }

                    // 这里可以继续你的帧编码和写入操作
                }


                if (ret < 0) { // 如果出现错误，退出内层的 while 循环



                    std::cerr << "Error receiving packet for encoding: "  << std::endl;
                    av_frame_unref(frame);
                    break;
                }
            }

            if (ret < 0) { // 如果解码或编码的过程出现错误，跳出外层 while 循环

                std::cerr << "ret: " << ret << std::endl;

                std::cerr << "Error receiving frame for decoding: " << std::endl;
                break;
            }

            av_frame_unref(frame); // 清理帧
        }

        av_packet_unref(packet); // 清理数据包
    }




    av_write_trailer(output_format_ctx);

    if (!(output_format_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_format_ctx->pb);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avcodec_free_context(&output_codec_ctx);
    avformat_close_input(&format_ctx);
    avformat_free_context(output_format_ctx);

    return 0;
}
