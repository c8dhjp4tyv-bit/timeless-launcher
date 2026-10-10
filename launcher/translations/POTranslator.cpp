#include "POTranslator.h"

#include <QDebug>
#include "FileSystem.h"

struct POEntry {
    QString text;
    bool fuzzy;
    /// The forms of a plural entry, the first of which is `text`; empty when the entry has no plural
    QStringList plurals;
};

struct POTranslatorPrivate {
    QString filename;
    QHash<QByteArray, POEntry> mapping;
    QHash<QByteArray, POEntry> mapping_disambiguatrion;
    bool loaded = false;

    void reload();
};

class ParserArray : public QByteArray {
   public:
    ParserArray(const QByteArray& in) : QByteArray(in) {}
    bool chomp(const char* data, int length)
    {
        if (startsWith(data)) {
            remove(0, length);
            return true;
        }
        return false;
    }
    bool chompString(QByteArray& appendHere)
    {
        QByteArray msg;
        bool escape = false;
        if (size() < 2) {
            qDebug() << "String fragment is too short";
            return false;
        }
        if (!startsWith('"')) {
            qDebug() << "String fragment does not start with \"";
            return false;
        }
        if (!endsWith('"')) {
            qDebug() << "String fragment does not end with \", instead, there is" << at(size() - 1);
            return false;
        }
        for (int i = 1; i < size() - 1; i++) {
            char c = operator[](i);
            if (escape) {
                switch (c) {
                    case 'r':
                        msg += '\r';
                        break;
                    case 'n':
                        msg += '\n';
                        break;
                    case 't':
                        msg += '\t';
                        break;
                    case 'v':
                        msg += '\v';
                        break;
                    case 'a':
                        msg += '\a';
                        break;
                    case 'b':
                        msg += '\b';
                        break;
                    case 'f':
                        msg += '\f';
                        break;
                    case '"':
                        msg += '"';
                        break;
                    case '\\':
                        msg.append('\\');
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7': {
                        // up to three digits, as in C; the end of the string is the closing quote
                        unsigned value = 0;
                        for (int digits = 0; digits < 3 && i < size() - 1 && at(i) >= '0' && at(i) <= '7'; digits++, i++) {
                            value = value * 8 + static_cast<unsigned>(at(i) - '0');
                        }
                        msg += static_cast<char>(value & 0xFF);
                        i--;  // the loop moves on to the character after the escape
                        break;
                    }
                    case 'x': {
                        // chomp the 'x'
                        i++;
                        unsigned value = 0;
                        int digits = 0;
                        for (; i < size() - 1 && isxdigit(static_cast<unsigned char>(at(i))); digits++, i++) {
                            value = ((value << 4) | static_cast<unsigned>(QByteArray(1, at(i)).toUInt(nullptr, 16))) & 0xFF;
                        }
                        if (digits == 0) {
                            qDebug() << "A hex escape string without digits...";
                            return false;
                        }
                        msg += static_cast<char>(value);
                        i--;  // the loop moves on to the character after the escape
                        break;
                    }
                    default: {
                        qDebug() << "Invalid escape sequence character:" << c;
                        return false;
                    }
                }
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else {
                msg += c;
            }
        }
        if (escape) {
            qDebug() << "Unterminated escape sequence...";
            return false;
        }
        appendHere += msg;
        return true;
    }
};

void POTranslatorPrivate::reload()
{
    QFile file(filename);
    if (!file.open(QFile::OpenMode::enum_type::ReadOnly | QFile::OpenMode::enum_type::Text)) {
        qDebug() << "Failed to open PO file:" << filename << "error:" << file.errorString();
        return;
    }

    QByteArray context;
    QByteArray disambiguation;
    QByteArray id;
    QByteArray str;
    // the forms of a plural entry: msgstr[0], msgstr[1], ...; the strings that follow a msgstr[N] belong to that one
    QList<QByteArray> pluralStrs;
    bool fuzzy = false;
    bool nextFuzzy = false;

    enum class Mode { First, MessageContext, MessageId, MessageString } mode = Mode::First;

    int lineNumber = 0;
    QHash<QByteArray, POEntry> newMapping;
    QHash<QByteArray, POEntry> newMapping_disambiguation;
    auto endEntry = [&]() {
        QStringList plurals;
        for (const auto& form : pluralStrs) {
            plurals << QString::fromUtf8(form);
        }
        auto strStr = plurals.isEmpty() ? QString::fromUtf8(str) : plurals.first();
        // NOTE: PO header has empty id. We skip it.
        if (!id.isEmpty()) {
            auto normalKey = context + "|" + id;
            newMapping.insert(normalKey, { strStr, fuzzy, plurals });
            if (!disambiguation.isEmpty()) {
                auto disambiguationKey = context + "|" + id + "@" + disambiguation;
                newMapping_disambiguation.insert(disambiguationKey, { strStr, fuzzy, plurals });
            }
        }
        context.clear();
        disambiguation.clear();
        id.clear();
        str.clear();
        pluralStrs.clear();
        fuzzy = nextFuzzy;
        nextFuzzy = false;
    };
    while (!file.atEnd()) {
        ParserArray line = file.readLine();
        if (line.endsWith('\n')) {
            line.resize(line.size() - 1);
        }
        if (line.endsWith('\r')) {
            line.resize(line.size() - 1);
        }

        if (!line.size()) {
            // NIL
        } else if (line[0] == '#') {
            if (line.contains(", fuzzy")) {
                nextFuzzy = true;
            }
        } else if (line.startsWith('"')) {
            QByteArray* out = nullptr;

            switch (mode) {
                case Mode::First:
                    qDebug() << "Unexpected escaped string during initial state... line:" << lineNumber;
                    return;
                case Mode::MessageString:
                    out = pluralStrs.isEmpty() ? &str : &pluralStrs.last();
                    break;
                case Mode::MessageContext:
                    out = &context;
                    break;
                case Mode::MessageId:
                    out = &id;
                    break;
            }
            if (!line.chompString(*out)) {
                qDebug() << "Badly formatted string on line:" << lineNumber;
                return;
            }
        } else if (line.chomp("msgctxt ", 8)) {
            switch (mode) {
                case Mode::First:
                    break;
                case Mode::MessageString:
                    endEntry();
                    break;
                case Mode::MessageContext:
                case Mode::MessageId:
                    qDebug() << "Unexpected msgctxt line:" << lineNumber;
                    return;
            }
            if (line.chompString(context)) {
                auto parts = context.split('|');
                context = parts[0];
                if (parts.size() > 1 && !parts[1].isEmpty()) {
                    disambiguation = parts[1];
                }
                mode = Mode::MessageContext;
            }
        } else if (line.chomp("msgid ", 6)) {
            switch (mode) {
                case Mode::MessageContext:
                case Mode::First:
                    break;
                case Mode::MessageString:
                    endEntry();
                    break;
                case Mode::MessageId:
                    qDebug() << "Unexpected msgid line:" << lineNumber;
                    return;
            }
            if (line.chompString(id)) {
                mode = Mode::MessageId;
            }
        } else if (line.chomp("msgid_plural ", 13)) {
            // the plural of the source text: the translator is asked for the text with the count, and the plural is not a key
            if (mode != Mode::MessageId) {
                qDebug() << "Unexpected msgid_plural line:" << lineNumber;
                return;
            }
            QByteArray ignored;
            if (!line.chompString(ignored)) {
                qDebug() << "Badly formatted plural on line:" << lineNumber;
                return;
            }
        } else if (line.startsWith("msgstr[")) {
            if (mode != Mode::MessageId && mode != Mode::MessageString) {
                qDebug() << "Unexpected msgstr[] line:" << lineNumber;
                return;
            }
            const auto close = line.indexOf("] ");
            if (close < 0) {
                qDebug() << "Badly formatted msgstr[] on line:" << lineNumber;
                return;
            }
            line.remove(0, close + 2);
            pluralStrs.append(QByteArray());
            if (line.chompString(pluralStrs.last())) {
                mode = Mode::MessageString;
            }
        } else if (line.chomp("msgstr ", 7)) {
            switch (mode) {
                case Mode::First:
                case Mode::MessageString:
                case Mode::MessageContext:
                    qDebug() << "Unexpected msgstr line:" << lineNumber;
                    return;
                case Mode::MessageId:
                    break;
            }
            if (line.chompString(str)) {
                mode = Mode::MessageString;
            }
        } else {
            qDebug() << "I did not understand line:" << lineNumber << ":" << QString::fromUtf8(line);
        }
        lineNumber++;
    }
    endEntry();
    mapping = std::move(newMapping);
    mapping_disambiguatrion = std::move(newMapping_disambiguation);
    loaded = true;
}

POTranslator::POTranslator(const QString& filename, QObject* parent) : QTranslator(parent)
{
    d = new POTranslatorPrivate;
    d->filename = filename;
    d->reload();
}

POTranslator::~POTranslator()
{
    delete d;
}

/// The text of an entry for a count. The files don't say how many forms there are in a way that is read here, so the first form is
/// for one, and the second for any other count, as in English and the languages like it; with no count, or one form only, it is the
/// first.
static QString textFor(const POEntry& entry, int n)
{
    if (n < 0 || entry.plurals.size() < 2) {
        return entry.text;
    }
    return entry.plurals.at(n == 1 ? 0 : 1);
}

QString POTranslator::translate(const char* context, const char* sourceText, const char* disambiguation, int n) const
{
    if (disambiguation) {
        auto disambiguationKey = QByteArray(context) + "|" + QByteArray(sourceText) + "@" + QByteArray(disambiguation);
        auto iter = d->mapping_disambiguatrion.find(disambiguationKey);
        if (iter != d->mapping_disambiguatrion.end()) {
            auto& entry = *iter;
            if (entry.text.isEmpty()) {
                qDebug() << "Translation entry has no content:" << disambiguationKey;
            }
            if (entry.fuzzy) {
                qDebug() << "Translation entry is fuzzy:" << disambiguationKey << "->" << entry.text;
            }
            return textFor(entry, n);
        }
    }
    auto key = QByteArray(context) + "|" + QByteArray(sourceText);
    auto iter = d->mapping.find(key);
    if (iter != d->mapping.end()) {
        auto& entry = *iter;
        if (entry.text.isEmpty()) {
            qDebug() << "Translation entry has no content:" << key;
        }
        if (entry.fuzzy) {
            qDebug() << "Translation entry is fuzzy:" << key << "->" << entry.text;
        }
        return textFor(entry, n);
    }
    return QString();
}

bool POTranslator::isEmpty() const
{
    return !d->loaded;
}
