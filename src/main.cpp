#include "../include/applicationmanager.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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

    const QUrl url(QStringLiteral("qrc:/qt/qml/Recycle_Vending_Machine_LCD/qml/Main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
