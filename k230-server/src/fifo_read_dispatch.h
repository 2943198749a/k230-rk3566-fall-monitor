/*
 * 交付版注释
 * 文件：k230/little-core/rtsp-server/src/fifo_read_dispatch.h
 * 职责：强制 DataFIFO 消费顺序：先使用共享帧，再发送 READ_DONE。
 * 设计原因：提前释放会让生产者复用槽位，RTSP 仍在读时可能出现花屏或损坏码流。
 * 数据所有权：遵循本文件接口；涉及跨线程、DataFIFO 或 FFmpeg 缓冲时见函数旁注释。
 * 修改原则：注释不改变来源代码的可执行行为。
 */

#pragma once

// A DataFIFO reader owns a returned item until it acknowledges it with
// DATAFIFO_CMD_READ_DONE. Consume the item before releasing it so a producer
// cannot reuse the shared slot while the consumer still reads it.
template <typename Payload, typename Consume, typename Release>
inline void dispatch_fifo_payload(Payload *payload, Consume consume,
                                  Release release)
{
    consume(payload);
    release(payload);
}
