#include "AvUpdateScheduler.h"

#include <chrono>

AvUpdateScheduler::AvUpdateScheduler(AvDatabaseStorage& storage)
    : storage_(storage)
{
}

AvUpdateScheduler::~AvUpdateScheduler()
{
    Stop();
}

void AvUpdateScheduler::Start()
{
    if (running_) {
        return;
    }

    running_ = true;

    worker_ = std::thread(
        &AvUpdateScheduler::Worker,
        this
    );
}

void AvUpdateScheduler::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;

    if (worker_.joinable()) {
        worker_.join();
    }
}

void AvUpdateScheduler::Worker()
{
    while (running_) {
        // Учебное расписание: проверка обновления раз в 60 секунд.
        // В реальном продукте здесь был бы HTTPS-запрос к серверу обновлений.
        std::this_thread::sleep_for(std::chrono::seconds(60));

        if (!running_) {
            break;
        }

        storage_.CreateBackup();

        // Имитация обновления: пересохраняем текущие базы.
        // Если запись/загрузка сломается, можно будет восстановиться из backup.
        if (!storage_.Save()) {
            storage_.RestoreBackup();
        }
    }
}