// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Timeless Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <minecraft/auth/MinecraftAccount.h>
#include <minecraft/skins/SkinList.h>

namespace {

/// The list brings itself up to date when the folder changes. The test asks for it instead of waiting for the file system.
struct OpenList : SkinList {
    using SkinList::SkinList;
    using SkinList::update;
};

/// The images in a folder, whatever they are called
QStringList imagesIn(const QString& folder)
{
    QStringList images;
    for (const auto& name : QDir(folder).entryList(QDir::Files)) {
        const auto suffix = QFileInfo(name).suffix().toLower();
        if (suffix == "png" || suffix == "bmp" || suffix == "jpg") {
            images << name;
        }
    }
    return images;
}

}  // namespace

class SkinListTest : public QObject {
    Q_OBJECT

   private slots:
    void installSkin_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("format");
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");
        QTest::addColumn<bool>("installed");

        QTest::newRow("a skin") << "skin.png" << "PNG" << 64 << 64 << true;
        QTest::newRow("a skin of the old shape") << "old.png" << "PNG" << 64 << 32 << true;
        // a file picker on Windows takes SKIN.PNG for a PNG, and so does the player
        QTest::newRow("a skin with its suffix in capitals") << "SKIN.PNG" << "PNG" << 64 << 64 << true;
        QTest::newRow("a picture") << "photo.png" << "PNG" << 512 << 512 << false;
        QTest::newRow("a texture for another size") << "hd.png" << "PNG" << 128 << 128 << false;
        QTest::newRow("a skin that is no PNG") << "skin.bmp" << "BMP" << 64 << 64 << false;
    }

    void installSkin()
    {
        QFETCH(const QString, fileName);
        QFETCH(const QString, format);
        QFETCH(const int, width);
        QFETCH(const int, height);
        QFETCH(const bool, installed);

        QTemporaryDir from;
        QTemporaryDir skins;
        QVERIFY(from.isValid() && skins.isValid());

        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::darkGreen);
        const auto path = from.filePath(fileName);
        QVERIFY(image.save(path, qPrintable(format)));

        OpenList list(nullptr, skins.path(), MinecraftAccount::createOffline("Steve"));
        QCOMPARE(list.rowCount(), 0);

        const auto message = list.installSkin(path);
        QCOMPARE(message.isEmpty(), installed);

        // what is not a skin is not copied into the folder either, and what is one is on the list
        QCOMPARE(imagesIn(skins.path()).size(), installed ? 1 : 0);
        list.update();
        QCOMPARE(list.rowCount(), installed ? 1 : 0);
    }

    void aFileThatIsNoImage()
    {
        QTemporaryDir from;
        QTemporaryDir skins;
        QVERIFY(from.isValid() && skins.isValid());

        const auto path = from.filePath("notes.png");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a picture");
        file.close();

        OpenList list(nullptr, skins.path(), MinecraftAccount::createOffline("Steve"));
        QVERIFY(!list.installSkin(path).isEmpty());
        QCOMPARE(imagesIn(skins.path()).size(), 0);
    }

    void safeFileName_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");

        QTest::newRow("a name") << "steve.png" << "steve.png";
        QTest::newRow("a name with spaces inside") << "my skin.png" << "my skin.png";
        QTest::newRow("non-ASCII letters") << "çağan ığdır.png" << "çağan ığdır.png";
        QTest::newRow("nothing") << "" << "skin";
        QTest::newRow("only dots") << ".." << "skin";
        QTest::newRow("only spaces") << "   " << "skin";
        QTest::newRow("a path up") << "../../evil.png" << "-..-evil.png";
        QTest::newRow("a path with backslashes") << "..\\..\\evil.png" << "-..-evil.png";
        QTest::newRow("an absolute path") << "/etc/passwd" << "-etc-passwd";
        QTest::newRow("a drive") << "C:\\Windows\\x.png" << "C--Windows-x.png";
        QTest::newRow("what Windows refuses") << "a*b?c\"d<e>f|g.png" << "a-b-c-d-e-f-g.png";
        QTest::newRow("a control character") << "a\tb\nc.png" << "a-b-c.png";
        QTest::newRow("dots and spaces at the ends") << " .skin.png. " << "skin.png";
    }

    void safeFileName()
    {
        QFETCH(const QString, text);
        QFETCH(const QString, expected);

        const auto name = SkinList::safeFileName(text);
        QCOMPARE(name, expected);
        // whatever it is, it is a name and not a path
        QCOMPARE(QFileInfo(name).fileName(), name);
        QVERIFY(name != "." && name != "..");
    }

    void aNameWithAPathStaysInTheFolder()
    {
        QTemporaryDir from;
        QTemporaryDir skins;
        QVERIFY(from.isValid() && skins.isValid());
        QDir(skins.path()).mkdir("inner");

        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::darkGreen);
        const auto path = from.filePath("skin.png");
        QVERIFY(image.save(path, "PNG"));

        OpenList list(nullptr, QDir(skins.path()).filePath("inner"), MinecraftAccount::createOffline("Steve"));
        QVERIFY(list.installSkin(path, "../escaped.png").isEmpty());

        QCOMPARE(imagesIn(skins.path()).size(), 0);
        QCOMPARE(imagesIn(QDir(skins.path()).filePath("inner")).size(), 1);
    }
};

QTEST_GUILESS_MAIN(SkinListTest)

#include "SkinList_test.moc"
