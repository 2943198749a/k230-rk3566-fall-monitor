# K230 H.264 Video and TCP Fall-Status Reporting Design

## Objective

Keep the K230-to-RK3566 video path stable while reporting per-frame fall
status to the RK3566 TCP monitor.  Video must remain playable before status
reporting is enabled.

## Confirmed Baseline

- The known-good deployment is `/sharefs/app/falldown_rtsp/`.
- Its COM7 program is `falldown_rtsp_big.elf`; it produces H.264 video.
- Its COM6 command is `./rtspServer -p 1628c000 -t h264`.
- RK3566 successfully plays `rtsp://192.168.10.2:8554/BackChannelTest`.
- The K230 board's compatible media runtime logs a `Nov 6 2024` build date.
- The validated SDK tree contained matching `Nov 6 2024` VICP
  media assets.  The prior diagnostic ELF was invalid because it linked an
  older `Jun 14 2024` media SDK.

## Architecture

```text
K230 COM7 (one matching-SDK ELF)
  camera -> fall detector -> post-NMS counts
       |                       |
       |                       `-> status DataFIFO -> COM6 status client
       |                                                    |
       `-> H.264 encoder -> video DataFIFO -> COM6 RTSP server -> RK3566
                                                             |
                                                             `-> TCP :9000
```

Video and detection-status records never share a FIFO or packet layout.

## Video Contract

- The verified baseline uses `K_PT_H264` with `VENC_PROFILE_H264_MAIN` in
  every sensor configuration branch.
- It keeps the existing video DataFIFO physical address and packet layout:
  64-bit PTS, 32-bit NAL length, then encoded H.264 bytes.
- COM6 starts `rtspServer` with `-t h264` only after COM7 has opened the video
  FIFO.
- The video writer retains the original, verified DataFIFO behavior.

## Status Contract

- COM7 creates one independent writer DataFIFO for fixed-size
  `DetectionStatus` messages.
- Each message contains magic, version, sequence, total, standing, and fallen
  counts.  It is validated before sending and must satisfy
  `total == standing + fallen` without arithmetic overflow.
- COM6's `k230_status_tcp_client` reads only that FIFO, copies a complete
  status record before acknowledging it, and reconnects to `192.168.10.3:9000`
  if the TCP peer is unavailable.
- A valid record becomes exactly one line:

  ```text
  STATUS seq=<n> total=<n> standing=<n> fallen=<n> state=<SAFE|FALL>\n
  ```

## Deployment and Validation

### Phase 1: Video only

1. Build the status-safe big ELF from the original video source using the
   matching `k230-v3-work` SDK assets.
2. Preserve the original contiguous RGB copy.  Do not use the experimental
   RGB888P stride/three-plane copy path, which was proven to corrupt video.
3. Deploy under a new filename in `/sharefs/app/falldown_rtsp_tcp/`; do not
   overwrite the working application.
4. Start only the new COM7 H.264 program and COM6 RTSP server (`-t h264`).
5. Verify RK3566 receives a decodable H.264 stream with normal video.

### Phase 2: Status only after video passes

1. Start the RK3566 status server on port 9000.
2. Start `k230_status_tcp_client` with the status FIFO physical address
   printed by COM7.
3. Verify `SAFE -> FALL -> SAFE` status lines on RK3566 while the RTSP video
   remains playable.

## Rollback

No Phase 1 or Phase 2 deployment overwrites `/sharefs/app/falldown_rtsp/`.
On any failed validation, stop the new COM7 and COM6 programs and return to:

```sh
# COM7
cd /sharefs/app/falldown_rtsp
./falldown_rtsp_big.elf yolov5n-falldown.kmodel 0.5 0.45 0

# COM6
cd /sharefs/app/falldown_rtsp
./rtspServer -p 1628c000 -t h264
```

## Acceptance Criteria

1. Phase 1 displays stable H.264 video on RK3566 for at least two minutes
   without decode errors or a blank frame.
2. Phase 2 emits valid TCP status lines to `192.168.10.3:9000`.
3. Starting or stopping the status client does not corrupt, stop, or change
   the RTSP video stream.
4. The known-good application remains untouched and usable as rollback.
