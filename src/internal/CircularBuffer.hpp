#pragma once

#include <cassert>
#include <list>
#include <optional>

namespace cmd::internal {

template<typename T>
class CircularBuffer {
public:
    using iterator = typename std::list<T>::iterator;

    explicit CircularBuffer(size_t max_size)
        : _max_size{max_size}
    {}

    void push_back(const T& t)
    {
        push_back_impl(t);
    }

    void push_back(T&& t)
    {
        push_back_impl(std::move(t));
    }

    auto size() const -> size_t { return _container.size(); }

    auto max_size() const -> size_t { return _max_size; }

    void set_max_size(size_t new_max_size)
    {
        _max_size = new_max_size;
        shrink_left();
    }

    void set_max_size_and_preserve_given_iterator(size_t new_max_size, iterator iterator_to_preserve)
    {
        _max_size = new_max_size;
        shrink_while_preserving(iterator_to_preserve);
    }

    void shrink_and_preserve_given_iterator(size_t new_max_size, iterator iterator_to_preserve)
    {
        const auto tmp = _max_size;
        _max_size      = new_max_size;
        shrink_while_preserving(iterator_to_preserve);
        _max_size = tmp;
    }

    void resize(size_t new_size)
    {
        _container.resize(new_size);
    }

    auto begin() { return _container.begin(); }
    auto begin() const { return _container.begin(); }
    auto end() { return _container.end(); }
    auto end() const { return _container.end(); }

    auto back() { return _container.back(); }
    void pop_back() { _container.pop_back(); }

    auto is_empty() const -> bool { return _container.empty(); }

    void erase_all_starting_at(iterator it)
    {
        _container.erase(it, _container.end());
    }

    auto underlying_container() -> std::list<T>& { return _container; }
    auto underlying_container() const -> const std::list<T>& { return _container; }

    auto operator==(const CircularBuffer<T>& other) const -> bool
    {
        return _container == other._container;
    }

private:
    template<typename Tref>
    void push_back_impl(Tref&& t)
    {
        _container.push_back(std::forward<Tref>(t));
        shrink_left();
    }

    void shrink_left()
    {
        while (_container.size() > _max_size)
        {
            _container.pop_front();
        }
    }

    void shrink_while_preserving(iterator iterator_to_preserve)
    {
        if (iterator_to_preserve == _container.end())
        {
            shrink_left();
        }
        else
        {
            if (_max_size == 0) // we need to be able to assume that _max_size > 0 in the else branch
            {
                _container.clear();
            }
            else
            {
                while (_container.size() > _max_size)
                {
                    if (iterator_to_preserve != iterator_to_last_element()) // we know that _max_size > 0 so it is safe to call iterator_to_last_element()
                    {
                        _container.pop_back();
                    }
                    else
                    {
                        _container.pop_front();
                    }
                }
            }
        }
    }

    auto iterator_to_last_element()
    {
        assert(!_container.empty());
        return std::prev(_container.end());
    }

private:
    std::list<T> _container;
    size_t       _max_size;
};

} // namespace cmd::internal
