#!/bin/bash
# Phiên LiteGCS trên Raspberry Pi OS (autostart của labwc gọi file này):
#   * chạy app; mã thoát 42 = "khởi động lại vào bản mới", 0 = thoát hẳn,
#     khác = crash ⇒ chạy lại sau 2 s
#   * nền: litegcs-update.sh lúc khởi động rồi mỗi 30 phút

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
APP="$REPO/cpp/build/LiteGCS"

export LITEGCS_SESSION=1
export LITEGCS_UPDATE_SCRIPT="$REPO/tools/pi/litegcs-update.sh"

(
	sleep 20   # chờ Wi-Fi lên
	while true; do
		"$LITEGCS_UPDATE_SCRIPT"
		sleep 1800
	done
) &
UPDATER=$!
trap 'kill $UPDATER 2>/dev/null' EXIT

while true; do
	cd "$(dirname "$APP")" || exit 1
	"$APP" "$@" >> /tmp/litegcs.log 2>&1
	code=$?
	echo "$(date '+%F %T') LiteGCS thoát, mã $code" >> /tmp/litegcs.log
	case $code in
		42) continue ;;   # khởi động lại để dùng bản mới
		0) break ;;       # nút THOÁT
		*) sleep 2 ;;     # crash
	esac
done
