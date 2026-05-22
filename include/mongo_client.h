#ifndef MONGO_CLIENT_H
#define MONGO_CLIENT_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>

/**
 * MongoClient — MongoDB Atlas Data API REST wrapper.
 *
 *  Atlas Data API endpoints look like:
 *    POST https://data.mongodb-api.com/app/<APP_ID>/endpoint/data/v1/action/find
 *    POST /action/insertOne, /action/updateOne, /action/deleteOne ...
 *
 *  Auth: a static "api-key" header (no JWT needed for kiosk apps).
 *  Body : JSON describing dataSource / database / collection / operation.
 *
 *  Usage:
 *    mongo.configure("https://data.mongodb-api.com/app/data-xxxx/endpoint/data/v1",
 *                    "<api-key>", "Cluster0", "rewingo");
 *    mongo.insertOne("transactions",
 *                    { {"kind","recycle"}, {"amount",10} }, cb);
 *    mongo.find("products", QJsonObject{ {"active",true} }, cb);
 */
class MongoClient : public QObject {
    Q_OBJECT
public:
    using JsonObjectCallback = std::function<void(bool ok, const QJsonObject &)>;
    using JsonArrayCallback  = std::function<void(bool ok, const QJsonArray  &)>;
    using BoolCallback       = std::function<void(bool ok)>;

    explicit MongoClient(QObject *parent = nullptr);

    /** Set the project endpoint URL + Data API key + cluster + database. */
    void configure(const QString &endpointUrl,
                   const QString &apiKey,
                   const QString &dataSource,
                   const QString &database);

    bool isConfigured() const { return !m_url.isEmpty() && !m_apiKey.isEmpty(); }

    // ---- CRUD ----
    void find(const QString &collection,
              const QJsonObject &filter,
              JsonArrayCallback cb);

    void findOne(const QString &collection,
                 const QJsonObject &filter,
                 JsonObjectCallback cb);

    void insertOne(const QString &collection,
                   const QJsonObject &document,
                   JsonObjectCallback cb);

    void insertMany(const QString &collection,
                    const QJsonArray &documents,
                    BoolCallback cb);

    void updateOne(const QString &collection,
                   const QJsonObject &filter,
                   const QJsonObject &update,
                   BoolCallback cb);

    void deleteOne(const QString &collection,
                   const QJsonObject &filter,
                   BoolCallback cb);

signals:
    void error(const QString &message);

private:
    QJsonObject baseBody(const QString &collection) const;
    void post(const QString &action, const QJsonObject &body,
              std::function<void(bool, const QByteArray &)> cb);

    QNetworkAccessManager m_net;
    QString m_url;
    QString m_apiKey;
    QString m_dataSource;
    QString m_database;
};

#endif // MONGO_CLIENT_H
