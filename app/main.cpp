// vwatch: a virtual machine for smartwatch firmware.
//   vwatch <project-dir>     live reload: rebuild with make on save, reboot
//   vwatch <firmware.elf>    run an ELF, reboot when it changes
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>

#include "vmwindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("vwatch");

    QCommandLineParser cli;
    cli.setApplicationDescription("A virtual machine for smartwatch firmware.");
    cli.addHelpOption();
    cli.addPositionalArgument("target", "Project directory (with a Makefile) or firmware .elf", "<target>");
    QCommandLineOption shot("screenshot", "Save a PNG of the window after --after seconds, then quit.", "png");
    QCommandLineOption after("after", "Seconds before --screenshot (default 5).", "s", "5");
    cli.addOption(shot);
    cli.addOption(after);
    cli.process(app);

    const QStringList args = cli.positionalArguments();
    const QString target = args.isEmpty() ? QString("examples/hello_face") : args.first();
    if (!QFileInfo::exists(target)) {
        qCritical("vwatch: %s does not exist", qPrintable(target));
        return 1;
    }
    VmWindow w(target);
    w.show();
    if (cli.isSet(shot)) {
        const QString path = cli.value(shot);
        QTimer::singleShot(int(cli.value(after).toDouble() * 1000), &app, [&w, path] {
            if (!w.grab().save(path)) qCritical("vwatch: cannot write %s", qPrintable(path));
            QApplication::quit();
        });
    }
    return app.exec();
}
