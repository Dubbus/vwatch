#include "watchview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

// Button ids match VW_BTN_* in sdk/include/vwatch.h.
enum { LIGHT = 0, UP, DOWN, START, BACK };
static const char *const kNames[] = {"LIGHT", "UP", "DOWN", "START", "BACK"};

WatchView::WatchView(QWidget *parent) : QWidget(parent), m_hold(new QTimer(this)) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(360, 360);
    m_hold->setSingleShot(true);
    m_hold->setInterval(1000);                     // long press, as on the watch
    connect(m_hold, &QTimer::timeout, this, [this] { if (m_down >= 0) emit button(m_down, 2); });
}

void WatchView::press(int id) {
    m_down = id;
    emit button(id, 1);
    m_hold->start();
    update();
}

void WatchView::release() {
    if (m_down < 0) return;
    m_hold->stop();
    emit button(m_down, 0);
    m_down = -1;
    update();
}

// The 64 RGB222 colours as they appear on the display. "Realism" models a
// transflective MIP panel: unlit it reflects ambient light (grey "paper"
// whites, lifted blacks, muted colour); lit it is brighter and cooler.
QVector<QRgb> WatchView::palette(bool lit, bool realism) {
    QVector<QRgb> t(64);
    for (int c = 0; c < 64; ++c) {
        double rgb[3] = {((c >> 4) & 3) / 3.0, ((c >> 2) & 3) / 3.0, (c & 3) / 3.0};
        if (!realism) {
            t[c] = qRgb(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255));
            continue;
        }
        const double black[3] = {lit ? 8.0 : 26.0, lit ? 8.0 : 26.0, lit ? 12.0 : 24.0};
        const double white[3] = {lit ? 236.0 : 192.0, lit ? 241.0 : 193.0, lit ? 250.0 : 184.0};
        const double sat = lit ? 1.0 : 0.78;
        const double luma = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
        int out[3];
        for (int i = 0; i < 3; ++i) {
            const double v = luma + (rgb[i] - luma) * sat;
            out[i] = int(black[i] + (white[i] - black[i]) * v + 0.5);
        }
        t[c] = qRgb(out[0], out[1], out[2]);
    }
    return t;
}

WatchView::Geometry WatchView::geometry() const {
    Geometry g;
    g.c = QPointF(width() / 2.0, height() / 2.0);
    const qreal fit = qMin(width(), height()) / 320.0;           // 240 px screen + bezel + buttons
    g.scale = qMax(1.0, qFloor(fit * 2) / 2.0);                  // 1x, 1.5x, 2x ... keeps pixels crisp-ish
    g.screenR = 120 * g.scale;
    g.bezelR = g.screenR + 22 * g.scale;
    // Garmin-style layout: LIGHT/UP/DOWN on the left, START/BACK on the right (angles from 12 o'clock).
    const qreal angles[5] = {-60, -100, -140, 60, 140};
    for (int i = 0; i < 5; ++i) {
        const qreal a = qDegreesToRadians(angles[i]);
        const QPointF p = g.c + QPointF(qSin(a), -qCos(a)) * (g.bezelR + 4 * g.scale);
        g.buttons[i] = QRectF(p - QPointF(9, 9) * g.scale, QSizeF(18, 18) * g.scale);
    }
    return g;
}

void WatchView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    const Geometry g = geometry();

    // buttons (behind the case)
    for (int i = 0; i < 5; ++i) {
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.setBrush(i == m_down ? QColor(200, 90, 40) : QColor(90, 90, 92));
        p.drawRoundedRect(g.buttons[i], 3 * g.scale, 3 * g.scale);
    }

    // case + bezel
    QRadialGradient bez(g.c - QPointF(g.bezelR / 3, g.bezelR / 3), g.bezelR * 1.6);
    bez.setColorAt(0, QColor(120, 120, 122));
    bez.setColorAt(0.5, QColor(55, 55, 57));
    bez.setColorAt(1, QColor(25, 25, 26));
    p.setPen(QPen(QColor(15, 15, 15), 1.5));
    p.setBrush(bez);
    p.drawEllipse(g.c, g.bezelR, g.bezelR);

    // screen
    const QRectF screenRect(g.c - QPointF(g.screenR, g.screenR), QSizeF(2 * g.screenR, 2 * g.screenR));
    p.save();
    if (m_round) {
        QPainterPath clip;
        clip.addEllipse(screenRect);
        p.setClipPath(clip);
    }
    p.fillRect(screenRect, Qt::black);
    if (m_screen) {
        QImage img = *m_screen;                                   // shallow copy; detaches on palette set
        if (img.format() == QImage::Format_Indexed8) img.setColorTable(palette(m_backlight > 0, m_realism));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);   // keep pixels sharp
        p.drawImage(screenRect, img);
        const qreal px = screenRect.width() / 240.0;
        if (m_realism && px >= 2.0) {                              // faint MIP pixel grid
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setPen(QPen(QColor(0, 0, 0, m_backlight > 0 ? 28 : 40), 1));
            for (int i = 1; i < 240; ++i) {
                const qreal o = i * px;
                p.drawLine(QPointF(screenRect.left() + o, screenRect.top()), QPointF(screenRect.left() + o, screenRect.bottom()));
                p.drawLine(QPointF(screenRect.left(), screenRect.top() + o), QPointF(screenRect.right(), screenRect.top() + o));
            }
            p.setRenderHint(QPainter::Antialiasing, true);
        }
    }
    if (!m_overlay.isEmpty()) {
        p.fillRect(screenRect, QColor(0, 0, 0, 170));
        p.setPen(Qt::white);
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.3);
        p.setFont(f);
        p.drawText(screenRect, Qt::AlignCenter | Qt::TextWordWrap, m_overlay);
    }
    p.restore();

    // button labels
    p.setPen(QColor(120, 120, 120));
    QFont small = font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() * 0.8));
    p.setFont(small);
    for (int i = 0; i < 5; ++i) {
        const QRectF b = g.buttons[i];
        const bool left = b.center().x() < g.c.x();
        const QRectF label = left ? QRectF(b.left() - 60, b.top(), 54, b.height())
                                  : QRectF(b.right() + 6, b.top(), 60, b.height());
        p.drawText(label, (left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, kNames[i]);
    }
}

void WatchView::mousePressEvent(QMouseEvent *e) {
    const Geometry g = geometry();
    for (int i = 0; i < 5; ++i)
        if (g.buttons[i].adjusted(-4, -4, 4, 4).contains(e->position())) {
            press(i);
            return;
        }
}

void WatchView::mouseReleaseEvent(QMouseEvent *) { release(); }

int WatchView::keyToButton(int key) {
    switch (key) {
    case Qt::Key_L: return LIGHT;
    case Qt::Key_Up: return UP;
    case Qt::Key_Down: return DOWN;
    case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space: return START;
    case Qt::Key_Escape: case Qt::Key_Backspace: return BACK;
    default: return -1;
    }
}

void WatchView::keyPressEvent(QKeyEvent *e) {
    const int b = keyToButton(e->key());
    if (b < 0 || e->isAutoRepeat()) return QWidget::keyPressEvent(e);
    press(b);
}

void WatchView::keyReleaseEvent(QKeyEvent *e) {
    const int b = keyToButton(e->key());
    if (b < 0 || e->isAutoRepeat()) return QWidget::keyReleaseEvent(e);
    release();
}
