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

        // what Java 21 printed with "Xms128m" among the JVM arguments
        QTest::newRow("JVM argument without a dash") << "Error: Could not find or load main class Xms128m\n"
                                                        "Caused by: java.lang.ClassNotFoundException: Xms128m\n"
                                                     << 21 << QList<QStringList>{ { "Java took Xms128m", "missing the -" } };

        QTest::newRow("JVM argument without a dash, Java 8") << "Error: Could not find or load main class Files\\Java\\agent.jar\n"
                                                             << 8 << QList<QStringList>{ { "Java took Files\\Java\\agent.jar" } };

        QTest::newRow("launcher's own class not found") << "Error: Could not find or load main class org.timelesslauncher.EntryPoint\n"
                                                           "Caused by: java.lang.ClassNotFoundException: org.timelesslauncher.EntryPoint\n"
                                                        << 21 << QList<QStringList>{};

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

        // what Java 21 printed when a library named after NVIDIA's driver crashed, but for a shorter path
        QTest::newRow("native crash in NVIDIA's driver")
            << "#\n"
               "# A fatal error has been detected by the Java Runtime Environment:\n"
               "#\n"
               "#  SIGSEGV (0xb) at pc=0x00007fa35fa1e115, pid=8868, tid=8869\n"
               "#\n"
               "# JRE version: OpenJDK Runtime Environment (21.0.10+7) (build 21.0.10+7-Ubuntu-124.04)\n"
               "# Java VM: OpenJDK 64-Bit Server VM (21.0.10+7-Ubuntu-124.04, mixed mode, sharing, tiered, compressed oops, compressed "
               "class ptrs, g1 gc, linux-amd64)\n"
               "# Problematic frame:\n"
               "# C  [libnvidia-glcore.so.535.54.03+0x1115]  Java_Segv_crash+0x1c\n"
               "#\n"
               "# No core dump will be written. Core dumps have been disabled. To enable core dumping, try \"ulimit -c unlimited\" "
               "before starting Java again\n"
               "#\n"
               "# An error report file with more information is saved as:\n"
               "# /home/alex/minecraft/hs_err_pid8868.log\n"
               "#\n"
            << 21 << QList<QStringList>{ { "graphics driver (libnvidia-glcore.so.535.54.03)" } };

        QTest::newRow("native crash in AMD's driver")
            << "#  EXCEPTION_ACCESS_VIOLATION (0xc0000005) at pc=0x00007ffb1d2b0c5e, pid=1234, tid=5678\n"
               "# Problematic frame:\n"
               "# C  [atio6axx.dll+0x1d0c5e]\n"
            << 17 << QList<QStringList>{ { "graphics driver (atio6axx.dll)" } };

        QTest::newRow("native crash in Intel's driver") << "# Problematic frame:\r\n# C  [ig9icd64.dll+0x3a1b2c]\r\n"
                                                        << 17 << QList<QStringList>{ { "graphics driver (ig9icd64.dll)" } };

        QTest::newRow("native crash in Mesa") << "# Problematic frame:\n# C  [radeonsi_dri.so+0x5d2a61]\n"
                                              << 17 << QList<QStringList>{ { "graphics driver (radeonsi_dri.so)" } };

        QTest::newRow("native crash in an Intel Mac's driver")
            << "# Problematic frame:\n# C  [AMDRadeonX6000GLDriver+0x2d7c1]\n"
            << 17 << QList<QStringList>{ { "graphics driver (AMDRadeonX6000GLDriver)" } };

        QTest::newRow("native crash in OpenGL on Apple's chips")
            << "# Problematic frame:\n# C  [AppleMetalOpenGLRenderer+0x1e7d8]  GLDContextRec::flushContextInternal()+0x80\n"
            << 17 << QList<QStringList>{ { "graphics driver (AppleMetalOpenGLRenderer)" } };

        QTest::newRow("native crash elsewhere")
            << "# Problematic frame:\n"
               "# C  [lwjgl.dll+0xd28b]\n"
               "#\n"
               "# An error report file with more information is saved as:\n"
               "# C:\\Users\\Alex Doe\\AppData\\Roaming\\TimelessLauncher\\instances\\1.20.1\\minecraft\\hs_err_pid1234.log\n"
            << 17
            << QList<QStringList>{ { "crashed in lwjgl.dll",
                                     "C:\\Users\\Alex Doe\\AppData\\Roaming\\TimelessLauncher\\instances\\1.20.1\\minecraft\\hs_err_"
                                     "pid1234.log." } };

        QTest::newRow("Java itself crashing") << "# Problematic frame:\n# V  [libjvm.so+0x9d7b91]  Unsafe_PutLong+0x51\n"
                                              << 21 << QList<QStringList>{ { "crashed in libjvm.so, outside the game's code." } };

        QTest::newRow("crash in compiled Java code")
            << "# Problematic frame:\n"
               "# J 1234 c2 java.lang.String.hashCode()I java.base@21.0.2 (60 bytes) @ 0x00007f2a8d1c3a4b [0x00007f2a8d1c3a00+0x4b]\n"
            << 21 << QList<QStringList>{};

        QTest::newRow("two causes") << "Unrecognized VM option 'ZGenerational'\n"
                                       "java.lang.OutOfMemoryError: Java heap space\n"
                                    << 17 << QList<QStringList>{ { "ZGenerational" }, { "ran out of memory" } };

        QTest::newRow("Fabric with a solution")
            << "[12:00:00] [main/ERROR]: Incompatible mods found!\n"
               "net.fabricmc.loader.impl.FormattedException: Some of your mods are incompatible with the game or each other!\n"
               "A potential solution has been determined, this may resolve your problem:\n"
               "\t - Install fabric-api, version 0.92.2 or later.\n"
               "\t - Replace 'Sodium' (sodium) 0.4.10 with version 0.5.4 or later.\n"
               "More details:\n"
               "\t - Mod 'Sodium Extra' (sodium-extra) 0.5.4 requires version 0.5.4 or later of mod 'Sodium' (sodium), but only the "
               "wrong version is present: 0.4.10!\n"
            << 17
            << QList<QStringList>{ { "It suggests: Install fabric-api, version 0.92.2 or later. Replace 'Sodium' (sodium) 0.4.10 with "
                                     "version 0.5.4 or later." } };

        QTest::newRow("Fabric without a solution")
            << "net.fabricmc.loader.impl.FormattedException: Mod resolution encountered an incompatible mod set!\n"
               "\t - Mod 'Iris' (iris) 1.6.4 requires any version of sodium, which is missing!\n"
            << 17 << QList<QStringList>{ { "can't load these mods together", "which mod needs what" } };

        QTest::newRow("Forge dependencies")
            << "[main/ERROR] [net.minecraftforge.fml.loading.ModSorter/LOADING]: Missing or unsupported mandatory dependencies:\n"
               "\tMod ID: 'architectury', Requested by: 'roughlyenoughitems', Expected range: '[9.1.12,)', Actual version: "
               "'[MISSING]'\n"
               "\tMod ID: 'cloth_config', Requested by: 'roughlyenoughitems', Expected range: '[11.1.106,)', Actual version: "
               "'11.0.99'\n"
            << 17
            << QList<QStringList>{ { "architectury (roughlyenoughitems needs it)",
                                     "cloth_config 11.0.99 (roughlyenoughitems needs [11.1.106,))" } };

        QTest::newRow("mixin of a named mod")
            << "[12:00:00] [main/ERROR]: Mixin apply for mod iris failed iris.mixins.json:MixinGameRenderer from mod iris -> "
               "net.minecraft.class_757: org.spongepowered.asm.mixin.injection.throwables.InvalidInjectionException: Critical "
               "injection failure\n"
               "Caused by: org.spongepowered.asm.mixin.throwables.MixinApplyError: Mixin [iris.mixins.json:MixinGameRenderer] from "
               "phase [DEFAULT] in config [iris.mixins.json] FAILED during APPLY\n"
            << 17 << QList<QStringList>{ { "The mod iris couldn't make its changes" } };

        QTest::newRow("mixin config only")
            << "Caused by: org.spongepowered.asm.mixin.throwables.MixinApplyError: Mixin [sodium.mixins.json:core.MixinWindow] from "
               "phase [DEFAULT] in config [sodium.mixins.json] FAILED during APPLY\n"
            << 17 << QList<QStringList>{ { "the mixins in sodium.mixins.json" } };
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
