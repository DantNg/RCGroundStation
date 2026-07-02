# Trạm Điều Khiển Mặt Đất — bản C++/Qt (RC_version)

Bản dựng lại bằng **C++17 + Qt 6** của ứng dụng Python/PySide6, việt hoá toàn bộ
giao diện và tối ưu để chạy trên **Raspberry Pi / Linux**. Kiến trúc SOLID được
giữ nguyên như bản Python:

```
cpp/
  src/
    domain/      đối tượng dữ liệu telemetry + bảng chế độ bay + nhiệm vụ (thuần dữ liệu)
    interfaces/  ITelemetryLink (đọc) + ICommandSink (ghi)  (cổng DIP / ISP)
    mavlink/     MavlinkLink, TelemetryDecoder, CommandService, MissionService
    app/         TelemetryStore (mutex), LinkManager (luồng worker), GcsController
    ui/          HUD, bản đồ, bảng, thanh trên, MainWindow  (view Qt thụ động)
    config.*     cài đặt kết nối được lưu
  main.cpp        điểm vào
  third_party/mavlink/   thư viện MAVLink C (sinh từ ardupilotmega.xml)
```

So với bản Python, `pymavlink` được thay bằng **thư viện MAVLink C**, `pyserial`
bằng **Qt SerialPort**, UDP/TCP bằng **Qt Network**, webcam bằng **Qt
Multimedia**, và quả cầu Cesium 3D chạy trong **Qt WebEngine** (dùng lại y hệt
`assets/` của bản Python).

## Phụ thuộc

- CMake ≥ 3.21, trình biên dịch C++17 (GCC/Clang trên Linux, MinGW/MSVC trên Windows)
- Qt 6: `Core Gui Widgets Network SerialPort Multimedia MultimediaWidgets WebChannel`
- **Qt WebEngine** (tuỳ chọn — cho bản đồ 3D). Nếu thiếu, chế độ 3D hiện
  placeholder còn phần còn lại vẫn chạy đầy đủ.

## Build trên Linux / Raspberry Pi

```bash
sudo apt install cmake build-essential \
     qt6-base-dev qt6-serialport-dev qt6-multimedia-dev \
     qt6-webengine-dev libqt6webenginewidgets6

cd desktop/cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/LiteGCS
```

> Trên Raspberry Pi, Qt WebEngine (Chromium) là phần nặng nhất. Nếu quả cầu 3D
> quá tải, cứ dùng bản đồ 2D — nó chạy hoàn toàn bằng QPainter và rất nhẹ.

## Build trên Windows (MinGW)

Qt cho MinGW **không kèm** WebEngine, nên bản Windows sẽ tắt chế độ 3D
(placeholder). Dùng kit MSVC nếu cần 3D trên Windows.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.3/mingw_64" \
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Sinh lại thư viện MAVLink C (khi đổi dialect)

```bash
python -c "from pymavlink.generator import mavgen; \
  o=type('O',(),{'language':'C','wire_protocol':'2.0',\
  'output':'third_party/mavlink','error_limit':200,\
  'validate':True,'strict_units':False}); \
  mavgen.mavgen(o, ['.../message_definitions/v1.0/ardupilotmega.xml'])"
```

## Thử không cần drone

Chạy bộ mô phỏng của bản Python ở một cửa sổ (`python ../tools/sim_udp.py`) rồi
kết nối bằng **UDP cổng 14550** trong ứng dụng.
