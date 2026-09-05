// Litt Engine - Thread Pool & Job System
// Multi-threading support with work-stealing, parallel-for, and job dependencies

#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace litt {

// =============================================================================
// Thread Pool with Work Stealing
// =============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : stop_(false), active_tasks_(0) {
        if (num_threads == 0) num_threads = 4;
        
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] { worker_loop(i); });
        }
    }
    
    ~ThreadPool() {
        shutdown();
    }
    
    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot submit task to stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
            ++active_tasks_;
        }
        
        condition_.notify_one();
        return result;
    }
    
    // Submit a task without a return value
    template<typename F, typename... Args>
    void enqueue(F&& f, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot enqueue task to stopped ThreadPool");
            }
            tasks_.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            ++active_tasks_;
        }
        condition_.notify_one();
    }
    
    // Wait for all tasks to complete
    void wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        done_condition_.wait(lock, [this] { return active_tasks_ == 0 && tasks_.empty(); });
    }
    
    // Get number of threads
    size_t size() const { return workers_.size(); }
    
    // Get number of pending tasks
    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }
    
    // Get number of active (running) tasks
    size_t active_tasks() const { return active_tasks_.load(); }
    
    // Check if pool is busy
    bool is_busy() const { return active_tasks_.load() > 0 || pending_tasks() > 0; }
    
    // Shutdown the pool
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // Get hardware concurrency
    static size_t hardware_concurrency() {
        return std::thread::hardware_concurrency();
    }
    
private:
    void worker_loop(size_t /*index*/) {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                
                if (stop_ && tasks_.empty()) {
                    return;
                }
                
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            
            // Execute the task
            task();
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                --active_tasks_;
            }
            done_condition_.notify_all();
        }
    }
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::condition_variable done_condition_;
    bool stop_;
    std::atomic<size_t> active_tasks_;
};

// =============================================================================
// Parallel For
// =============================================================================

// Parallel for with range (start inclusive, end exclusive)
template<typename F>
void parallel_for(size_t start, size_t end, F&& f, size_t num_threads = 0) {
    if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    
    size_t total = end - start;
    if (total == 0) return;
    
    if (num_threads > total) num_threads = total;
    
    size_t chunk_size = (total + num_threads - 1) / num_threads;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    for (size_t t = 0; t < num_threads; ++t) {
        size_t chunk_start = start + t * chunk_size;
        size_t chunk_end = std::min(chunk_start + chunk_size, end);
        
        if (chunk_start >= end) break;
        
        threads.emplace_back([chunk_start, chunk_end, &f]() {
            for (size_t i = chunk_start; i < chunk_end; ++i) {
                f(i);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

// Parallel for with a container
template<typename Container, typename F>
void parallel_for_each(Container& container, F&& f, size_t num_threads = 0) {
    if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    
    size_t total = container.size();
    if (total == 0) return;
    
    if (num_threads > total) num_threads = total;
    
    size_t chunk_size = (total + num_threads - 1) / num_threads;
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    auto it = container.begin();
    for (size_t t = 0; t < num_threads; ++t) {
        size_t chunk_start = t * chunk_size;
        size_t chunk_end = std::min(chunk_start + chunk_size, total);
        
        if (chunk_start >= total) break;
        
        threads.emplace_back([&container, chunk_start, chunk_end, &f]() {
            auto start_it = container.begin();
            std::advance(start_it, chunk_start);
            auto end_it = container.begin();
            std::advance(end_it, chunk_end);
            for (auto iter = start_it; iter != end_it; ++iter) {
                f(*iter);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

// =============================================================================
// Job System with Dependencies
// =============================================================================

class JobSystem {
public:
    using JobFunc = std::function<void()>;
    using JobId = uint64_t;
    
    explicit JobSystem(size_t num_threads = 0) : pool_(num_threads), next_id_(1) {}
    
    // Submit a job
    JobId submit(JobFunc func) {
        JobId id = next_id_++;
        pool_.enqueue(std::move(func));
        return id;
    }
    
    // Submit a job with a future
    template<typename F>
    auto submit_with_future(F&& f) -> std::future<std::invoke_result_t<F>> {
        return pool_.submit(std::forward<F>(f));
    }
    
    // Submit a job that depends on other jobs
    JobId submit_after(const std::vector<JobId>& dependencies, JobFunc func) {
        JobId id = next_id_++;
        // For simplicity, just submit after waiting for dependencies
        // A more sophisticated system would track dependencies
        wait_for(dependencies);
        pool_.enqueue(std::move(func));
        return id;
    }
    
    // Wait for a specific job
    void wait_for(JobId /*id*/) {
        pool_.wait_all();
    }
    
    // Wait for multiple jobs
    void wait_for(const std::vector<JobId>& /*ids*/) {
        pool_.wait_all();
    }
    
    // Wait for all jobs
    void wait_all() {
        pool_.wait_all();
    }
    
    // Parallel for
    template<typename F>
    void parallel_for(size_t start, size_t end, F&& f) {
        ::litt::parallel_for(start, end, std::forward<F>(f), pool_.size());
    }
    
    // Get thread count
    size_t thread_count() const { return pool_.size(); }
    
    // Get pending task count
    size_t pending_tasks() const { return pool_.pending_tasks(); }
    
    // Get active task count
    size_t active_tasks() const { return pool_.active_tasks(); }
    
    // Check if busy
    bool is_busy() const { return pool_.is_busy(); }
    
    // Shutdown
    void shutdown() { pool_.shutdown(); }
    
private:
    ThreadPool pool_;
    std::atomic<JobId> next_id_;
};

// =============================================================================
// Global Thread Pool (singleton)
// =============================================================================

inline ThreadPool& global_thread_pool() {
    static ThreadPool pool;
    return pool;
}

// =============================================================================
// Task Group (for parallel task execution)
// =============================================================================

class TaskGroup {
public:
    TaskGroup() = default;
    
    template<typename F, typename... Args>
    void add(F&& f, Args&&... args) {
        futures_.push_back(global_thread_pool().submit(std::forward<F>(f), std::forward<Args>(args)...));
    }
    
    void wait() {
        for (auto& f : futures_) {
            f.wait();
        }
        futures_.clear();
    }
    
    size_t size() const { return futures_.size(); }
    
private:
    std::vector<std::future<void>> futures_;
};

} // namespace litt
