#ifndef _CAMERA_RTSP_DEMO_H
#define _CAMERA_RTSP_DEMO_H
#include <atomic>
#include <vector>
#include <string>
#include "rtsp_server.h"
#include "media.h"

// 总控制对象：把 RTSP 服务、摄像头采集和编码回调连接起来。
class MyCameraRtspDemo:public IOnVEncData {
  public:
    MyCameraRtspDemo();
    int Init(const KdMediaInputConfig &config,const std::string &stream_url = "test", int port = 8554);
    int DeInit();
    int Start();
    int Stop();

    static void OnUpdateOsdRegions(OsdRegion* regions, int region_count, void* user_data);

  protected:
    virtual void OnVEncData(unsigned char *data, size_t size, bool bKeyFrame, uint64_t timestamp) override;

  private:
    KdRtspServer rtsp_server_; // 对象：监听 RTSP 连接并发送编码后的视频
    std::string stream_url_; // 变量：流名称，本项目为 yolo
    std::atomic<bool> started_{false}; // 变量：服务是否已启动，跨线程读取
    KdMedia     media_; // 对象：负责摄像头采集、OSD 和硬件编码
    KdMediaFeatureConfig feature_config_; // 配置对象：保存编码完成后的回调
    KdMediaInputConfig input_config_; // 配置对象：宽、高、码率和设备名
    char        sps_pps_[1024] = {0};
    size_t      sps_pps_size_ = 0;
};



#endif // _CAMERA_RTSP_DEMO_H