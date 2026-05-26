#ifndef OFF_CLIENT_H
#define OFF_CLIENT_H

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QString>

class QNetworkAccessManager;

/**
 * OpenFoodFactsClient — looks up packaged products on world.openfoodfacts.org.
 *
 * Why OFF: it's the only large free product database with no API key,
 * permissive licensing, and ~3M products including most drinks and snacks
 * a vending machine would carry. Each search returns name, brand, image
 * URL, and `product_quantity` (grams), which is exactly what we need to
 * pre-populate a catalog entry.
 *
 * Wire format (response shape we use):
 *   GET /cgi/search.pl?search_terms=<q>&search_simple=1&json=1&page_size=10
 *   → { products: [ { product_name, brands, image_front_url, product_quantity,
 *                     quantity, code (barcode), ... }, ... ] }
 *
 * Network failures fall through quietly — the admin can always fill in
 * fields manually, this is just a convenience.
 *
 * QML usage:
 *     OffClient.search("coca cola 330ml")
 *     Connections {
 *         target: OffClient
 *         function onResults(query, candidates) { ... }
 *     }
 */
class OpenFoodFactsClient : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(OffClient)
    QML_SINGLETON
public:
    explicit OpenFoodFactsClient(QObject *parent = nullptr);
    static OpenFoodFactsClient *create(QQmlEngine *, QJSEngine *) {
        return s_instance;
    }
    static OpenFoodFactsClient *s_instance;

public slots:
    /** Fire an async search. Results land in the `results` signal. */
    Q_INVOKABLE void search(const QString &query, int pageSize = 8);

signals:
    /** Emitted with the parsed list of candidates. Each entry is a
     *  QVariantMap with keys: name, brand, imageUrl, weightG, barcode.
     *  weightG may be 0 if OFF doesn't have a quantity recorded.        */
    void results(const QString &query, const QVariantList &candidates);

    /** Network or parse error. The admin UI just falls back to manual entry. */
    void searchFailed(const QString &query, const QString &reason);

private:
    QNetworkAccessManager *m_net = nullptr;
};

#endif // OFF_CLIENT_H
