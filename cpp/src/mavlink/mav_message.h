// Bí danh cho khung tin MAVLink đã phân tích.
//
// Chỉ tầng ``mavlink/`` mới #include header MAVLink C. Các tầng khác truyền
// ``MavMessage`` một cách trong suốt (giống việc bản Python truyền một message
// "duck-typed" quanh ứng dụng).
#pragma once

// Tắt cảnh báo từ thư viện MAVLink sinh tự động.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#  pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <ardupilotmega/mavlink.h>

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

namespace gcs {
using MavMessage = mavlink_message_t;
}
