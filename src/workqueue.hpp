#ifndef USER_THREAD_WORKQUEUE_HPP
#define USER_THREAD_WORKQUEUE_HPP

#include <queue>

#include "util.hpp"

namespace orks {
namespace userthread {
namespace detail {

template<typename T>
class ThreadSafeQueue {
    std::mutex mutex;
    std::queue<T> queue;

public:

    void push(const T& t) {
        auto lock = util::make_unique_lock(mutex);
        queue.push(t);
    }

    /*
     * return true if queue is not empty
     * return false if else
     */
    bool pop(T& t) {
        auto lock = util::make_unique_lock(mutex);
        if (queue.empty()) {
            return false;
        }

        t = queue.front();
        queue.pop();
        return true;
    }

};


/*
 * pop時にnullptrが返った場合はqueueがcloseされたことを表す。
 */
template<typename T,
         template<typename U> class ThreadSafeQueue = ThreadSafeQueue>
class WorkStealQueue {
    std::vector<ThreadSafeQueue<T*>> work_queues;
    std::atomic_bool closed = { false };


public:

    class WorkQueue {
        ThreadSafeQueue<T*>& queue;
        WorkStealQueue& wsq;
        int queue_num;

    public:
        explicit WorkQueue(ThreadSafeQueue<T*>& q, WorkStealQueue& wsq,
                           int queue_num) :
            queue(q), wsq(wsq), queue_num(queue_num) {

        }
        void push(T& t) {
            debug::printf("WorkQueue::push %p\n", &t);
            queue.push(&t);
        }

        T* pop() {

            T* t;
            if (queue.pop(t)) {
                debug::printf("WorkQueue::pop %p\n", t);
                return t;
            }

            return wsq.steal();
        }

        void close() {
            wsq.close();
        }

        bool is_closed() {
            return wsq.is_closed();
        }

    };

    explicit WorkStealQueue(int num_of_worker) :
        work_queues(num_of_worker) {

    }

    /*
     * return thread local work queue that can steal work from other work queue
     * *must* index < num_of_worker
     */
    WorkQueue get_local_queue(int index) {
        return WorkQueue { work_queues.at(index), *this, index };
    }


    /*
     * return false if no works left
     * return true if else
     */
    T* steal() {

        for (;;) { //for (auto i : boost::irange(0, 100)) {
            debug::printf("WorkQueue::steal loop\n");
            if (closed) {
                debug::printf("WorkQueue::steal closed\n");
                return nullptr;
            }


            for (int i : boost::irange(0, static_cast<int>(work_queues.size()))) {
                auto& queue = work_queues[i];
                T* t;
                if (queue.pop(t)) {
                    debug::printf("WorkQueue::steal %p\n", t);
                    return t;
                }
            }

        }
        return nullptr;
    }

    void close() {
        closed = true;
    }

    bool is_closed() {
        return closed;
    }

};
}
}
}
#endif //USER_THREAD_WORKQUEUE_HPP
