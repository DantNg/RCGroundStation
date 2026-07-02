#include "ui/round_hud.h"

#include "domain/flight_modes.h"
#include "ui/theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gcs::ui {

namespace {
constexpr double R2D = 57.29577951;
double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

RoundHud::RoundHud(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(80, 96);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void RoundHud::updateFrom(const domain::TelemetrySnapshot &s)
{
    m_roll = s.attitude.roll;
    m_pitch = s.attitude.pitch;
    m_heading = s.position.headingDeg;
    m_alt = s.position.altRel;
    m_airspeed = s.vfr.airspeed;
    m_climb = s.vfr.climb;
    m_valid = s.heartbeatSeen;
    m_mode = s.heartbeatSeen
        ? domain::flight_modes::modeName(s.mode.autopilot, s.mode.customMode)
        : QStringLiteral("—");
    m_armed = s.mode.armed;
    m_battV = s.battery.voltage;
    m_battPct = s.battery.remaining;
    m_gpsFix = s.gps.fixType;
    m_gpsSats = s.gps.satellites;
    m_linkUp = s.link.linkUp;
    update();
}

void RoundHud::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const int w = width(), h = height();
    const double s = clampd(std::min(w, h) / 220.0, 0.42, 1.6);

    p.setPen(QPen(QColor(theme::PANEL_BORDER), 1));
    p.setBrush(QBrush(QColor(13, 17, 23)));
    p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10 * s, 10 * s);

    const int lineH = int(16 * s);
    const int stripH = lineH + int(8 * s);
    const int avail = h - stripH;
    const double R = std::max(20.0, std::min(w, avail) / 2.0 - 4 * s);
    const double cx = w / 2.0;
    const double cy = 4 * s + R;

    drawBall(p, cx, cy, R, s);
    drawBezelAndRoll(p, cx, cy, R, s);
    drawBoresight(p, cx, cy, s);
    drawHeadingBox(p, cx, cy, R, s);
    drawAlt(p, w, h - stripH, stripH, s);
    if (!m_valid)
        drawNoData(p, cx, cy, R, s);
}

void RoundHud::drawBall(QPainter &p, double cx, double cy, double R, double s)
{
    p.save();
    QPainterPath clip;
    clip.addEllipse(QPointF(cx, cy), R, R);
    p.setClipPath(clip);

    const double a = -m_roll;
    const double ca = std::cos(a), sa = std::sin(a);
    const double ux = ca, uy = sa;
    const double nx = -sa, ny = ca;
    const double ppd = R / 26.0;
    const double pitchDeg = m_pitch * R2D;
    const double ccx = cx + nx * pitchDeg * ppd;
    const double ccy = cy + ny * pitchDeg * ppd;
    const double L = R * 3.0;
    const double D = R * 3.0;

    auto quad = [&](double off1, double off2, const QBrush &brush) {
        p.setPen(Qt::NoPen);
        p.setBrush(brush);
        p.drawPolygon(QPolygonF({
            QPointF(ccx - ux * L + nx * off1, ccy - uy * L + ny * off1),
            QPointF(ccx + ux * L + nx * off1, ccy + uy * L + ny * off1),
            QPointF(ccx + ux * L + nx * off2, ccy + uy * L + ny * off2),
            QPointF(ccx - ux * L + nx * off2, ccy - uy * L + ny * off2),
        }));
    };

    const double span = R * 2.0;
    QLinearGradient sky(QPointF(ccx - nx * span, ccy - ny * span), QPointF(ccx, ccy));
    sky.setColorAt(0.0, theme::hudSkyTop());
    sky.setColorAt(1.0, theme::hudSkyHorizon());
    QLinearGradient gnd(QPointF(ccx, ccy), QPointF(ccx + nx * span, ccy + ny * span));
    gnd.setColorAt(0.0, theme::hudGroundHorizon());
    gnd.setColorAt(1.0, theme::hudGroundBottom());
    quad(-D, 0, QBrush(sky));
    quad(0, D, QBrush(gnd));

    p.setPen(QPen(theme::hudLine(), std::max(1.0, std::round(2 * s))));
    p.drawLine(QPointF(ccx - ux * L, ccy - uy * L), QPointF(ccx + ux * L, ccy + uy * L));

    p.setFont(QFont("Segoe UI", std::max(6, int(8 * s))));
    for (int m = -20; m <= 20; m += 10) {
        if (m == 0)
            continue;
        const double off = -m * ppd;
        const double mx = ccx + nx * off, my = ccy + ny * off;
        const double half = 16 * s;
        p.setPen(QPen(theme::hudLine(), std::max(1.0, std::round(1.4 * s))));
        p.drawLine(QPointF(mx - ux * half, my - uy * half),
                   QPointF(mx + ux * half, my + uy * half));
    }
    p.restore();
}

void RoundHud::drawBezelAndRoll(QPainter &p, double cx, double cy, double R, double s)
{
    p.setPen(QPen(QColor("#0b0f14"), std::max(2.0, std::round(3 * s))));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), R, R);
    p.setPen(QPen(QColor(255, 255, 255, 60), std::max(1.0, std::round(s))));
    p.drawEllipse(QPointF(cx, cy), R, R);

    const int ticks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
    for (int t : ticks) {
        const double ang = (-90 + t) * M_PI / 180.0;
        const double co = std::cos(ang), si = std::sin(ang);
        const double ln = (t % 30 == 0 ? 8 : 5) * s;
        p.setPen(QPen(theme::hudLine(), std::max(1.0, std::round((t % 30 == 0 ? 2 : 1) * s))));
        p.drawLine(QPointF(cx + co * (R - ln), cy + si * (R - ln)),
                   QPointF(cx + co * R, cy + si * R));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(theme::hudLine()));
    const double tp = 6 * s;
    p.drawPolygon(QPolygonF({
        QPointF(cx, cy - R + 1),
        QPointF(cx - tp, cy - R + tp * 1.6),
        QPointF(cx + tp, cy - R + tp * 1.6)}));

    const double ang = (-90 + m_roll * R2D) * M_PI / 180.0;
    const double co = std::cos(ang), si = std::sin(ang);
    const double px = -si, py = co;
    const double ap = 6 * s;
    p.setBrush(QBrush(theme::hudAccent()));
    p.drawPolygon(QPolygonF({
        QPointF(cx + co * (R - 1), cy + si * (R - 1)),
        QPointF(cx + co * (R - ap * 2) + px * ap, cy + si * (R - ap * 2) + py * ap),
        QPointF(cx + co * (R - ap * 2) - px * ap, cy + si * (R - ap * 2) - py * ap)}));
}

void RoundHud::drawBoresight(QPainter &p, double cx, double cy, double s)
{
    const double ww = 26 * s, wi = 9 * s, drop = 6 * s;
    QPen pen(theme::hudAccent(), std::max(2.0, std::round(3 * s)));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QPolygonF({QPointF(cx - ww, cy), QPointF(cx - wi, cy), QPointF(cx - wi, cy + drop)}));
    p.drawPolyline(QPolygonF({QPointF(cx + ww, cy), QPointF(cx + wi, cy), QPointF(cx + wi, cy + drop)}));
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(theme::hudAccent()));
    p.drawEllipse(QPointF(cx, cy), std::max(2.0, 2.5 * s), std::max(2.0, 2.5 * s));
}

void RoundHud::drawHeadingBox(QPainter &p, double cx, double cy, double R, double s)
{
    const double bw = 42 * s, bh = 18 * s;
    const QRectF rect(cx - bw / 2, cy - R - bh - 4 * s, bw, bh);
    p.setPen(QPen(theme::hudCyan(), std::max(1.0, std::round(s))));
    p.setBrush(QBrush(theme::hudBoxBg()));
    p.drawRoundedRect(rect, 3 * s, 3 * s);
    p.setPen(QPen(theme::hudText()));
    QFont hf("Consolas");
    hf.setPixelSize(std::max(7, int(bh * 0.72)));
    p.setFont(hf);
    const int hdg = ((int(std::round(m_heading)) % 360) + 360) % 360;
    p.drawText(rect, Qt::AlignCenter, QStringLiteral("%1").arg(hdg, 3, 10, QLatin1Char('0')));
}

void RoundHud::drawAlt(QPainter &p, int w, double y, double stripH, double s)
{
    QFont f("Consolas");
    f.setBold(true);
    f.setPixelSize(std::max(9, int(stripH * 0.62)));
    p.setFont(f);
    p.setPen(QPen(theme::hudTextDim()));
    p.drawText(QRectF(0, y, w, stripH), Qt::AlignCenter,
               QStringLiteral("ĐỘ CAO  %1 m").arg(m_alt, 0, 'f', 0));
    Q_UNUSED(s);
}

void RoundHud::drawNoData(QPainter &p, double cx, double cy, double R, double s)
{
    p.setBrush(QBrush(QColor(0, 0, 0, 120)));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), R, R);
    p.setPen(QPen(theme::hudTextDim()));
    p.setFont(QFont("Segoe UI", std::max(8, int(10 * s)), QFont::Bold));
    p.drawText(QRectF(cx - R, cy - 10 * s, 2 * R, 20 * s), Qt::AlignCenter,
               QStringLiteral("KHÔNG CÓ DỮ LIỆU"));
}

} // namespace gcs::ui
