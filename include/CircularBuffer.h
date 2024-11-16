#ifndef CircularBuffer_h
#define CircularBuffer_h

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cstring>

template <class T>
class CircularBuffer {
private:
    std::vector<T> buffer;
    int head;
    int tail;
    int capacity;

public:
    // Constructor to intialize circular buffer's data
    // members
    CircularBuffer(int capacity)
    {
        this->capacity = capacity;
        this->head = 0;
        this->tail = 0;
        buffer.resize(capacity);
    }

    // Function to add an element to the buffer
    void push_back(T element)
    {
        buffer[head] = element;
        head = (head + 1) % capacity;
        if (head == tail) {
            tail = (tail + 1) % capacity;
        }
    }

    // Function to remove an element from the buffer
    T pop()
    {
        if (empty()) {
            throw std::out_of_range("Buffer is empty");
        }
        T ret = buffer[tail];
        tail = (tail + 1) % capacity;
        return ret;
    }

    int peek(T* write_to, int number){
        if(number > size()) number = size();
        // e.g.
        //   1.2  3.5  7.8  1.3  6.5  1.4
        //   ^         ^
        //   h=0       t=2
        // then, if number = 5:
        // memcpy(write_to, buffer.data() + 2, 4);
        // memcpy(write_to + 4, buffer.data(), 1);
        int num_to_copy_first = std::max(number, capacity - tail);
        int num_to_copy_second = std::max(0, number - (capacity - tail));
        std::memcpy(write_to, buffer.data() + tail, num_to_copy_first);
        if(num_to_copy_second > 0) std::memcpy(write_to + num_to_copy_first, buffer.data(), num_to_copy_second);

        return number;
    }

    // Function to check if the buffer is empty
    bool empty() const { return head == tail; }

    // Function to check if the buffer is full
    bool full() const
    {
        return (head + 1) % capacity == tail;
    }

    // Function to get the size of the buffer
    int size() const
    {
        if (head >= tail) {
            return head - tail;
        }
        return capacity - (tail - head);
    }
};

#endif // CircularBuffer_h