#include <QTemporaryDir>
#include <QTest>

#include <GZip.h>
#include <random>

void fib(int& prev, int& cur)
{
    auto ret = prev + cur;
    prev = cur;
    cur = ret;
}

class GZipTest : public QObject {
    Q_OBJECT
   private slots:

    void test_Through()
    {
        // test up to 10 MB
        static const int size = 10 * 1024 * 1024;
        QByteArray random;
        QByteArray compressed;
        QByteArray decompressed;
        std::default_random_engine eng((std::random_device())());
        std::uniform_int_distribution<uint16_t> idis(0, std::numeric_limits<uint8_t>::max());

        // initialize random buffer
        for (int i = 0; i < size; i++) {
            random.append(static_cast<char>(idis(eng)));
        }

        // initialize fibonacci
        int prev = 1;
        int cur = 1;

        // test if fibonacci long random buffers pass through GZip
        do {
            QByteArray copy = random;
            copy.resize(cur);
            compressed.clear();
            decompressed.clear();
            QVERIFY(GZip::zip(copy, compressed));
            QVERIFY(GZip::unzip(compressed, decompressed));
            QCOMPARE(decompressed, copy);
            fib(prev, cur);
        } while (cur < size);
    }

    void test_readByLines()
    {
        // The file is read 16 KiB at a time. The first line puts a character that takes three bytes across the end of the first
        // of those blocks, and the ones after it have characters that take two or more bytes all along.
        QStringList lines{ QString(16382, 'a') + QStringLiteral("\u2714") };
        for (int i = 0; i < 2000; i++) {
            lines << QStringLiteral("[12:00:00] [Render thread/INFO]: Hall\u00e5 \u00e9t\u00e9 \u65e5\u672c %1 \u2718").arg(i);
        }
        // the last line has no line break
        lines << QStringLiteral("Stopping \u00e9");

        QByteArray compressed;
        QVERIFY(GZip::zip(lines.join('\n').toUtf8(), compressed));
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath("latest.log.gz"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(compressed), compressed.size());
        file.close();

        QStringList read;
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(GZip::readGzFileByLines(&file,
                                         [&read](const QString& line) {
                                             read << line;
                                             return true;
                                         }),
                 QString());
        QCOMPARE(read, lines);

        // and it stops when told to
        read.clear();
        QVERIFY(file.seek(0));
        QCOMPARE(GZip::readGzFileByLines(&file,
                                         [&read](const QString& line) {
                                             read << line;
                                             return read.size() < 10;
                                         }),
                 QString());
        QCOMPARE(read, lines.first(10));
    }
};

QTEST_GUILESS_MAIN(GZipTest)

#include "GZip_test.moc"
