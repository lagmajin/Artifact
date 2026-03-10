module;
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <wobjectimpl.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <limits>

module Artifact.Widgets.SoftwareRenderTest;
import Artifact.Render.SoftwareCompositor;

namespace Artifact {

namespace {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec2 {
    float x;
    float y;
};

struct VertexOut {
    Vec2 screen;
    float depth;
};

struct Tri {
    int i0;
    int i1;
    int i2;
    QRgb color;
};

float edgeFunction(const Vec2& a, const Vec2& b, const Vec2& c)
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

} // namespace

W_OBJECT_IMPL(ArtifactSoftwareRenderTestWidget)

class ArtifactSoftwareRenderTestWidget::Impl {
public:
    enum class CompositeBackend {
        QtPainter,
        OpenCV
    };

    enum class BlendMode {
        Normal,
        Add,
        Multiply,
        Screen
    };

    enum class CvEffectMode {
        None,
        GaussianBlur,
        EdgeOverlay
    };

    QTimer* timer = nullptr;
    float angleY = 0.0f;
    float angleX = 0.35f;
    bool solid = true;
    bool showCube = true;
    CompositeBackend backend = CompositeBackend::QtPainter;
    BlendMode blendMode = BlendMode::Normal;
    CvEffectMode cvEffect = CvEffectMode::None;
    float overlayOpacity = 0.75f;
    QPointF overlayOffset = QPointF(0.0, 0.0);
    float overlayScale = 1.0f;
    float overlayRotationDeg = 0.0f;
    QImage backgroundImage;
    QImage overlayImage;
    QString backgroundPath;
    QString overlayPath;

    std::array<Vec3, 8> cube = {{
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
    }};

    std::array<Tri, 12> tris = {{
        {0, 1, 2, qRgb(220, 88, 88)}, {0, 2, 3, qRgb(220, 88, 88)},   // back
        {4, 6, 5, qRgb(88, 180, 255)}, {4, 7, 6, qRgb(88, 180, 255)}, // front
        {0, 4, 5, qRgb(120, 210, 120)}, {0, 5, 1, qRgb(120, 210, 120)}, // bottom
        {3, 2, 6, qRgb(255, 200, 90)}, {3, 6, 7, qRgb(255, 200, 90)}, // top
        {1, 5, 6, qRgb(180, 120, 230)}, {1, 6, 2, qRgb(180, 120, 230)}, // right
        {0, 3, 7, qRgb(100, 220, 220)}, {0, 7, 4, qRgb(100, 220, 220)}  // left
    }};

    Vec3 rotate(const Vec3& v) const
    {
        const float cy = std::cos(angleY);
        const float sy = std::sin(angleY);
        const float cx = std::cos(angleX);
        const float sx = std::sin(angleX);

        Vec3 r{};
        r.x = v.x * cy + v.z * sy;
        r.z = -v.x * sy + v.z * cy;
        const float y = v.y * cx - r.z * sx;
        const float z = v.y * sx + r.z * cx;
        r.y = y;
        r.z = z;
        return r;
    }

    bool project(const Vec3& v, int w, int h, VertexOut* out) const
    {
        const float z = v.z + 4.0f;
        if (z <= 0.01f) {
            return false;
        }

        constexpr float fov = 75.0f;
        const float tanHalf = std::tan((fov * 0.5f) * 3.1415926535f / 180.0f);
        const float aspect = static_cast<float>(w) / static_cast<float>(h);

        const float ndcX = (v.x / (z * tanHalf * aspect));
        const float ndcY = (v.y / (z * tanHalf));

        out->screen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(w - 1);
        out->screen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(h - 1);
        out->depth = z;
        return true;
    }

    void drawWire(QPainter& painter, const VertexOut& a, const VertexOut& b, const VertexOut& c, const QColor& color) const
    {
        painter.setPen(QPen(color, 1.0));
        painter.drawLine(QPointF(a.screen.x, a.screen.y), QPointF(b.screen.x, b.screen.y));
        painter.drawLine(QPointF(b.screen.x, b.screen.y), QPointF(c.screen.x, c.screen.y));
        painter.drawLine(QPointF(c.screen.x, c.screen.y), QPointF(a.screen.x, a.screen.y));
    }

    void rasterizeTriangle(QImage& image, std::vector<float>& zbuf, const VertexOut& v0, const VertexOut& v1, const VertexOut& v2, QRgb color) const
    {
        const int w = image.width();
        const int h = image.height();

        const float minXf = std::min({v0.screen.x, v1.screen.x, v2.screen.x});
        const float maxXf = std::max({v0.screen.x, v1.screen.x, v2.screen.x});
        const float minYf = std::min({v0.screen.y, v1.screen.y, v2.screen.y});
        const float maxYf = std::max({v0.screen.y, v1.screen.y, v2.screen.y});

        const int minX = std::max(0, static_cast<int>(std::floor(minXf)));
        const int maxX = std::min(w - 1, static_cast<int>(std::ceil(maxXf)));
        const int minY = std::max(0, static_cast<int>(std::floor(minYf)));
        const int maxY = std::min(h - 1, static_cast<int>(std::ceil(maxYf)));

        const float area = edgeFunction(v0.screen, v1.screen, v2.screen);
        if (std::abs(area) < 1e-6f) {
            return;
        }

        QRgb* pixels = reinterpret_cast<QRgb*>(image.bits());
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const Vec2 p = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                const float w0 = edgeFunction(v1.screen, v2.screen, p) / area;
                const float w1 = edgeFunction(v2.screen, v0.screen, p) / area;
                const float w2 = edgeFunction(v0.screen, v1.screen, p) / area;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }

                const float depth = w0 * v0.depth + w1 * v1.depth + w2 * v2.depth;
                const int idx = y * w + x;
                if (depth < zbuf[idx]) {
                    zbuf[idx] = depth;
                    pixels[idx] = color;
                }
            }
        }
    }

    static QString blendModeText(BlendMode mode)
    {
        switch (mode) {
        case BlendMode::Normal:   return QStringLiteral("Normal");
        case BlendMode::Add:      return QStringLiteral("Add");
        case BlendMode::Multiply: return QStringLiteral("Multiply");
        case BlendMode::Screen:   return QStringLiteral("Screen");
        default:                  return QStringLiteral("Normal");
        }
    }

    static QString backendText(CompositeBackend backend)
    {
        switch (backend) {
        case CompositeBackend::QtPainter: return QStringLiteral("QImage/QPainter");
        case CompositeBackend::OpenCV:    return QStringLiteral("OpenCV");
        default:                          return QStringLiteral("QImage/QPainter");
        }
    }

    static QString cvEffectText(CvEffectMode mode)
    {
        switch (mode) {
        case CvEffectMode::None:         return QStringLiteral("None");
        case CvEffectMode::GaussianBlur: return QStringLiteral("GaussianBlur");
        case CvEffectMode::EdgeOverlay:  return QStringLiteral("EdgeOverlay");
        default:                         return QStringLiteral("None");
        }
    }

    static QPainter::CompositionMode compositionMode(BlendMode mode)
    {
        switch (mode) {
        case BlendMode::Add:      return QPainter::CompositionMode_Plus;
        case BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
        case BlendMode::Screen:   return QPainter::CompositionMode_Screen;
        case BlendMode::Normal:
        default:
            return QPainter::CompositionMode_SourceOver;
        }
    }

    bool loadBackgroundImage(QWidget* owner)
    {
        const QString path = QFileDialog::getOpenFileName(
            owner,
            QStringLiteral("Load Background Image"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (path.isEmpty()) {
            return false;
        }
        QImage loaded(path);
        if (loaded.isNull()) {
            return false;
        }
        backgroundImage = loaded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        backgroundPath = path;
        return true;
    }

    bool loadOverlayImage(QWidget* owner)
    {
        const QString path = QFileDialog::getOpenFileName(
            owner,
            QStringLiteral("Load Overlay Image"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (path.isEmpty()) {
            return false;
        }
        QImage loaded(path);
        if (loaded.isNull()) {
            return false;
        }
        overlayImage = loaded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        overlayPath = path;
        return true;
    }

    void compositeImages(QImage& target, const QImage& cubeLayer) const
    {
        QPainter painter(&target);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        if (!backgroundImage.isNull()) {
            const QImage bgScaled = backgroundImage.scaled(
                target.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const int x = (target.width() - bgScaled.width()) / 2;
            const int y = (target.height() - bgScaled.height()) / 2;
            painter.drawImage(x, y, bgScaled);
        }

        if (showCube) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(0, 0, cubeLayer);
        }

        if (!overlayImage.isNull()) {
            const QImage ovScaled = overlayImage.scaled(
                target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const int x = (target.width() - ovScaled.width()) / 2;
            const int y = (target.height() - ovScaled.height()) / 2;
            painter.setOpacity(std::clamp(overlayOpacity, 0.0f, 1.0f));
            painter.setCompositionMode(compositionMode(blendMode));
            painter.drawImage(x, y, ovScaled);
            painter.setOpacity(1.0);
        }
    }

    static cv::Mat qImageToMatRGBA(const QImage& image)
    {
        QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
        cv::Mat view(rgba.height(), rgba.width(), CV_8UC4,
                     const_cast<uchar*>(rgba.bits()), rgba.bytesPerLine());
        return view.clone();
    }

    static QImage matRGBAToQImage(const cv::Mat& mat)
    {
        if (mat.empty()) {
            return {};
        }
        cv::Mat rgba;
        if (mat.type() == CV_8UC4) {
            rgba = mat;
        } else if (mat.type() == CV_8UC3) {
            cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
        } else {
            return {};
        }
        QImage image(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888);
        return image.copy();
    }

    static void blendBgrInPlace(cv::Mat& dstBgr, const cv::Mat& srcBgr, float opacity, BlendMode mode)
    {
        const float a = std::clamp(opacity, 0.0f, 1.0f);
        if (a <= 0.0f) {
            return;
        }

        cv::Mat dstF;
        cv::Mat srcF;
        dstBgr.convertTo(dstF, CV_32FC3, 1.0 / 255.0);
        srcBgr.convertTo(srcF, CV_32FC3, 1.0 / 255.0);

        cv::Mat blended = dstF.clone();
        switch (mode) {
        case BlendMode::Normal:
            blended = srcF;
            break;
        case BlendMode::Add:
            cv::add(dstF, srcF, blended);
            cv::min(blended, 1.0f, blended);
            break;
        case BlendMode::Multiply:
            cv::multiply(dstF, srcF, blended);
            break;
        case BlendMode::Screen:
            blended = 1.0f - (1.0f - dstF).mul(1.0f - srcF);
            break;
        }

        cv::Mat mixed = dstF * (1.0f - a) + blended * a;
        mixed.convertTo(dstBgr, CV_8UC3, 255.0);
    }

    void compositeImagesOpenCV(QImage& target, const QImage& cubeLayer) const
    {
        cv::Mat canvasBgr(target.height(), target.width(), CV_8UC3, cv::Scalar(28, 24, 22));

        if (!backgroundImage.isNull()) {
            const QImage bgScaled = backgroundImage.scaled(
                target.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            cv::Mat bgRgba = qImageToMatRGBA(bgScaled);
            cv::Mat bgBgr;
            cv::cvtColor(bgRgba, bgBgr, cv::COLOR_RGBA2BGR);
            if (bgBgr.size() == canvasBgr.size()) {
                bgBgr.copyTo(canvasBgr);
            } else {
                cv::resize(bgBgr, canvasBgr, canvasBgr.size(), 0.0, 0.0, cv::INTER_LINEAR);
            }
        }

        if (showCube) {
            cv::Mat cubeRgba = qImageToMatRGBA(cubeLayer);
            cv::Mat cubeBgr;
            cv::cvtColor(cubeRgba, cubeBgr, cv::COLOR_RGBA2BGR);
            blendBgrInPlace(canvasBgr, cubeBgr, 1.0f, BlendMode::Normal);
        }

        if (!overlayImage.isNull()) {
            const QImage ovScaled = overlayImage.scaled(
                target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QImage overlayCanvas(target.size(), QImage::Format_RGBA8888);
            overlayCanvas.fill(Qt::transparent);
            {
                QPainter p(&overlayCanvas);
                const int x = (target.width() - ovScaled.width()) / 2;
                const int y = (target.height() - ovScaled.height()) / 2;
                p.drawImage(x, y, ovScaled);
            }
            cv::Mat ovRgba = qImageToMatRGBA(overlayCanvas);
            cv::Mat ovBgr;
            cv::cvtColor(ovRgba, ovBgr, cv::COLOR_RGBA2BGR);
            blendBgrInPlace(canvasBgr, ovBgr, overlayOpacity, blendMode);
        }

        if (cvEffect == CvEffectMode::GaussianBlur) {
            cv::GaussianBlur(canvasBgr, canvasBgr, cv::Size(9, 9), 0.0);
        } else if (cvEffect == CvEffectMode::EdgeOverlay) {
            cv::Mat gray;
            cv::cvtColor(canvasBgr, gray, cv::COLOR_BGR2GRAY);
            cv::Mat edges;
            cv::Canny(gray, edges, 80.0, 160.0);
            cv::Mat edgesBgr;
            cv::cvtColor(edges, edgesBgr, cv::COLOR_GRAY2BGR);
            cv::Mat mixed;
            cv::addWeighted(canvasBgr, 0.85, edgesBgr, 0.65, 0.0, mixed);
            canvasBgr = mixed;
        }

        cv::Mat outRgba;
        cv::cvtColor(canvasBgr, outRgba, cv::COLOR_BGR2RGBA);
        target = matRGBAToQImage(outRgba);
    }

    QImage renderFrame(int w, int h) const
    {
        QImage cubeLayer(w, h, QImage::Format_ARGB32_Premultiplied);
        cubeLayer.fill(Qt::transparent);
        std::vector<float> zbuf(static_cast<size_t>(w) * static_cast<size_t>(h), std::numeric_limits<float>::infinity());

        std::array<VertexOut, 8> projected{};
        std::array<bool, 8> valid{};
        for (size_t i = 0; i < cube.size(); ++i) {
            const Vec3 rv = rotate(cube[i]);
            valid[i] = project(rv, w, h, &projected[i]);
        }

        if (solid) {
            for (const auto& t : tris) {
                if (!(valid[t.i0] && valid[t.i1] && valid[t.i2])) {
                    continue;
                }
                rasterizeTriangle(cubeLayer, zbuf, projected[t.i0], projected[t.i1], projected[t.i2], t.color);
            }
        }

        {
            QPainter cubePainter(&cubeLayer);
            for (const auto& t : tris) {
                if (!(valid[t.i0] && valid[t.i1] && valid[t.i2])) {
                    continue;
                }
                drawWire(cubePainter, projected[t.i0], projected[t.i1], projected[t.i2], QColor(230, 230, 235, 180));
            }
        }

        SoftwareRender::CompositeRequest request;
        request.background = backgroundImage;
        request.foreground = cubeLayer;
        request.overlay = overlayImage;
        request.outputSize = QSize(w, h);
        request.overlayOpacity = overlayOpacity;
        request.overlayOffset = overlayOffset;
        request.overlayScale = overlayScale;
        request.overlayRotationDeg = overlayRotationDeg;
        request.useForeground = showCube;

        switch (blendMode) {
        case BlendMode::Normal:   request.blendMode = SoftwareRender::BlendMode::Normal; break;
        case BlendMode::Add:      request.blendMode = SoftwareRender::BlendMode::Add; break;
        case BlendMode::Multiply: request.blendMode = SoftwareRender::BlendMode::Multiply; break;
        case BlendMode::Screen:   request.blendMode = SoftwareRender::BlendMode::Screen; break;
        }
        request.backend = (backend == CompositeBackend::OpenCV)
            ? SoftwareRender::CompositeBackend::OpenCV
            : SoftwareRender::CompositeBackend::QtPainter;
        switch (cvEffect) {
        case CvEffectMode::None:         request.cvEffect = SoftwareRender::CvEffectMode::None; break;
        case CvEffectMode::GaussianBlur: request.cvEffect = SoftwareRender::CvEffectMode::GaussianBlur; break;
        case CvEffectMode::EdgeOverlay:  request.cvEffect = SoftwareRender::CvEffectMode::EdgeOverlay; break;
        }

        return SoftwareRender::compose(request);
    }
};

ArtifactSoftwareRenderTestWidget::ArtifactSoftwareRenderTestWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setWindowTitle(QStringLiteral("Software 3D Render Test"));
    setMinimumSize(640, 400);
    setFocusPolicy(Qt::StrongFocus);

    impl_->timer = new QTimer(this);
    impl_->timer->setInterval(16);
    connect(impl_->timer, &QTimer::timeout, this, [this]() {
        impl_->angleY += 0.018f;
        update();
    });
    impl_->timer->start();
}

ArtifactSoftwareRenderTestWidget::~ArtifactSoftwareRenderTestWidget()
{
    delete impl_;
    impl_ = nullptr;
}

void ArtifactSoftwareRenderTestWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const int w = std::max(1, width());
    const int h = std::max(1, height());

    QImage image = impl_->renderFrame(w, h);

    QPainter painter(this);
    painter.drawImage(0, 0, image);

    painter.setPen(QColor(240, 240, 240));
    painter.drawText(
        QRect(10, 10, w - 20, 44),
        Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("CPU Software Render Test  |  Space: Fill  C: Cube  B: BG  O: Overlay  S: Save PNG"));
    painter.drawText(
        QRect(10, 34, w - 20, 44),
        Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("V: Backend(%1)  E: Effect(%2)  M: Blend(%3)  [ / ]: Opacity(%4%)")
            .arg(Impl::backendText(impl_->backend))
            .arg(Impl::cvEffectText(impl_->cvEffect))
            .arg(Impl::blendModeText(impl_->blendMode))
            .arg(static_cast<int>(std::round(impl_->overlayOpacity * 100.0f))));
    painter.drawText(
        QRect(10, 58, w - 20, 44),
        Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("Arrow: Offset(%1,%2)  -/=: Scale(%3)  R/T: Rot(%4 deg)")
            .arg(static_cast<int>(std::round(impl_->overlayOffset.x())))
            .arg(static_cast<int>(std::round(impl_->overlayOffset.y())))
            .arg(QString::number(impl_->overlayScale, 'f', 2))
            .arg(QString::number(impl_->overlayRotationDeg, 'f', 1)));
    if (!impl_->backgroundPath.isEmpty()) {
        painter.drawText(QRect(10, h - 42, w - 20, 18), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("BG: %1").arg(QFileInfo(impl_->backgroundPath).fileName()));
    }
    if (!impl_->overlayPath.isEmpty()) {
        painter.drawText(QRect(10, h - 22, w - 20, 18), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Overlay: %1").arg(QFileInfo(impl_->overlayPath).fileName()));
    }
}

void ArtifactSoftwareRenderTestWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        impl_->solid = !impl_->solid;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_C) {
        impl_->showCube = !impl_->showCube;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_B) {
        if (impl_->loadBackgroundImage(this)) {
            update();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_O) {
        if (impl_->loadOverlayImage(this)) {
            update();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_M) {
        switch (impl_->blendMode) {
        case Impl::BlendMode::Normal: impl_->blendMode = Impl::BlendMode::Add; break;
        case Impl::BlendMode::Add: impl_->blendMode = Impl::BlendMode::Multiply; break;
        case Impl::BlendMode::Multiply: impl_->blendMode = Impl::BlendMode::Screen; break;
        case Impl::BlendMode::Screen: impl_->blendMode = Impl::BlendMode::Normal; break;
        }
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_V) {
        if (impl_->backend == Impl::CompositeBackend::QtPainter) {
            impl_->backend = Impl::CompositeBackend::OpenCV;
        } else {
            impl_->backend = Impl::CompositeBackend::QtPainter;
        }
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_E) {
        switch (impl_->cvEffect) {
        case Impl::CvEffectMode::None:         impl_->cvEffect = Impl::CvEffectMode::GaussianBlur; break;
        case Impl::CvEffectMode::GaussianBlur: impl_->cvEffect = Impl::CvEffectMode::EdgeOverlay; break;
        case Impl::CvEffectMode::EdgeOverlay:  impl_->cvEffect = Impl::CvEffectMode::None; break;
        }
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_BracketLeft) {
        impl_->overlayOpacity = std::max(0.0f, impl_->overlayOpacity - 0.05f);
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_BracketRight) {
        impl_->overlayOpacity = std::min(1.0f, impl_->overlayOpacity + 0.05f);
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left) {
        impl_->overlayOffset.rx() -= 8.0;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        impl_->overlayOffset.rx() += 8.0;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up) {
        impl_->overlayOffset.ry() -= 8.0;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        impl_->overlayOffset.ry() += 8.0;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        impl_->overlayScale = std::max(0.05f, impl_->overlayScale - 0.05f);
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Equal) {
        impl_->overlayScale = std::min(8.0f, impl_->overlayScale + 0.05f);
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_R) {
        impl_->overlayRotationDeg -= 5.0f;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_T) {
        impl_->overlayRotationDeg += 5.0f;
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_S) {
        const QImage frame = impl_->renderFrame(std::max(1, width()), std::max(1, height()));
        const QString defaultName = QStringLiteral("software_render_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        const QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Composited Frame"),
            defaultName,
            QStringLiteral("PNG Image (*.png)"));
        if (!path.isEmpty()) {
            frame.save(path, "PNG");
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace Artifact
