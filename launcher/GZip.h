#pragma once
#include <QByteArray>
#include <QFile>

#include <functional>

namespace GZip {

bool unzip(const QByteArray& compressedBytes, QByteArray& uncompressedBytes);
bool zip(const QByteArray& uncompressedBytes, QByteArray& compressedBytes);
QString readGzFileByBlocks(QFile* source, std::function<bool(const QByteArray&)> handleBlock);
/// Reads a gzipped UTF-8 text file line by line, without the line breaks, for as long as handleLine returns true. Returns an
/// error message if the file can't be read.
QString readGzFileByLines(QFile* source, const std::function<bool(const QString&)>& handleLine);

}  // namespace GZip
