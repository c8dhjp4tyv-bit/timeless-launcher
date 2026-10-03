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

    void mojangError_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QString>("expected");

        const QString unknownApp = "Invalid app registration, see https://aka.ms/AppRegInfo for more information";
        auto refusal = [](const QJsonObject& more) {
            QJsonObject object{ { "path", "/launcher/login" }, { "errorType", "FORBIDDEN" }, { "error", "FORBIDDEN" } };
            for (auto it = more.begin(); it != more.end(); ++it) {
                object.insert(it.key(), it.value());
            }
            return QJsonDocument(object).toJson(QJsonDocument::Compact);
        };

        QTest::newRow("a message") << refusal({ { "errorMessage", unknownApp }, { "developerMessage", "Forbidden" } }) << unknownApp;
        QTest::newRow("only the message for developers") << refusal({ { "developerMessage", unknownApp } }) << unknownApp;
        QTest::newRow("a message of nothing") << refusal({ { "errorMessage", "  " }, { "developerMessage", unknownApp } }) << unknownApp;
        QTest::newRow("blanks around it") << refusal({ { "errorMessage", "  Forbidden\n" } }) << "Forbidden";
        QTest::newRow("a message that is no text") << refusal({ { "errorMessage", 403 } }) << "";
        QTest::newRow("no message") << refusal({}) << "";
        QTest::newRow("a message that goes on and on") << refusal({ { "errorMessage", QString(1000, 'x') } }) << QString(300, 'x');
        QTest::newRow("nothing") << QByteArray() << "";
        QTest::newRow("not JSON") << QByteArray("<html>Forbidden</html>") << "";
        QTest::newRow("an array") << QByteArray("[\"Forbidden\"]") << "";
        QTest::newRow("null") << QByteArray("null") << "";
    }

    void mojangError()
    {
        QFETCH(const QByteArray, data);
        QFETCH(const QString, expected);

        QCOMPARE(Parsers::parseMojangError(data), expected);
    }

    void minecraftEntitlements_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("understood");
        QTest::addColumn<bool>("owns");
        QTest::addColumn<bool>("canPlay");

        const auto answer = [](const QStringList& names) {
            QJsonArray items;
            for (const auto& name : names) {
                items.append(QJsonObject{ { "name", name }, { "signature", "x" } });
            }
            return QJsonDocument(QJsonObject{ { "items", items }, { "signature", "x" }, { "keyId", "1" } }).toJson(QJsonDocument::Compact);
        };

        QTest::newRow("the game") << answer({ "product_minecraft", "game_minecraft" }) << true << true << true;
        QTest::newRow("game pass") << answer({ "game_minecraft" }) << true << false << true;
        QTest::newRow("owns it, can't play yet") << answer({ "product_minecraft" }) << true << true << false;
        QTest::newRow("other things only") << answer({ "game_minecraft_bedrock" }) << true << false << false;
        // an account that owns nothing gets a list with nothing in it...
        QTest::newRow("an empty list") << answer({}) << true << false << false;
        // ...and what is not a list of what the account owns is not read as an account that owns nothing
        QTest::newRow("a refusal") << QByteArray(
                                          R"({"path":"/entitlements/license","errorType":"TOO_MANY_REQUESTS","error":"TOO_MANY_REQUESTS"})")
                                   << false << true << true;
        QTest::newRow("items that are not a list") << QByteArray(R"({"items":5})") << false << true << true;
        QTest::newRow("an object with nothing in it") << QByteArray("{}") << false << true << true;
        QTest::newRow("an array") << QByteArray("[]") << false << true << true;
        QTest::newRow("not JSON") << QByteArray("<html>Bad gateway</html>") << false << true << true;
        QTest::newRow("nothing") << QByteArray() << false << true << true;
    }

    void minecraftEntitlements()
    {
        QFETCH(const QByteArray, data);
        QFETCH(const bool, understood);
        QFETCH(const bool, owns);
        QFETCH(const bool, canPlay);

        // an account that was found to own the game, and what is read from the answer goes over it
        MinecraftEntitlement entitlement;
        entitlement.ownsMinecraft = true;
        entitlement.canPlayMinecraft = true;

        auto copy = data;
        QCOMPARE(Parsers::parseMinecraftEntitlements(copy, entitlement), understood);
        QCOMPARE(entitlement.ownsMinecraft, owns);
        QCOMPARE(entitlement.canPlayMinecraft, canPlay);
    }
};

QTEST_GUILESS_MAIN(AuthParsersTest)

#include "AuthParsers_test.moc"
