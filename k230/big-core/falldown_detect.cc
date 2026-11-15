/*
 * 交付版注释
 * 文件：k230/big-core/falldown_detect.cc
 * 职责：实现跌倒 YOLO 模型的预处理、推理调用、后处理和结果绘制。
 * 设计原因：模型相关尺寸、阈值和类别解释集中在一个类中，主循环只负责组织数据流。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.f
 */

#include "falldown_detect.h"
#include "utils.h"


// for image
falldownDetect::falldownDetect(const char *kmodel_file, float obj_thresh,float nms_thresh,  const int debug_mode): obj_thresh_(obj_thresh),nms_thresh_(nms_thresh), AIBase(kmodel_file,"falldownDetect", debug_mode)
{

    model_name_ = "falldownDetect";
    
    ai2d_out_tensor_ = get_input_tensor(0);
}   

// for video
falldownDetect::falldownDetect(const char *kmodel_file, float obj_thresh,float nms_thresh, FrameCHWSize isp_shape, uintptr_t vaddr, uintptr_t paddr, const int debug_mode): obj_thresh_(obj_thresh),nms_thresh_(nms_thresh), AIBase(kmodel_file,"falldownDetect", debug_mode)
{
    model_name_ = "falldownDetect";
    
    vaddr_ = vaddr;

    isp_shape_ = isp_shape;
    dims_t in_shape{1, isp_shape_.channel, isp_shape_.height, isp_shape_.width};
    int isp_size = isp_shape_.channel * isp_shape_.height * isp_shape_.width;
    #if 0
    ai2d_in_tensor_ = host_runtime_tensor::create(typecode_t::dt_uint8, in_shape, { (gsl::byte *)vaddr, isp_size },
        true, hrt::pool_shared).expect("cannot create input tensor");
    #else
    ai2d_in_tensor_ = hrt::create(typecode_t::dt_uint8, in_shape, hrt::pool_shared).expect("create ai2d input tensor failed");
    #endif

    // ai2d_out_tensor
    ai2d_out_tensor_ = get_input_tensor(0);
    // fixed padding resize param
    Utils::padding_resize(isp_shape_, {input_shapes_[0][3], input_shapes_[0][2]}, ai2d_builder_, ai2d_in_tensor_, ai2d_out_tensor_, cv::Scalar(114, 114, 114));
}

falldownDetect::~falldownDetect()
{

}

// ai2d for image
void falldownDetect::pre_process(cv::Mat ori_img)
{
    ScopedTiming st(model_name_ + " pre_process image", debug_mode_);
    // OpenCV 图片默认是 HWC 排列，而此 KModel 的输入为 CHW。先重排，再用
    // 114 灰色 letterbox 保持宽高比，避免直接拉伸改变人体姿态特征。
    std::vector<uint8_t> chw_vec;
    Utils::hwc_to_chw(ori_img, chw_vec);
    Utils::padding_resize({ori_img.channels(), ori_img.rows, ori_img.cols}, chw_vec, {input_shapes_[0][3], input_shapes_[0][2]}, ai2d_out_tensor_, cv::Scalar(114, 114, 114));
}

// ai2d for video
void falldownDetect::pre_process()
{
    ScopedTiming st(model_name_ + " pre_process video", debug_mode_);
    #if 0
    ai2d_builder_->invoke().expect("error occurred in ai2d running");
    #else
    // 摄像头路径的输入地址来自 VICAP/VB 映射。这里复制到 nncase 输入张量后
    // sync_write_back，确保 NPU 看到 CPU 刚写入的数据，而不是旧缓存内容。
    size_t isp_size = isp_shape_.channel * isp_shape_.height * isp_shape_.width;
    auto buf = ai2d_in_tensor_.impl()->to_host().unwrap()->buffer().as_host().unwrap().map(map_access_::map_write).unwrap().buffer();
    memcpy(reinterpret_cast<char *>(buf.data()), (void *)vaddr_, isp_size);
    hrt::sync(ai2d_in_tensor_, sync_op_t::sync_write_back, true).expect("sync write_back failed");
    ai2d_builder_->invoke(ai2d_in_tensor_,ai2d_out_tensor_).expect("error occurred in ai2d running");
    // run ai2d
    #endif
    
}

void falldownDetect::inference()
{
    this->run();
    this->get_output();
}

void falldownDetect::post_process(FrameSize frame_size,std::vector<BoxInfo> &result)
{
    // 三个输出分别对应不同尺度的特征层。解码后统一做 NMS，再把网络输入坐标
    // 映射回传入的 frame_size；调用者拿到的是可直接绘制和统计的最终框。
    int net_len = input_shapes_[0][2];
    // first output
    {

        float *output_0 = p_outputs_[0];

        int first_len = net_len / 8;
        int first_size = first_len * first_len;
       
        auto boxes0 = Utils::decode_infer(output_0, net_len, 8, classes_num_, frame_size, anchors_0_, obj_thresh_);
        result.insert(result.begin(), boxes0.begin(), boxes0.end());
    }

    // second output
    {

        float *output_1 = p_outputs_[1];

        int second_len = net_len / 16;
        int second_size = second_len * second_len;

        auto boxes1 = Utils::decode_infer(output_1, net_len, 16, classes_num_, frame_size, anchors_1_, obj_thresh_);
        result.insert(result.begin(), boxes1.begin(), boxes1.end());
    }
    
    // third output
    {
        float *output_2 = p_outputs_[2];

        int third_len = net_len / 32;
        int third_size = third_len * third_len;

        auto boxes2 = Utils::decode_infer(output_2, net_len, 32, classes_num_, frame_size, anchors_2_, obj_thresh_);
        result.insert(result.begin(), boxes2.begin(), boxes2.end());
    }

    Utils::nms(result, nms_thresh_);
}

