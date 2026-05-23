#pragma once

#include "AvDatabaseStorage.h"

#include <atomic>
#include <thread>

class AvUpdateScheduler
{
public:
    explicit AvUpdateScheduler(AvDatabaseStorage& storage);

    ~AvUpdateScheduler();

    void Start();

    void Stop();

private:
    void Worker();

private:
    AvDatabaseStorage& storage_;

    std::atomic_bool running_ = false;
    std::thread worker_;
};