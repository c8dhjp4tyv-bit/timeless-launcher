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

#include <logs/CrashHints.h>

class CrashHintsTest : public QObject {
    Q_OBJECT

   private slots:
    void find_data()
    {
        QTest::addColumn<QString>("log");
        QTest::addColumn<int>("javaMajorVersion");
        // one entry per hint expected, each a list of words the hint has to contain
        QTest::addColumn<QList<QStringList>>("expected");

        QTest::newRow("clean exit") << "[12:00:00] [Render thread/INFO]: Stopping!\nProcess exited with code 0." << 17
                                    << QList<QStringList>{};

        QTest::newRow("unrelated crash")
            << "---- Minecraft Crash Report ----\n"
               "Description: Rendering overlay\n\n"
               "java.lang.NullPointerException: Cannot invoke \"Object.toString()\" because \"value\" is null\n"
               "\tat com.example.Mod.render(Mod.java:42)\n"
            << 17 << QList<QStringList>{};

        QTest::newRow("Java 17 for Minecraft 1.20.5")
            << "Exception in thread \"main\" java.lang.UnsupportedClassVersionError: net/minecraft/client/main/Main has been compiled by "
               "a more recent version of the Java Runtime (class file version 65.0), this version of the Java Runtime only recognizes "
               "class file versions up to 61.0\n"
               "\tat java.base/java.lang.ClassLoader.defineClass1(Native Method)\n"
            << 17 << QList<QStringList>{ { "Java 21", "Java 17", "newer Java" } };

        QTest::newRow("Java 8 with mods for Java 17 and 21")
            << "java.lang.UnsupportedClassVersionError: a/B has been compiled by a more recent version of the Java Runtime (class file "
               "version 61.0), this version of the Java Runtime only recognizes class file versions up to 52.0\n"
               "java.lang.UnsupportedClassVersionError: c/D has been compiled by a more recent version of the Java Runtime (class file "
               "version 65.0), this version of the Java Runtime only recognizes class file versions up to 52.0\n"
            << 8 << QList<QStringList>{ { "Java 21", "Java 8" } };

        QTest::newRow("Java 7") << "Exception in thread \"main\" java.lang.UnsupportedClassVersionError: net/minecraft/client/main/Main : "
                                   "Unsupported major.minor version 52.0\n"
                                << 7 << QList<QStringList>{ { "Java 8", "older Java" } };

        QTest::newRow("Forge 1.12.2 on Java 17")
            << "Exception in thread \"main\" java.lang.ClassCastException: class jdk.internal.loader.ClassLoaders$AppClassLoader cannot be "
               "cast to class java.net.URLClassLoader (jdk.internal.loader.ClassLoaders$AppClassLoader and java.net.URLClassLoader are in "
               "module java.base of loader 'bootstrap')\n"
               "\tat net.minecraft.launchwrapper.Launch.<init>(Launch.java:34)\n"
            << 17 << QList<QStringList>{ { "only runs on Java 8" } };

        QTest::newRow("mod loader too old for Java 21") << "java.lang.IllegalArgumentException: Unsupported class file major version 65\n"
                                                           "\tat org.objectweb.asm.ClassReader.<init>(ClassReader.java:199)\n"
                                                        << 21 << QList<QStringList>{ { "doesn't support Java 21", "was made for" } };

        QTest::newRow("mod made for a newer Java than the game runs on")
            << "java.lang.IllegalArgumentException: Unsupported class file major version 65\n"
            << 17 << QList<QStringList>{ { "made for Java 21", "newer version of Minecraft" } };

        QTest::newRow("out of memory") << "[12:00:00] [Server thread/ERROR]: Encountered an unexpected exception\n"
                                          "java.lang.OutOfMemoryError: Java heap space\n"
                                       << 17 << QList<QStringList>{ { "ran out of memory", "Raise the maximum memory" } };

        QTest::newRow("garbage collector overhead")
            << "Exception in thread \"Worker-Main-3\" java.lang.OutOfMemoryError: GC overhead limit exceeded\n"
            << 8 << QList<QStringList>{ { "ran out of memory" } };

        QTest::newRow("heap too large to reserve") << "Error occurred during initialization of VM\n"
                                                      "Could not reserve enough space for 4194304KB object heap\n"
                                                   << 8 << QList<QStringList>{ { "Lower the maximum memory", "64-bit" } };

        QTest::newRow("heap too large for 32 bits") << "Invalid maximum heap size: -Xmx8192m\n"
                                                       "The specified size exceeds the maximum representable size.\n"
                                                       "Error: Could not create the Java Virtual Machine.\n"
                                                    << 8 << QList<QStringList>{ { "Lower the maximum memory" } };

        QTest::newRow("no memory left") << "# There is insufficient memory for the Java Runtime Environment to continue.\n"
                                           "# Native memory allocation (mmap) failed to map 4294967296 bytes for G1 virtual space\n"
                                        << 17 << QList<QStringList>{ { "ran out of memory for Java", "Close other programs" } };

        QTest::newRow("unknown -XX option") << "Unrecognized VM option 'ZGenerational'\n"
                                               "Did you mean '(+/-)ZGenerational'?\n"
                                               "Error: Could not create the Java Virtual Machine.\n"
                                               "Error: A fatal exception has occurred. Program will exit.\n"
                                            << 17 << QList<QStringList>{ { "ZGenerational", "JVM arguments" } };

        QTest::newRow("unknown argument") << "Unrecognized option: --add-opens\n"
                                             "Error: Could not create the Java Virtual Machine.\n"
                                          << 8 << QList<QStringList>{ { "--add-opens", "too old" } };

        QTest::newRow("experimental option")
            << "Error: VM option 'UseZGC' is experimental and must be enabled via -XX:+UnlockExperimentalVMOptions.\n"
               "Error: Could not create the Java Virtual Machine.\n"
            << 11 << QList<QStringList>{ { "UseZGC", "after -XX:+UnlockExperimentalVMOptions" } };

        QTest::newRow("no OpenGL driver") << "[12:00:00] [Render thread/ERROR]: GLFW error during init: [0x10006]WGL: The driver does not "
                                             "appear to support OpenGL\n"
                                          << 17 << QList<QStringList>{ { "driver", "OpenGL" } };

        QTest::newRow("no OpenGL driver, numbered") << "GLFW error 65542: WGL: The driver does not appear to support OpenGL\n"
                                                    << 17 << QList<QStringList>{ { "doesn't provide OpenGL" } };

        QTest::newRow("no OpenGL driver, LWJGL 2") << "org.lwjgl.LWJGLException: Pixel format not accelerated\n"
                                                   << 8 << QList<QStringList>{ { "doesn't provide OpenGL" } };

        QTest::newRow("OpenGL too old")
            << "GLFW error 65543: WGL: OpenGL profile requested but WGL_ARB_create_context_profile is unavailable\n"
            << 17 << QList<QStringList>{ { "OpenGL version" } };

        QTest::newRow("two causes") << "Unrecognized VM option 'ZGenerational'\n"
                                       "java.lang.OutOfMemoryError: Java heap space\n"
                                    << 17 << QList<QStringList>{ { "ZGenerational" }, { "ran out of memory" } };
    }

    void find()
    {
        QFETCH(QString, log);
        QFETCH(int, javaMajorVersion);
        QFETCH(QList<QStringList>, expected);

        const auto hints = CrashHints::find(log, javaMajorVersion);
        QCOMPARE(hints.size(), expected.size());
        for (int i = 0; i < hints.size(); i++) {
            for (const auto& word : expected.at(i)) {
                if (!hints.at(i).contains(word)) {
                    QFAIL(qPrintable(QString("\"%1\" isn't in the hint: %2").arg(word, hints.at(i))));
                }
            }
        }
    }
};

QTEST_GUILESS_MAIN(CrashHintsTest)

#include "CrashHints_test.moc"
