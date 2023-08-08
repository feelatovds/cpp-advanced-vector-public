#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

#include <iostream>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
    : buffer_(Allocate(capacity))
    , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Deallocate(buffer_);
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

public:
    Vector() = default;

    explicit Vector(size_t size)
    : data_(size)
    , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    Vector(const Vector& other)
    : data_(other.size_)
    , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }
    Vector(Vector&& other) noexcept
    : data_(other.size_)
    , size_(other.size_)  //
    {
        data_ = std::move(other.data_);
        size_ = std::exchange(other.size_, 0);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (rhs.size_ < size_) {
                    for (size_t i = 0; i < rhs.size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    DestroyN(data_ + rhs.size_, size_ - rhs.size_);
                    size_ = rhs.size_;
                } else {
                    for (size_t i = 0; i < size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_.GetAddress());
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }
    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }
    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    void Resize(size_t new_size) {
        if (new_size < size_) {
            DestroyN(data_ + new_size, size_ - new_size);
        } else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }

        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            Emplace(begin() + size_, value);
        } else {
            new (data_ + size_) T(value);
            ++size_;
        }
    }
    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            Emplace(begin() + size_, std::move(value));
        } else {
            new (data_ + size_) T(std::move(value));
            ++size_;
        }
    }

    void PopBack() noexcept {
        if (size_ != 0) {
            Destroy(data_ + (size_ - 1));
            --size_;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            return *Emplace(begin() + size_, std::forward<Args>(args)...);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        auto delta = pos - begin();

        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + delta) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                TryInitMove(delta, new_data);
            } else {
                TryInitCopy(delta, new_data);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_ = std::move(new_data);

        } else {
            if (size_ == 0) {
                new (begin()) T(std::forward<Args>(args)...);
            } else {
                T tmp(std::forward<Args>(args)...);
                new (end()) T(std::forward<T>(*(end() - 1)));
                std::move_backward(begin() + delta, end() - 1, end());
                data_[delta] = std::move(tmp);
            }
        }

        ++size_;
        return begin() + delta;
    }

    iterator Erase(const_iterator pos) noexcept {
        auto delta = pos - begin();
        for (size_t i = delta + 1; i < size_; ++i) {
            data_[i - 1] = std::move(data_[i]);
        }
        Destroy(end() - 1);
        --size_;
        return begin() + delta;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

private:
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    static void Destroy(T* buf) noexcept {
        std::destroy_at(buf);
    }

private:
    void TryInitMove(int delta, RawMemory<T>& new_data) {
        try {
            std::uninitialized_move_n(data_.GetAddress(), delta, new_data.GetAddress());
        } catch (...) {
            Destroy(new_data + delta);
            throw ;
        }
        try {
            std::uninitialized_move_n(data_ + delta, size_ - delta, new_data + delta + 1);
        } catch (...) {
            DestroyN(new_data.GetAddress(), delta + 1);
            throw ;
        }
    }

    void TryInitCopy(int delta, RawMemory<T>& new_data) {
        try {
            std::uninitialized_copy_n(data_.GetAddress(), delta, new_data.GetAddress());
        } catch (...) {
            Destroy(new_data + delta);
            throw ;
        }
        try {
            std::uninitialized_copy_n(data_ + delta, size_ - delta, new_data + delta + 1);
        } catch (...) {
            DestroyN(new_data.GetAddress(), delta + 1);
            throw ;
        }
    }

};
