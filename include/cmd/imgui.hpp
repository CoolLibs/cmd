#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include "cmd.hpp"

namespace cmd {

namespace internal {

template<typename T>
auto size_as_string(float multiplier) -> std::string
{
    const auto total_size_in_megabytes = static_cast<float>(sizeof(T))
                                         * multiplier
                                         / 1'000'000.f;
    // return std::format("{:.2f} Mb", total_size_in_megabytes); // Compilers don't support std::format() just yet :(
    std::stringstream stream;
    stream << std::fixed << std::setprecision(2) << total_size_in_megabytes << " Mb";
    return stream.str();
}

inline void imgui_help_marker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

struct InputResult {
    bool is_item_deactivated_after_edit;
    bool is_item_active;
};

template<Command CommandT>
inline auto imgui_input_history_size(size_t* value, size_t previous_value, int uuid)
{
    static_assert(sizeof(size_t) == 8, "The ImGui widget expects a u64 integer");
    ImGui::PushID(uuid);
    ImGui::SetNextItemWidth(12.f + ImGui::CalcTextSize(std::to_string(*value).c_str()).x); // Adapt the widget size to exactly fit the text input
    ImGui::InputScalar("", ImGuiDataType_U64, value);
    ImGui::PopID();
    const auto ret = InputResult{
        .is_item_deactivated_after_edit = ImGui::IsItemDeactivatedAfterEdit(),
        .is_item_active                 = ImGui::IsItemActive(),
    };
    ImGui::SameLine();
    ImGui::Text("commits (%s)", internal::size_as_string<CommandT>(static_cast<float>(*value)).c_str());
    if (*value != previous_value)
    {
        ImGui::TextDisabled("Previously: %lld", previous_value);
    }
    return ret;
}

} // namespace internal

struct UiForHistory {
    std::chrono::steady_clock::time_point last_push_date{std::chrono::steady_clock::now()};
    size_t                                uncommited_max_size{};

    template<Command CommandT>
    void push(History<CommandT>& history, const CommandT& command)
    {
        last_push_date = std::chrono::steady_clock::now();
        history.push(command);
    }

    template<Command CommandT>
    void push(History<CommandT>& history, CommandT&& command)
    {
        last_push_date = std::chrono::steady_clock::now();
        history.push(std::move(command));
    }

    template<Command CommandT, typename CommandToString>
    void imgui_show(const History<CommandT>& history, CommandToString&& command_to_string)
    {
        const auto& commands = history.underlying_container();
        bool        drawn    = false;
        for (auto it = commands.cbegin(); it != commands.cend(); ++it)
        {
            if (it == history.current_command_iterator())
            {
                drawn = true;
                ImGui::Separator();
                using namespace std::chrono_literals;
                // if (time_since_last_push() < 500ms)
                {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::Text("%s", command_to_string(*it).c_str());
        }
        if (!drawn)
        {
            ImGui::Separator();
        }
    }

    template<Command CommandT>
    auto imgui_max_size(History<CommandT>& history) -> bool
    {
        ImGui::Text("History maximum size");
        internal::imgui_help_marker(
            "This is how far you can go back in the history, "
            "i.e. the number of undo you can perform.");
        const auto res = internal::imgui_input_history_size<CommandT>(
            &uncommited_max_size,
            history.max_size(),
            1354321);
        if (res.is_item_deactivated_after_edit)
        {
            history.set_max_size(uncommited_max_size);
        }
        if (!res.is_item_active) // Sync with the current max_size if we are not editing // Must be after the check for IsItemDeactivatedAfterEdit() otherwise the value can't be set properly when we finish editing
        {
            uncommited_max_size = history.max_size();
        }
        if (uncommited_max_size < history.size())
        {
            ImGui::TextColored({1.f, 1.f, 0.f, 1.f},
                               "Some commits will be erased because you are reducing the size of the history!\nThe current size is %lld.",
                               history.size());
        }
        return res.is_item_deactivated_after_edit;
    }

    auto time_since_last_push() const { return std::chrono::steady_clock::now() - last_push_date; }
};

template<Command CommandT>
class HistoryWithUi {
public:
    template<typename CommandToString>
    void imgui_show(CommandToString&& command_to_string)
    {
        _ui.imgui_show(_history, std::forward<CommandToString>(command_to_string));
    }

    auto imgui_max_size() -> bool { return _ui.imgui_max_size(_history); }

    // ---Boilerplate to replicate the API of an History---
    explicit HistoryWithUi(size_t max_size = 1000)
        : _history{max_size} {}
    void push(const CommandT& command) { _ui.push(_history, command); }
    void push(CommandT&& command) { _ui.push(_history, std::move(command)); }
    template<typename ExecutorT>
    requires Executor<ExecutorT, CommandT>
    void move_forward(ExecutorT& executor)
    {
        _history.move_forward(executor);
    }
    template<typename ReverterT>
    requires Reverter<ReverterT, CommandT>
    void move_backward(ReverterT& reverter)
    {
        _history.move_backward(reverter);
    }
    // ---End of boilerplate---

private:
    History<CommandT> _history;
    UiForHistory      _ui{};
};

} // namespace cmd