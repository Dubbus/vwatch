// A tiny plot: your firmware's live metric over time vs the watch's own value.
// Plain QWidget (no signals/slots), so it needs no moc.
#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QWidget>

class TracePlot : public QWidget {
public:
    explicit TracePlot(QWidget *parent = nullptr) : QWidget(parent) { setMinimumHeight(130); }
    void setTruth(double v) { m_truth = v; update(); }
    void setMax(double v) { m_max = v > 0 ? v : 5; update(); }
    void push(double v) { m_series.append(v); update(); }
    void clear() { m_series.clear(); update(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(26, 26, 28));
        const int h = height(), w = width();
        auto Y = [&](double v) { return h - 6 - (v / m_max) * (h - 16); };

        p.setPen(QColor(48, 48, 52));
        for (int i = 0; i <= 5; ++i) {
            const double y = Y(i * m_max / 5);
            p.drawLine(0, int(y), w, int(y));
        }
        if (m_truth > 0) {                                    // the watch's own value
            p.setPen(QPen(QColor(130, 130, 140), 1, Qt::DashLine));
            const double y = Y(m_truth);
            p.drawLine(0, int(y), w, int(y));
            p.setPen(QColor(175, 175, 185));
            p.drawText(5, int(y) - 3, QString("watch %1").arg(m_truth, 0, 'f', 1));
        }
        if (m_series.size() > 1) {                            // your firmware's output
            p.setPen(QPen(QColor(233, 84, 32), 2));
            QPainterPath path;
            for (int i = 0; i < m_series.size(); ++i) {
                const double x = double(i) / (m_series.size() - 1) * w;
                const double y = Y(m_series[i]);
                i ? path.lineTo(x, y) : path.moveTo(x, y);
            }
            p.drawPath(path);
        }
    }

private:
    QVector<double> m_series;
    double m_truth = 0, m_max = 5;
};
