// One running virtual watch: a QEMU process plus the vw_proto link to it.
#pragma once

#include <QImage>
#include <QObject>
#include <QProcess>

class QLocalServer;
class QLocalSocket;

class Session : public QObject {
    Q_OBJECT
public:
    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    bool start(const QString &elf, QString *error);
    void stop();
    bool isRunning() const { return m_sock != nullptr; }
    bool tickPending() const { return m_tickPending; }

    void sendTick(quint32 tMs, float hr, float speed, float cadence);
    void sendButton(int button, int event);   // 0 released, 1 pressed, 2 held

    // 240x240 Indexed8: each pixel is the firmware's RGB222 colour index (0..63),
    // so the view can render it as the real display would (lit or reflective).
    const QImage &screen() const { return m_screen; }

signals:
    void booted(const QString &appName, bool round);
    void screenChanged(const QRect &region);
    void logLine(const QString &line);
    void tickDone(quint32 tMs, quint32 frameBytes);
    void backlightChanged(int level);   // 0 off (reflective only), 1..100 on
    void died(const QString &reason);

private:
    void onConnection();
    void onReadyRead();
    void send(quint8 type, const QByteArray &payload = {});
    void handle(quint8 type, const QByteArray &payload);

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_sock = nullptr;
    QProcess *m_qemu = nullptr;
    QByteArray m_rx;
    QImage m_screen;
    bool m_tickPending = false;
    bool m_stopping = false;
};
