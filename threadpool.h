//
// Created by jacky1 on 2021/8/7.
//

#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>

constexpr int MAX_THREADS = 100;

class ThreadPool{
public:
    ThreadPool(const ThreadPool& ) = delete;
    ThreadPool& operator = (const ThreadPool& ) = delete;

public:
    using Task = std::function<void()>;
    explicit ThreadPool(int num): _thread_num(num){};
    ~ThreadPool(){
        stop();
    }

    void start(){
        for(int i = 0; i < _thread_num; i++){
            _threads.emplace_back(std::thread(&ThreadPool::work, this));
        }
    }

    void stop(){
        {
            std::unique_lock<std::mutex> lk(_mtx);
            _cond.notify_all();
        }

        for(std::thread& t : _threads){
            if (t.joinable()){
                t.join();
            }
        }
    }

    void appendWork(const Task& task){
        std::unique_lock<std::mutex> lk(_mtx);
        _tasks.push(task);
        _cond.notify_one();
    }

private:
    void work(){
        printf("begin work thread: %d\n", std::this_thread::get_id());
        Task task;
        std::unique_lock<std::mutex> lk(_mtx);
        if (!_tasks.empty()) {
            task = _tasks.front();
            _tasks.pop();
        }
        else {
            _cond.wait(lk);
        }
        if (task){
            task();
        }else{
            std::this_thread::yield();
        }

        printf("end work thread: %d\n", std::this_thread::get_id());
    }

private:
    std::vector<std::thread> _threads;
    std::queue<Task> _tasks;
    std::mutex _mtx;
    std::condition_variable _cond;
    int _thread_num;
};
