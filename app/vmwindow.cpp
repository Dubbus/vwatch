#include "vmwindow.h"

#include "session.h"
#include "traceplot.h"
#include "watchview.h"

#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QTextStream>
#include <QVBoxLayout>

#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

static const char *const kButtonNames[] = {"Light", "Up", "Down", "Start", "Back"};

VmWindow::VmWindow(const QString &target, QWidget *parent)
    : QMainWindow(parent), m_session(new Session(this)), m_ticker(new QTimer(this)),
      m_debounce(new QTimer(this)), m_watcher(new QFileSystemWatcher(this)) {
    buildUi();

    connect(m_session, &Session::booted, this, [this](const QString &name, bool round) {
        m_appName = name;
        m_view->setRound(round);
        m_view->setOverlay({});
        log(QString("booted %1").arg(name), QColor(60, 180, 75));
        updateStatus();
    });
    connect(m_session, &Session::screenChanged, m_view, [this](const QRect &) { m_view->update(); });
    connect(m_session, &Session::backlightChanged, m_view, &WatchView::setBacklight);
    connect(m_session, &Session::logLine, this, [this](const QString &l) {
        log(l);
        // A firmware "reports" a metric by logging e.g. "te 3.22 ...". Plot it
        // against the activity's own value when replaying a run.
        static const QRegularExpression re(R"(\bte\s+([0-9]+\.?[0-9]*))");
        const auto m = re.match(l);
        if (m.hasMatch()) {
            m_liveValue = m.captured(1).toDouble();
            if (m_runActive) m_plot->push(m_liveValue);
            updateGroundTruth();
        }
    });
    connect(m_session, &Session::tickDone, this, [this](quint32, quint32 bytes) {
        m_lastBytes = bytes;
        updateStatus();
    });
    connect(m_session, &Session::died, this, [this](const QString &why) {
        log("watch stopped: " + why, QColor(240, 80, 70));
        m_view->setOverlay("Stopped\n" + why);
    });

    m_ticker->setTimerType(Qt::PreciseTimer);
    connect(m_ticker, &QTimer::timeout, this, &VmWindow::onTick);
    setSpeed(1);

    m_debounce->setSingleShot(true);
    m_debounce->setInterval(250);
    connect(m_debounce, &QTimer::timeout, this, [this] { m_project.isEmpty() ? boot(m_elf) : rebuild(); });

    const QFileInfo fi(target);
    if (fi.isDir()) {
        m_project = fi.absoluteFilePath();
        setWindowTitle(QString("%1 [Running] - vwatch").arg(fi.fileName()));
        watchProject();
        rebuild();
    } else {
        m_elf = fi.absoluteFilePath();
        setWindowTitle(QString("%1 [Running] - vwatch").arg(fi.completeBaseName()));
        m_watcher->addPath(m_elf);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &VmWindow::scheduleReload);
        boot(m_elf);
    }
    updateStatus();
}

void VmWindow::buildUi() {
    m_view = new WatchView(this);
    m_view->setScreen(&m_session->screen());
    setCentralWidget(m_view);
    connect(m_view, &WatchView::button, this, [this](int id, int ev) {
        m_session->sendButton(id, ev);
        if (ev == 1) log(QString("[input] %1").arg(kButtonNames[id]), Qt::gray);
        if (ev == 2) log(QString("[input] %1 (held)").arg(kButtonNames[id]), Qt::gray);
    });

    // ---- menus (VirtualBox-style) ----
    QMenu *file = menuBar()->addMenu("&File");
    file->addAction("&Open Firmware...", QKeySequence::Open, this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, "Open firmware", {}, "Firmware (*.elf)");
        if (p.isEmpty()) return;
        m_project.clear();
        m_watcher->removePaths(m_watcher->files() + m_watcher->directories());
        m_elf = p;
        m_watcher->addPath(m_elf);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &VmWindow::scheduleReload, Qt::UniqueConnection);
        boot(m_elf);
    });
    file->addAction("Open &Activity...", QKeySequence("Ctrl+Shift+O"), this, [this] {
        const QString p = QFileDialog::getOpenFileName(this, "Open activity", {}, "vwatch run (*.vwr)");
        if (!p.isEmpty()) openActivity(p);
    });
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu *machine = menuBar()->addMenu("&Machine");
    QAction *pause = machine->addAction("&Pause");
    pause->setCheckable(true);
    pause->setShortcut(QKeySequence("Ctrl+P"));
    connect(pause, &QAction::toggled, this, [this](bool on) { m_paused = on; updateStatus(); });
    machine->addAction("&Reset", QKeySequence("Ctrl+R"), this, [this] {
        m_tMs = 0;
        m_project.isEmpty() ? boot(m_elf) : rebuild();
    });
    machine->addAction("Re&build", QKeySequence("Ctrl+B"), this, [this] { if (!m_project.isEmpty()) rebuild(); });
    QMenu *speed = machine->addMenu("&Speed");
    auto *speeds = new QActionGroup(this);
    for (int m : {1, 10, 60, 300}) {
        QAction *a = speed->addAction(QString("%1x").arg(m));
        a->setCheckable(true);
        a->setChecked(m == 1);
        speeds->addAction(a);
        connect(a, &QAction::triggered, this, [this, m] { setSpeed(m); });
    }

    QMenu *input = menuBar()->addMenu("&Input");
    const char *keys[] = {"L", "Up", "Down", "Return", "Esc"};
    for (int i = 0; i < 5; ++i) {
        QAction *a = input->addAction(QString("Press %1  (%2)").arg(kButtonNames[i], keys[i]));
        connect(a, &QAction::triggered, this, [this, i] {
            m_session->sendButton(i, 1);
            m_session->sendButton(i, 0);
        });
        QAction *h = input->addAction(QString("Hold %1").arg(kButtonNames[i]));
        connect(h, &QAction::triggered, this, [this, i] {
            m_session->sendButton(i, 1);
            m_session->sendButton(i, 2);
            m_session->sendButton(i, 0);
        });
    }

    QMenu *view = menuBar()->addMenu("&View");
    QAction *mip = view->addAction("MIP Display Realism");
    mip->setCheckable(true);
    mip->setChecked(true);
    connect(mip, &QAction::toggled, m_view, &WatchView::setRealism);
    view->addSeparator();
    QMenu *help = menuBar()->addMenu("&Help");
    help->addAction("&About vwatch", this, [this] {
        QMessageBox::about(this, "About vwatch",
                           "vwatch: a virtual machine for smartwatch firmware.\n"
                           "Cortex-M4 (QEMU mps2-an386), 240x240 round display.\n\n"
                           "Edit your firmware in any editor; vwatch rebuilds on save.");
    });

    // ---- sensors toolbar ----
    auto *tb = addToolBar("Sensors");
    tb->setMovable(false);
    auto *box = new QWidget;
    auto *row = new QHBoxLayout(box);
    row->setContentsMargins(6, 0, 6, 0);
    m_hr = new QSlider(Qt::Horizontal);
    m_hr->setRange(0, 210);
    m_hr->setValue(72);
    m_hr->setFixedWidth(160);
    m_hrLabel = new QLabel;
    m_speed = new QSlider(Qt::Horizontal);
    m_speed->setRange(0, 70);                 // 0.0 .. 7.0 m/s
    m_speed->setValue(0);
    m_speed->setFixedWidth(120);
    m_speedLabel = new QLabel;
    row->addWidget(new QLabel("Heart rate"));
    row->addWidget(m_hr);
    row->addWidget(m_hrLabel);
    row->addSpacing(16);
    row->addWidget(new QLabel("Speed"));
    row->addWidget(m_speed);
    row->addWidget(m_speedLabel);
    row->addStretch();
    tb->addWidget(box);
    auto sync = [this] {
        m_hrLabel->setText(m_hr->value() ? QString("%1 bpm").arg(m_hr->value()) : "off");
        m_speedLabel->setText(QString("%1 m/s").arg(m_speed->value() / 10.0, 0, 'f', 1));
    };
    connect(m_hr, &QSlider::valueChanged, this, sync);
    connect(m_speed, &QSlider::valueChanged, this, sync);
    sync();

    // ---- serial console dock ----
    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(2000);
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto *dock = new QDockWidget("Serial Console", this);
    dock->setWidget(m_console);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    view->addAction(dock->toggleViewAction());
    view->addAction(tb->toggleViewAction());

    setupGroundTruth();

    m_status = new QLabel;
    statusBar()->addWidget(m_status, 1);
    resize(980, 860);
}

void VmWindow::setupGroundTruth() {
    auto *panel = new QWidget;
    auto *col = new QVBoxLayout(panel);
    auto *form = new QGroupBox("Activity");
    auto *fl = new QFormLayout(form);
    m_gtName = new QLabel("No activity loaded");
    m_gtName->setWordWrap(true);
    m_gtSport = new QLabel("-");
    m_gtTruth = new QLabel("-");
    m_gtLive = new QLabel("-");
    m_gtDelta = new QLabel("-");
    QFont big = m_gtLive->font();
    big.setPointSizeF(big.pointSizeF() * 1.4);
    big.setBold(true);
    m_gtLive->setFont(big);
    fl->addRow(m_gtName);
    fl->addRow("Sport", m_gtSport);
    fl->addRow("Watch TE", m_gtTruth);
    fl->addRow("Your firmware", m_gtLive);
    fl->addRow("Difference", m_gtDelta);
    col->addWidget(form);
    m_plot = new TracePlot;
    col->addWidget(new QLabel("Your output (orange) vs the watch (dashed)"));
    col->addWidget(m_plot, 1);
    auto *hint = new QLabel("File > Open Activity plays a recorded run through your "
                            "firmware. Export one with host/fit_to_vwr.py.");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray;");
    col->addWidget(hint);

    auto *dock = new QDockWidget("Ground Truth", this);
    dock->setWidget(panel);
    dock->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    m_view->addAction(dock->toggleViewAction());
}

void VmWindow::log(const QString &line, const QColor &color) {
    if (color.isValid())
        m_console->appendHtml(QString("<span style='color:%1'>%2</span>").arg(color.name(), line.toHtmlEscaped()));
    else
        m_console->appendPlainText(line);
}

void VmWindow::setSpeed(int multiplier) {
    m_mult = multiplier;
    m_ticker->start(50);                 // fixed 20 fps; the accumulator sets how many sim-seconds advance
    updateStatus();
}

void VmWindow::boot(const QString &elf) {
    if (!QFileInfo::exists(elf)) {
        log("firmware not found: " + elf, QColor(240, 80, 70));
        return;
    }
    QString err;
    m_view->setOverlay("Booting...");
    if (!m_session->start(elf, &err)) {
        log(err, QColor(240, 80, 70));
        m_view->setOverlay("Cannot start\n" + err);
        return;
    }
    log(QString("--- boot %1 ---").arg(QFileInfo(elf).fileName()), QColor(90, 150, 255));
    m_tickSent.invalidate();
}

void VmWindow::onTick() {
    if (m_paused || !m_session->isRunning()) return;
    if (m_session->tickPending()) {
        if (m_tickSent.isValid() && m_tickSent.elapsed() > 3000) {   // watchdog
            log("watchdog: firmware didn't finish a tick in 3 s (infinite loop or fault?)", QColor(240, 80, 70));
            m_view->setOverlay("Not responding\n(fix the code and save to reload)");
            m_session->stop();
        }
        return;   // lockstep: never queue ticks behind a slow app_tick()
    }
    m_simAccum += m_mult * 0.05;         // 50 ms ticker -> m_mult sim-seconds per wall second
    if (m_simAccum < 1.0) return;        // advance exactly one sim-second each send (keeps the model 1 Hz)
    m_simAccum -= 1.0;

    if (m_runActive) {
        if (m_runPos >= m_run.size()) {
            stopReplay();
            return;
        }
        const RunSample &s = m_run[m_runPos++];
        m_tMs = (s.tS + 1) * 1000;
        m_session->sendTick(m_tMs, s.hr, s.speed, s.cadence);
        m_tickSent.start();
        return;
    }
    m_tMs += 1000;
    m_session->sendTick(m_tMs, float(m_hr->value()), m_speed->value() / 10.0f, m_speed->value() ? 165.0f : 0.0f);
    m_tickSent.start();
}

void VmWindow::openActivity(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log("cannot open " + path, QColor(240, 80, 70));
        return;
    }
    QVector<RunSample> run;
    double truth = 0;
    QString name = QFileInfo(path).fileName(), sport;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line[0].isLetter()) {                             // header key value
            const int sp = line.indexOf(' ');
            if (sp < 0) continue;
            const QString k = line.left(sp), v = line.mid(sp + 1);
            if (k == "name") name = v;
            else if (k == "sport") sport = v;
            else if (k == "garmin_te") truth = v.toDouble();
        } else {                                              // t hr speed cadence
            const QStringList c = line.split(' ', Qt::SkipEmptyParts);
            if (c.size() >= 4)
                run.append({c[0].toUInt(), c[1].toFloat(), c[2].toFloat(), c[3].toFloat()});
        }
    }
    if (run.isEmpty()) {
        log("no samples in " + path, QColor(240, 80, 70));
        return;
    }
    m_run = run;
    m_runPos = 0;
    m_runActive = true;
    m_runTruth = truth;
    m_runName = name;
    m_runSport = sport.isEmpty() ? "unknown" : sport;
    m_liveValue = 0;
    m_tMs = 0;
    m_simAccum = 0;
    m_hr->setEnabled(false);
    m_speed->setEnabled(false);
    m_plot->clear();
    m_plot->setTruth(truth);
    log(QString("--- replay %1 (%2 s, watch TE %3) ---").arg(name).arg(run.size()).arg(truth, 0, 'f', 1),
        QColor(90, 150, 255));
    updateGroundTruth();
    m_project.isEmpty() ? boot(m_elf) : rebuild();            // fresh boot for the run
}

void VmWindow::stopReplay() {
    if (!m_runActive) return;
    m_runActive = false;
    m_hr->setEnabled(true);
    m_speed->setEnabled(true);
    const double d = m_liveValue - m_runTruth;
    log(QString("replay done: your firmware %1, watch %2, diff %3%4")
            .arg(m_liveValue, 0, 'f', 2).arg(m_runTruth, 0, 'f', 1)
            .arg(d >= 0 ? "+" : "").arg(d, 0, 'f', 2),
        QColor(60, 180, 75));
    updateGroundTruth();
}

void VmWindow::updateGroundTruth() {
    if (!m_gtName) return;
    if (m_run.isEmpty()) {
        m_gtName->setText("No activity loaded");
        return;
    }
    m_gtName->setText(m_runName);
    m_gtSport->setText(m_runSport);
    m_gtTruth->setText(m_runTruth > 0 ? QString::number(m_runTruth, 'f', 1) : "n/a");
    m_gtLive->setText(QString::number(m_liveValue, 'f', 2));
    const double d = m_liveValue - m_runTruth;
    m_gtDelta->setText(m_runTruth > 0 ? QString("%1%2").arg(d >= 0 ? "+" : "").arg(d, 0, 'f', 2) : "-");
    m_gtDelta->setStyleSheet(qAbs(d) <= 0.3 ? "color: #3cb44b;" : "color: #e07038;");
}

void VmWindow::updateStatus() {
    const quint32 s = m_tMs / 1000;
    m_status->setText(QString("%1   |   t = %2:%3:%4   |   %5x%6   |   last redraw %7 B   |   %8")
                          .arg(m_appName)
                          .arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'))
                          .arg(m_mult).arg(m_paused ? " (paused)" : "")
                          .arg(m_lastBytes)
                          .arg(m_runActive
                                   ? QString("replay %1 / %2 s").arg(m_runPos).arg(m_run.size())
                                   : m_project.isEmpty() ? "watching " + QFileInfo(m_elf).fileName()
                                                         : "live reload: " + QDir(m_project).dirName()));
}

// ---------- live reload ----------

void VmWindow::watchProject() {
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &VmWindow::scheduleReload, Qt::UniqueConnection);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &VmWindow::scheduleReload, Qt::UniqueConnection);
    // Watch the project's sources (and directories, to catch new files and
    // editors that save by replacing the file).
    QStringList paths{m_project};
    QDirIterator it(m_project, {"*.c", "*.h", "Makefile", "*.mk"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString p = it.next();
        if (!p.contains("/build/")) paths << p << QFileInfo(p).absolutePath();
    }
    paths.removeDuplicates();
    const QStringList have = m_watcher->files() + m_watcher->directories();
    QStringList add;
    for (const QString &p : paths)
        if (!have.contains(p)) add << p;
    if (!add.isEmpty()) m_watcher->addPaths(add);
}

void VmWindow::scheduleReload() {
    if (!m_project.isEmpty()) watchProject();   // re-arm paths replaced by atomic saves
    else if (!m_watcher->files().contains(m_elf) && QFileInfo::exists(m_elf)) m_watcher->addPath(m_elf);
    m_debounce->start();
}

void VmWindow::rebuild() {
    if (m_make && m_make->state() != QProcess::NotRunning) {   // a save during a build: build again after
        m_debounce->start();
        return;
    }
    delete m_make;
    m_make = new QProcess(this);
    m_make->setWorkingDirectory(m_project);
    m_make->setProcessChannelMode(QProcess::MergedChannels);
    m_view->setOverlay("Building...");
    log("$ make", QColor(90, 150, 255));
    connect(m_make, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString out = QString::fromLocal8Bit(m_make->readAll()).trimmed();
        for (const QString &l : out.split('\n', Qt::SkipEmptyParts)) {
            const bool bad = l.contains("error") || l.contains("Error");
            const bool warn = !bad && l.contains("warning");
            log(l, bad ? QColor(240, 80, 70) : warn ? QColor(230, 160, 40) : QColor());
        }
        if (code != 0) {
            log("build failed: still running the previous firmware", QColor(240, 80, 70));
            m_view->setOverlay(m_session->isRunning() ? QString() : "Build failed\nsee the console");
            return;
        }
        m_elf = newestElf();
        if (m_elf.isEmpty()) {
            log("build succeeded but no .elf found in build/", QColor(240, 80, 70));
            return;
        }
        boot(m_elf);
    });
    m_make->start("make", {});
}

QString VmWindow::newestElf() const {
    QFileInfo best;
    QDirIterator it(m_project + "/build", {"*.elf"}, QDir::Files);
    while (it.hasNext()) {
        const QFileInfo fi(it.next());
        if (!best.exists() || fi.lastModified() > best.lastModified()) best = fi;
    }
    return best.exists() ? best.absoluteFilePath() : QString();
}
