#include "../include/yolo_runner.h"
#include "../include/logger.h"

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#ifdef HAVE_OPENCV
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#endif

YoloRunner::YoloRunner(QObject *parent) : QObject(parent) {}
YoloRunner::~YoloRunner()
{
#ifdef HAVE_OPENCV
    delete static_cast<cv::dnn::Net *>(m_impl);
#endif
    m_impl = nullptr;
}

bool YoloRunner::loadModel(const QString &onnxPath, const QString &labelsPath,
                           int inputWidth, int inputHeight)
{
    m_inputW = inputWidth;
    m_inputH = inputHeight;
    m_classNames.clear();

    // ---- labels ----
    QFile lf(labelsPath);
    if (lf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream s(&lf);
        while (!s.atEnd()) {
            const QString line = s.readLine().trimmed();
            if (!line.isEmpty()) m_classNames << line;
        }
    } else {
        Logger::warn("Yolo", "Labels file missing", { {"path", labelsPath} });
    }

#ifdef HAVE_OPENCV
    try {
        auto *net = new cv::dnn::Net(
            cv::dnn::readNetFromONNX(onnxPath.toStdString()));
        // Prefer CPU; switch to CUDA/Vulkan if available on the deploy hardware.
        net->setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net->setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_impl   = net;
        m_loaded = true;
        Logger::info("Yolo", "Model loaded",
                     { {"path", onnxPath}, {"classes", m_classNames.size()} });
        return true;
    } catch (const std::exception &e) {
        Logger::error("Yolo", "Model load failed",
                      { {"path", onnxPath}, {"err", QString::fromUtf8(e.what())} });
        return false;
    }
#else
    Logger::warn("Yolo", "OpenCV not compiled in — using mock detections",
                 { {"path", onnxPath} });
    m_loaded = true;     // mock mode still "works"
    return true;
#endif
}

QVector<YoloDetection> YoloRunner::detect(const QImage &frame,
                                          float confThreshold,
                                          float nmsThreshold)
{
    QVector<YoloDetection> out;
    if (!m_loaded) return out;

#ifdef HAVE_OPENCV
    auto *net = static_cast<cv::dnn::Net *>(m_impl);
    if (!net) return out;

    // Qt → OpenCV image (BGR, packed)
    const QImage rgb = frame.convertToFormat(QImage::Format_RGB888);
    cv::Mat src(rgb.height(), rgb.width(), CV_8UC3,
                const_cast<uchar *>(rgb.constBits()),
                static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr; cv::cvtColor(src, bgr, cv::COLOR_RGB2BGR);

    cv::Mat blob = cv::dnn::blobFromImage(
        bgr, 1.0/255.0, cv::Size(m_inputW, m_inputH),
        cv::Scalar(), true, false);
    net->setInput(blob);

    std::vector<cv::Mat> outs;
    net->forward(outs, net->getUnconnectedOutLayersNames());

    // YOLOv5/8 raw output post-processing:
    //   each row = [cx, cy, w, h, obj, class_scores...]
    std::vector<int>    classIds;
    std::vector<float>  confidences;
    std::vector<cv::Rect> boxes;
    const float xScale = float(bgr.cols) / m_inputW;
    const float yScale = float(bgr.rows) / m_inputH;

    for (const auto &output : outs) {
        const auto rows = output.size[1];
        const auto cols = output.size[2];
        const float *data = (float *)output.data;
        for (int r = 0; r < rows; ++r) {
            const float *row = data + r * cols;
            const float obj = row[4];
            if (obj < confThreshold) continue;
            // class scoring (cols - 5 classes after the 4 bbox + 1 obj)
            int   bestClass = 0;
            float bestScore = 0.0f;
            for (int c = 5; c < cols; ++c) {
                if (row[c] > bestScore) { bestScore = row[c]; bestClass = c - 5; }
            }
            const float conf = obj * bestScore;
            if (conf < confThreshold) continue;
            const float cx = row[0] * xScale;
            const float cy = row[1] * yScale;
            const float w  = row[2] * xScale;
            const float h  = row[3] * yScale;
            boxes.emplace_back(int(cx - w/2), int(cy - h/2), int(w), int(h));
            confidences.push_back(conf);
            classIds.push_back(bestClass);
        }
    }

    // Non-max suppression
    std::vector<int> kept;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, kept);
    for (int i : kept) {
        YoloDetection d;
        d.classId    = classIds[i];
        d.className  = (classIds[i] >= 0 && classIds[i] < m_classNames.size())
                       ? m_classNames[classIds[i]]
                       : QString("class_%1").arg(classIds[i]);
        d.confidence = confidences[i];
        d.box        = QRectF(boxes[i].x, boxes[i].y, boxes[i].width, boxes[i].height);
        out.append(d);
    }
#else
    // Mock: pretend we found one face/bottle in the centre at 95% conf.
    YoloDetection d;
    d.classId    = 0;
    d.className  = m_classNames.value(0, "mock");
    d.confidence = 0.95f;
    d.box        = QRectF(frame.width() * 0.3, frame.height() * 0.25,
                          frame.width() * 0.4, frame.height() * 0.5);
    out.append(d);
#endif

    std::sort(out.begin(), out.end(),
              [](auto &a, auto &b){ return a.confidence > b.confidence; });
    return out;
}

QVector<float> YoloRunner::extractFaceEmbedding(const QImage &frame)
{
    // Real implementation: after detecting the face, crop it, run a
    // dedicated embedding model (ArcFace / FaceNet) on the crop, return
    // the 512-D L2-normalised vector.
    //
    // Stub: deterministic pseudo-embedding seeded by image content so
    // the registration flow can be tested end-to-end.
    QVector<float> emb(512, 0.0f);
    if (frame.isNull()) return emb;
    const QImage tiny = frame.scaled(8, 8, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                            .convertToFormat(QImage::Format_Grayscale8);
    for (int y = 0; y < tiny.height(); ++y) {
        for (int x = 0; x < tiny.width(); ++x) {
            const int idx = (y * tiny.width() + x) % 512;
            emb[idx] += tiny.constScanLine(y)[x] / 255.0f;
        }
    }
    // L2 normalise
    float n = 0; for (float v : emb) n += v*v;
    n = std::sqrt(n > 0 ? n : 1);
    for (float &v : emb) v /= n;
    return emb;
}

float YoloRunner::similarity(const QVector<float> &a, const QVector<float> &b)
{
    if (a.size() != b.size() || a.isEmpty()) return 0.0f;
    float dot = 0;
    for (int i = 0; i < a.size(); ++i) dot += a[i] * b[i];
    // Assuming both vectors are already L2-normalised → cosine = dot
    return std::max(0.0f, std::min(1.0f, dot));
}
