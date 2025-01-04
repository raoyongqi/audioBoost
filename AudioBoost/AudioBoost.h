// AudioBoost.h: 标准系统包含文件的包含文件
// 或项目特定的包含文件。

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <stdexcept>




extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

#pragma comment(lib, "C:\\vcpkg\\packages\\ffmpeg_x64-windows\\lib\\avformat.lib")
#pragma comment(lib, "C:\\vcpkg\\packages\\ffmpeg_x64-windows\\lib\\avcodec.lib")
#pragma comment(lib, "C:\\vcpkg\\packages\\ffmpeg_x64-windows\\lib\\avutil.lib")

// TODO: 在此处引用程序需要的其他标头。
