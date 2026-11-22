#!/bin/bash
# Tự cập nhật LiteGCS trên Raspberry Pi OS: lấy commit mới nhất của nhánh hiện
# tại, build lại cpp/build/LiteGCS. KHÔNG khởi động lại app — app thấy trạng
# thái "ready" trong ~/.lite_gcs/update.json và hỏi người dùng (khoá khi ARM).
#
# An toàn:
#   * chỉ fast-forward (không đè thay đổi local; nhánh lệch ⇒ báo, không làm gì)
#   * build lỗi ⇒ đưa mã nguồn về commit cũ (git reset --keep), binary cũ giữ nguyên
#   * flock: không chạy chồng
#
# Gọi bởi litegcs-session.sh (lúc khởi động + mỗi 30 phút) hoặc nút KIỂM TRA trong app.

set -u

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$HOME/.lite_gcs"
STATUS="$DIR/update.json"
LOG="$DIR/update.log"
BUILT="$REPO/cpp/build/.litegcs_built_commit"   # commit của lần build thành công gần nhất

mkdir -p "$DIR"
exec 9>"$DIR/update.lock"
flock -n 9 || exit 0

# log gọn: giữ ~200 KB cuối
[ -f "$LOG" ] && [ "$(stat -c %s "$LOG")" -gt 400000 ] && tail -c 200000 "$LOG" > "$LOG.tmp" && mv "$LOG.tmp" "$LOG"
echo "=== $(date '+%F %T')" >> "$LOG"

status() { # state commit message
	local msg="${3//\"/\'}"
	printf '{"state":"%s","commit":"%s","message":"%s","time":"%s"}\n' \
		"$1" "$2" "$msg" "$(date '+%F %T')" > "$STATUS.tmp" && mv "$STATUS.tmp" "$STATUS"
	echo "$1 $2 $msg" >> "$LOG"
}

cd "$REPO" || exit 1
OLD="$(git rev-parse HEAD)"
OLD_SHORT="$(git rev-parse --short=7 HEAD)"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"

status checking "$OLD_SHORT" "Đang kiểm tra bản mới"
if ! timeout 60 git fetch -q origin "$BRANCH" >> "$LOG" 2>&1; then
	status offline "$OLD_SHORT" "Không tới được máy chủ git (chưa có mạng?)"
	exit 0
fi

NEW="$(git rev-parse "origin/$BRANCH")"
NEW_SHORT="$(git rev-parse --short=7 "origin/$BRANCH")"

if [ "$NEW" != "$OLD" ]; then
	if ! git merge-base --is-ancestor HEAD "origin/$BRANCH"; then
		status failed "$OLD_SHORT" "Nhánh local đã lệch origin/$BRANCH — cần cập nhật tay"
		exit 0
	fi
	status building "$NEW_SHORT" "Đang tải bản $NEW_SHORT"
	if ! git merge --ff-only -q "origin/$BRANCH" >> "$LOG" 2>&1; then
		status failed "$OLD_SHORT" "git không cập nhật được (file sửa tay trên Pi bị trùng?)"
		exit 0
	fi
fi

HEAD_FULL="$(git rev-parse HEAD)"
HEAD_SHORT="$(git rev-parse --short=7 HEAD)"
if [ -x "$REPO/cpp/build/LiteGCS" ] && [ "$(cat "$BUILT" 2>/dev/null)" = "$HEAD_FULL" ]; then
	status uptodate "$HEAD_SHORT" "Đang dùng bản mới nhất"
	exit 0
fi

status building "$HEAD_SHORT" "Đang build bản $HEAD_SHORT"
cd "$REPO/cpp"
if [ ! -f build/CMakeCache.txt ]; then
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >> "$LOG" 2>&1
fi
# nice + -j3: app đang chạy vẫn mượt trong lúc build
if nice -n 15 cmake --build build -j3 >> "$LOG" 2>&1; then
	echo "$HEAD_FULL" > "$BUILT"
	status ready "$HEAD_SHORT" "Bản $HEAD_SHORT đã sẵn sàng"
else
	cd "$REPO"
	if [ "$HEAD_FULL" != "$OLD" ]; then
		git reset -q --keep "$OLD" >> "$LOG" 2>&1
	fi
	status failed "$OLD_SHORT" "Build bản $HEAD_SHORT lỗi — giữ bản cũ (xem ~/.lite_gcs/update.log)"
fi
