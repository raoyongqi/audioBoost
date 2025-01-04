#include "AudioBoost.h"


using namespace std;


static double calculate_max_dbfs(AVFrame* frame) {
    double max_amplitude = 0.0;
    int num_samples = frame->nb_samples;
    int num_channels = frame->ch_layout.nb_channels;

    // 遍历每个音频样本，计算最大绝对幅度
    for (int channel = 0; channel < num_channels; ++channel) {
        int16_t* data = (int16_t*)frame->data[channel];
        for (int i = 0; i < num_samples; ++i) {
            // Ensure that both values are of type double before comparing
            max_amplitude = std::max(max_amplitude, static_cast<double>(std::abs(data[i])));
        }
    }

    // 防止除以零的情况，确保 max_amplitude 不为零
    if (max_amplitude == 0.0) {
        return -INFINITY;  // or some other indication of silence
    }

    // 将最大幅度转换为 dBFS
    return 20.0 * log10(max_amplitude / 32767.0);
}


// 获取音频时长和通道信息
void get_audio_duration(const std::string& file_path) {
    AVFormatContext* format_ctx = nullptr;

    // 检查文件是否存在
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw runtime_error("文件不存在: " + file_path);
    }

    // 打开音频文件
    if (avformat_open_input(&format_ctx, file_path.c_str(), nullptr, nullptr) != 0) {
        throw runtime_error("无法打开输入文件: " + file_path);
    }

    // 获取流信息
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        throw runtime_error("无法获取流信息");
    }

    int audio_stream_index = -1;
    AVCodecParameters* codecpar = nullptr;
    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            codecpar = format_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (audio_stream_index == -1) {
        throw runtime_error("未找到音频流");
    }

    // 获取音频流的通道数、采样率和通道布局
    int sample_rate = codecpar->sample_rate;
    AVChannelLayout ch_layout = codecpar->ch_layout;  // AVChannelLayout

    // 获取通道数
    int num_channels = ch_layout.nb_channels;

    cout << "采样率: " << sample_rate << " Hz" << endl;
    cout << "通道数: " << num_channels << endl;

    // 输出通道布局
    char ch_layout_desc[128];
    if (av_channel_layout_describe(&ch_layout, ch_layout_desc, sizeof(ch_layout_desc)) >= 0) {
        cout << "通道布局: " << ch_layout_desc << endl;
    }
    else {
        cout << "通道布局: 无法描述" << endl;
    }

    // 输出文件时长
    double duration_seconds = static_cast<double>(format_ctx->duration) / AV_TIME_BASE;
    cout << "文件时长: " << duration_seconds << " 秒" << endl;

    // 关闭文件并释放资源
    avformat_close_input(&format_ctx);
}

// 放大音频文件的音量


void boost_audio(const string& input_file, const string& output_file, double target_dbfs = 0.0, int segment_duration_ms = 2 * 60 * 1000) {
    
    
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    const AVCodec* codec = nullptr;
    AVPacket packet;
    AVFrame* frame = nullptr;
    int audio_stream_index = -1;

    // 打开输入文件
    if (avformat_open_input(&format_ctx, input_file.c_str(), nullptr, nullptr) != 0) {
        throw runtime_error("无法打开输入文件: " + input_file);
    }

    // 获取流信息
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        throw runtime_error("无法获取流信息");
    }

    // 查找音频流
    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        throw runtime_error("未找到音频流");
    }

    // 获取音频解码器
    AVCodecParameters* codecpar = format_ctx->streams[audio_stream_index]->codecpar;
    codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        throw runtime_error("未找到音频解码器");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw runtime_error("无法分配解码上下文");
    }

    // 设置解码上下文
    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        throw runtime_error("无法从流中设置解码器参数");
    }

    // 打开解码器
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        throw runtime_error("无法打开解码器");
    }

    // 初始化音频帧
    frame = av_frame_alloc();
    if (!frame) {
        throw runtime_error("无法分配音频帧");
    }

    // 打开输出文件
    AVFormatContext* out_format_ctx = nullptr;
    const AVOutputFormat* out_format = nullptr;
    AVStream* out_stream = nullptr;

    avformat_alloc_output_context2(&out_format_ctx, nullptr, nullptr, output_file.c_str());
    if (!out_format_ctx) {
        throw runtime_error("无法分配输出格式上下文");
    }

    out_format = out_format_ctx->oformat;
    out_stream = avformat_new_stream(out_format_ctx, codec);
    if (!out_stream) {
        throw runtime_error("无法创建输出流");
    }

    // 设置输出流参数
    if (avcodec_parameters_copy(out_stream->codecpar, codecpar) < 0) {
        throw runtime_error("无法复制音频流参数到输出流");
    }

    // 打开输出文件
    if (avio_open(&out_format_ctx->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
        throw runtime_error("无法打开输出文件");
    }

    // 写文件头
    if (avformat_write_header(out_format_ctx, nullptr) < 0) {
        throw runtime_error("无法写入文件头");
    }

    // 创建编码器上下文
    const AVCodec* out_codec = avcodec_find_encoder(out_stream->codecpar->codec_id);
    if (!out_codec) {
        throw runtime_error("无法找到编码器");
    }

    AVCodecContext* out_codec_ctx = avcodec_alloc_context3(out_codec);
    if (!out_codec_ctx) {
        throw runtime_error("无法分配编码器上下文");
    }

    // 使用流的编码参数初始化编码器上下文
    if (avcodec_parameters_to_context(out_codec_ctx, out_stream->codecpar) < 0) {
        throw runtime_error("无法从流的参数初始化编码器上下文");
    }

    // 打开编码器
    if (avcodec_open2(out_codec_ctx, out_codec, nullptr) < 0) {
        throw runtime_error("无法打开编码器");
    }

    // 处理每个分段
    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == audio_stream_index) {
            int ret = avcodec_send_packet(codec_ctx, &packet);
            if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, sizeof(errbuf));
                fprintf(stderr, "Error sending packet to decoder: %s\n", errbuf);
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                else if (ret < 0) {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    fprintf(stderr, "Error receiving frame from decoder: %s\n", errbuf);
                    break;
                }

                // 检查帧数据中的无效值（NaN或Inf）
                bool contains_invalid_samples = false;
                for (int i = 0; i < frame->nb_samples; i++) {
                    for (int ch = 0; ch < codec_ctx->ch_layout.nb_channels; ch++) {
                        // 获取当前样本数据指针
                        float* sample_data = (float*)frame->extended_data[ch] + i;

                        // 检查是否存在 NaN 或 Inf
                        if (std::isnan(*sample_data) || std::isinf(*sample_data)) {
                            contains_invalid_samples = true;
                            break;
                        }
                    }
                    if (contains_invalid_samples) break;
                }

                if (contains_invalid_samples) {
                    fprintf(stderr, "Frame contains invalid samples (NaN or Inf)\n");
                    throw runtime_error("Frame contains invalid samples (NaN or Inf).");
                }

                // 处理音频帧数据（例如增益处理）
                // boost_frame(frame);
            }
        }
        av_packet_unref(&packet);
    }

    int64_t segment_start_time = 0;
    //while (av_read_frame(format_ctx, &packet) >= 0) {
    //    if (packet.stream_index == audio_stream_index) {
    //        int ret = avcodec_send_packet(codec_ctx, &packet);
    //        if (ret < 0) {
    //            throw runtime_error("无法发送包到解码器");
    //        }

    //        while (ret >= 0) {
    //            ret = avcodec_receive_frame(codec_ctx, frame);
    //            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    //                break;
    //            }
    //            if (ret < 0) {
    //                throw runtime_error("解码音频帧失败");
    //            }

    //            // 检查是否到了一个新分段的末尾
    //            if (segment_start_time + segment_duration_ms <= packet.pts) {
    //                // 计算当前音频段的最大 dBFS
    //                double max_dbfs = calculate_max_dbfs(frame);

    //                // 计算增益
    //                double gain_needed = target_dbfs - max_dbfs;

    //                // 如果增益大于0，应用增益
    //                if (gain_needed > 0) {
    //                    int num_samples = frame->nb_samples;
    //                    int num_channels = codecpar->ch_layout.nb_channels;
    //                    for (int i = 0; i < num_samples; ++i) {
    //                        for (int j = 0; j < num_channels; ++j) {
    //                            int16_t* sample = (int16_t*)frame->data[j] + i;
    //                            // 增益应用：防止溢出
    //                            *sample = static_cast<int16_t>(std::max(std::min(*sample * gain_needed, 32767.0), -32768.0));
    //                        }
    //                    }
    //                }

    //                // 将处理过的音频帧发送到编码器

    //                // 初始化 AVPacket
    //                AVPacket out_packet;
    //                av_init_packet(&out_packet);
    //                out_packet.data = nullptr;
    //                out_packet.size = 0;

    //                // 打印调试信息：检查帧和编码器上下文的参数
    //                printf("Sending frame to encoder...\n");
    //                printf("Frame info:\n");
    //                printf("  Number of samples: %d\n", frame->nb_samples);
    //                printf("  Format: %d\n", frame->format);  // 通常为 AV_SAMPLE_FMT_S16 等
    //                printf("  PTS: %" PRId64 "\n", frame->pts);
    //                printf("Encoder context info:\n");
    //                printf("  Sample rate: %d\n", out_codec_ctx->sample_rate);
    //                printf("  Time Base: %d/%d\n", out_codec_ctx->time_base.num, out_codec_ctx->time_base.den);
    //                printf("  Codec ID: %d\n", out_codec_ctx->codec_id);

    //                // 检查音频数据是否包含 NaN 或 Inf
    //                bool contains_invalid_samples = false;
    //                for (int i = 0; i < frame->nb_samples; i++) {
    //                    for (int ch = 0; ch < out_codec_ctx->ch_layout.nb_channels; ch++) {
    //                        // 获取当前样本的数据指针，假设数据是浮点类型
    //                        float* sample_data = (float*)frame->extended_data[ch] + i;

    //                        // 检查是否存在 NaN 或 Inf
    //                        if (std::isnan(*sample_data) || std::isinf(*sample_data)) {
    //                            contains_invalid_samples = true;
    //                            break;
    //                        }
    //                    }
    //                    if (contains_invalid_samples) break;
    //                }

    //                if (contains_invalid_samples) {
    //                    fprintf(stderr, "Frame contains invalid samples (NaN or Inf)\n");
    //                    throw runtime_error("Frame contains invalid samples (NaN or Inf).");
    //                }

    //                printf(" Checked: %d\n");


    //                // 发送帧到编码器
    //                ret = avcodec_send_frame(out_codec_ctx, frame);
    //                if (ret < 0) {
    //                    // 获取并打印详细的错误信息
    //                    char errbuf[AV_ERROR_MAX_STRING_SIZE];  // 用于存储错误信息
    //                    av_strerror(ret, errbuf, sizeof(errbuf));  // 将错误码转换为字符串
    //                    fprintf(stderr, "Error sending frame to encoder: %s\n", errbuf);

    //                    // 根据不同的错误类型抛出不同的异常或处理方式
    //                    if (ret == AVERROR(EINVAL)) {
    //                        throw runtime_error("Invalid argument passed to the encoder.");
    //                    }
    //                    else if (ret == AVERROR(ENOMEM)) {
    //                        throw runtime_error("Memory allocation failure while sending frame.");
    //                    }
    //                    else if (ret == AVERROR(EAGAIN)) {
    //                        throw runtime_error("The encoder is not ready to receive another frame.");
    //                    }
    //                    else {
    //                        throw runtime_error("Unable to send frame to the encoder.");
    //                    }
    //                }


    //                // 获取编码后的包
    //                ret = avcodec_receive_packet(out_codec_ctx, &out_packet);
    //                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    //                    break;
    //                }
    //                if (ret < 0) {
    //                    throw runtime_error("无法从编码器接收包");
    //                }

    //                // 写包到输出文件
    //                av_write_frame(out_format_ctx, &out_packet);
    //                av_packet_unref(&out_packet);

    //                // 更新分段起始时间
    //                segment_start_time = packet.pts;
    //            }
    //        }
    //    }
    //    av_packet_unref(&packet);
    //}

    // 写文件尾并清理资源
    av_write_trailer(out_format_ctx);
    avcodec_free_context(&out_codec_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    avformat_free_context(out_format_ctx);
    av_frame_free(&frame);
}




int main() {
    const string input_file = "C:/Users/r/Desktop/url/BV1mS4y1C7ta_508165150.m4a";
    const string output_file = "C:/Users/r/Desktop/BV15t4y117jL_boosted.m4a";

    try {
        cout << "正在处理文件: " << input_file << endl;
        boost_audio(input_file, output_file);  // 放大音量
        //cout << "音频文件已保存为: " << output_file << endl;
    }
    catch (const exception& e) {
        cerr << "发生错误: " << e.what() << endl;
    }

    return 0;
}

