#include "session.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <cstring>

extern "C" {
#include "vw_proto.h"
}

static constexpr int VW_SCREEN_W = 240;   // matches the SDK's VW_W/VW_H
static constexpr int VW_SCREEN_H = 240;

Session::Session(QObject *parent) : QObject(parent), m_screen(VW_SCREEN_W, VW_SCREEN_H, QImage::Format_RGB32) {
    m_screen.fill(Qt::black);
}

Session::~Session() { stop(); }

bool Session::start(const QString &elf, QString *error) {
    stop();
    m_stopping = false;
    m_rx.clear();
    m_tickPending = false;

    const QString qemu = QStandardPaths::findExecutable("qemu-system-arm");
    if (qemu.isEmpty()) {
        *error = "qemu-system-arm not found on PATH (brew install qemu)";
        return false;
    }

    static int counter = 0;
    const QString sockPath = QDir::temp().filePath(
        QString("vwatch-%1-%2.sock").arg(QCoreApplication::applicationPid()).arg(++counter));
    QLocalServer::removeServer(sockPath);
    m_server = new QLocalServer(this);
    if (!m_server->listen(sockPath)) {
        *error = "cannot listen on " + sockPath + ": " + m_server->errorString();
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &Session::onConnection);

    m_qemu = new QProcess(this);
    m_qemu->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_qemu, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (!m_stopping) {
            const QString out = QString::fromLocal8Bit(m_qemu->readAll()).trimmed();
            emit died(QString("emulator exited (code %1)%2").arg(code).arg(out.isEmpty() ? "" : ": " + out));
        }
    });
    m_qemu->start(qemu, {"-machine", "mps2-an386", "-display", "none", "-monitor", "none",
                         "-serial", "unix:" + sockPath, "-kernel", elf});
    if (!m_qemu->waitForStarted(3000)) {
        *error = "failed to start qemu: " + m_qemu->errorString();
        return false;
    }
    return true;
}

void Session::stop() {
    m_stopping = true;
    if (m_sock) {
        m_sock->disconnect(this);
        m_sock->abort();
        m_sock->deleteLater();
        m_sock = nullptr;
    }
    if (m_qemu) {
        m_qemu->disconnect(this);
        m_qemu->kill();
        m_qemu->waitForFinished(1000);
        m_qemu->deleteLater();
        m_qemu = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_tickPending = false;
}

void Session::onConnection() {
    m_sock = m_server->nextPendingConnection();
    connect(m_sock, &QLocalSocket::readyRead, this, &Session::onReadyRead);
    connect(m_sock, &QLocalSocket::disconnected, this, [this] {
        if (!m_stopping) emit died("firmware disconnected");
    });
    send(VW_MSG_HELLO);
}

void Session::send(quint8 type, const QByteArray &payload) {
    if (!m_sock) return;
    QByteArray frame;
    frame.reserve(5 + payload.size());
    frame.append(char(VW_SYNC0)).append(char(VW_SYNC1)).append(char(type));
    frame.append(char(payload.size() & 0xFF)).append(char((payload.size() >> 8) & 0xFF));
    frame.append(payload);
    m_sock->write(frame);
}

void Session::sendTick(quint32 tMs, float hr, float speed, float cadence) {
    if (!m_sock || m_tickPending) return;
    vw_msg_tick_t t{tMs, hr, speed, cadence};
    m_tickPending = true;
    send(VW_MSG_TICK, QByteArray(reinterpret_cast<const char *>(&t), sizeof t));
}

void Session::sendButton(int button, bool pressed) {
    vw_msg_button_t b{quint8(button), quint8(pressed ? 1 : 0)};
    send(VW_MSG_BUTTON, QByteArray(reinterpret_cast<const char *>(&b), sizeof b));
}

void Session::onReadyRead() {
    m_rx.append(m_sock->readAll());
    for (;;) {
        const int sync = m_rx.indexOf(QByteArray("\xA5\x5A", 2));
        if (sync < 0) {                                  // keep a trailing 0xA5 in case it's a split SYNC
            m_rx = m_rx.endsWith(char(VW_SYNC0)) ? QByteArray(1, char(VW_SYNC0)) : QByteArray();
            return;
        }
        if (sync > 0) m_rx.remove(0, sync);
        if (m_rx.size() < 5) return;
        const quint8 type = quint8(m_rx[2]);
        const int len = quint8(m_rx[3]) | (quint8(m_rx[4]) << 8);
        if (len > VW_MAX_PAYLOAD) { m_rx.remove(0, 2); continue; }   // corrupt header: resync
        if (m_rx.size() < 5 + len) return;
        const QByteArray payload = m_rx.mid(5, len);
        m_rx.remove(0, 5 + len);
        handle(type, payload);
    }
}

void Session::handle(quint8 type, const QByteArray &p) {
    switch (type) {
    case VW_MSG_INFO: {
        if (p.size() < int(sizeof(vw_msg_info_t))) return;
        vw_msg_info_t info;
        std::memcpy(&info, p.constData(), sizeof info);
        const QString name = QString::fromUtf8(info.app_name, int(strnlen(info.app_name, sizeof info.app_name)));
        emit booted(name, info.round != 0);
        break;
    }
    case VW_MSG_RECT: {
        if (p.size() < int(sizeof(vw_msg_rect_t))) return;
        vw_msg_rect_t r;
        std::memcpy(&r, p.constData(), sizeof r);
        if (r.x + r.w > VW_SCREEN_W || r.y + r.h > VW_SCREEN_H) return;
        if (p.size() < int(sizeof r) + r.w * r.h) return;
        const auto *px = reinterpret_cast<const quint8 *>(p.constData() + sizeof r);
        for (int y = 0; y < r.h; ++y) {
            auto *line = reinterpret_cast<QRgb *>(m_screen.scanLine(r.y + y)) + r.x;
            for (int x = 0; x < r.w; ++x) {
                const quint8 c = px[y * r.w + x];
                line[x] = qRgb(((c >> 4) & 3) * 85, ((c >> 2) & 3) * 85, (c & 3) * 85);
            }
        }
        emit screenChanged(QRect(r.x, r.y, r.w, r.h));
        break;
    }
    case VW_MSG_LOG:
        emit logLine(QString::fromUtf8(p));
        break;
    case VW_MSG_TICK_DONE: {
        vw_msg_tick_done_t d{};
        if (p.size() >= int(sizeof d)) std::memcpy(&d, p.constData(), sizeof d);
        m_tickPending = false;
        emit tickDone(d.t_ms, d.frame_bytes);
        break;
    }
    default:
        break;
    }
}
