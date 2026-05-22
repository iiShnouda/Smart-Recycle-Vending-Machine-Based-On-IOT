#ifndef YOLO_RUNNER_H
#define YOLO_RUNNER_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QVector>
#include <QQmlEngine>

/**
 * YoloRunner — generic wrapper around a YOLO ONNX model.
 *
 * Two instances run side-by-side in this project:
 *   - "face"    → returns face bounding boxes + 512-D embedding per face
 *   - "recycle" → returns class labels {plastic_bottle, aluminum_can}
 *
 * Backend is OpenCV's cv::dnn (works on the Pi 5 NPU when built with NN-RT,
 * otherwise CPU). The actual cv::dnn calls live in yolo_runner.cpp inside
 * #ifdef HAVE_OPENCV blocks — until you `apt install libopencv-dev` and
 * re-cmake with -DUSE_OPENCV=ON, the class returns mock detections so
 * the QML flow still works end-to-end.
 */
struct YoloDetection {
    int     classId;        // YOLO class index (0..nClasses-1)
    QString className;      // mapped via the .names labels file
    float   confidence;     // 0..1
    QRectF  box;            // in image pixel coordinates
    QVector<float> embedding;  // optional — only for face model
};

class YoloRunner : public QObject {
    Q_OBJECT
public:
    explicit YoloRunner(QObject *parent = nullptr);
    ~YoloRunner() override;

    /** Load an ONNX model + labels file (one class per line). */
    bool loadModel(const QString &onnxPath, const QString &labelsPath,
                   int inputWidth = 640, int inputHeight = 640);

    /** Run inference. Returns 0 or more detections, sorted by confidence. */
    QVector<YoloDetection> detect(const QImage &frame,
                                  float confThreshold = 0.5f,
                                  float nmsThreshold  = 0.45f);

    /** Specifically for face models: extract a 512-D embedding for the
     *  highest-confidence face. Returns empty vector if no face found. */
    QVector<float> extractFaceEmbedding(const QImage &frame);

    /** Convenience: cosine similarity between two embeddings (0..1).
     *  ≥0.6 is typically "same person". */
    static float similarity(const QVector<float> &a,
                            const QVector<float> &b);

    bool isLoaded() const { return m_loaded; }

private:
    bool        m_loaded = false;
    QStringList m_classNames;
    int         m_inputW = 640;
    int         m_inputH = 640;

    // Opaque pimpl pointer — holds cv::dnn::Net when OpenCV is available.
    void *m_impl = nullptr;
};

#endif // YOLO_RUNNER_H
