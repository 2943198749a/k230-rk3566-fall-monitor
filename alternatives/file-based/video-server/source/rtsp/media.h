#ifndef _KD_MEDIA_H
#define _KD_MEDIA_H

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <list>
#include <mutex>
#include "osd.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}

enum class KdMediaVideoType {
  kVideoTypeH264,
  kVideoTypeH265,
  kVideoTypeMjpeg,
  kVideoTypeButt
};

typedef void (*OsdRegionCallback)(OsdRegion* regions, int region_count, void* user_data);

// 输入配置结构体类型：实例由主程序创建并传给视频服务。
struct KdMediaInputConfig {
  KdMediaVideoType video_type = KdMediaVideoType::kVideoTypeH264; // 变量：编码格式
  int venc_width = 1280; // 变量：视频宽度，启动参数会覆盖默认值
  int venc_height = 720; // 变量：视频高度
  int bitrate_kbps = 2000; // 变量：码率，单位 kbps
  std::string camera_device = "/dev/video1"; // 变量：V4L2 采集设备路径
  int enable_log = 0; // 变量：是否开启额外日志
  int osd_region = 0; // 变量：叠加区域数量，1080p 本例使用 4
  OsdRegionCallback osd_callback = nullptr; // 函数指针：更新检测框的位置与像素
  void* osd_user_data = nullptr; // 回调上下文指针
};

class IOnVEncData {
  public:
  virtual ~IOnVEncData() {}
  virtual void OnVEncData( unsigned char *data, size_t size, bool bKeyFrame, uint64_t timestamp) = 0;
};

class IOnFrameData {
  public:
  virtual ~IOnFrameData() {}
  virtual void OnFrameData(unsigned char *data, size_t size, bool bKeyFrame, uint64_t timestamp) = 0;
};

struct KdMediaFeatureConfig {
  IOnVEncData *on_venc_data = nullptr;
  IOnFrameData *on_frame_data = nullptr;
};

/**
 * @class KdMedia
 * @brief Class to manage media features and operations.
 */
class KdMedia {
  public:
  /**
   * @brief Configure media features.
   * @param input_config Configuration for media input.
   * @param feature_config Configuration for media features.
   * @return Status of the configuration operation.
   */
  int configure_media_features(const KdMediaInputConfig &input_config, const KdMediaFeatureConfig &feature_config);

  /**
   * @brief Enable media features.
   * @return Status of the enable operation.
   */
  int enable_media_features();

  /**
   * @brief Disable media features.
   * @return Status of the disable operation.
   */
  int disable_media_features();

  /**
   * @brief Destroy media features.
   * @return Status of the destroy operation.
   */
  int destroy_media_features();

  protected:
  static void *camera_capture_frame_thread(void *arg);
  static void *camera_venc_stream_thread(void *arg);
  int _init_encoder(AVCodecContext *&codec_ctx, AVFrame *&frame);
  int _init_camera(AVFormatContext *&fmt_ctx);
  int _init_osd();

  private:
  KdMediaInputConfig input_config_; // 结构体对象：保存输入参数
  KdMediaFeatureConfig feature_config_; // 结构体对象：保存输出回调

  AVFormatContext *fmt_ctx_ = nullptr; // 指针：FFmpeg 摄像头输入上下文
  AVCodecContext *codec_ctx_ = nullptr; // 指针：H.264 编码器上下文
  AVPacket *pkt_ = nullptr; // 指针：FFmpeg 数据包
  AVFrame *frame_ = nullptr; // 指针：输入编码器的图像帧

  pthread_t camera_venc_stream_tid_{0}; // Thread ID for encoding camera stream
  pthread_t camera_capture_frame_tid_{0}; // Thread ID for capturing frames
  bool stop_flag_{false}; // Flag to stop

  std::list<AVPacket*> packet_list_; // 容器对象：缓存待编码的采集帧
  std::mutex list_mutex_;

  bool camera_dmabuf_=false;
  OsdManager osd_manager_;
  OsdRegion regions_[NONAI2D_OSD_REGION_NUM];
};

#endif // _KD_MEDIA_H
