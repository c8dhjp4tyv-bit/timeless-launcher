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

#include <ui/widgets/InfoFrame.h>

class ColorCodesTest : public QObject {
    Q_OBJECT

   private slots:
    void renderColorCodes_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        QTest::newRow("plain") << "Plain text" << "<html>Plain text</html>";
        QTest::newRow("color") << "§aGreen" << R"(<html><span style="color: #55FF55;">Green</span></html>)";
        // The game reads codes case-insensitively.
        QTest::newRow("upper case color") << "§AGreen" << R"(<html><span style="color: #55FF55;">Green</span></html>)";
        QTest::newRow("upper case formatting") << "§LBold§R plain" << "<html><b>Bold</b> plain</html>";
        // A color ends the formatting before it, so this is gold but not bold in game...
        QTest::newRow("formatting then color") << "§l§6Gold" << R"(<html><b></b><span style="color: #FFAA00;">Gold</span></html>)";
        // ...while this is both.
        QTest::newRow("color then formatting") << "§6§lGold" << R"(<html><span style="color: #FFAA00;"><b>Gold</b></span></html>)";
        QTest::newRow("obfuscated") << "§kSecret" << "<html>Secret</html>";
        QTest::newRow("unknown code") << "§zText" << "<html>§zText</html>";
        QTest::newRow("trailing section sign") << "Text§" << "<html>Text§</html>";
        QTest::newRow("line break") << "One\nTwo" << "<html>One<br>Two</html>";
    }

    void renderColorCodes()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);

        QCOMPARE(InfoFrame::renderColorCodes(input), expected);
    }
};

QTEST_GUILESS_MAIN(ColorCodesTest)

#include "ColorCodes_test.moc"
