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

#include <QSet>
#include <QTest>

#include <minecraft/mod/ModBisect.h>

class ModBisectTest : public QObject {
    Q_OBJECT

    struct Outcome {
        ModBisect::State state = ModBisect::State::Searching;
        QStringList suspects;
        int tests = 0;
    };

    /// Runs a search the way a person would answer it when the problem comes from the culprit (or from no mod, if it's empty),
    /// checking before each test that no mod that stays on needs one that is off
    static void search(const QStringList& mods, const QHash<QString, QStringList>& needs, const QString& culprit, Outcome& outcome)
    {
        ModBisect bisect(mods, needs);
        const int expectedAtMost = bisect.testsLeft();
        while (bisect.state() == ModBisect::State::Searching || bisect.state() == ModBisect::State::Confirming) {
            const auto offList = bisect.modsToTurnOff();
            const QSet<QString> off(offList.begin(), offList.end());
            QVERIFY(!off.isEmpty());
            for (const auto& mod : mods) {
                for (const auto& needed : needs.value(mod)) {
                    QVERIFY2(off.contains(mod) || !off.contains(needed), qPrintable(mod + " stays on without " + needed));
                }
            }
            bisect.report(culprit.isEmpty() || !off.contains(culprit));
            outcome.tests++;
            QVERIFY(outcome.tests <= expectedAtMost);
        }
        outcome.state = bisect.state();
        outcome.suspects = bisect.suspects();
    }

    static QStringList numbered(const QString& prefix, int count)
    {
        QStringList mods;
        for (int i = 0; i < count; i++) {
            mods << QString("%1%2.jar").arg(prefix).arg(i, 2, 10, QChar('0'));
        }
        return mods;
    }

   private slots:
    void independentMods()
    {
        const auto mods = numbered("mod", 13);
        for (const auto& culprit : mods) {
            Outcome outcome;
            search(mods, {}, culprit, outcome);
            if (QTest::currentTestFailed()) {
                return;
            }
            QCOMPARE(outcome.state, ModBisect::State::Found);
            QCOMPARE(outcome.suspects, QStringList{ culprit });
            // ceil(log2(13)) halvings and the test that confirms it
            QVERIFY(outcome.tests <= 5);
        }
    }

    void modsThatNeedOthers()
    {
        // an API, a library on top of it, two mods on top of the library, and mods that need nothing
        auto mods = numbered("other", 6);
        mods << "api.jar" << "library.jar" << "feature-a.jar" << "feature-b.jar";
        const QHash<QString, QStringList> needs{
            { "library.jar", { "api.jar" } },
            { "feature-a.jar", { "library.jar" } },
            { "feature-b.jar", { "library.jar", "api.jar", "missing.jar" } },
        };
        for (const auto& culprit : mods) {
            Outcome outcome;
            search(mods, needs, culprit, outcome);
            if (QTest::currentTestFailed()) {
                return;
            }
            QCOMPARE(outcome.state, ModBisect::State::Found);
            QCOMPARE(outcome.suspects, QStringList{ culprit });
        }
    }

    void noTestTwice()
    {
        // the last halving turns off just the culprit, and the problem goes away: that is already the confirmation
        Outcome outcome;
        search({ "a.jar", "b.jar" }, {}, "b.jar", outcome);
        QCOMPARE(outcome.state, ModBisect::State::Found);
        QCOMPARE(outcome.suspects, QStringList{ "b.jar" });
        QCOMPARE(outcome.tests, 1);

        // when it stays, the one left over still has to be tested on its own
        search({ "a.jar", "b.jar" }, {}, "a.jar", outcome = {});
        QCOMPARE(outcome.state, ModBisect::State::Found);
        QCOMPARE(outcome.suspects, QStringList{ "a.jar" });
        QCOMPARE(outcome.tests, 2);
    }

    void notOneMod()
    {
        // the problem stays whatever is turned off
        Outcome outcome;
        search(numbered("mod", 8), {}, {}, outcome);
        QCOMPARE(outcome.state, ModBisect::State::NotFound);
    }

    void oneMod()
    {
        ModBisect bisect({ "only.jar" }, {});
        QCOMPARE(bisect.state(), ModBisect::State::Confirming);
        QCOMPARE(bisect.modsToTurnOff(), QStringList{ "only.jar" });
        QCOMPARE(bisect.testsLeft(), 1);
        bisect.report(false);
        QCOMPARE(bisect.state(), ModBisect::State::Found);
        QCOMPARE(bisect.suspects(), QStringList{ "only.jar" });
        QVERIFY(bisect.modsToTurnOff().isEmpty());
    }

    void noMods()
    {
        const ModBisect bisect({}, {});
        QCOMPARE(bisect.state(), ModBisect::State::NotFound);
        QCOMPARE(bisect.testsLeft(), 0);
    }

    void modsThatNeedEachOther()
    {
        // they can only be turned off together, so the search can't tell which of the two it is
        auto mods = numbered("other", 5);
        mods << "a.jar" << "b.jar";
        const QHash<QString, QStringList> needs{ { "a.jar", { "b.jar" } }, { "b.jar", { "a.jar" } } };
        for (const auto& culprit : { QString("a.jar"), QString("b.jar") }) {
            Outcome outcome;
            search(mods, needs, culprit, outcome);
            if (QTest::currentTestFailed()) {
                return;
            }
            QCOMPARE(outcome.state, ModBisect::State::Found);
            QCOMPARE(outcome.suspects, (QStringList{ "a.jar", "b.jar" }));
        }

        // and the others are still found on their own
        Outcome outcome;
        search(mods, needs, "other02.jar", outcome);
        QCOMPARE(outcome.state, ModBisect::State::Found);
        QCOMPARE(outcome.suspects, QStringList{ "other02.jar" });
    }
};

QTEST_GUILESS_MAIN(ModBisectTest)

#include "ModBisect_test.moc"
