#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QObject>
#include <QTranslator>
#include <QString>

class TranslationManager : public QObject {
    Q_OBJECT

public:
    explicit TranslationManager(QObject *parent = nullptr);

    bool loadTranslation(const QString &languageCode);
    QString currentLanguage() const { return m_currentLanguage; }
    QString getTranslationPath(const QString &languageCode) const;

signals:
    void translationLoaded(const QString &languageCode);
    void translationFailed(const QString &reason);

private:
    QTranslator *m_translator = nullptr;
    QString m_currentLanguage = "en";

};

#endif // TRANSLATIONMANAGER_H

