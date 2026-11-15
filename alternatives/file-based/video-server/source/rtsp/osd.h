#ifndef OSD_H
#define OSD_H

#include <string>
#include <cstdint>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/buffer.h>
#include <libavutil/rational.h>
#include <libavutil/frame.h>
}

#define NONAI2D_OSD_REGION_NUM 8

// 叠加区域结构体类型。一个对象描述画布中一块检测框图像。
typedef struct {
    int x; // 变量：区域左上角横坐标
    int y; // 变量：区域左上角纵坐标
    int width; // 变量：区域宽度，单位像素
    int height; // 变量：区域高度，单位像素
    const unsigned int *osd_image_data; // 指针变量：ARGB 图像缓冲区
    int osd_image_size; // 变量：图像数据字节数
    int enabled; // 变量：1 表示启用，0 表示关闭
} OsdRegion;

typedef struct {
    std::string device;
    int x[NONAI2D_OSD_REGION_NUM];
    int y[NONAI2D_OSD_REGION_NUM];
    int index[NONAI2D_OSD_REGION_NUM];
    int bg_alpha[NONAI2D_OSD_REGION_NUM];
    int osd_alpha[NONAI2D_OSD_REGION_NUM];
    int video_alpha[NONAI2D_OSD_REGION_NUM];
    int add_order[NONAI2D_OSD_REGION_NUM];
    uint32_t bg_color[NONAI2D_OSD_REGION_NUM];
    char *data[NONAI2D_OSD_REGION_NUM];
    int width[NONAI2D_OSD_REGION_NUM];
    int height[NONAI2D_OSD_REGION_NUM];
    int pixfmt[NONAI2D_OSD_REGION_NUM];
    int valid[NONAI2D_OSD_REGION_NUM];
} Nonai2dOsdConfig;

class OsdManager {
public:
    OsdManager();
    ~OsdManager();

    int Init(int width, int height, AVRational time_base, const char* in_mem_type, const char* out_mem_type);
    int Apply(AVFrame *frame, OsdRegion* regions, int region_count);
    void Deinit();
    bool IsReady() const;

private:
    AVFilterGraph *graph_;
    AVFilterContext *main_src_;
    AVFilterContext *osd_ctx_;
    AVFilterContext *sink_;
    int width_;
    int height_;
    AVRational time_base_;
    bool inited_;
    int frame_cnt_;
    Nonai2dOsdConfig osd_config_;
};

#endif // OSD_H