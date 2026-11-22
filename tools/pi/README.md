# Tự cập nhật LiteGCS trên Raspberry Pi OS

Khi Pi có mạng, nó tự tải bản mới nhất của nhánh đang dùng và build sẵn. App **không bao giờ tự khởi động lại**; bạn quyết định lúc nào chuyển sang bản mới.

## Cài một lần trên mỗi Pi

```bash
cd ~/Desktop/RCGroundStation
git pull
(cd cpp && cmake --build build -j4)
./tools/pi/install-autoupdate.sh
```

Script cài đặt đổi autostart (`~/.config/autostart/litegcs.desktop`) sang `litegcs-session.sh`. Lần khởi động sau sẽ chạy theo cơ chế mới.

## Cách hoạt động

1. `litegcs-session.sh` chạy LiteGCS, đồng thời chạy `litegcs-update.sh` ở nền: 20 giây sau khi khởi động, rồi cứ 30 phút một lần.
2. `litegcs-update.sh` làm các bước:
   - `git fetch` nhánh hiện tại.
   - Chỉ cập nhật kiểu **fast-forward**, nên không đè lên file đã sửa tay trên Pi.
   - Build lại với `nice` và 3 luồng, để app đang chạy vẫn mượt.
   - Ghi trạng thái vào `~/.lite_gcs/update.json` và log vào `~/.lite_gcs/update.log`.
3. Build xong, thanh trạng thái hiện **⬆ BẢN MỚI**. Chạm vào đó, hoặc vào ⚙ → CẤU HÌNH → PHẦN MỀM, rồi bấm **⬆ CẬP NHẬT**. App khởi động lại vào bản mới trong khoảng 5 giây. Nút này bị khoá khi drone đang ARM.
4. Nếu build lỗi, mã nguồn được đưa về commit cũ và app cũ vẫn chạy. Dòng PHẦN MỀM báo lỗi; chi tiết xem trong `update.log`.

Nút **KIỂM TRA** trong dòng PHẦN MỀM chạy kiểm tra ngay, không phải chờ 30 phút.

| Trạng thái | Nghĩa |
|---|---|
| `checking` | đang kiểm tra |
| `building` | đang build |
| `ready` | bản mới đã build xong, chờ khởi động lại |
| `uptodate` | đang chạy bản mới nhất |
| `offline` | không có mạng hoặc không tới được máy chủ git |
| `failed` | cần xử lý tay (nhánh lệch, file sửa tay bị trùng, build lỗi) |

Image Buildroot (thư mục `buildroot/`) không dùng cơ chế này, vì image đó không có git và trình biên dịch.
