
// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Timeless Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <FileSystem.h>

#include <minecraft/mod/ShaderPack.h>
#include <minecraft/mod/tasks/LocalShaderPackParseTask.h>

class ShaderPackParseTest : public QObject {
    Q_OBJECT

   private slots:
    void test_parseZIP()
    {
        QString source = QFINDTESTDATA("testdata/ShaderPackParse");

        QString zip_sp = FS::PathCombine(source, "shaderpack1.zip");
        ShaderPack pack{ QFileInfo(zip_sp) };

        bool valid = ShaderPackUtils::processZIP(pack);

        QVERIFY(pack.packFormat() == ShaderPackFormat::VALID);
        QVERIFY(valid == true);
    }

    void test_parseFolder()
    {
        QString source = QFINDTESTDATA("testdata/ShaderPackParse");

        QString folder_sp = FS::PathCombine(source, "shaderpack2");
        ShaderPack pack{ QFileInfo(folder_sp) };

        bool valid = ShaderPackUtils::processFolder(pack);

        QVERIFY(pack.packFormat() == ShaderPackFormat::VALID);
        QVERIFY(valid == true);
    }

    void test_parseZIP2()
    {
        QString source = QFINDTESTDATA("testdata/ShaderPackParse");

        QString folder_sp = FS::PathCombine(source, "shaderpack3.zip");
        ShaderPack pack{ QFileInfo(folder_sp) };

        bool valid = ShaderPackUtils::process(pack);

        QVERIFY(pack.packFormat() == ShaderPackFormat::INVALID);
        QVERIFY(valid == false);
    }

    void cutShortZipIsStillListed()
    {
        // A pack whose download stopped right after its shaders folder. It is still listed, rather than left out as if it weren't
        // a shader pack, as it was before reading a zip to its end was checked.
        QFile whole(FS::PathCombine(QFINDTESTDATA("testdata/ShaderPackParse"), "shaderpack1.zip"));
        QVERIFY(whole.open(QIODevice::ReadOnly));
        const auto data = whole.readAll();
        // the header of the next entry starts 30 bytes before its name
        const auto next = data.indexOf("shaders/shaders.properties") - 30;
        QVERIFY(next > 0);

        const QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto path = temp.filePath("shaderpack1.zip");
        QFile cut(path);
        QVERIFY(cut.open(QIODevice::WriteOnly));
        QCOMPARE(cut.write(data.first(next)), next);
        cut.close();

        ShaderPack pack{ QFileInfo(path) };
        QVERIFY(ShaderPackUtils::process(pack));
        QCOMPARE(pack.packFormat(), ShaderPackFormat::VALID);
    }
};

QTEST_GUILESS_MAIN(ShaderPackParseTest)

#include "ShaderPackParse_test.moc"
