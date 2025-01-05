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

void boost_audio(const string& input_file, const string& output_file, double target_dbfs = 0.0) {
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

    // 定义 20 分钟的时间段（1200 秒）
    const double segment_duration = 1200.0;  // 以秒为单位
    double segment_start_time = 0.0;  // 分段起始时间，初始化为 0
    bool processed_segment = false; // 用于标记是否处理过当前2分钟音频段

    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (packet.stream_index == audio_stream_index) {
            int ret = avcodec_send_packet(codec_ctx, &packet);

            if (ret < 0) {
                throw runtime_error("无法发送包到解码器");
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    throw runtime_error("解码音频帧失败");
                }

                // 使用 packet.pts 作为时间戳
                double current_time = packet.pts * av_q2d(format_ctx->streams[packet.stream_index]->time_base);

                // 如果当前时间超过了分段的结束时间（2分钟），则打印并重置起始时间
                if (current_time >= segment_start_time + segment_duration && !processed_segment) {
                    printf("已处理完2分钟音频，当前时间: %f\n", current_time);
                    processed_segment = true; // 标记当前段已处理
                    segment_start_time = current_time; // 重置时间段
                }

                // 应用增益或其他处理操作
                // （保留原有代码逻辑）

                // 编码并写入音频帧到输出文件
                AVPacket out_packet;
                av_init_packet(&out_packet);
                out_packet.data = nullptr;
                out_packet.size = 0;

                // 发送帧到编码器
                ret = avcodec_send_frame(out_codec_ctx, frame);
                if (ret < 0) {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    fprintf(stderr, "Error sending frame to encoder: %s\n", errbuf);
                    throw runtime_error("无法发送帧到编码器");
                }

                // 获取编码后的包
                ret = avcodec_receive_packet(out_codec_ctx, &out_packet);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    throw runtime_error("无法从编码器接收包");
                }

                // 写包到输出文件
                av_write_frame(out_format_ctx, &out_packet);
                av_packet_unref(&out_packet);
            }
        }
        av_packet_unref(&packet);

        // 如果已处理完2分钟，退出循环
        if (segment_start_time >= segment_duration) {
            break;
        }
    }

    // 写文件尾并清理资源
    av_write_trailer(out_format_ctx);

    // 释放资源
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
