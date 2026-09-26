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

#include "CrashHints.h"

#include <QRegularExpression>

#include <algorithm>

namespace {

/// The Java version that writes class files of this major version: 52 is Java 8, 65 is Java 21
int javaForClassVersion(int classVersion)
{
    return classVersion - 44;
}

/// The highest Java version any match of the pattern asks for, going by the class file version in its first group
int highestJavaAskedFor(const QString& log, const QRegularExpression& pattern)
{
    int highest = 0;
    auto matches = pattern.globalMatch(log);
    while (matches.hasNext()) {
        highest = std::max(highest, javaForClassVersion(matches.next().captured(1).toInt()));
    }
    return highest;
}

}  // namespace

QStringList CrashHints::find(const QString& log, int javaMajorVersion)
{
    QStringList hints;

    // Java refusing to start at all, which leaves nothing else in the log to go by

    static const QRegularExpression s_lockedOption(
        R"(VM option '([^']+)' is (?:experimental|diagnostic) and must be enabled via (-XX:\+Unlock\w+VMOptions))");
    static const QRegularExpression s_unknownVmOption(R"(Unrecognized VM option '([^']+)')");
    static const QRegularExpression s_unknownOption(R"(Unrecognized option: (\S+))");
    const auto locked = s_lockedOption.match(log);
    const auto unknownVm = s_unknownVmOption.match(log);
    const auto unknown = s_unknownOption.match(log);
    if (locked.hasMatch()) {
        hints << tr("Java only accepts the option %1 after %2. Add %2 in front of it in the JVM arguments, in the Java tab of this "
                    "instance's settings or in the launcher's settings, wherever %1 is set.")
                     .arg(locked.captured(1), locked.captured(2));
    } else if (unknownVm.hasMatch()) {
        hints << tr("Java doesn't know the option %1. Remove or correct it in the JVM arguments, in the Java tab of this instance's "
                    "settings or in the launcher's settings, wherever it is set.")
                     .arg(unknownVm.captured(1));
    } else if (unknown.hasMatch()) {
        hints << tr("Java doesn't accept the argument %1. If it's one of the JVM arguments in the Java tab of this instance's settings "
                    "or in the launcher's settings, remove or correct it there. If it isn't, this Java is probably too old for this "
                    "version of the game.")
                     .arg(unknown.captured(1));
    }

    static const QRegularExpression s_heapReservation(
        R"(Could not reserve enough space for (?:\d+KB )?object heap|Invalid maximum heap size)");
    if (log.contains(s_heapReservation)) {
        hints << tr(
            "Java couldn't set aside as much memory as this instance lets the game use. Lower the maximum memory usage in the "
            "Java tab of this instance's settings. A 32-bit Java can only use a small amount of memory, so if this one is "
            "32-bit, use a 64-bit Java instead.");
    } else if (log.contains("There is insufficient memory for the Java Runtime Environment to continue")) {
        hints << tr(
            "The computer ran out of memory for Java. Close other programs, or lower the maximum memory usage in the Java tab "
            "of this instance's settings.");
    }

    // The wrong Java for the game, its mod loader or its mods

    static const QRegularExpression s_newerClassFile(
        R"(class file version (\d+)\.\d+\), this version of the Java Runtime only recognizes class file versions up to (\d+)\.\d+)");
    static const QRegularExpression s_newerClassFileBeforeJava8(R"(Unsupported major\.minor version (\d+)\.\d+)");
    if (const auto newer = s_newerClassFile.match(log); newer.hasMatch()) {
        hints << tr("The game or one of its mods needs Java %1 or newer, but the game was started with Java %2. Pick a newer Java in "
                    "the Java tab of this instance's settings.")
                     .arg(highestJavaAskedFor(log, s_newerClassFile))
                     .arg(javaForClassVersion(newer.captured(2).toInt()));
    } else if (log.contains(s_newerClassFileBeforeJava8)) {
        hints << tr("The game or one of its mods needs Java %1 or newer, but the game was started with an older Java. Pick a newer "
                    "Java in the Java tab of this instance's settings.")
                     .arg(highestJavaAskedFor(log, s_newerClassFileBeforeJava8));
    }

    // LaunchWrapper, which starts Minecraft 1.12.2 and older with a mod loader, relies on something Java 9 took away
    static const QRegularExpression s_needsJava8(R"(ClassLoaders\$AppClassLoader cannot be cast to (?:class )?java\.net\.URLClassLoader)");
    if (log.contains(s_needsJava8)) {
        hints << tr(
            "This version of Minecraft or of its mod loader only runs on Java 8, but the game was started with a newer Java. "
            "Pick Java 8 in the Java tab of this instance's settings.");
    }

    // ASM, which mod loaders read classes with, only knows the class file versions that existed when it was released
    static const QRegularExpression s_unreadableClassFile(R"(Unsupported class file major version (\d+))");
    if (const auto unreadable = s_unreadableClassFile.match(log); unreadable.hasMatch()) {
        const int java = javaForClassVersion(unreadable.captured(1).toInt());
        if (javaMajorVersion == 0 || java == javaMajorVersion) {
            hints << tr("This version of the mod loader doesn't support Java %1, which the game was started with. Pick the Java "
                        "version this version of Minecraft was made for in the Java tab of this instance's settings.")
                         .arg(java);
        } else {
            hints << tr("The mod loader couldn't read a file made for Java %1, which this version of it doesn't support. A mod may "
                        "have been made for a newer version of Minecraft than this instance's.")
                         .arg(java);
        }
    }

    if (log.contains("java.lang.OutOfMemoryError: Java heap space") ||
        log.contains("java.lang.OutOfMemoryError: GC overhead limit exceeded")) {
        hints << tr("The game ran out of memory. Raise the maximum memory usage in the Java tab of this instance's settings.");
    }

    // Graphics drivers, going by GLFW's errors for a missing API (65542) and a missing version (65543), or LWJGL 2's

    static const QRegularExpression s_noOpenGl(
        R"(GLFW error 65542|\[0x10006\]|The driver does not appear to support OpenGL|Pixel format not accelerated)");
    static const QRegularExpression s_oldOpenGl(R"(GLFW error 65543|\[0x10007\]|WGL_ARB_create_context_profile is unavailable)");
    if (log.contains(s_noOpenGl)) {
        hints << tr(
            "The graphics driver doesn't provide OpenGL, which the game needs. Install the latest driver from the maker of the "
            "graphics card (NVIDIA, AMD or Intel); on Windows, this happens when only the basic display driver is installed.");
    } else if (log.contains(s_oldOpenGl)) {
        hints << tr(
            "The graphics card or its driver doesn't support the OpenGL version this version of Minecraft needs. Installing "
            "the latest graphics driver may help; a very old graphics card can only run older versions of Minecraft.");
    }

    // Java crashing in native code, which it reports in lines starting with #, naming the library it crashed in
    static const QRegularExpression s_problematicFrame(R"(# Problematic frame:\s*\n#\s+\w\s+\[([^\]+]+))");
    static const QRegularExpression s_errorReport(R"(# An error report file with more information is saved as:\s*\n#\s*([^\n]+))");
    // the OpenGL drivers of AMD, NVIDIA and Intel on Windows, NVIDIA's and Mesa's on Linux, and the ones macOS comes with: a
    // driver per graphics card on Intel Macs, and OpenGL on top of Metal on Apple's own chips
    static const QRegularExpression s_graphicsDriver(
        R"(^(?:(?:atio6axx|atioglxx|nvoglv32|nvoglv64|ig\w*icd32|ig\w*icd64)\.dll|lib(?:nvidia-glcore|nvidia-eglcore|GLX_nvidia|GLX_mesa)\.so.*|libgallium-.*\.so|\w+_dri\.so|\w+GLDriver|AppleMetalOpenGLRenderer|GLEngine)$)",
        QRegularExpression::CaseInsensitiveOption);
    if (const auto frame = s_problematicFrame.match(log); frame.hasMatch()) {
        const auto library = frame.captured(1);
        if (library.contains(s_graphicsDriver)) {
            hints << tr("Java crashed inside the graphics driver (%1). Install the latest driver from the maker of the graphics card "
                        "(NVIDIA, AMD or Intel).")
                         .arg(library);
        } else if (const auto report = s_errorReport.match(log); report.hasMatch()) {
            hints << tr("Java crashed in %1, outside the game's code. It wrote what it knows about the crash to %2.")
                         .arg(library, report.captured(1).trimmed());
        } else {
            hints << tr("Java crashed in %1, outside the game's code.").arg(library);
        }
    }

    // Mods the mod loader can't load, as it explains it

    // Fabric and Quilt, which also work out what to change when they can
    static const QRegularExpression s_incompatibleMods(
        "Mod resolution encountered an incompatible mod set!|Some of your mods are incompatible with the game or each other!");
    if (log.contains(s_incompatibleMods)) {
        QStringList suggestions;
        if (const auto solution = log.indexOf("A potential solution has been determined"); solution >= 0) {
            const auto lines = log.mid(solution).split('\n');
            for (auto it = lines.begin() + 1; it != lines.end() && it->trimmed().startsWith("- "); ++it) {
                suggestions << it->trimmed().mid(2);
            }
        }
        if (!suggestions.isEmpty()) {
            hints << tr("The mod loader can't load these mods together. It suggests: %1").arg(suggestions.join(' '));
        } else {
            hints << tr("The mod loader can't load these mods together. The log above says which mod needs what.");
        }
    }

    // Forge and NeoForge list each dependency they can't satisfy
    static const QRegularExpression s_unmetDependency(
        R"(Mod ID: '([^']+)', Requested by: '([^']+)', Expected range: '([^']*)', Actual version: '([^']*)')");
    QStringList unmet;
    for (auto matches = s_unmetDependency.globalMatch(log); matches.hasNext();) {
        const auto match = matches.next();
        const auto entry = match.captured(4) == "[MISSING]"
                               ? tr("%1 (%2 needs it)").arg(match.captured(1), match.captured(2))
                               : tr("%1 %3 (%2 needs %4)").arg(match.captured(1), match.captured(2), match.captured(4), match.captured(3));
        if (!unmet.contains(entry)) {
            unmet << entry;
        }
    }
    if (!unmet.isEmpty()) {
        hints << tr("Some mods need other mods that are missing or in a version they can't use: %1. Install or update those.")
                     .arg(unmet.join(", "));
    }

    // A mod that failed to change the game's code, usually because it was made for another version of the game
    static const QRegularExpression s_mixinFailedForMod(R"(Mixin apply for mod ([\w.-]+) failed)");
    static const QRegularExpression s_mixinFailedInConfig(R"(in config \[([^\]]+)\] FAILED during)");
    if (const auto mixin = s_mixinFailedForMod.match(log); mixin.hasMatch()) {
        hints << tr("The mod %1 couldn't make its changes to the game's code. It may not be made for this version of Minecraft or "
                    "of the mod loader, or another mod clashes with it: update it, or take it out to see whether the game starts.")
                     .arg(mixin.captured(1));
    } else if (const auto config = s_mixinFailedInConfig.match(log); config.hasMatch()) {
        hints << tr("A mod couldn't make its changes to the game's code (the mixins in %1). It may not be made for this version "
                    "of Minecraft or of the mod loader, or another mod clashes with it: update it, or take it out to see whether "
                    "the game starts.")
                     .arg(config.captured(1));
    }

    return hints;
}
