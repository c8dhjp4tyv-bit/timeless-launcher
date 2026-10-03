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
#include <ui/dialogs/MSALoginDialog.h>

class MSALoginTest : public QObject {
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

    /// What a server says is shown as the text it is
    void failureTextIsText()
    {
        QCOMPARE(MSALoginDialog::failureText("Forbidden"), "<font color='red'>Forbidden</font><br />");
        QCOMPARE(MSALoginDialog::failureText("Failed to get a token\nInvalid app registration, see https://aka.ms/AppRegInfo"),
                 "<font color='red'>Failed to get a token</font><br />"
                 "<font color='red'>Invalid app registration, see https://aka.ms/AppRegInfo</font><br />");
        QCOMPARE(MSALoginDialog::failureText("one\n\ntwo"), "<font color='red'>one</font><br /><br /><font color='red'>two</font><br />");

        const auto shown = MSALoginDialog::failureText("<a href=\"https://example.org\">Fix it</a> & <b>more</b>");
        QVERIFY(!shown.contains("<a "));
        QVERIFY(!shown.contains("<b>"));
        QVERIFY(shown.contains("&lt;a href=&quot;https://example.org&quot;&gt;Fix it&lt;/a&gt; &amp; &lt;b&gt;more&lt;/b&gt;"));
    }

    void failureTextLinks_data()
    {
        QTest::addColumn<QString>("reason");
        QTest::addColumn<bool>("kept");

        // what the launcher itself says when Xbox refuses an account
        QTest::newRow("the store")
            << "Buy the game on <a href=\"https://www.minecraft.net/en-us/store/minecraft-java-edition\">minecraft.net</a> first." << true;
        QTest::newRow("the help of Minecraft")
            << "Set up your account according to <a "
               "href=\"https://help.minecraft.net/hc/en-us/articles/4408968616077\">help.minecraft.net</a>."
            << true;
        QTest::newRow("the login of Microsoft")
            << "Login to <a href=\"https://login.live.com/login.srf\">login.live.com</a> to prove your age." << true;
        QTest::newRow("the family page") << "Login to <a href=\"https://account.microsoft.com/family/\">account.microsoft.com</a> now."
                                         << true;
        // and what a server may put in its answers is not a link
        QTest::newRow("another page") << "<a href=\"https://example.org\">Fix it</a>" << false;
        QTest::newRow("a page that starts like one of ours")
            << "<a href=\"https://login.live.com.example.org/\">login.live.com</a>" << false;
        QTest::newRow("a page behind a name that is one of ours")
            << "<a href=\"https://login.live.com@example.org/\">login.live.com</a>" << false;
        QTest::newRow("a page without https") << "<a href=\"http://login.live.com/\">login.live.com</a>" << false;
        QTest::newRow("a link that does more than link")
            << "<a href=\"https://login.live.com/\" onclick=\"steal()\">login.live.com</a>" << false;
        QTest::newRow("markup in the name of a link") << "<a href=\"https://login.live.com/\"><b>login.live.com</b></a>" << false;
        QTest::newRow("a link that is not closed") << "<a href=\"https://login.live.com/\">login.live.com" << false;
    }

    /// The links of the launcher's own messages stay links, and nothing else that looks like markup does
    void failureTextLinks()
    {
        QFETCH(const QString, reason);
        QFETCH(const bool, kept);

        const auto shown = MSALoginDialog::failureText(reason);
        if (kept) {
            QCOMPARE(shown, "<font color='red'>" + reason + "</font><br />");
        } else {
            QCOMPARE(shown, "<font color='red'>" + reason.toHtmlEscaped() + "</font><br />");
        }
    }
};

QTEST_GUILESS_MAIN(MSALoginTest)

#include "MSALogin_test.moc"
