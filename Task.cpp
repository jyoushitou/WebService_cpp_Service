//Task.cpp
//任务系统实现文件
//功能：任务ID生成、任务投递/查询/更新、全局变量定义

#include "Task.h"
#include <random>       // 随机数
#include <sstream>      // 字符串流
#include <iomanip>      // 输入输出格式化
#include <ctime>        // 时间日期

namespace TaskSystem{
    // 全局变量定义（唯一实例）
    std::map<int, std::unique_ptr<UserThreadInfo>> g_userThreads;       // 用户 ID -> 用户线程信息 映射表
    std::mutex g_userThreadsMutex;                                      // 保护 g_userThreads 的互斥锁
    std::unordered_map<std::string, TaskResult> g_taskResults;         // 任务 ID -> 任务结果 哈希表
    std::mutex g_taskResultsMutex;                                      // 保护 g_taskResults 的互斥锁

    // 生成唯一任务ID（时间戳十六进制 + 16位随机十六进制数）
    std::string Generate_Task_Id() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        std::time_t now = std::time(nullptr);
        ss << std::hex << now;
        for (int i = 0; i < 16; i++) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }

    // 向指定用户的线程投递一个任务
    std::string Post_User_Task(int userId, UserTaskType type,
                             const std::string& input,
                             const std::string& source) {
        std::lock_guard<std::mutex> lock(g_userThreadsMutex);
        auto it = g_userThreads.find(userId);
        if (it == g_userThreads.end()) return "";

        std::string taskId = Generate_Task_Id();

        UserTask task;
        task.taskId = taskId;
        task.type = type;
        task.input = input;
        task.source = source;
        task.createTime = std::time(nullptr);
        task.status = TaskStatus::PENDING;

        {
            std::lock_guard<std::mutex> qLock(it->second->taskMutex);
            it->second->taskQueue.push_back(std::move(task));
        }

        {
            std::lock_guard<std::mutex> rLock(g_taskResultsMutex);
            g_taskResults[taskId] = {taskId, userId, TaskStatus::PENDING,
                                     input, "", "", std::time(nullptr), 0};
        }

        it->second->taskCV.notify_one();
        return taskId;
    }

    // 更新任务执行结果
    void Update_Task_Result(const std::string& taskId, int userId,
                          TaskStatus status,
                          const std::string& output,
                          const std::string& errorMessage) {
        std::lock_guard<std::mutex> lock(g_taskResultsMutex);
        auto it = g_taskResults.find(taskId);
        if (it == g_taskResults.end()) return;
        it->second.status = status;
        it->second.output = output;
        it->second.errorMessage = errorMessage;
        it->second.completeTime = std::time(nullptr);
    }

    // 按 taskId 查询任务结果
    TaskResult* Get_Task_Result(const std::string& taskId) {
        std::lock_guard<std::mutex> lock(g_taskResultsMutex);
        auto it = g_taskResults.find(taskId);
        if (it == g_taskResults.end()) return nullptr;
        return &it->second;
    }
}