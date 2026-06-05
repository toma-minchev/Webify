#ifndef WEBIFY_CONVERTER_H
#define WEBIFY_CONVERTER_H


#include <QString>


// Convert one image with VIPS
bool convertOne(const QString &srcPath, const QString &dstPath, int outW, int outH, bool toWebp, int webpQuality);


#endif //WEBIFY_CONVERTER_H
