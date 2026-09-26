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

#include <QTest>

#include <modplatform/flame/PackManifest.h>

class FlameManifestTest : public QObject {
    Q_OBJECT

    static QMap<int, Flame::File> filesWithIds(const QList<int>& ids)
    {
        QMap<int, Flame::File> files;
        for (auto id : ids) {
            Flame::File file;
            file.fileId = id;
            file.projectId = id + 1000;
            files.insert(id, file);
        }
        return files;
    }

   private slots:
    void dropUnchangedFiles_data()
    {
        QTest::addColumn<QList<int>>("newIds");
        QTest::addColumn<QList<int>>("oldIds");
        QTest::addColumn<QList<int>>("toDownload");
        QTest::addColumn<QList<int>>("toRemove");

        // Erasing the only entry used to step past the end of the map, which never terminated.
        QTest::newRow("one file, unchanged") << QList<int>{ 1 } << QList<int>{ 1 } << QList<int>{} << QList<int>{};
        // Erasing the first entry used to skip the one after it, so it was downloaded and deleted again.
        QTest::newRow("all unchanged") << QList<int>{ 1, 2, 3 } << QList<int>{ 1, 2, 3 } << QList<int>{} << QList<int>{};
        QTest::newRow("one file replaced") << QList<int>{ 1, 2, 4 } << QList<int>{ 1, 2, 3 } << QList<int>{ 4 } << QList<int>{ 3 };
        QTest::newRow("first file replaced") << QList<int>{ 2, 3, 4 } << QList<int>{ 1, 2, 3 } << QList<int>{ 4 } << QList<int>{ 1 };
        QTest::newRow("added and removed") << QList<int>{ 1, 5 } << QList<int>{ 1, 2 } << QList<int>{ 5 } << QList<int>{ 2 };
        QTest::newRow("nothing installed yet") << QList<int>{ 1, 2 } << QList<int>{} << QList<int>{ 1, 2 } << QList<int>{};
    }

    void dropUnchangedFiles()
    {
        QFETCH(QList<int>, newIds);
        QFETCH(QList<int>, oldIds);
        QFETCH(QList<int>, toDownload);
        QFETCH(QList<int>, toRemove);

        auto files = filesWithIds(newIds);
        auto oldFiles = filesWithIds(oldIds);

        Flame::dropUnchangedFiles(files, oldFiles);

        QCOMPARE(files.keys(), toDownload);
        QCOMPARE(oldFiles.keys(), toRemove);
    }
};

QTEST_GUILESS_MAIN(FlameManifestTest)

#include "FlameManifest_test.moc"
