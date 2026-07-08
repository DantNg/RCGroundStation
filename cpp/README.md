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

## Build nhanh (script tự động)

Hai script tự dò Qt, Ninja và toolchain; ghi đè bằng biến `QT_PREFIX` / `MINGW_DIR`
(hoặc `-QtPrefix` / `-MingwDir` trên PowerShell) nếu cài ở nơi khác.

```bash
# Linux / Raspberry Pi — và cả Windows qua Git Bash / MSYS2
./build.sh              # cấu hình + build Release
./build.sh --clean      # dựng lại từ đầu
./build.sh --run        # build xong chạy luôn
./build.sh --debug -j 4 # build Debug, 4 luồng
```

```bat
:: Windows — cmd.exe
build.bat               # cấu hình + build Release
build.bat --clean       # dựng lại từ đầu
build.bat --run         # build xong chạy luôn
build.bat --debug -j 4  # build Debug, 4 luồng
```

Muốn tự tay từng bước thì xem hai mục dưới.

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

## Cần lái ảo (joystick) — bật/tắt

Điều khiển trực tiếp phương tiện bằng hai cần lái ảo trên màn cảm ứng, **bật/tắt
tuỳ ý**:

- Nút **CẦN LÁI** trong thanh CHẾ ĐỘ BAY (bên phải) bật/tắt tính năng. Khi bật,
  đèn báo sáng lục và hai cần lái hiện lên vùng xem chính.
- **Cần trái** — dọc = ga, ngang = yaw (xoay mũi). **Cần phải** — dọc = pitch
  (tiến/lùi), ngang = roll (nghiêng). Nhả tay → cần tự về giữa.
- Trạm bơm gói `MANUAL_CONTROL` tới phương tiện ở **~25 Hz** khi bật; tắt thì
  dừng luồng và đưa cần về giữa.

Kiến trúc: QML `JoystickOverlay`/`JoystickPad` → cầu nối `JoystickController`
(`src/bridge`) → `ICommandSink::manualControl()` → `MavlinkLink` đóng gói
`MANUAL_CONTROL`. Mọi khung đi qua **cổng gác quyền** (`AuthorityGuardedSink`),
nên bản dựng chỉ-xem không thể điều khiển dù nút có lộ ra.

> ⚠️ Cần lái ảo chỉ có tác dụng khi phương tiện đã **ARM** và ở **chế độ điều
> khiển tay** (STABILIZE / ALT_HOLD / LOITER…). Ga giữa (cần ở giữa) ≈ giữ độ cao
> ở ALT_HOLD. Vì `MANUAL_CONTROL` là luồng liên tục, tắt cần lái = ngừng gửi;
> hãy chuyển sang chế độ tự giữ (LOITER/RTL) trước khi tắt nếu đang bay.

## Build cho Android (APK)

Bản Android dùng **UDP/TCP qua Wi-Fi** (Android không có QtSerialPort — mã serial
bị biên dịch tắt bằng `GCS_NO_SERIAL`). Bản đồ vệ tinh cần Internet; định nghĩa
tile được gói vào `assets/` của APK.

**Cài một lần:**

1. Kit **Qt cho Android**: chạy `C:\Qt\MaintenanceTool.exe` → *Add or remove
   components* → Qt 6.10.3 → tích **Android** → sinh `C:\Qt\6.10.3\android_arm64_v8a`.
2. **Android SDK** (Platform API 34) + **NDK** + build-tools + platform-tools —
   cài dễ nhất qua Qt Creator (*Tools → Devices → Android*) hoặc Android Studio.
3. **OpenJDK 17** (Temurin/Microsoft) và biến `JAVA_HOME` trỏ tới nó.

**Dựng APK** (cmd.exe) — đặt biến môi trường rồi chạy script:

```bat
set QT_ANDROID=C:\Qt\6.10.3\android_arm64_v8a
set QT_HOST_PATH=C:\Qt\6.10.3\mingw_64
set ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\Sdk
set ANDROID_NDK_ROOT=%LOCALAPPDATA%\Android\Sdk\ndk\<phiên-bản>
set JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17...

build-android.bat            :: cấu hình + build APK (Release)
build-android.bat --clean    :: dựng lại từ đầu
build-android.bat --install  :: build xong cài vào thiết bị qua adb
```

Hoặc trực tiếp qua CMake preset (đã khai báo trong `CMakePresets.json`):

```bash
cmake --preset android-arm64
cmake --build build-android --target apk
```

APK nằm trong `build-android/android-build/build/outputs/apk/`. Cấu hình gói
(quyền, hướng ngang, min/target SDK) ở `cpp/android/AndroidManifest.xml`.

> Máy phát triển hiện **chưa cài** kit Qt cho Android nên chưa dựng được APK ngay;
> toàn bộ scaffolding (preset, script, manifest, guard serial) đã sẵn sàng — chỉ
> cần cài ba thứ ở trên là build được.
