#include <afina/concurrency/Executor.h>
#include <algorithm>

namespace Afina {
namespace Concurrency {

void process(Executor *executor) {
    try {
        std::function<void()> current_task;
        bool busy = false;
        executor->free_threads++;
        while (true) {
            if (executor->state != Executor::State::kRun) {
                executor->free_threads--;
                break;
            }
            std::unique_lock<std::mutex> lock(executor->mutex);
            auto time_limit = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            if (executor->empty_condition.wait_until(lock, time_limit, [&]() { return !executor->tasks.empty(); })) {
                current_task = executor->tasks.front();
                executor->tasks.pop_front();
                busy = true;
                executor->free_threads--;
            } else {
                executor->free_threads--;
                if (executor->number_of_threads > executor->low_watermark) {
                    break;
                } else {
                    busy = false;
                }
            }
            if (busy) {
                current_task();
                busy = false;
                executor->free_threads++;
            }
        }
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->number_of_threads--;
        if (executor->state == Executor::State::kStopping && executor->tasks.empty() &&
            executor->number_of_threads == 0) {
            executor->state = Executor::State::kStopped;
            executor->stop_work.notify_all();
        }
    } catch (std::exception &ex) {
        std::terminate();
    }
}
void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(end_mutex);
    if (state == State::kRun) {
        state = State::kStopping;
    }
    if (await && number_of_threads > 0) {
        stop_work.wait(lock, [&]() { return number_of_threads == 0; });
        state = State::kStopped;
    } else if (number_of_threads == 0) {
        state = State::kStopped;
    }
}

void Executor::Start() {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    for (int i = 0; i < low_watermark; i++) {
        number_of_threads++;
        std::thread(&process, this).detach();
    }
}

} // namespace Concurrency
} // namespace Afina
