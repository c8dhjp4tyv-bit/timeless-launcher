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

    // Java takes the first argument that isn't an option for the class to run, and the launcher's own class comes after the JVM
    // arguments. When that goes wrong for the launcher's class itself, it may be for several reasons, so that gets no hint.
    static const QRegularExpression s_mainClassNotFound(R"(Error: Could not find or load main class (\S+))");
    if (const auto mainClass = s_mainClassNotFound.match(log); mainClass.hasMatch() && !mainClass.captured(1).endsWith(".EntryPoint")) {
        hints << tr("Java took %1 for the class to run, so it must be in the JVM arguments without being an option: an argument there "
                    "is probably missing the - in front of it, or has a path with spaces that isn't in quotes. Check the JVM arguments "
                    "in the Java tab of this instance's settings and in the launcher's settings.")
                     .arg(mainClass.captured(1));
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

    // A full disk, as Java reports it on Linux and macOS, and on Windows in English
    if (log.contains("No space left on device") || log.contains("There is not enough space on the disk")) {
        hints << tr(
            "The disk ran out of space while the game was writing to it. Free up some space, then start the game again. If a "
            "world doesn't load properly afterwards, restore it from a backup with Restore on the Worlds page.");
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

    // Native libraries, LWJGL's mostly, built for another kind of processor than Java. Windows and Linux report it through Java,
    // which names both (Java 8 wrote "AMD 64-bit" where later ones write "AMD 64", and newer ones name only the word width when
    // that differs), and macOS through its loader, which names what the file has and what the process needs.
    static const QRegularExpression s_nativeForOtherProcessor(
        R"(can't load ([\w ]+?)(?:-bit)? \.(?:dll|so) on a ([\w ]+?)(?:-bit)? platform)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression s_nativeForOtherMac(R"(incompatible architecture \(have '(\w+)', need '(\w+)')");
    const auto processorName = [](const QString& name) {
        const auto key = name.toLower().remove(' ').remove('_').remove('-');
        if (key == "amd64" || key == "x8664") {
            return tr("64-bit Intel and AMD processors (x86-64)");
        }
        if (key == "ia32" || key == "i386" || key == "x86") {
            return tr("32-bit Intel and AMD processors (x86)");
        }
        if (key == "aarch64" || key == "arm64" || key == "arm64e") {
            return tr("64-bit ARM processors (arm64)");
        }
        if (key == "arm") {
            return tr("32-bit ARM processors");
        }
        if (key == "64" || key == "32") {
            return tr("%1-bit processors").arg(key);
        }
        return name;
    };
    const auto nativeForOther = s_nativeForOtherProcessor.match(log);
    const auto nativeForOtherMac = s_nativeForOtherMac.match(log);
    if (const auto& found = nativeForOther.hasMatch() ? nativeForOther : nativeForOtherMac; found.hasMatch()) {
        const auto libraries = processorName(found.captured(1));
        auto hint = tr("Java can't load the game's native libraries: they are built for %1, and this Java for %2. Pick a Java built "
                       "for %1 in the Java tab of this instance's settings.")
                        .arg(libraries, processorName(found.captured(2)));
        if (!nativeForOther.hasMatch() && libraries == processorName("x86_64")) {
            hint += ' ' + tr("On a Mac with an Apple chip, macOS runs such a Java through Rosetta 2.");
        }
        hints << hint;
    } else if (log.contains("architecture word width mismatch")) {
        hints << tr(
            "Java can't load the game's native libraries, because one is 32-bit and the other 64-bit. A 32-bit Java can't "
            "load 64-bit libraries, so pick a 64-bit Java in the Java tab of this instance's settings.");
    }

    // Mods the mod loader can't load, as it explains it

    // A mod file Fabric can't read as a jar at all, as after a download that was cut short. It names the files it was looking at,
    // those of the mod the jar came in for a jar inside a mod.
    static const QRegularExpression s_unreadableModFile(
        R"(Error analyzing (?:nested jar \S+ from )?\[([^\]\n]+)\]: java\.util\.zip\.ZipException: ([^\n]+))");
    QStringList unreadable;
    for (auto matches = s_unreadableModFile.globalMatch(log); matches.hasNext();) {
        const auto match = matches.next();
        // the file's name is enough, and the path may be a Windows one
        const auto path = match.captured(1).split(", ").first();
        const auto fileName = path.mid(std::max(path.lastIndexOf('/'), path.lastIndexOf('\\')) + 1);
        const auto entry = tr("%1 (%2)").arg(fileName, match.captured(2).trimmed());
        if (!unreadable.contains(entry)) {
            unreadable << entry;
        }
    }
    if (unreadable.size() == 1) {
        hints << tr("The mod file %1 is damaged or isn't a mod at all, as when its download was cut short. Download it again, or take "
                    "it out of the mods folder.")
                     .arg(unreadable.first());
    } else if (!unreadable.isEmpty()) {
        hints << tr("These mod files are damaged or aren't mods at all, as when their downloads were cut short: %1. Download them "
                    "again, or take them out of the mods folder.")
                     .arg(unreadable.join(", "));
    }

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
    const auto mixin = s_mixinFailedForMod.match(log);
    const auto mixinConfig = s_mixinFailedInConfig.match(log);
    if (mixin.hasMatch()) {
        hints << tr("The mod %1 couldn't make its changes to the game's code. It may not be made for this version of Minecraft or "
                    "of the mod loader, or another mod clashes with it: update it, or take it out to see whether the game starts.")
                     .arg(mixin.captured(1));
    } else if (mixinConfig.hasMatch()) {
        hints << tr("A mod couldn't make its changes to the game's code (the mixins in %1). It may not be made for this version "
                    "of Minecraft or of the mod loader, or another mod clashes with it: update it, or take it out to see whether "
                    "the game starts.")
                     .arg(mixinConfig.captured(1));
    }

    // A mod that ran into an error while the game started, as the mod loader names it. A failed mixin shows up in whichever
    // mod's code happens to load the class first, so the hint above names the one to blame then.
    static const QRegularExpression s_entrypointFailed(
        R"((?:Could not execute entrypoint stage '\w+' due to errors, provided by|Exception while loading entries for entrypoint '\w+' provided by) '([^']+)')");
    static const QRegularExpression s_modFailed(
        R"(Failure message: ([^\n]+?) \(([\w.-]+)\) (?:has failed to load correctly|encountered an error during the))");
    static const QRegularExpression s_modFailedBefore1_13(R"(Caught exception from ([^\n]+) \(([\w.-]+)\)\s*$)",
                                                          QRegularExpression::MultilineOption);
    QStringList failedMods;
    if (!mixin.hasMatch() && !mixinConfig.hasMatch()) {
        for (auto matches = s_entrypointFailed.globalMatch(log); matches.hasNext();) {
            failedMods << matches.next().captured(1);
        }
        for (const auto* pattern : { &s_modFailed, &s_modFailedBefore1_13 }) {
            for (auto matches = pattern->globalMatch(log); matches.hasNext();) {
                const auto match = matches.next();
                failedMods << tr("%1 (%2)").arg(match.captured(1).trimmed(), match.captured(2));
            }
        }
        failedMods.removeDuplicates();
    }
    if (failedMods.size() == 1) {
        hints << tr("The game crashed while the mod %1 was starting. Update it, or take it out to see whether the game starts. The "
                    "lines starting with \"Caused by\" above say what went wrong.")
                     .arg(failedMods.first());
    } else if (!failedMods.isEmpty()) {
        hints << tr("The game crashed while these mods were starting: %1. Update them, or take them out to see whether the game "
                    "starts. The lines starting with \"Caused by\" above say what went wrong.")
                     .arg(failedMods.join(", "));
    }

    // Forge's crash reports list the mods whose code the crash went through, leaving out the game and Forge itself: each mod on a
    // line indented once, what points to it on the lines under it, and a blank line between mods. That is only a lead, so it is
    // given when nothing above explains the crash.
    static const QRegularExpression s_suspectedMods(R"(Suspected Mods?: *\n((?:\t.*\n|\n)+))");
    static const QRegularExpression s_suspectedMod(R"(^\t([^\t\n]+) \(([\w.-]+)\), Version: (.*)$)", QRegularExpression::MultilineOption);
    QStringList suspects;
    if (hints.isEmpty()) {
        for (auto blocks = s_suspectedMods.globalMatch(log); blocks.hasNext();) {
            for (auto mods = s_suspectedMod.globalMatch(blocks.next().captured(1)); mods.hasNext();) {
                const auto mod = mods.next();
                suspects << tr("%1 (%2) %3").arg(mod.captured(1), mod.captured(2), mod.captured(3).trimmed());
            }
        }
        suspects.removeDuplicates();
    }
    if (suspects.size() == 1) {
        hints << tr("The crash report points to the mod %1: the crash went through its code. It may not be at fault itself, but "
                    "updating it, or taking it out to see whether the game still crashes, is the first thing to try.")
                     .arg(suspects.first());
    } else if (!suspects.isEmpty()) {
        hints << tr("The crash report points to these mods: %1. The crash went through their code. They may not be at fault "
                    "themselves, but updating them, or taking them out to see whether the game still crashes, is the first thing "
                    "to try.")
                     .arg(suspects.join(", "));
    }

    return hints;
}
