#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
#include <sched.h>
#include <pthread.h>

class threadPool
{
public:
  threadPool(int n_threads)
  : should_terminate_(false),
    unfinished_jobs_(0),
    threads_(n_threads) {
      for (int i = 0; i < n_threads; i++) {
        threads_.at(i) = std::thread(&threadPool::MainLoop, this, i);
    }
  }

  ~threadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        should_terminate_ = true;
    }
    mutex_condition_.notify_all();
    for (std::thread& active_thread : threads_) {
        active_thread.join();
    }
    threads_.clear();
  }

  // void JobEnqueue(std::function<void()> job) {
  //   {
  //     std::unique_lock<std::mutex> lock(queue_mutex_);
  //     jobs.push(job);
  //     unfinished_jobs_ += 1;
  //   }
  //   mutex_condition_.notify_one();
  // }
  
  void JobEnqueue(std::function<void()> job) {
    jobs.push(job);
    unfinished_jobs_ += 1;
  }

  std::mutex& GetMutex() {
    return queue_mutex_;
  }

  void NotifyAll() {
    mutex_condition_.notify_all();
  }

  bool IsBusy() {
    return (unfinished_jobs_ > 0);
  }

private:
  void MainLoop(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id, &cpuset);
    int rc = pthread_setaffinity_np(threads_[thread_id].native_handle(),
      sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      throw std::runtime_error("Error to set affinity for thread " + std::to_string(thread_id));
    }
    while (true) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        mutex_condition_.wait(lock, [this] {
            return !jobs.empty() || should_terminate_;
        });
        if (should_terminate_) {
            return;
        }
        job = jobs.front();
        jobs.pop();
        unfinished_jobs_ -= 1;
      }
      job();
    }
  }

  // void MainLoop(int thread_id) {
  //   while (true) {
  //     {
  //       std::unique_lock<std::mutex> lock(queue_mutex_);
  //       mutex_condition_.wait(lock, [this, thread_id] {
  //           return !threads_pause_[thread_id] || should_terminate_;
  //       });
  //     }
  //     if (should_terminate_) {
  //       return;
  //     }
  //     std::function<void()> job;
  //     int finished_jobs = 0;
  //     for (int i = thread_id; i < jobs.size(); i += threads_.size()) {
  //       job = jobs[i];
  //       job();
  //       finished_jobs += 1;
  //     }
  //     unfinished_jobs_ -= finished_jobs;
  //     threads_pause_[thread_id] = true;
  //   }
  // }

  std::mutex queue_mutex_;
  std::condition_variable mutex_condition_;
  std::queue<std::function<void()>> jobs;
  std::atomic<bool> should_terminate_;
  std::atomic<int> unfinished_jobs_;
  std::vector<std::thread> threads_;
  // std::vector<bool> threads_pause_;
};