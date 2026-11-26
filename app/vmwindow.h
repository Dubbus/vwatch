// The VM window: watch canvas, sensor controls, console, live reload.
#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QProcess>

class QFileSystemWatcher;
class QLabel;
class QPlainTextEdit;
class QSlider;
class QTimer;
class Session;
class WatchView;

class VmWindow : public QMainWindow {
    Q_OBJECT
public:
    // target: a firmware .elf, or a project directory containing a Makefile.
    explicit VmWindow(const QString &target, QWidget *parent = nullptr);

private:
    void buildUi();
    void boot(const QString &elf);
    void rebuild();
    void watchProject();
    void scheduleReload();
    void onTick();
    void setSpeed(int multiplier);
    void log(const QString &line, const QColor &color = QColor());
    void updateStatus();
    QString newestElf() const;

    QString m_project;      // project dir (live rebuild), or empty
    QString m_elf;          // firmware being run
    Session *m_session;
    WatchView *m_view;
    QPlainTextEdit *m_console;
    QSlider *m_hr, *m_speed;
    QLabel *m_hrLabel, *m_speedLabel, *m_status;
    QTimer *m_ticker, *m_debounce;
    QFileSystemWatcher *m_watcher;
    QProcess *m_make = nullptr;
    QString m_appName = "-";
    quint32 m_tMs = 0, m_lastBytes = 0;
    int m_mult = 1;
    bool m_paused = false;
    QElapsedTimer m_tickSent;
};
