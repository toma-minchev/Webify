#include "Converter.h"
#include <QFileInfo>
#include <vips/vips8>

using namespace vips;

bool convertOne(const QString &srcPath, const QString &dstPath, const int outW, const int outH, const bool toWebp, const int webpQuality) {
    try {
        const VImage image = VImage::new_from_file(srcPath.toUtf8().constData(), VImage::option()->set("access", "sequential"));

        const double hscale = static_cast<double>(outW) / image.width();
        const double vscale = static_cast<double>(outH) / image.height();
        const VImage resized = image.resize(std::min(hscale, vscale));

        const QFileInfo fi(srcPath);
        const QString outSuffix = toWebp ? "webp" : fi.suffix().toLower();
        const QString outFile = dstPath + "." + outSuffix;

        if (toWebp) {
            resized.write_to_file(outFile.toUtf8().constData(), VImage::option()->set("Q", webpQuality));
        } else {
            resized.write_to_file(outFile.toUtf8().constData());
        }

        return true;
    } catch (const VError &e) {
        qWarning("libvips error: %s", e.what());
        return false;
    }
}