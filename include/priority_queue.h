#ifndef priority_queue_h
#define priority_queue_h

#include <iostream>
#include <algorithm> // for std::swap
#include <Arduino.h>

#ifndef Arduino_h
#define PR_QUEUE_EXCEPTION_HANDLING
#endif


/*
template <typename T, size_t Capacity, typename Compare>
class StaticPriorityQueue {
private:
    T data[Capacity];
    size_t size;
    Compare comp
*/

template <typename T, size_t Capacity>
class StaticPriorityQueue {
private:
    T data[Capacity];
    size_t size;
    const static inline bool comp(my_pair left, my_pair right) {
                const uint32_t now_time = millis();
                bool overflow_1 = false, overflow_2 = false;

                if (now_time - left.first  < left.first  - now_time) overflow_1 = true;
                if (now_time - right.first < right.first - now_time) overflow_2 = true;
  
                if (overflow_1 ^ overflow_2) {
                    // случай (1, 0), (0, 1)
                    return overflow_1 < overflow_2;
                } else {
                    // случай (0, 0), (1, 1)
                    return left.first > right.first;
                }
    };;
// use of deleted function '<lambda(std::pair<long unsigned int, std::function<void()>*>, std::pair<long unsigned int, std::function<void()>*>)>::<lambda>()'
public:
    StaticPriorityQueue() { size = 0; }

//    StaticPriorityQueue (const Compare &__x): comp(__x) {  size = 0; }



    bool empty() const {
        return size == 0;
    }

    size_t _size() const {
        return size;
    }

    const T& top() const {
#ifdef PR_QUEUE_EXCEPTION_HANDLING
        if (empty()) {
            throw std::out_of_range("Priority queue is empty");
        }
#endif
        return data[0];
    }

    void push(const T& value) {
#ifdef PR_QUEUE_EXCEPTION_HANDLING
        if (size >= Capacity) {
            throw std::overflow_error("Priority queue is full");
        }
#endif
        data[size++] = value;
        std::push_heap(data, data + size, comp);
    }

    void pop() {
#ifdef PR_QUEUE_EXCEPTION_HANDLING
        if (empty()) {
            throw std::out_of_range("Priority queue is empty");
        }
#else
        if (empty()) return;
#endif
        std::pop_heap(data, data + size, comp);
        size--;
    }
};



// int main() {
//     StaticPriorityQueue<int, 5> pq; // Example with integers and capacity of 5

//     pq.push(10);
//     pq.push(30);
//     pq.push(20);
//     pq.push(10);
//     pq.push(1);
    
    

//     std::cout << "Top element: " << pq.top() << std::endl; // Output: 30

//     pq.pop();

//     std::cout << "Top element after pop: " << pq.top() << std::endl; // Output: 20
//     pq.pop();
// std::cout << "Top element after pop: " << pq.top() << std::endl; // Output: 20
//     pq.pop();
// std::cout << "Top element after pop: " << pq.top() << std::endl; // Output: 20
//     pq.pop();
// std::cout << "Top element after pop: " << pq.top() << std::endl; // Output: 20
//     return 0;
// }
#endif