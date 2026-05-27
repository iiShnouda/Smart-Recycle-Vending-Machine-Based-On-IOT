#include "../include/applicationmanager.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFile>
#include <QDirIterator>
#include <QDebug>

int main(int argc, char *argv[])
{
    // Tell Qt to use the on-screen virtual keyboard as the IME backend.
    // Must be set BEFORE QGuiApplication is created.
    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");

    // Identity used by QSettings + QStandardPaths.
    QGuiApplication::setOrganizationName("ReWinGo");
    QGuiApplication::setOrganizationDomain("rewingo.local");
    QGuiApplication::setApplicationName("ReWinGoKiosk");

    ApplicationManager appManager;
    appManager.initialize();

    QQmlApplicationEngine engine;
    appManager.setQmlEngine(&engine);

    engine.rootContext()->setContextProperty("appManager", &appManager);

    // Find Main.qml in the qrc. qt_add_qml_module's prefix shifted across
    // Qt 6 minor versions depending on the QTP0001 policy:
    //   :/qt/qml/<URI>/qml/Main.qml   ← new (Qt 6.5 with QTP0001 = NEW)
    //   :/<URI>/qml/Main.qml          ← legacy (QTP0001 unset / OLD)
    //   :/qml/Main.qml                ← oldest (manual .qrc)
    // Probe each in order. If none exist, dump the qrc contents so the
    // mismatch is visible at first launch instead of a cryptic crash.
    const QStringList candidates {
        QStringLiteral(":/Recycle_Vending_Machine_LCD/qml/Main.qml"),
        QStringLiteral(":/Recycle_Vending_Machine_LCD/qml/Main.qml"),
        QStringLiteral(":/qml/Main.qml"),
    };
    QString found;
    for (const QString &p : candidates) {
        if (QFile::exists(p)) { found = p; break; }
    }
    if (found.isEmpty()) {
        qCritical().noquote() << "[MAIN] Main.qml not found in qrc. Resource tree:";
        QDirIterator it(QStringLiteral(":/"),
                        QDirIterator::Subdirectories);
        int n = 0;
        while (it.hasNext() && n++ < 200) {
            qCritical().noquote() << "    " << it.next();
        }
        return -1;
    }

    qInfo().noquote() << "[MAIN] Loading entry-point from qrc" << found;
    engine.load(QUrl(QStringLiteral("qrc") + found));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
