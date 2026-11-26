// The guest display: a white canvas with the watch (bezel, buttons, screen).
#pragma once

#include <QImage>
#include <QRectF>
#include <QWidget>

class WatchView : public QWidget {
    Q_OBJECT
public:
    explicit WatchView(QWidget *parent = nullptr);
    void setScreen(const QImage *screen) { m_screen = screen; update(); }
    void setRound(bool round) { m_round = round; update(); }
    void setOverlay(const QString &text) { m_overlay = text; update(); }   // e.g. "Building..."
    QSize sizeHint() const override { return {640, 600}; }

signals:
    void button(int id, bool pressed);

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

    const QImage *m_screen = nullptr;
    bool m_round = true;
    QString m_overlay;
    int m_down = -1;
};
