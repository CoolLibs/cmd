#pragma once

#include <optional>
#include <vector>
#include "Command.hpp"
#include "Executor.hpp"
#include "internal/CircularBuffer.hpp"

namespace cmd {

template<CommandC CommandT>
class History {
public:
    explicit History(size_t max_size = 1000)
        : _commands{max_size}
    {}

    History(History&&) noexcept            = default;
    History& operator=(History&&) noexcept = default;

    History clone() const
    {
        History copy{*this};
        copy.unsafe_set_next_command_to_execute( // To properly "copy" the iterator
            this->unsafe_get_next_command_to_execute()
        );
        return copy;
    }

    template<typename ExecutorT>
        requires ExecutorC<ExecutorT, CommandT>
    void move_forward(ExecutorT& executor)
    {
        if (_next_command_to_execute)
        {
            if (*_next_command_to_execute != _commands.end())
            {
                executor.execute(**_next_command_to_execute); // If execute throws, then _next_command_to_execute won't be modified and that's what we want because the exception means that the command couldn't be applied
                (*_next_command_to_execute)++;
            }
        }
        _can_try_to_merge_next_command = false;
    }

    template<typename ReverterT>
        requires ReverterC<ReverterT, CommandT>
    void move_backward(ReverterT& reverter)
    {
        if (_next_command_to_execute)
        {
            if (*_next_command_to_execute != _commands.begin())
            {
                reverter.revert(*std::prev(*_next_command_to_execute)); // If revert throws, then _next_command_to_execute won't be modified and that's what we want because the exception means that the command couldn't be reverted
                (*_next_command_to_execute)--;
            }
        }
        _can_try_to_merge_next_command = false;
    }

    template<typename MergerT>
        requires MergerC<MergerT, CommandT>
    void push(const CommandT& command, const MergerT& merger)
    {
        push_impl(command, merger);
    }

    template<typename MergerT>
        requires MergerC<MergerT, CommandT>
    void push(CommandT&& command, const MergerT& merger)
    {
        push_impl(std::move(command), merger);
    }

    /// The history is eager to merge commands: it will try to do it unless you explicitly tell it not to
    void dont_merge_next_command() const { _can_try_to_merge_next_command = false; }

    auto size() const -> size_t { return _commands.size(); }

    auto max_size() const -> size_t { return _commands.max_size(); }

    /// If you reduce max_size, we will have to delete some commits from the history.
    /// We start deleting commits that are furthest away in the future, until we reach one commit before the current one.
    /// This means that you will be able to move forward at least once after setting max_size (unless you set it to 0, or you were already at the most forward point in your history)
    /// If there is still a need to delete commits, we will then start deleting the commits that are furthest away in the past.
    /// NB: this choice was done because it was the simplest to implement, but we could consider adding other policies of which commits to keep.
    void set_max_size(size_t new_max_size)
    {
        if (_next_command_to_execute)
        {
            _commands.set_max_size_and_preserve_given_iterator(new_max_size, *_next_command_to_execute);
            if (new_max_size == 0)
            {
                _next_command_to_execute.reset();
            }
        }
        else
        {
            _commands.set_max_size(new_max_size);
        }
    }

    /// Removes commits until the size of the history is <= max_size
    void shrink(size_t max_size)
    {
        if (_next_command_to_execute)
        {
            _commands.shrink_and_preserve_given_iterator(max_size, *_next_command_to_execute);
            if (max_size == 0)
            {
                _next_command_to_execute.reset();
            }
        }
        else
        {
            // No need to do anything since _commands is already empty
            assert(_commands.is_empty());
        }
    }

    auto underlying_container() const -> const std::list<CommandT>& { return _commands.underlying_container(); }
    auto underlying_container() -> std::list<CommandT>& { return _commands.underlying_container(); }

    auto current_command_iterator() const { return _next_command_to_execute; }

    // Exposed for serialization purposes. Don't use this unless you have a really good reason to.
    void unsafe_set_next_command_to_execute(std::optional<size_t> index)
    {
        if (index)
        {
            _next_command_to_execute = _commands.begin();
            for (size_t i = 0; i < *index; ++i)
            {
                (*_next_command_to_execute)++;
            }
        }
        else
        {
            _next_command_to_execute.reset();
        }
    }

    // Exposed for serialization purposes. Don't use this unless you have a really good reason to.
    auto unsafe_get_next_command_to_execute() const -> std::optional<size_t>
    {
        if (_next_command_to_execute)
        {
            size_t dist{0};
            for (auto it = _commands.begin(); it != *_next_command_to_execute; ++it)
            {
                ++dist;
            }
            return dist;
        }
        else
        {
            return std::nullopt;
        }
    }

    auto operator==(const History<CommandT>& other) const -> bool
    {
        auto commands_equal = _commands == other._commands;
        // auto next_equal     = _next_command_to_execute == other._next_command_to_execute;
        auto next_equal = unsafe_get_next_command_to_execute() == other.unsafe_get_next_command_to_execute();
        return commands_equal && next_equal;
    }

private:
    template<typename CommandType, typename MergerType> // CommandType instead of CommandT to not override CommandT which is already the template parameter of the whole class; CommandT and CommandType need to be different otherwise perfect forwarding won't kick in
    void push_impl(CommandType&& command, const MergerType& merger)
    {
        if (_next_command_to_execute)
        {
            _commands.erase_all_starting_at(*_next_command_to_execute);
        }
        if (!_commands.is_empty()
            && _can_try_to_merge_next_command)
        {
            const auto merged = merger.merge(_commands.back(), command);
            if (merged)
            {
                _commands.pop_back();
                _commands.push_back(*merged);
            }
            else
            {
                _commands.push_back(std::forward<CommandType>(command));
            }
        }
        else
        {
            _commands.push_back(std::forward<CommandType>(command));
        }
        _next_command_to_execute       = _commands.end();
        _can_try_to_merge_next_command = true;
    }

    History(const History&)            = default; // Use `clone()` instead
    History& operator=(const History&) = default; // if you want a copy of your history

private:
    internal::CircularBuffer<CommandT>                                   _commands;
    std::optional<typename internal::CircularBuffer<CommandT>::iterator> _next_command_to_execute{};
    mutable bool                                                         _can_try_to_merge_next_command{false};
};

} // namespace cmd
