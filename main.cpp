#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Messgeraete"));
    app.setApplicationName(QStringLiteral("PeakTech2275"));
    app.setApplicationDisplayName(QStringLiteral("PeakTech 2275"));
    app.setApplicationVersion(QStringLiteral(PEAKTECH_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/device-icon.ico")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    auto font = app.font();
    font.setFamily(QFontDatabase::hasFamily(QStringLiteral("Inter")) ?
                   QStringLiteral("Inter") : QStringLiteral("Segoe UI"));
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    QSettings settings;
    const auto geometryKey = QStringLiteral("window/geometry");
    window.restoreGeometry(settings.value(geometryKey).toByteArray());
    window.show();

    const int exitCode = app.exec();
    settings.setValue(geometryKey, window.saveGeometry());
    return exitCode;
}
