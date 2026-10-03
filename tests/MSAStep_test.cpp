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

#include <BuildConfig.h>
#include <minecraft/auth/steps/MSAStep.h>

class MSAStepTest : public QObject {
    Q_OBJECT

   private slots:
    /// Without an address to go on to, the browser is left on a page that says the sign-in went through
    void callbackPageOfItsOwn()
    {
        const auto page = MSAStep::callbackPage({});

        QVERIFY(page.contains("Login successful"));
        QVERIFY(page.contains(BuildConfig.LAUNCHER_DISPLAYNAME));
        QVERIFY(!page.contains("window.location"));
        QVERIFY(!page.contains("Refresh"));
        QVERIFY(!page.contains("http"));
    }

    /// A build can name the address the browser goes on to
    void callbackPageThatGoesOn()
    {
        const auto page = MSAStep::callbackPage("https://example.org/signed-in");

        QVERIFY(page.contains("window.location.replace(\"https://example.org/signed-in\")"));
        QVERIFY(page.contains("content=\"0; URL=https://example.org/signed-in\""));
    }
};

QTEST_GUILESS_MAIN(MSAStepTest)

#include "MSAStep_test.moc"
