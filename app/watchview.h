// The guest display: a white canvas with the watch (bezel, buttons, screen).
#pragma once

#include <QImage>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QTimer;

class WatchView : public QWidget {
    Q_OBJECT
public:
    explicit WatchView(QWidget *parent = nullptr);
    void setScreen(const QImage *screen) { m_screen = screen; update(); }
    void setRound(bool round) { m_round = round; update(); }
    void setOverlay(const QString &text) { m_overlay = text; update(); }   // e.g. "Building..."
    void setBacklight(int level) { m_backlight = level; update(); }
    // Render like a transflective MIP panel (reflective when unlit, pixel grid)
    // instead of ideal RGB.
    void setRealism(bool on) { m_realism = on; update(); }
    bool realism() const { return m_realism; }
    QSize sizeHint() const override { return {640, 600}; }

signals:
    void button(int id, int event);   // VW_RELEASED 0 / VW_PRESSED 1 / VW_HELD 2

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;

private:
    struct Geometry { QPointF c; qreal scale, screenR, bezelR; QRectF buttons[5]; };
    Geometry geometry() const;
    static int keyToButton(int key);
    static QVector<QRgb> palette(bool lit, bool realism);
    void press(int id);
    void release();

    const QImage *m_screen = nullptr;
    bool m_round = true;
    QString m_overlay;
    int m_down = -1;
    int m_backlight = 0;
    bool m_realism = true;
    QTimer *m_hold;
};
