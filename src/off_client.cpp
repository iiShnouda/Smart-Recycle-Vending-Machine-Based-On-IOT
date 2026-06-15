#include "../include/off_client.h"
#include "../include/logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

OpenFoodFactsClient *OpenFoodFactsClient::s_instance = nullptr;

OpenFoodFactsClient::OpenFoodFactsClient(QObject *parent) : QObject(parent)
{
    if (!s_instance) s_instance = this;
    m_net = new QNetworkAccessManager(this);
}

void OpenFoodFactsClient::search(const QString &query, int pageSize)
{
    if (query.trimmed().isEmpty()) {
        emit results(query, {});
        return;
    }

    // Build the search URL. We hit the "simple" search endpoint which is
    // fastest and returns the fields we want. `json=1` switches the
    // response from HTML to JSON. Page-size 8 keeps the candidate dialog
    // manageable and the network response under 200 KB.
    QUrl url("https://world.openfoodfacts.org/cgi/search.pl");
    QUrlQuery q;
    q.addQueryItem("search_terms",  query);
    q.addQueryItem("search_simple", "1");
    q.addQueryItem("action",        "process");
    q.addQueryItem("json",          "1");
    q.addQueryItem("page_size",     QString::number(pageSize));
    // Only fetch the fields we actually use — cuts payload by ~10x.
    q.addQueryItem("fields",
        "code,product_name,brands,image_front_small_url,image_front_url,"
        "product_quantity,quantity");
    url.setQuery(q);

    QNetworkRequest req(url);
    // OFF requests a UA string so they can attribute traffic; this is
    // courtesy, not auth. See https://wiki.openfoodfacts.org/API
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "ReWinGo-Kiosk/1.0 (https://github.com/)");
    auto *reply = m_net->get(req);

    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, query]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::warn("OFF", "Search failed",
                         { {"query", query}, {"err", reply->errorString()} });
            emit searchFailed(query, reply->errorString());
            return;
        }

        const QByteArray body = reply->readAll();
        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            emit searchFailed(query, "bad json");
            return;
        }

        QVariantList list;
        const QJsonArray products = doc.object().value("products").toArray();
        for (const QJsonValue &pv : products) {
            const QJsonObject p = pv.toObject();

            // Pull the per-product fields. OFF is messy — `product_quantity`
            // is sometimes a number, sometimes a string, sometimes missing.
            // We do our best.
            int weightG = 0;
            const QJsonValue qv = p.value("product_quantity");
            if (qv.isDouble())      weightG = qRound(qv.toDouble());
            else if (qv.isString()) weightG = qv.toString().toInt();

            QVariantMap r;
            r["name"]     = p.value("product_name").toString();
            r["brand"]    = p.value("brands").toString();
            r["barcode"]  = p.value("code").toString();
            r["weightG"]  = weightG;
            // Prefer the full-resolution image, fall back to the thumb.
            const QString hires = p.value("image_front_url").toString();
            r["imageUrl"] = hires.isEmpty()
                          ? p.value("image_front_small_url").toString()
                          : hires;
            // Skip entries with no name — those are useless to the admin.
            if (r["name"].toString().isEmpty()) continue;
            list.append(r);
        }
        Logger::info("OFF", "Search OK",
                     { {"query", query}, {"hits", list.size()} });
        emit results(query, list);
    });
}

void OpenFoodFactsClient::lookupBarcode(const QString &barcode)
{
    const QString code = barcode.trimmed();
    if (code.isEmpty()) {
        emit productResolved(code, false, { {"barcode", code} });
        return;
    }

    // OFF product endpoint: /api/v0/product/<barcode>.json → { status, product }
    QUrl url("https://world.openfoodfacts.org/api/v0/product/" + code + ".json");
    QUrlQuery q;
    q.addQueryItem("fields",
        "code,product_name,brands,image_front_url,image_front_small_url,"
        "product_quantity,quantity");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "ReWinGo-Kiosk/1.0 (https://github.com/)");
    auto *reply = m_net->get(req);

    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, code]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::warn("OFF", "Barcode lookup failed",
                         { {"barcode", code}, {"err", reply->errorString()} });
            emit productResolved(code, false, { {"barcode", code} });
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject root = doc.object();
        // status 1 = found, 0 = not found.
        if (root.value("status").toInt() != 1) {
            emit productResolved(code, false, { {"barcode", code} });
            return;
        }
        const QJsonObject p = root.value("product").toObject();
        int weightG = 0;
        const QJsonValue qv = p.value("product_quantity");
        if (qv.isDouble())      weightG = qRound(qv.toDouble());
        else if (qv.isString()) weightG = qv.toString().toInt();

        const QString hires = p.value("image_front_url").toString();
        QVariantMap r;
        r["name"]     = p.value("product_name").toString();
        r["brand"]    = p.value("brands").toString();
        r["barcode"]  = code;
        r["weightG"]  = weightG;
        r["imageUrl"] = hires.isEmpty()
                      ? p.value("image_front_small_url").toString() : hires;
        Logger::info("OFF", "Barcode resolved",
                     { {"barcode", code}, {"name", r["name"].toString()} });
        emit productResolved(code, !r["name"].toString().isEmpty(), r);
    });
}
