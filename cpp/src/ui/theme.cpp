#include "ui/theme.h"

#include <QPainter>

namespace gcs::theme {

QPixmap tintedPixmap(const QString &resourcePath, const QColor &color)
{
    QPixmap src(resourcePath);
    if (src.isNull())
        return src;
    QPixmap out(src.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, src);                                  // hình dạng (alpha)
    p.setCompositionMode(QPainter::CompositionMode_SourceIn); // giữ alpha, thay màu
    p.fillRect(out.rect(), color);
    p.end();
    out.setDevicePixelRatio(src.devicePixelRatio());
    return out;
}

QIcon railIcon(const QString &resourcePath)
{
    const QPixmap src(resourcePath);
    if (src.isNull())
        return QIcon();
    QIcon icon;
    // thường (chưa chọn): mực sáng
    icon.addPixmap(tintedPixmap(resourcePath, QColor(TEXT)), QIcon::Normal, QIcon::Off);
    // được chọn: nền xanh sáng nên dùng mực tối cho tương phản
    const QPixmap dark = tintedPixmap(resourcePath, QColor("#04101f"));
    icon.addPixmap(dark, QIcon::Normal, QIcon::On);
    icon.addPixmap(dark, QIcon::Active, QIcon::On);
    // vô hiệu: xám mờ như QSS
    icon.addPixmap(tintedPixmap(resourcePath, QColor("#5a6472")), QIcon::Disabled, QIcon::Off);
    return icon;
}

QString severityColor(domain::Severity sev)
{
    using domain::Severity;
    switch (sev) {
    case Severity::Emergency:
    case Severity::Alert:
    case Severity::Critical:
    case Severity::Error:   return QString::fromLatin1(BAD);
    case Severity::Warning: return QString::fromLatin1(WARN);
    case Severity::Notice:  return QStringLiteral("#58a6ff");
    case Severity::Info:    return QString::fromLatin1(TEXT);
    case Severity::Debug:   return QString::fromLatin1(TEXT_DIM);
    }
    return QString::fromLatin1(TEXT);
}

QString stylesheet()
{
    // Cổng chuyển thẳng từ theme.py; giá trị màu đã nội tuyến.
    return QStringLiteral(R"QSS(
* { outline: none; }
QWidget {
    background-color: #0a0e14;
    color: #eef2f7;
    font-family: "Segoe UI Variable", "Segoe UI", "Inter", "DejaVu Sans", sans-serif;
    font-size: 13px;
}
QToolTip {
    background-color: #1b232d;
    color: #eef2f7;
    border: 1px solid #3a4654;
    border-radius: 6px;
    padding: 5px 8px;
}

/* cards */
QFrame#Panel { background-color: transparent; border: none; }
QLabel#PanelTitle {
    color: #b8c3d1;
    font-size: 11px;
    font-weight: 800;
    letter-spacing: 1.6px;
}

/* buttons */
QPushButton {
    background-color: #1b232d;
    border: 1px solid #313b47;
    border-radius: 8px;
    padding: 9px 14px;
    font-weight: 600;
    color: #eef2f7;
}
QPushButton:hover { background-color: #232c37; border-color: #4a586a; }
QPushButton:pressed { background-color: #161d25; }
QPushButton:disabled { color: #5a6472; background-color: #131820; border-color: #20272f; }
QPushButton:checked { background-color: #223449; border-color: #5aa6ff; color: #ffffff; }

QPushButton#Arm { background-color: #4cc463; color: #032010; border: none; }
QPushButton#Arm:hover { background-color: #5cd873; }
QPushButton#Disarm { background-color: #ff6459; color: #2a0707; border: none; }
QPushButton#Disarm:hover { background-color: #ff7a70; }
QPushButton#Mode { font-size: 15px; padding: 12px 8px; }
QPushButton#Connect { background-color: #2ea043; color: #ffffff; border: none; padding: 8px 18px; font-weight: 700; }
QPushButton#Connect:hover { background-color: #3ab953; }
QPushButton#Connect:pressed { background-color: #268038; }

QPushButton#IconButton {
    padding: 6px; min-width: 36px; min-height: 36px;
    border-radius: 8px; font-size: 16px; font-weight: 700;
}
QPushButton#Ghost {
    background-color: rgba(255, 255, 255, 0.08);
    border: 1px solid rgba(255, 255, 255, 0.16);
    border-radius: 8px; padding: 5px 12px; color: #eef2f7;
}
QPushButton#Ghost:hover { background-color: rgba(255, 255, 255, 0.15); border-color: rgba(255, 255, 255, 0.24); }
QPushButton#ChipDisconnect {
    background-color: rgba(255, 93, 82, 0.16);
    border: 1px solid rgba(255, 93, 82, 0.45);
    color: #ff8a82; border-radius: 8px; padding: 5px 12px;
}
QPushButton#ChipDisconnect:hover { background-color: rgba(255, 93, 82, 0.28); }

/* inputs */
QComboBox, QSpinBox, QLineEdit, QDoubleSpinBox {
    background-color: #0c1118;
    border: 1px solid #333d4a;
    border-radius: 8px;
    padding: 6px 9px;
    color: #eef2f7;
    selection-background-color: #5aa6ff;
    selection-color: #061424;
}
QComboBox:hover, QSpinBox:hover, QLineEdit:hover, QDoubleSpinBox:hover { border-color: #465566; }
QComboBox:focus, QSpinBox:focus, QLineEdit:focus, QDoubleSpinBox:focus {
    border: 2px solid #5aa6ff; padding: 5px 8px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #aab6c4;
    margin-right: 8px;
}
QComboBox::down-arrow:hover { border-top-color: #eef2f7; }
QComboBox QAbstractItemView {
    background-color: #141a22;
    border: 1px solid #333d4a;
    border-radius: 8px;
    selection-background-color: #223449;
    selection-color: #ffffff;
    outline: none; padding: 4px;
}

/* check box */
QCheckBox { spacing: 7px; color: #cdd6e0; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1px solid #3a4654; border-radius: 5px; background-color: #0c1118;
}
QCheckBox::indicator:hover { border-color: #5aa6ff; }
QCheckBox::indicator:checked { background-color: #5aa6ff; border-color: #5aa6ff; }

/* text panels */
QPlainTextEdit, QTextEdit {
    background-color: #080b10;
    border: 1px solid #2a323d;
    border-radius: 8px;
    font-family: "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 12px;
}

/* scrollbars */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #313b47; border-radius: 5px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: #3e4a59; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #313b47; border-radius: 5px; min-width: 28px; }
QScrollBar::handle:horizontal:hover { background: #3e4a59; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* connection chip */
QFrame#ConnChip {
    background-color: rgba(20, 26, 34, 0.92);
    border: 1px solid #3a4654; border-radius: 14px;
}
QLabel#ConnLabel { font-weight: 600; color: #eef2f7; }

/* thẻ kết nối (acrylic) — thanh này tự vẽ nền kính mờ trong paintEvent, nên QSS
   phải giữ trong suốt; và các nhãn con phải trong suốt để không hiện hộp đen đục
   do luật "QWidget { background-color }" ở trên. */
QFrame#ConnBar { background-color: transparent; border: none; }
QFrame#ConnBar QLabel { background: transparent; color: #aab6c4; }
/* các container (QStackedWidget + trang serial/udp/tcp) cũng phải trong suốt,
   nếu không chúng tạo một dải tối sau nhóm Cổng/Baud. Chỉ nhắm container trực
   tiếp (> QWidget) để KHÔNG chạm nền tối cố ý của các ô combo/spinbox/lineedit. */
QFrame#ConnBar QStackedWidget { background: transparent; }
QFrame#ConnBar QStackedWidget > QWidget { background: transparent; }

/* control dock (acrylic) */
QFrame#TopBar { background-color: transparent; border: none; }
QFrame#TopBar QLabel { color: #aab6c4; }
QFrame#TopBar QLabel#ConnLabel { color: #eef2f7; font-weight: 600; }
QFrame#TopBar QPushButton { padding: 6px 11px; }
QFrame#TopBar QPushButton#IconButton { min-width: 30px; min-height: 30px; padding: 4px; font-size: 15px; }
QFrame#TopBar QComboBox { padding: 5px 8px; }

QLabel#CamPlaceholder { color: #aab6c4; }

/* top status bar */
QFrame#StatusBar { background-color: transparent; border: none; }
QFrame#StatusBar QLabel { color: #eef2f7; }
QLabel#Vehicle, QLabel#ViewTag, QLabel#ConnDot { background: transparent; }
QLabel#Vehicle { font-size: 15px; font-weight: 700; }
QLabel#ViewTag { color: #aab6c4; font-weight: 700; letter-spacing: 0.6px; }
QLabel#StatChip {
    background-color: rgba(255, 255, 255, 0.08);
    border: 1px solid rgba(255, 255, 255, 0.14);
    border-radius: 9px; padding: 4px 10px;
    color: #eef2f7;
    font-family: "Cascadia Mono", "Consolas", monospace; font-weight: 700;
}
QPushButton#ModePill {
    background-color: rgba(90, 166, 255, 0.16);
    border: 1px solid rgba(90, 166, 255, 0.45);
    color: #cfe2ff;
    border-radius: 14px; padding: 6px 16px; font-weight: 800;
}
QPushButton#ModePill:hover { background-color: rgba(90, 166, 255, 0.28); border-color: rgba(90, 166, 255, 0.65); }
QPushButton#ArmPill {
    background-color: rgba(63, 185, 80, 0.18);
    border: 1px solid rgba(63, 185, 80, 0.55);
    color: #7ee48a; border-radius: 14px; padding: 6px 16px; font-weight: 800;
}
QPushButton#ArmPill:hover { background-color: rgba(63, 185, 80, 0.30); }
QPushButton#DisarmPill {
    background-color: rgba(255, 93, 82, 0.20);
    border: 1px solid rgba(255, 93, 82, 0.60);
    color: #ff9089; border-radius: 14px; padding: 6px 16px; font-weight: 800;
}
QPushButton#DisarmPill:hover { background-color: rgba(255, 93, 82, 0.32); }
QPushButton#ArmPill:disabled, QPushButton#DisarmPill:disabled, QPushButton#ModePill:disabled {
    color: #555e6b; background-color: rgba(255,255,255,0.03); border-color: rgba(255,255,255,0.06);
}

/* left mode/action rail */
QFrame#ModeRail { background-color: transparent; border: none; }
QToolButton#RailBtn {
    background-color: rgba(20, 26, 34, 0.62);
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 12px; padding: 6px 2px; color: #eef2f7;
    font-size: 10px; font-weight: 700; letter-spacing: 0.5px;
}
QToolButton#RailBtn:hover { background-color: rgba(36, 44, 56, 0.82); border-color: rgba(255,255,255,0.22); }
QToolButton#RailBtn:checked { background-color: #5aa6ff; border-color: #5aa6ff; color: #04101f; }
QToolButton#RailBtn:disabled { color: #5a6472; background-color: rgba(20,26,34,0.4); }
)QSS");
}

} // namespace gcs::theme
