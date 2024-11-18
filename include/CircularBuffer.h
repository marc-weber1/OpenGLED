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
        // If you want to get the last C items you pushed, you need a capacity of C + 1 since head doesn't count
        this->capacity = capacity + 1;
        this->head = 0;
        this->tail = 0;
        buffer.resize(capacity + 1);
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
        //   1.2  0.0  0.0  0.0  0.0  0.0
        //   ^    ^
        //   t=0  h=1
        // then, if number = 1 and typeof(T) is char:
        // memcpy(write_to, buffer.data() + 0, 1);
        // e.g. 2
        //   1.7  1.3  1.4  1.5  1.6
        //        ^    ^
        //        h=1  t=2
        // then, if number = 4, and typeof(T) is char:
        // memcpy(write_to, buffer.data() + 2, 3);
        // memcpy(write_to + 3, buffer.data(), 1);

        int num_to_copy_first = std::min(number, capacity - tail);
        int num_to_copy_second = std::max(0, number - (capacity - tail));
        std::memcpy(write_to, buffer.data() + tail, num_to_copy_first * sizeof(T));
        if(num_to_copy_second > 0) std::memcpy(write_to + num_to_copy_first, buffer.data(), num_to_copy_second * sizeof(T));

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