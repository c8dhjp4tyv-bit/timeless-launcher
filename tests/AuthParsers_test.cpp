#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <minecraft/auth/Parsers.h>

class AuthParsersTest : public QObject {
    Q_OBJECT

    /// The profile of a player as the session server gives it, with what the textures property holds
    static QByteArray profile(const QByteArray& textures)
    {
        const QJsonObject property{ { "name", "textures" }, { "value", QString::fromLatin1(textures.toBase64()) } };
        return QJsonDocument(QJsonObject{ { "id", "069a79f444e94726a5befca90e38aaf5" },
                                          { "name", "Notch" },
                                          { "properties", QJsonArray{ property } } })
            .toJson();
    }

   private slots:
    void mojangProfile()
    {
        const QJsonObject skin{ { "url", "http://textures.minecraft.net/texture/abc" },
                                { "metadata", QJsonObject{ { "model", "slim" } } } };
        auto data = profile(QJsonDocument(QJsonObject{ { "textures", QJsonObject{ { "SKIN", skin } } } }).toJson());

        MinecraftProfile parsed;
        QVERIFY(Parsers::parseMinecraftProfileMojang(data, parsed));
        QCOMPARE(parsed.name, "Notch");
        QCOMPARE(parsed.skin.url, "https://textures.minecraft.net/texture/abc");
        QCOMPARE(parsed.skin.variant, "slim");
    }

    void mojangProfileThatIsNotWhatItShouldBe_data()
    {
        QTest::addColumn<QByteArray>("data");

        // A server, or whatever answers in its place, can give any JSON. The result is that the profile can't be read, and not an
        // exception that ends the launcher.
        QTest::newRow("not JSON") << QByteArray("<html>Bad gateway</html>");
        QTest::newRow("an array") << QByteArray("[]");
        QTest::newRow("a string") << QByteArray("\"profile\"");
        QTest::newRow("null") << QByteArray("null");
        QTest::newRow("textures that are an array") << profile("[]");
        QTest::newRow("textures that are a string") << profile("\"textures\"");
        QTest::newRow("textures that are not JSON") << profile("textures");
        QTest::newRow("no textures") << profile("{}");
    }

    void mojangProfileThatIsNotWhatItShouldBe()
    {
        QFETCH(const QByteArray, data);

        auto copy = data;
        MinecraftProfile parsed;
        QVERIFY(!Parsers::parseMinecraftProfileMojang(copy, parsed));
    }
};

QTEST_GUILESS_MAIN(AuthParsersTest)

#include "AuthParsers_test.moc"
