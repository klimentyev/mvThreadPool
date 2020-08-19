#pragma once

//-----------------------------------------------------------------------------
// mvThreadPool
//
//     - This is a simple thread pool implementation from our repository at
//       https://github.com/RaylockLLC/mvThreadPool
//     
//-----------------------------------------------------------------------------

#include <memory>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <deque>

namespace Marvel {

    //-----------------------------------------------------------------------------
    // mvThreadJoiner
    //-----------------------------------------------------------------------------
    class mvThreadJoiner
    {

    public:

        explicit mvThreadJoiner(std::vector<std::thread>& threads)
            : m_threads(threads)
        {

        }

        ~mvThreadJoiner()
        {
            for (auto& thread : m_threads)
                if (thread.joinable())
                    thread.join();
        }

    private:

        std::vector<std::thread>& m_threads;

    };

    //-----------------------------------------------------------------------------
    // mvFunctionWrapper
    //     - Reguired because packaged_task reguires movable function objects
    //-----------------------------------------------------------------------------
    class mvFunctionWrapper
    {
        struct impl_base 
        {
            virtual void call() = 0;
            virtual ~impl_base() = default;
        };

        template<typename F>
        struct impl_type : impl_base
        {
            F f;
            explicit impl_type(F&& f) : f(std::move(f)) {}
            void call() override { f(); }
        };

    public:

        mvFunctionWrapper() = default;

        template<typename F>
        mvFunctionWrapper(F&& f) : m_impl(new impl_type<F>(std::move(f))) {}

        mvFunctionWrapper(mvFunctionWrapper&& other) noexcept
            : m_impl(std::move(other.m_impl))
        {

        }

        mvFunctionWrapper& operator=(mvFunctionWrapper&& other)
        {
            m_impl = std::move(other.m_impl);
            return *this;
        }

        // delete copy constructor and assignment operator
        mvFunctionWrapper(const mvFunctionWrapper&) = delete;
        mvFunctionWrapper(mvFunctionWrapper&) = delete;
        mvFunctionWrapper& operator=(const mvFunctionWrapper&) = delete;

        void operator()()
        {
            m_impl->call();
        }

    private:

        std::unique_ptr<impl_base> m_impl;

    };

    //-----------------------------------------------------------------------------
    // mvWorkStealingQueue
    //-----------------------------------------------------------------------------
    class mvWorkStealingQueue
    {

    public:

        mvWorkStealingQueue() {}
        ~mvWorkStealingQueue() = default;

        // deleted copy constructor/assignment operator.
        mvWorkStealingQueue(const mvWorkStealingQueue& other) = delete;
        mvWorkStealingQueue& operator=(const mvWorkStealingQueue& other) = delete;

        void push(mvFunctionWrapper data)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_front(std::move(data));
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        bool try_pop(mvFunctionWrapper& res)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
                return false;

            res = std::move(m_queue.front());
            m_queue.pop_front();
            return true;
        }

        bool try_steal(mvFunctionWrapper& res)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
                return false;

            res = std::move(m_queue.back());
            m_queue.pop_back();
            return true;
        }

    private:

        std::deque<mvFunctionWrapper> m_queue;
        mutable std::mutex            m_mutex;

    };

    //-----------------------------------------------------------------------------
    // mvThreadPool
    //-----------------------------------------------------------------------------
    class mvThreadPool
    {

        typedef mvFunctionWrapper task_type;

    public:

        explicit mvThreadPool(unsigned threadcount = 0) :
            m_done(false), m_joiner(m_threads)
        {

            unsigned thread_count = threadcount;

            if (threadcount == 0)
                thread_count = std::thread::hardware_concurrency();

            try
            {

                for (unsigned i = 0; i < thread_count; ++i)
                    m_queues.push_back(std::make_unique<mvWorkStealingQueue>());

                for (unsigned i = 0; i < thread_count; ++i)
                    m_threads.emplace_back(
                        &mvThreadPool::worker_thread, this, i);

            }
            catch (...)
            {
                m_done = true;
                throw;
            }
        }

        ~mvThreadPool() { m_done = true; }

        static const char* getVersion() { return "v0.3"; }

        template<typename F, typename ...Args>
        std::future<typename std::invoke_result<F, Args...>::type> submit(F f)
        {
            typedef typename std::invoke_result<F, Args...>::type result_type;
            std::packaged_task<result_type()> task(std::move(f));
            std::future<result_type> res(task.get_future());
            if (m_local_work_queue)
                m_local_work_queue->push(std::move(task));
            else
                m_pool_work_queue.push(std::move(task));

            return res;
        }

    private:

        void worker_thread(unsigned index)
        {
            m_index = index;

            m_local_work_queue = m_queues[m_index].get();

            while (!m_done)
                run_pending_task();
        }

        void run_pending_task()
        {
            task_type task;
            if (pop_task_from_local_queue(task) ||
                pop_task_from_pool_queue(task) ||
                pop_task_from_other_thread_queue(task))
            {
                task();
            }

            else
                std::this_thread::yield();
        }

        bool pop_task_from_local_queue(task_type& task)
        {
            return m_local_work_queue && m_local_work_queue->try_pop(task);
        }

        bool pop_task_from_pool_queue(task_type& task)
        {
            return m_pool_work_queue.try_pop(task);
        }

        bool pop_task_from_other_thread_queue(task_type& task)
        {
            for (unsigned i = 0; i < m_queues.size(); i++)
            {
                const unsigned index = (m_index + i + 1) % m_queues.size();
                if (m_queues[index]->try_steal(task))
                    return true;
            }

            return false;
        }

    private:

        std::atomic_bool                                   m_done;
        mvWorkStealingQueue                                m_pool_work_queue;
        std::vector<std::unique_ptr<mvWorkStealingQueue> > m_queues;
        std::vector<std::thread>                           m_threads;
        mvThreadJoiner                                     m_joiner;
        static thread_local mvWorkStealingQueue*           m_local_work_queue;
        static thread_local unsigned                       m_index;

    };

    thread_local mvWorkStealingQueue* mvThreadPool::m_local_work_queue;
    thread_local unsigned mvThreadPool::m_index;
}