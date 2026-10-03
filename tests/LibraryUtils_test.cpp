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

#include <QFileInfo>
#include <QRegularExpression>
#include <QTest>

#include <LibraryUtils.h>

class LibraryUtilsTest : public QObject {
    Q_OBJECT

   private slots:
    void findsLibrariesThatAreThere_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("the C library") << "libc.so.6";
        QTest::newRow("the math library") << "libm.so.6";
    }

    void findsLibrariesThatAreThere()
    {
#ifdef __GLIBC__
        QFETCH(const QString, name);

        const auto path = LibraryUtils::find(name);
        QVERIFY(!path.isEmpty());
        QCOMPARE(QFileInfo(path).fileName(), name);
        QVERIFY(QFileInfo::exists(path));
#else
        QSKIP("Libraries are only looked for with glibc");
#endif
    }

    /// The library is named in the message of why it was not found
    void namesTheLibraryThatIsNotThere_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("short") << "libx.so";
        QTest::newRow("the name of a library of the game") << "libglfw.so";
        QTest::newRow("long") << "libthere-is-no-such-library-for-the-launcher-to-find-in-this-test.so.1";
    }

    void namesTheLibraryThatIsNotThere()
    {
#ifdef __GLIBC__
        QFETCH(const QString, name);

        QTest::ignoreMessage(QtCriticalMsg, QRegularExpression("^dlopen\\(\\) failed: " + QRegularExpression::escape(name) + ": "));
        QVERIFY(LibraryUtils::find(name).isEmpty());
#else
        QSKIP("Libraries are only looked for with glibc");
#endif
    }
};

QTEST_GUILESS_MAIN(LibraryUtilsTest)

#include "LibraryUtils_test.moc"
