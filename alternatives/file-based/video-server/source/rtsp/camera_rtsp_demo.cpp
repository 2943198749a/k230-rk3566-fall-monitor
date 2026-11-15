#include <opencv2/imgproc.hpp>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>
#include "camera_rtsp_demo.h"
#include <iostream>
#include <unistd.h>
#include <thread>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "osd.h"
#include "OSD1_40x40_argb.c"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}

static unsigned int g_fullscreen_osd_data[1280 * 720] __attribute__((aligned(0x1000)));
static unsigned int g_small_osd_data[40 * 40] __attribute__((aligned(0x1000)));
static int g_frame_count = 0;

static void GenerateRainbowPixel(unsigned int *data, int x, int y, int time_offset, int width) {
    int hue = (time_offset + x * 10 + y * 10) % 360;
    int red, green, blue;
    if (hue < 60) {
        red = 255; green = hue * 255 / 60; blue = 0;
    } else if (hue < 120) {
        red = 255 - (hue - 60) * 255 / 60; green = 255; blue = 0;
    } else if (hue < 180) {
        red = 0; green = 255; blue = (hue - 120) * 255 / 60;
    } else if (hue < 240) {
        red = 0; green = 255 - (hue - 180) * 255 / 60; blue = 255;
    } else if (hue < 300) {
        red = (hue - 240) * 255 / 60; green = 0; blue = 255;
    } else {
        red = 255; green = 0; blue = 255 - (hue - 300) * 255 / 60;
    }
    int alpha = 200;
    data[y * width + x] = (alpha << 24) | (red << 16) | (green << 8) | blue;
}

static void GenerateFullscreenRainbow() {
    if (g_frame_count > 0)
    {
        return;
    }

    g_frame_count++;
    int time_offset = (g_frame_count / 2) % 360;
    for (int y = 0; y < 720; y++) {
        for (int x = 0; x < 1280; x++) {
            GenerateRainbowPixel(g_fullscreen_osd_data, x, y, time_offset, 1280);
        }
    }
}

static void GenerateSmallRainbow() {
    g_frame_count++;
    int time_offset = (g_frame_count / 2) % 360;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            GenerateRainbowPixel(g_small_osd_data, x, y, time_offset, 40);
        }
    }
}

MyCameraRtspDemo::MyCameraRtspDemo() {
}

int MyCameraRtspDemo::Init(const KdMediaInputConfig &config,const std::string &stream_url, int port) {
    //init rtsp server
    input_config_ = config;

    if (rtsp_server_.Init(port, nullptr) < 0) {
        return -1;
    }
    // enable audio-track
    SessionAttr session_attr;
    session_attr.with_audio = true;
    session_attr.with_audio_backchannel = false;
    session_attr.with_video = true;

    if (config.video_type == KdMediaVideoType::kVideoTypeH264) {
        session_attr.video_type = VideoType::kVideoTypeH264;
    } else if (config.video_type == KdMediaVideoType::kVideoTypeH265) {
        session_attr.video_type = VideoType::kVideoTypeH265;
    } else {
        printf("video codec type not supported yet\n");
        return -1;
    }

    if (rtsp_server_.CreateSession(stream_url, session_attr) < 0) {
        return -1;
    }

    stream_url_ = stream_url;

    feature_config_.on_venc_data = this;

    if (input_config_.osd_region > 0) {
        input_config_.osd_callback = OnUpdateOsdRegions;
        input_config_.osd_user_data = this;
    }

    media_.configure_media_features(input_config_, feature_config_);
    return 0;
}

int MyCameraRtspDemo::DeInit() {
    Stop();
    media_.destroy_media_features();

    rtsp_server_.DeInit();
    
    return 0;
}

int MyCameraRtspDemo::Start() {
    if(started_) return 0;
    media_.enable_media_features();

    rtsp_server_.Start();

    started_ = true;

    printf("Play this stream using the URL:%s\n",rtsp_server_.GetRtspUrl(stream_url_));

    return 0;
}

int MyCameraRtspDemo::Stop() {
    if (!started_) return 0;

    rtsp_server_.Stop();

    started_ = false;
    media_.disable_media_features();
    return 0;
}

#include <time.h>
static uint64_t get_precise_timestamp_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;  // 纳秒转微秒
}

// 中文复刻注释：读取推理结果，换算坐标并绘制透明 ARGB 叠加层；超过 3 秒的结果不显示。
void MyCameraRtspDemo::OnUpdateOsdRegions(OsdRegion* regions, int region_count, void*) {
    if (!regions || region_count < 4) return;
    // 硬件 OSD 要求 4096 字节对齐；四块小区域共同覆盖 1080p 画面。
    alignas(4096) static unsigned int overlay_pixels[4][519168];
    static unsigned int canvas_pixels[1920 * 1080];
    static cv::Mat canvas(1080, 1920, CV_8UC4, canvas_pixels);
    canvas.setTo(cv::Scalar(0,0,0,0));
    struct stat st;
    if (stat("/tmp/yolo_fall_boxes", &st) == 0 && time(nullptr) - st.st_mtime < 3) {
        std::ifstream input("/tmp/yolo_fall_boxes");
        int label; float score,x1,y1,x2,y2;
        while (input >> label >> score >> x1 >> y1 >> x2 >> y2) {
            int left=std::clamp(int(x1),0,1919), top=std::clamp(int(y1),0,1079);
            int right=std::clamp(int(x2),0,1919), bottom=std::clamp(int(y2),0,1079);
            if (right <= left || bottom <= top || label < 0 || label > 1) continue;
            cv::Scalar color = label == 0 ? cv::Scalar(255,255,0,0) : cv::Scalar(255,0,255,0);
            cv::rectangle(canvas, cv::Point(left,top), cv::Point(right,bottom), color, 3);
            char text[48]; snprintf(text,sizeof(text),"%s %.2f",label==0 ? "Fall" : "NoFall",score);
            cv::putText(canvas,text,cv::Point(left,std::max(20,top-6)),cv::FONT_HERSHEY_SIMPLEX,0.7,color,2);
        }
    }
    for (int i=0; i<4; ++i) {
        int x=(i%2)*960, y=(i/2)*540;
        cv::Mat tile(540,960,CV_8UC4,overlay_pixels[i]);
        canvas(cv::Rect(x,y,960,540)).copyTo(tile);
        regions[i].x=x; regions[i].y=y; regions[i].width=960; regions[i].height=540;
        regions[i].osd_image_data=overlay_pixels[i];
        regions[i].osd_image_size=960*540*4; regions[i].enabled=1;
    }
}

void MyCameraRtspDemo::OnVEncData(unsigned char *data, size_t size, bool bKeyFrame, uint64_t timestamp)
{
    if (!started_) return ;

    // Process encoded packet here
    rtsp_server_.SendVideoData(stream_url_, data, size, timestamp);
    
}
