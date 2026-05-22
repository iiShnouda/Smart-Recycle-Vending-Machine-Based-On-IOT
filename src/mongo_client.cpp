#include "../include/mongo_client.h"
#include "../include/logger.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>

MongoClient::MongoClient(QObject *parent) : QObject(parent) {}

void MongoClient::configure(const QString &endpointUrl,
                            const QString &apiKey,
                            const QString &dataSource,
                            const QString &database)
{
    m_url        = endpointUrl;
    if (m_url.endsWith('/')) m_url.chop(1);
    m_apiKey     = apiKey;
    m_dataSource = dataSource;
    m_database   = database;
}

QJsonObject MongoClient::baseBody(const QString &collection) const
{
    return {
        { "dataSource", m_dataSource },
        { "database",   m_database   },
        { "collection", collection   },
    };
}

void MongoClient::post(const QString &action, const QJsonObject &body,
                       std::function<void(bool, const QByteArray &)> cb)
{
    if (!isConfigured()) {
        Logger::warn("Mongo", "Not configured");
        if (cb) cb(false, {});
        return;
    }
    // Our local Python backend uses /<action> directly (not /action/<action>).
    // If you point this client at Atlas Data API (legacy), change the URL
    // to "/action/" + action.
    QNetworkRequest req(QUrl(m_url + "/" + action));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("Accept",    "application/json");

    QNetworkReply *r = m_net.post(req, QJsonDocument(body).toJson());
    connect(r, &QNetworkReply::finished, this, [r, cb, action, this]() {
        r->deleteLater();
        const QByteArray data = r->readAll();
        const bool ok = (r->error() == QNetworkReply::NoError);
        if (!ok) {
            Logger::warn("Mongo", "Request failed",
                         QVariantMap{ {"action", action},
                                      {"err",    r->errorString()} });
            emit error(r->errorString());
        }
        if (cb) cb(ok, data);
    });
}

void MongoClient::find(const QString &collection,
                       const QJsonObject &filter,
                       JsonArrayCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["filter"] = filter;
    post("find", body, [cb](bool ok, const QByteArray &data) {
        const auto doc = QJsonDocument::fromJson(data);
        if (cb) cb(ok, doc.object().value("documents").toArray());
    });
}

void MongoClient::findOne(const QString &collection,
                          const QJsonObject &filter,
                          JsonObjectCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["filter"] = filter;
    post("findOne", body, [cb](bool ok, const QByteArray &data) {
        const auto doc = QJsonDocument::fromJson(data);
        if (cb) cb(ok, doc.object().value("document").toObject());
    });
}

void MongoClient::insertOne(const QString &collection,
                            const QJsonObject &document,
                            JsonObjectCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["document"] = document;
    post("insertOne", body, [cb](bool ok, const QByteArray &data) {
        if (cb) cb(ok, QJsonDocument::fromJson(data).object());
    });
}

void MongoClient::insertMany(const QString &collection,
                             const QJsonArray &documents,
                             BoolCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["documents"] = documents;
    post("insertMany", body, [cb](bool ok, const QByteArray &) {
        if (cb) cb(ok);
    });
}

void MongoClient::updateOne(const QString &collection,
                            const QJsonObject &filter,
                            const QJsonObject &update,
                            BoolCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["filter"] = filter;
    body["update"] = update;     // e.g. { "$set": { ... } }
    post("updateOne", body, [cb](bool ok, const QByteArray &) {
        if (cb) cb(ok);
    });
}

void MongoClient::deleteOne(const QString &collection,
                            const QJsonObject &filter,
                            BoolCallback cb)
{
    QJsonObject body = baseBody(collection);
    body["filter"] = filter;
    post("deleteOne", body, [cb](bool ok, const QByteArray &) {
        if (cb) cb(ok);
    });
}
