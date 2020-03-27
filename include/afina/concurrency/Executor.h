#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>

namespace Afina {
namespace Concurrency {

class Executor;
void perform(Executor *executor);
/**
 * # Thread pool
 */
class Executor {
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
            kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
            kStopping,

        // Threadppol is stopped
            kStopped
    };

public:
    Executor(std::string name, int size, int low, int high, int time) :
        max_queue_size(size),
        hight_watermark(high),
        low_watermark(low),
        idle_time(time),
        number_of_threads(0),
        free_threads(0) {};

    ~Executor() {Stop(true);};

    void Start();

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || tasks.size() >= max_queue_size) {
            return false;
        }

        // Enqueue new task
        tasks.push_back(exec);
        if (free_threads == 0 && number_of_threads < hight_watermark) {
            std::thread(&perform, this).detach();
            number_of_threads++;
        }
        empty_condition.notify_one();
        return true;
    }
    void Stopping();

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

    void eraseThread();

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    int low_watermark;
    int hight_watermark;
    int max_queue_size;
    int idle_time;
    int free_threads;
    int number_of_threads;

    std::condition_variable stop_work;

};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
