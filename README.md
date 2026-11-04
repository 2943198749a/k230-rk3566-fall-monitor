# K230 & RK3566 Edge Status Reporting

This repository contains the parts I implemented and maintain for an edge-side fall-status reporting project:

- `k230-server/`: K230-side RTSP/media service program.
- `rk3566-qt-viewer/`: RK3566 Qt monitoring interface, including RTSP playback, detection-count receiving, retry logic, and unit tests.

## Scope

The repository intentionally does **not** include AI detection models, model-training material, K230 detection/client code, SDK binaries, or other modules not owned by this project scope.

## Build notes

- The K230 service requires the corresponding K230 SDK and cross-compilation toolchain. See `k230-server/build.sh` and `k230-server/CMakeLists.txt`.
- The RK3566 viewer is a Qt/C++ project. Configure the RTSP address in `rk3566-qt-viewer/src/app_config.h` for your own device and network before building.

## Directory layout

```text
.
├── k230-server/        # K230 service-side source
└── rk3566-qt-viewer/   # RK3566 Qt monitoring interface
```

