#include <doctest/doctest.h>
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cmd/cereal.hpp>
#include <cmd/cmd.hpp>
#include <fstream>

struct MyCommand {
    int something;

    auto operator==(const MyCommand&) const -> bool = default;

    friend class cereal::access;
    template<class Archive>
    void serialize(Archive& archive)
    {
        archive(
            cereal::make_nvp("something", something)
        );
    }
};

struct Merger {
    [[nodiscard]] auto merge(MyCommand, MyCommand) const -> std::optional<MyCommand>
    {
        return std::nullopt;
    }
};

TEST_CASE("Saving then loading History keeps content")
{
    const auto                               merger = Merger{};
    cmd::HistoryWithSerialization<MyCommand> history1;
    cmd::HistoryWithSerialization<MyCommand> history2;

    MyCommand command{
        .something = 14,
    };

    history1.push(command, merger);

    command.something = 16;

    history1.push(command, merger);

    {
        std::ofstream             file("./test.json");
        cereal::JSONOutputArchive archive{file};
        archive(history1);
    }
    {
        std::ifstream            file("./test.json");
        cereal::JSONInputArchive archive{file};
        archive(history2);
    }

    CHECK(history1 == history2);
}
