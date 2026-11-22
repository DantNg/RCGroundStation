#!/bin/bash
# Cài một lần trên mỗi Pi: autostart chạy litegcs-session.sh (tự cập nhật)
# thay vì gọi thẳng binary.  ./tools/pi/install-autoupdate.sh

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
chmod +x "$HERE/litegcs-session.sh" "$HERE/litegcs-update.sh"

mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/litegcs.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=LiteGCS
Comment=Trạm điều khiển mặt đất RC GroundStation (tự cập nhật)
Exec=$HERE/litegcs-session.sh
Terminal=false
X-GNOME-Autostart-enabled=true
EOF

echo "Đã cài: $HOME/.config/autostart/litegcs.desktop → $HERE/litegcs-session.sh"
