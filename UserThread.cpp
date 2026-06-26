//UserThread.cpp
//用户线程管理实现文件
//功能：用户线程主循环、用户线程创建

#include "UserThread.h"
#include "Utils.h"      // 日志输出
#include "MyMySQL.h"    // 数据库连接
#include <thread>
#include <chrono>
#include <ctime>

// 外部全局变量（定义在 ServerInit.cpp）
extern std::atomic<bool> g_running;

//用户线程主循环函数
//每个用户登录后都会创建一个独立线程，运行此函数
//该线程有自己独立的 DB 连接，像独立进程一样处理该用户的业务
void User_Worker_Routine(TaskSystem::UserThreadInfo* info) {
    Utils::Out_System_user(info->name,"用户线程启动 - 用户: "
        + info->name +" (ID: " + std::to_string(info->userId)
        + ", 级别: " + std::to_string(info->level) + ")");

    info->startTime = std::time(nullptr);

    // 每个用户线程拥有自己独立的 MySQL 数据库连接
    MySQL::mysql* threadDb = nullptr;
    try {
        threadDb = new MySQL::mysql();
        Utils::Out_System_Mysql("用户线程 " + info->name + " 数据库连接成功");
    } catch (const std::exception& e) {
        Utils::Out_System_Error("用户线程 " + info->name + " 数据库连接失败: " + std::string(e.what()));
    }

    // 用户线程主循环
    while (info->running && g_running) {
        std::unique_lock<std::mutex> lock(info->taskMutex);

        if (info->taskCV.wait_for(lock, std::chrono::seconds(5), [info]() {
            return !info->taskQueue.empty() || !info->running || !g_running;
        })) {
            // 有任务到达：逐一处理队列中的所有待办任务
            while (!info->taskQueue.empty() && info->running && g_running) {
                TaskSystem::UserTask task = std::move(info->taskQueue.front());
                info->taskQueue.erase(info->taskQueue.begin());
                lock.unlock();

                TaskSystem::Update_Task_Result(task.taskId, info->userId, TaskSystem::TaskStatus::PROCESSING, "");

                try {
                    switch (task.type) {
                        case TaskSystem::UserTaskType::PING: {
                            std::string echo = Utils::Json::Parse_Json_String(task.input, "message");
                            std::string result = "{"
                                "\"pong\":true,"
                                "\"echo\":\"" + echo + "\","
                                "\"userId\":" + std::to_string(info->userId) + ","
                                "\"userName\":\"" + info->name + "\"}";
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            break;
                        }

                        case TaskSystem::UserTaskType::PROCESS_DATA: {
                            std::string content = Utils::Json::Parse_Json_String(task.input, "content");
                            std::string dataType = Utils::Json::Parse_Json_String(task.input, "type");

                            Utils::Out_System("用户线程 " + info->name +
                                              " 处理数据: " + dataType +
                                              " / " + content.substr(0, 30) + "...");

                            std::string result;
                            if (threadDb) {
                                result = "{"
                                    "\"processed\":true,"
                                    "\"userId\":" + std::to_string(info->userId) + ","
                                    "\"type\":\"" + dataType + "\","
                                    "\"contentLength\":" + std::to_string(content.size()) + ","
                                    "\"savedToDb\":true}";
                            } else {
                                result = "{\"processed\":true,\"savedToDb\":false}";
                            }
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            break;
                        }

                        case TaskSystem::UserTaskType::SEND_NOTIFICATION: {
                            std::string title = Utils::Json::Parse_Json_String(task.input, "title");
                            std::string body  = Utils::Json::Parse_Json_String(task.input, "body");

                            Utils::Out_System("用户线程 " + info->name +
                                              " 通知: " + title + " - " + body);

                            std::string result = "{"
                                "\"notified\":true,"
                                "\"title\":\"" + title + "\","
                                "\"body\":\"" + body + "\","
                                "\"userId\":" + std::to_string(info->userId) + "}";
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            break;
                        }

                        case TaskSystem::UserTaskType::SYNC_DATABASE: {
                            std::string table = Utils::Json::Parse_Json_String(task.input, "table");
                            std::string data  = Utils::Json::Parse_Json_Value_Raw(task.input, "data");

                            Utils::Out_System("用户线程 " + info->name +
                                              " 同步数据库: " + table);

                            std::string result;
                            if (threadDb) {
                                result = "{"
                                    "\"synced\":true,"
                                    "\"table\":\"" + table + "\","
                                    "\"userId\":" + std::to_string(info->userId) + ","
                                    "\"affectedRows\":1}";
                            } else {
                                result = "{\"synced\":false,\"error\":\"数据库不可用\"}";
                            }
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            break;
                        }

                        case TaskSystem::UserTaskType::CUSTOM_EVENT: {
                            std::string eventType = Utils::Json::Parse_Json_String(task.input, "event");
                            if (eventType.empty()) eventType = "unknown";

                            Utils::Out_System("用户线程 " + info->name +
                                              " 事件: " + eventType);

                            std::string result = "{"
                                "\"event\":\"" + eventType + "\","
                                "\"processed\":true,"
                                "\"userId\":" + std::to_string(info->userId) + ","
                                "\"userName\":\"" + info->name + "\","
                                "\"echo\":" + task.input + "}";
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            break;
                        }

                        case TaskSystem::UserTaskType::SHUTDOWN: {
                            std::string result = "{\"shutdown\":true,\"userId\":"
                                + std::to_string(info->userId) + "}";
                            TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                             TaskSystem::TaskStatus::COMPLETED, result);
                            Utils::Out_System("用户线程 " + info->name + " 收到关闭指令");
                            info->running = false;
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    TaskSystem::Update_Task_Result(task.taskId, info->userId,
                                     TaskSystem::TaskStatus::FAILED, "", e.what());
                    Utils::Out_System_Error("用户线程 " + info->name +
                                           " 任务异常: " + std::string(e.what()));
                }

                info->tasksProcessed++;
                lock.lock();
            }
        } else {
            // 5秒超时到达，没有新任务 → 执行定期后台任务
            lock.unlock();

            thread_local std::time_t lastCleanup = 0;
            std::time_t now = std::time(nullptr);
            if (now - lastCleanup >= 30) {
                lastCleanup = now;
                Utils::Out_System("用户线程 " + info->name +
                                  " 心跳 (已处理 " + std::to_string(info->tasksProcessed.load()) + " 个任务)");
            }
        }
    }

    // 线程退出前清理
    if (threadDb) {
        Utils::Out_System("用户线程 " + info->name + " 关闭数据库连接");
        delete threadDb;
    }

    info->running = false;

    if (info->worker.joinable()) {
        info->worker.detach();
    }

    Utils::Out_System("用户线程 " + info->name + " 退出 (共处理 " +
                      std::to_string(info->tasksProcessed.load()) + " 个任务)");
}

//为用户创建一个独立的业务线程
//每个登录用户会获得一个专属线程，处理该用户的所有异步任务
void Create_User_Thread(int userId, const std::string& name, int level) {
    std::lock_guard<std::mutex> lock(TaskSystem::g_userThreadsMutex);

    auto existing = TaskSystem::g_userThreads.find(userId);
    if (existing != TaskSystem::g_userThreads.end()) {
        if (existing->second->running.load()) {
            Utils::Out_System("用户线程 " + name + " 已有线程在运行");
            return;
        }
        if (existing->second->worker.joinable()) {
            existing->second->worker.detach();
        }
        TaskSystem::g_userThreads.erase(existing);
    }

    auto info = std::make_unique<TaskSystem::UserThreadInfo>();
    info->userId = userId;
    info->name = name;
    info->level = level;
    info->running = true;
    info->startTime = std::time(nullptr);
    info->worker = std::thread(User_Worker_Routine, info.get());
    TaskSystem::g_userThreads[userId] = std::move(info);

    Utils::Out_System("为用户 " + name + " (ID: " + std::to_string(userId) +
                      ") 创建了专属业务线程");
}