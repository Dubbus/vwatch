// The VM window: watch canvas, sensor controls, console, live reload.
#pragma once

#include <QElapsedTimer>
#include <QMainWindow>
#include <QProcess>

#include <QVector>

class QFileSystemWatcher;
class QLabel;
class QPlainTextEdit;
class QSlider;
class QTimer;
class Session;
class WatchView;
class TracePlot;

// One second of a recorded activity replayed through the firmware.
struct RunSample { quint32 tS; float hr, speed, cadence; };

class VmWindow : public QMainWindow {
    Q_OBJECT
public:
    // target: a firmware .elf, or a project directory containing a Makefile.
    explicit VmWindow(const QString &target, QWidget *parent = nullptr);

    // Load a .vwr activity and start replaying it through the firmware.
    void playActivity(const QString &vwr) { openActivity(vwr); }

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
    void setupGroundTruth();
    void openActivity(const QString &path);
    void stopReplay();
    void updateGroundTruth();

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
    double m_simAccum = 0;     // sim-seconds owed, so playback speed and the 1 Hz model are independent
    bool m_paused = false;
    QElapsedTimer m_tickSent;

    // activity replay (ground truth beside the firmware's live output)
    QVector<RunSample> m_run;
    int m_runPos = 0;
    bool m_runActive = false;
    double m_runTruth = 0, m_liveValue = 0;
    QString m_runName, m_runSport;
    QLabel *m_gtName, *m_gtSport, *m_gtTruth, *m_gtLive, *m_gtDelta;
    TracePlot *m_plot = nullptr;
};
