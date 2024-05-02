#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <sched.h>
#include <thread>
#include <vector>

class threadPool {
public:
  threadPool(int n_threads) : unfinished_jobs_(0), threads_(n_threads) {
    cpu_set_t cpuset;
    for (int i = 0; i < n_threads; i++) {
      threads_.at(i) = std::thread(&threadPool::MainLoop, this);
      int thread_id = i + 1;
      CPU_ZERO(&cpuset);
      CPU_SET(thread_id, &cpuset);
      int rc = pthread_setaffinity_np(threads_[i].native_handle(),
                                      sizeof(cpu_set_t), &cpuset);
      if (rc != 0) {
        throw std::runtime_error("Error to set affinity for thread " +
                                 std::to_string(i));
      }
    }
  }

  ~threadPool() {
    for (std::thread &active_thread : threads_) {
      active_thread.join();
    }
    threads_.clear();
  }

  void JobEnqueue(std::function<void()> job) {
    unfinished_jobs_ += 1;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      jobs.push(job);
    }
  }

  std::mutex &GetMutex() { return queue_mutex_; }

  bool IsBusy() { return (unfinished_jobs_ > 0); }

private:
  void MainLoop() {
    while (true) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (jobs.size() > 0) {
          job = jobs.front();
          jobs.pop();
        } else {
          continue;
        }
      }
      job();
      unfinished_jobs_ -= 1;
    }
  }

  std::mutex queue_mutex_;
  std::queue<std::function<void()>> jobs;
  std::atomic<int> unfinished_jobs_;
  std::vector<std::thread> threads_;
};