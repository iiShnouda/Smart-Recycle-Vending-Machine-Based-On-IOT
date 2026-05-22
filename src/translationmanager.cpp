#include "../include/translationmanager.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QDebug>

TranslationManager::TranslationManager(QObject *parent)
    : QObject(parent)
{
}

bool TranslationManager::loadTranslation(const QString &languageCode)
{
    if (languageCode == m_currentLanguage){
        return true;
    }

    const QString path = getTranslationPath(languageCode);

    qDebug() << "[TRANS] Request:" << languageCode << " path:" << path;

    if (path.isEmpty()) {
        emit translationFailed(QString("Unknown language: %1").arg(languageCode));
        return false;
    }

    // Load new translator FIRST
    auto *newTr = new QTranslator(this);
    if (!newTr->load(path)) {
        newTr->deleteLater();
        emit translationFailed(QString("Failed to load: %1").arg(path));
        return false;
    }

    // Swap only after success
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator);
        m_translator->deleteLater();
    }

    QCoreApplication::installTranslator(newTr);
    m_translator = newTr;
    m_currentLanguage = languageCode;

    // Locale + layout direction
    if (languageCode == "ar") {
        QLocale::setDefault(QLocale("ar_EG"));
        QGuiApplication::setLayoutDirection(Qt::RightToLeft);
    } else {
        QLocale::setDefault(QLocale("en_US"));
        QGuiApplication::setLayoutDirection(Qt::LeftToRight);
    }


    emit translationLoaded(languageCode);
    return true;
}

QString TranslationManager::getTranslationPath(const QString &languageCode) const
{
    if (languageCode == "en") {
        return ":/qt/qml/Recycle_Vending_Machine_LCD/resources/translations/Recycle_Vending_Machine_LCD_en_US.qm";
    }
    if (languageCode == "ar") {
        return ":/qt/qml/Recycle_Vending_Machine_LCD/resources/translations/Recycle_Vending_Machine_LCD_ar_EG.qm";
    }

    return {};
}

