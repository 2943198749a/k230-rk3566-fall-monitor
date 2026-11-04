/*
 * 交付版注释
 * 文件：k230/little-core/rtsp-server/src/main.cpp
 * 职责：K230 Linux 小核 RTSP 服务入口：读取视频 DataFIFO 并交给 KdRtspServer。
 * 设计原因：大核负责硬件编码，小核负责网络会话，符合 K230 双系统职责划分。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#include <iostream>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "k_datafifo.h"
#include "rtsp_server.h"
#include "media.h"
#include "fifo_read_dispatch.h"

// datafifo
#define READER_INDEX    0
std::atomic<bool> send_stop(false);
static k_s32 g_s32Index = 0;
static const k_s32 BLOCK_LEN = 1024000;
static k_datafifo_handle hDataFifo[2] = {(k_datafifo_handle)K_DATAFIFO_INVALID_HANDLE, (k_datafifo_handle)K_DATAFIFO_INVALID_HANDLE};

using namespace std::chrono_literals;

static void release(void* pStream)
{
    printf("release %p\n", pStream);
}

int datafifo_init(k_u64 reader_phyAddr)
{
    k_s32 s32Ret = K_SUCCESS;
    k_datafifo_params_s params_reader = {10, BLOCK_LEN, K_TRUE, DATAFIFO_READER};

    s32Ret = kd_datafifo_open_by_addr(&hDataFifo[READER_INDEX], &params_reader, reader_phyAddr);
    if (K_SUCCESS != s32Ret)
    {
        printf("open datafifo error:%x\n", s32Ret);
        return -1;
    }

    printf("datafifo_init finish\n");

    return 0;
}

void datafifo_deinit()
{
    k_s32 s32Ret = K_SUCCESS;
    if (K_SUCCESS != s32Ret)
    {
        printf("write error:%x\n", s32Ret);
    }

    kd_datafifo_close(hDataFifo[READER_INDEX]);
    printf("datafifo_deinit finish\n");
}

static void Usage() {
    std::cout << "Usage: ./rtspServer [-p phyAddr] [-t <codec_type>]" << std::endl;
    std::cout << "-p: phyAddr" << std::endl;
    std::cout << "-t: the video encoder type: h264/h265, default h265" << std::endl;
    exit(-1);
}

int parse_config(int argc, char *argv[], KdMediaInputConfig &config) {
    int result;
    opterr = 0;
    while ((result = getopt(argc, argv, "H:t:p:")) != -1) {
        switch(result) {
        case 'H' : {
            Usage(); break;
        }
        case 't': {
            std::string s = optarg;
            if (s == "h264") config.video_type = KdMediaVideoType::kVideoTypeH264;
            else if (s == "h265") config.video_type = KdMediaVideoType::kVideoTypeH265;
            else Usage();
            config.video_valid = true;
            break;
        }
        case 'p': {
            break;
        }
        default: Usage(); break;
        }
    }
    if (config.video_valid) {
        // validate the parameters... TODO
        std::cout << "Validate the input config, not implemented yet, TODO." << std::endl;
    }
    return 0;
}

class MyRtspServer : public IOnBackChannel, public IOnAEncData, public IOnVEncData {
  public:
    MyRtspServer() {}

    // IOnBackChannel
    virtual void OnBackChannelData(std::string &session_name, const uint8_t *data, size_t size, uint64_t timestamp) override {
        if (started_) {
            //  TODO， need to queue data to handle jitter and control accumulation
            //   gather data to get complete frame(40ms).
            if (backchannel_data_size == 0) {
                timestamp_backchanel = timestamp;
            }
            for (size_t i = 0; i < size ;i++) {
                g711_buffer_backchannel[backchannel_data_size++] = data[i];
                if (backchannel_data_size == 320) {
                    media_.SendData(g711_buffer_backchannel, backchannel_data_size, timestamp_backchanel);
                    backchannel_data_size = 0;
                    timestamp_backchanel = timestamp;
                }
            }
        }
    }

    // IOnAEncData
    virtual void OnAEncData(k_u32 chn_id, k_audio_stream* stream_data) override {
        if (started_) {
            rtsp_server_.SendAudioData(stream_url_, (const uint8_t*)stream_data->stream, stream_data->len, stream_data->time_stamp);
        }
    }

    // IOnVEncData
    virtual void OnVEncData(k_u32 chn_id, void *data, size_t size, uint64_t timestamp) override {
        if (started_) {
            // data 指向 DataFIFO 共享槽位，调用返回后读线程才会 READ_DONE。
            // timestamp 沿用大核 VENC 的 PTS，避免自行用系统时间造成播放抖动。
            rtsp_server_.SendVideoData(stream_url_, (const uint8_t*)data, size, timestamp);
        }
    }

    int Init(const KdMediaInputConfig &config, const std::string &stream_url = "BackChannelTest", int port = 8554) {
        if (rtsp_server_.Init(port, this) < 0) {
            return -1;
        }
        // enable audio-track and backchannel-track
        SessionAttr session_attr;
        session_attr.with_audio = false;
        session_attr.with_audio_backchannel = false;
        session_attr.with_video = config.video_valid;
        if (config.video_valid) {
            if (config.video_type == KdMediaVideoType::kVideoTypeH264) session_attr.video_type = VideoType::kVideoTypeH264;
            else if (config.video_type == KdMediaVideoType::kVideoTypeH265) session_attr.video_type = VideoType::kVideoTypeH265;
            else {
                std::cout << "video codec type not supported yet" << std::endl;
                return -1;
            }
        }
        if (rtsp_server_.CreateSession(stream_url, session_attr) < 0)  return -1;
        stream_url_ = stream_url;

        return 0;
    }
    int DeInit() {
        Stop();
        rtsp_server_.DeInit();
        return 0;
    }

    int Start() {
        if(started_) return 0;
        rtsp_server_.Start();
        started_ = true;
        return 0;
    }
    int Stop() {
        if (!started_) return 0;
        rtsp_server_.Stop();
        started_ = false;
        return 0;
    }

  private:
    KdRtspServer rtsp_server_;
    KdMedia media_;
    std::string stream_url_;
    std::atomic<bool> started_{false};
    uint8_t g711_buffer_backchannel[320];
    size_t backchannel_data_size = 0;
    uint64_t timestamp_backchanel;
};



void* read_send(void* arg)
{
    MyRtspServer *server = (MyRtspServer *)arg;
    k_u32 readLen = 0;
    k_char* pBuf;
    k_s32 s32Ret = K_SUCCESS;

    while (!send_stop)
    {
        readLen = 0;
        s32Ret = kd_datafifo_cmd(hDataFifo[READER_INDEX], DATAFIFO_CMD_GET_AVAIL_READ_LEN, &readLen);
        if (K_SUCCESS != s32Ret)
        {
            printf("get available read len error:%x\n", s32Ret);
            break;
        }

        if (readLen > 0)
        {
            s32Ret = kd_datafifo_read(hDataFifo[READER_INDEX], (void**)&pBuf);
            if (K_SUCCESS != s32Ret)
            {
                printf("read error:%x\n", s32Ret);
                break;
            }
            dispatch_fifo_payload(
                pBuf,
                [server](k_char *packet) {
                    // 大小核必须遵守相同二进制布局：8 字节 PTS、4 字节长度，
                    // 随后是 Annex-B H.264/H.265 数据。改变布局要同时升级两端。
                    const unsigned long pts = ((unsigned long *)packet)[0];
                    const unsigned int len = ((unsigned int *)packet)[2];
                    k_char *data = packet + sizeof(unsigned long) + sizeof(unsigned int);
                    server->OnVEncData(0, (void *)data, (size_t)len, pts);
                },
                [&](k_char *packet) {
                    // 必须在 SendVideoData 使用完共享内存后确认。若先 READ_DONE，
                    // 大核可能立即复用槽位，导致 RTSP 读取到被覆盖的半帧。
                    s32Ret = kd_datafifo_cmd(hDataFifo[READER_INDEX],
                                             DATAFIFO_CMD_READ_DONE, packet);
                });
            if (K_SUCCESS != s32Ret)
            {
                printf("read done error:%x\n", s32Ret);
                break;
            }
        }
    }

    // std::thread 会忽略这个返回值，但函数签名来自原 pthread 风格接口。
    // 显式返回可避免未定义返回值，并让 -Werror=return-type 构建保持干净。
    return nullptr;
}

int main(int argc, char *argv[]) {
    std::cout << "./rtspServer -H to show usage" << std::endl;
    std::cout << "./rtspServer -p 1628c000 -t h265" << std::endl;

    KdMediaInputConfig config;
    int ret = parse_config(argc, argv, config);

    // 初始化 datafifo
    k_s32 s32Ret = K_SUCCESS;
    k_u64 phyAddr[2];
    sscanf(argv[2], "%lx", &phyAddr[READER_INDEX]);
    s32Ret = datafifo_init(phyAddr[READER_INDEX]);   

    // 创建 rtsp 服务
    MyRtspServer *server = new MyRtspServer();
    if (!server || server->Init(config) < 0) {
        std::cout << "KdRtspServer Init failed." << std::endl;
        return -1;
    }
    server->Start();

    // 启动 数据发送 线程
    std::thread readThread(read_send, server);

    printf("Input q to exit: \n");
    while (getchar() != 'q')
    {
        usleep(10000);
    }

    send_stop = true;
    readThread.join();

    // 关闭rtsp服务
    server->Stop();
    server->DeInit();
    delete server;
    // datafifo反初始化
    datafifo_deinit();
    return 0;
}
