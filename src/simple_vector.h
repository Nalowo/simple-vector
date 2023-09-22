#pragma once

#include "array_ptr.h"

#include <cassert>
#include <initializer_list>
#include <algorithm>
#include <stdexcept>

struct ReserveProxyObj {
    explicit ReserveProxyObj(size_t value): value_to_reserve(value){}
    size_t value_to_reserve = 0;
};

template <typename Type>
class SimpleVector {

public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size): items_(ArrayPtr<Type>(size)), size_(size), capacity_(size){
        std::generate(items_.Get(), items_.Get() + size, [](){return Type{};});
        //в предыдущем варианте, я тут создавал массив через конструктор ArrayPtr<Type> принимающий сырой указатель, для того чтобы избежать лишнего прохода по массиву алгоритмом, там я сразу получал массив со значениями по умолчанию, а тут мне его нужно наполнять
    }

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, const Type& value): items_(ArrayPtr<Type>(size)), size_(size), capacity_(size){
        std::fill(items_.Get(), items_.Get() + size_, value);
    }

    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init): items_(ArrayPtr<Type>(init.size())), size_(init.size()), capacity_(init.size()) {
        std::move(init.begin(), init.end(), items_.Get());
    }

    SimpleVector(const SimpleVector<Type>& other): items_(ArrayPtr<Type>(other.GetSize())) {
        std::copy(other.begin(), other.end(), begin());
        size_ = other.GetSize();
        capacity_ = other.GetSize();
    }

    SimpleVector(SimpleVector<Type>&& other): SimpleVector() {
        items_ = std::move(other.items_);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    SimpleVector(ReserveProxyObj capacity_to_reserve): SimpleVector(){
        Reserve(capacity_to_reserve.value_to_reserve);
    }

    // Возвращает количество элементов в массиве
    size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        assert(index < size_);
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        assert(index < size_);
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        if (index >= size_) throw std::out_of_range("called index out of range");
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        if (index >= size_) throw std::out_of_range("called index out of range");
        return items_[index];
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity < capacity_) {
            return;
        } else if (new_capacity >= capacity_) {
            ArrayPtr<Type> buff(new_capacity);
            std::move(begin(), end(), &buff[0]);
            items_.swap(buff);
            capacity_ = new_capacity;
        } else {
            ArrayPtr<Type> buff(new_capacity);
            items_.swap(buff);
            capacity_ = new_capacity;
        }
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            if (new_size > capacity_) {
                size_t new_capacity = new_size * 2;
                ArrayPtr<Type> buff(new_capacity);
                auto start = std::move(begin(), end(), &buff[0]);
                std::generate(start, &buff[new_size], [](){return Type{};});
                items_.swap(buff);
                capacity_ = new_capacity;
                size_ = new_size;

            } else {
                std::generate(&items_[size_], &items_[new_size - 1], [](){return Type{};});
                size_ = new_size;
            }
        } else {
            size_ = new_size;
        }
    }

    SimpleVector<Type>& operator=(const SimpleVector<Type>& rhs) {
        SimpleVector<Type> buff(rhs);
        this->swap(buff);
        return *this;
    }

    SimpleVector<Type>& operator=(SimpleVector<Type>&& rhs) {
        items_ = std::move(rhs.items_);
        size_ = std::exchange(rhs.size_, 0);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    void PushBack(const Type& item) {
        if ((size_ + 1) > capacity_) {
            size_t new_capacity = (capacity_ == 0 ? size_ + 1 : size_) * 2;
            ArrayPtr<Type> buff(new Type[new_capacity]);
            std::copy(begin(), end(), &buff[0]);
            buff[size_] = item;
            items_.swap(buff);
            capacity_ = new_capacity;
            ++size_;
        } else {
            *this->end() = item;
            ++size_;
        }
    }

    void PushBack(Type&& item) {
        if ((size_ + 1) > capacity_) {
            size_t new_capacity = (capacity_ == 0 ? size_ + 1 : size_) * 2;
            ArrayPtr<Type> buff(new Type[new_capacity]);
            std::move(begin(), end(), &buff[0]);
            buff[size_] = std::move(item);
            items_.swap(buff);
            capacity_ = new_capacity;
            ++size_;
        } else {
            *this->end() = std::move(item);
            ++size_;
        }
    }

    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, const Type& value) {
        if (pos < begin() || pos > end()) throw std::out_of_range("iterator out of range");
        if ((size_ + 1) > capacity_) {
            size_t new_capacity = (capacity_ == 0 ? size_ + 1 : size_) * 2;
            ArrayPtr<Type> buff(new_capacity);
            auto position_insert = std::copy(cbegin(), pos, &buff[0]);
            *position_insert = value;
            std::copy(pos, cend(), (position_insert + 1));
            items_.swap(buff);
            capacity_ = new_capacity;
            ++size_;
            return position_insert;
        }
        auto position_insert = std::copy_backward(--pos, cend(), end() + 1);
        *position_insert = value;
        ++size_;
        return position_insert;
    }

    Iterator Insert(ConstIterator pos, Type&& value) {
        if (pos < begin() || pos > end()) throw std::out_of_range("iterator out of range");
        if ((size_ + 1) > capacity_) {
            size_t new_capacity = (capacity_ == 0 ? size_ + 1 : size_) * 2;
            auto pos_not_const = const_cast<Iterator>(pos);
            ArrayPtr<Type> buff(new_capacity);
            auto position_insert = std::move(begin(), pos_not_const, &buff[0]);
            *position_insert = std::move(value);
            std::move(pos_not_const, end(), (position_insert + 1));
            items_.swap(buff);
            capacity_ = new_capacity;
            ++size_;
            return position_insert;
        }
        auto pos_not_const = const_cast<Iterator>(pos);
        auto position_insert = std::move_backward(--pos_not_const, end(), end() + 1);
        *position_insert = std::move(value);
        ++size_;
        return position_insert;
    }

    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
    }

    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) {
        assert(size_ > 0);
        std::move(const_cast<Iterator>(pos) + 1, end(), const_cast<Iterator>(pos));
        --size_;
        return const_cast<Iterator>(pos);
    }

    // Обменивает значение с другим вектором
    void swap(SimpleVector<Type>& other) noexcept {
        items_.swap(other.items_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    inline bool operator<(const SimpleVector<Type>& rhs) const {
        return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
    }

    inline bool operator>(const SimpleVector<Type>& rhs) const {
        return rhs < *this;
    }

    inline bool operator==(const SimpleVector<Type>& rhs) const {
        return (this->GetSize() == rhs.GetSize() && std::equal(this->begin(), this->end(), rhs.begin(), rhs.end()));
        // в курсе говорится что лучше реализовать операции сравнения через одну <, но ладно
    }

    inline bool operator!=(const SimpleVector<Type>& rhs) const {
        return !(*this == rhs);
    }

    inline bool operator<=(const SimpleVector<Type>& rhs) const {
        return !(rhs < *this);
    }

    inline bool operator>=(const SimpleVector<Type>& rhs) const {
        return !(*this < rhs);
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        return items_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        return items_.Get() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return items_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return items_.Get() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return items_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return items_.Get() + size_;
    }
    
private:
    ArrayPtr<Type> items_;

    size_t size_ = 0;
    size_t capacity_ = 0;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}