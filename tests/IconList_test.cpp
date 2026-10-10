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

#include <icons/IconList.h>

class IconListTest : public QObject {
    Q_OBJECT

   private slots:
    void installIcon_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("fileName");

        QTest::newRow("a plain name") << "Flame_Pack_Icon.png" << "Flame_Pack_Icon.png";
        // the names of packs have these in them: ": " is in "All The Mods 9: Expert"
        QTest::newRow("a colon") << "Flame_All The Mods 9: Expert_Icon.png" << "Flame_All The Mods 9- Expert_Icon.png";
        QTest::newRow("a slash") << "Modrinth_Create/Update_Icon.png" << "Modrinth_Create-Update_Icon.png";
        QTest::newRow("a backslash") << "Modrinth_A\\B_Icon.png" << "Modrinth_A-B_Icon.png";
        QTest::newRow("a question mark") << "Flame_What?_Icon.png" << "Flame_What-_Icon.png";
    }

    void installIcon()
    {
        // The copy went to a folder that isn't there, and nothing said that the icon wasn't installed: the pack was given the key of an
        // icon that doesn't exist.
        QFETCH(QString, name);
        QFETCH(QString, fileName);

        QTemporaryDir from;
        QTemporaryDir icons;
        QVERIFY(from.isValid() && icons.isValid());
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::darkGreen);
        const auto source = from.filePath("logo.png");
        QVERIFY(image.save(source, "PNG"));

        IconList list({}, icons.path());
        QVERIFY(list.installIcon(source, name));

        QCOMPARE(QDir(icons.path()).entryList(QDir::Files), QStringList{ fileName });
        QVERIFY(QDir(icons.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void installIconSaysWhenItDidNotCopy()
    {
        QTemporaryDir from;
        QTemporaryDir icons;
        QVERIFY(from.isValid() && icons.isValid());
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::darkGreen);
        const auto source = from.filePath("logo.png");
        QVERIFY(image.save(source, "PNG"));
        QFile notAnIcon(from.filePath("notes.txt"));
        QVERIFY(notAnIcon.open(QIODevice::WriteOnly));
        notAnIcon.close();

        IconList list({}, icons.path());
        QVERIFY(list.installIcon(source, "one.png"));
        // the same name again: the file is there already
        QVERIFY(!list.installIcon(source, "one.png"));
        QVERIFY(!list.installIcon(from.filePath("missing.png"), "two.png"));
        QVERIFY(!list.installIcon(notAnIcon.fileName(), "three.png"));
        QCOMPARE(QDir(icons.path()).entryList(QDir::Files), QStringList{ "one.png" });
    }
};

QTEST_GUILESS_MAIN(IconListTest)

#include "IconList_test.moc"
