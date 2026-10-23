#include "POTranslator.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include "FileSystem.h"

struct POEntry {
    QString text;
    bool fuzzy;
    /// The forms of a plural entry, the first of which is `text`; empty when the entry has no plural
    QStringList plurals;
};

/// The "plural=" rule of the header of a catalog ("nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : ...);"): the expression, in the
/// subset of C that gettext allows, that gives the index of the form for a count.
class PluralRule {
   public:
    /// The rule of the header text (the msgstr of the entry with the empty msgid), or an invalid one when it has none that can be read
    static PluralRule fromHeader(const QString& header)
    {
        PluralRule rule;
        static const QRegularExpression s_forms(R"(nplurals\s*=\s*(\d+))");
        static const QRegularExpression s_expression(R"(plural\s*=\s*([^;]*);?)");
        const auto forms = s_forms.match(header);
        const auto expression = s_expression.match(header);
        if (!forms.hasMatch() || !expression.hasMatch()) {
            return rule;
        }
        rule.m_forms = forms.captured(1).toInt();
        if (rule.m_forms < 1 || !rule.tokenize(expression.captured(1))) {
            rule.m_forms = 0;
            return rule;
        }
        // an expression that doesn't parse to its end is not a rule
        qsizetype position = 0;
        rule.evaluateTernary(0, position);
        if (rule.m_failed || position != rule.m_tokens.size()) {
            rule.m_forms = 0;
        }
        return rule;
    }

    bool isValid() const { return m_forms > 0; }
    int forms() const { return m_forms; }

    /// The index of the form for the count, or -1 when the rule can't give one
    int formFor(unsigned long n) const
    {
        if (!isValid()) {
            return -1;
        }
        qsizetype position = 0;
        const auto index = evaluateTernary(static_cast<long long>(n), position);
        return (index >= 0 && index < m_forms) ? static_cast<int>(index) : -1;
    }

   private:
    // the tokens: numbers, "n", and the operators and brackets, each as text
    QStringList m_tokens;
    int m_forms = 0;
    mutable bool m_failed = false;

    bool tokenize(const QString& text)
    {
        qsizetype i = 0;
        while (i < text.size()) {
            const QChar c = text.at(i);
            if (c.isSpace()) {
                i++;
            } else if (c.isDigit()) {
                qsizetype end = i;
                while (end < text.size() && text.at(end).isDigit()) {
                    end++;
                }
                m_tokens << text.mid(i, end - i);
                i = end;
            } else if (c == 'n') {
                m_tokens << "n";
                i++;
            } else {
                static const QStringList s_operators = { "==", "!=", "<=", ">=", "&&", "||", "<", ">", "!",
                                                         "?",  ":",  "%",  "*",  "/",  "+",  "-", "(", ")" };
                const auto found = std::ranges::find_if(s_operators, [&](const QString& op) { return text.mid(i, op.size()) == op; });
                if (found == s_operators.end()) {
                    return false;
                }
                m_tokens << *found;
                i += found->size();
            }
        }
        return !m_tokens.isEmpty();
    }

    bool isNext(const QString& token, qsizetype position) const { return position < m_tokens.size() && m_tokens.at(position) == token; }

    // The grammar of C, in the order of its precedence. Every branch is evaluated, which is harmless: there is nothing in it that
    // changes anything, and a division by zero is zero.
    long long evaluateTernary(long long n, qsizetype& position) const
    {
        const auto condition = evaluateOr(n, position);
        if (!isNext("?", position)) {
            return condition;
        }
        position++;
        const auto whenTrue = evaluateTernary(n, position);
        if (!isNext(":", position)) {
            m_failed = true;
            return 0;
        }
        position++;
        const auto whenFalse = evaluateTernary(n, position);
        return condition != 0 ? whenTrue : whenFalse;
    }

    long long evaluateOr(long long n, qsizetype& position) const
    {
        auto value = evaluateAnd(n, position);
        while (isNext("||", position)) {
            position++;
            const auto right = evaluateAnd(n, position);
            value = (value != 0 || right != 0) ? 1 : 0;
        }
        return value;
    }

    long long evaluateAnd(long long n, qsizetype& position) const
    {
        auto value = evaluateEquality(n, position);
        while (isNext("&&", position)) {
            position++;
            const auto right = evaluateEquality(n, position);
            value = (value != 0 && right != 0) ? 1 : 0;
        }
        return value;
    }

    long long evaluateEquality(long long n, qsizetype& position) const
    {
        auto value = evaluateRelation(n, position);
        while (isNext("==", position) || isNext("!=", position)) {
            const bool equal = m_tokens.at(position) == "==";
            position++;
            const auto right = evaluateRelation(n, position);
            value = ((value == right) == equal) ? 1 : 0;
        }
        return value;
    }

    long long evaluateRelation(long long n, qsizetype& position) const
    {
        auto value = evaluateSum(n, position);
        while (isNext("<", position) || isNext(">", position) || isNext("<=", position) || isNext(">=", position)) {
            const auto op = m_tokens.at(position);
            position++;
            const auto right = evaluateSum(n, position);
            if (op == "<") {
                value = value < right;
            } else if (op == ">") {
                value = value > right;
            } else if (op == "<=") {
                value = value <= right;
            } else {
                value = value >= right;
            }
        }
        return value;
    }

    long long evaluateSum(long long n, qsizetype& position) const
    {
        auto value = evaluateProduct(n, position);
        while (isNext("+", position) || isNext("-", position)) {
            const bool add = m_tokens.at(position) == "+";
            position++;
            const auto right = evaluateProduct(n, position);
            value = add ? value + right : value - right;
        }
        return value;
    }

    long long evaluateProduct(long long n, qsizetype& position) const
    {
        auto value = evaluateUnary(n, position);
        while (isNext("*", position) || isNext("/", position) || isNext("%", position)) {
            const auto op = m_tokens.at(position);
            position++;
            const auto right = evaluateUnary(n, position);
            if (op == "*") {
                value = value * right;
            } else if (right == 0) {
                value = 0;
            } else {
                value = op == "/" ? value / right : value % right;
            }
        }
        return value;
    }

    long long evaluateUnary(long long n, qsizetype& position) const
    {
        if (isNext("!", position)) {
            position++;
            return evaluateUnary(n, position) == 0 ? 1 : 0;
        }
        if (isNext("-", position)) {
            position++;
            return -evaluateUnary(n, position);
        }
        if (position >= m_tokens.size()) {
            m_failed = true;
            return 0;
        }
        const auto token = m_tokens.at(position);
        position++;
        if (token == "n") {
            return n;
        }
        if (token == "(") {
            const auto value = evaluateTernary(n, position);
            if (!isNext(")", position)) {
                m_failed = true;
                return 0;
            }
            position++;
            return value;
        }
        bool ok = false;
        const auto number = token.toLongLong(&ok);
        if (!ok) {
            m_failed = true;
            return 0;
        }
        return number;
    }
};

struct POTranslatorPrivate {
    PluralRule pluralRule;
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
    // the plural of the source text can go on over several lines, and is no part of the key
    QByteArray pluralId;
    bool inPlural = false;
    PluralRule newRule;
    bool fuzzy = false;
    bool nextFuzzy = false;

    enum class Mode { First, MessageContext, MessageId, MessageString } mode = Mode::First;

    int lineNumber = 0;
    QHash<QByteArray, POEntry> newMapping;
    QHash<QByteArray, POEntry> newMapping_disambiguation;
    auto endEntry = [&]() {
        // The header is the entry with no text to translate; it says how the counts of this language go over the forms
        if (id.isEmpty() && context.isEmpty() && !str.isEmpty() && !newRule.isValid()) {
            newRule = PluralRule::fromHeader(QString::fromUtf8(str));
        }
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
        pluralId.clear();
        inPlural = false;
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
                    out = inPlural ? &pluralId : &id;
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
            inPlural = true;
            if (!line.chompString(pluralId)) {
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
    pluralRule = newRule;
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

/// The text of an entry for a count. The rule of the catalog says which form it is; without one (or when it gives none of the forms)
/// the first form is for one, and the second for any other count, as in English and the languages like it. With no count, or one form
/// only, it is the first.
static QString textFor(const POEntry& entry, const PluralRule& rule, int n)
{
    if (n < 0 || entry.plurals.size() < 2) {
        return entry.text;
    }
    auto form = rule.formFor(static_cast<unsigned long>(n));
    if (form < 0) {
        form = n == 1 ? 0 : 1;
    }
    return entry.plurals.at(std::min<qsizetype>(form, entry.plurals.size() - 1));
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
            return textFor(entry, d->pluralRule, n);
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
        return textFor(entry, d->pluralRule, n);
    }
    return QString();
}

bool POTranslator::isEmpty() const
{
    return !d->loaded;
}
