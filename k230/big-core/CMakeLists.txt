# 交付版注释
# 文件：k230/big-core/CMakeLists.txt
# 职责：定义大核 ELF 的源码、K230 MPP/nncase/OpenCV 头文件和静态库。
# 设计原因：用静态链接生成可直接在 RT-Smart 大核运行的单一 ELF。
# 修改原则：构建参数必须与目标板 SDK/工具链匹配。

set(src main.cc utils.cc ai_base.cc scoped_timing.hpp falldown_detect.cc)
set(bin falldown_rtsp_big.elf)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/common)

include_directories(${k230_sdk}/src/common/cdk/user/component/datafifo/include)
include_directories(${PROJECT_SOURCE_DIR}/rtsp_enc)
include_directories(${nncase_sdk_root}/riscv64/rvvlib/include)
include_directories(${k230_sdk}/src/big/mpp/userapps/api/)
include_directories(${k230_sdk}/src/big/mpp/include)
include_directories(${k230_sdk}/src/big/mpp/include/comm)
include_directories(${k230_sdk}/src/big/mpp/userapps/sample/sample_vo)

link_directories(${k230_sdk}/src/common/cdk/user/component/datafifo/host/lib)
link_directories(${nncase_sdk_root}/riscv64/rvvlib/)

add_executable(${bin} ${src})
target_link_libraries(${bin} -Wl,--start-group rvv Nncase.Runtime.Native nncase.rt_modules.k230 functional_k230 sys vicap vb cam_device cam_engine
 hal oslayer ebase fpga isp_drv binder auto_ctrol common cam_caldb isi 3a buffer_management cameric_drv video_in virtual_hal start_engine cmd_buffer
 switch cameric_reg_drv t_database_c t_mxml_c t_json_c t_common_c vo connector sensor atomic dma vdec vicap venc datafifo -Wl,--end-group)

target_link_libraries(${bin} opencv_imgproc opencv_imgcodecs opencv_core zlib libjpeg-turbo libopenjp2 libpng libtiff libwebp csi_cv)
install(TARGETS ${bin} DESTINATION bin)

