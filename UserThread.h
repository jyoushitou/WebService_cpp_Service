//UserThread.h
//用户线程管理头文件
//功能：用户线程管理、任务类型/状态枚举、任务/线程数据结构定义、任务投递/查询函数声明

#ifndef USERTHREAD_H
#define USERTHREAD_H
#include <string>               // 字符串
#include <map>                  // 映射容器
#include <unordered_map>        // 无序映射容器（哈希表）
#include <mutex>                // 互斥锁
#include <memory>               // 智能指针
#include <thread>               // 线程
#include <atomic>               // 原子操作
#include <vector>               // 动态数组
#include <condition_variable>   // 条件变量
#include <ctime>                // 时间日期

namespace TaskSystem{
    // 用户任务类型枚举
    enum class UserTaskType {
        PING,               // 心跳检测任务
        PROCESS_DATA,       // 数据处理任务
        SEND_NOTIFICATION,  // 发送通知任务
        SYNC_DATABASE,      // 数据库同步任务
        CUSTOM_EVENT,       // 自定义事件任务
        SHUTDOWN            // 关闭/停止任务
    };

    // 任务状态枚举
    enum class TaskStatus {
        PENDING,            // 任务等待处理
        PROCESSING,         // 任务正在处理中
        COMPLETED,          // 任务已完成
        FAILED              // 任务执行失败
    };

    // 任务执行结果的数据结构
    struct TaskResult {
        std::string taskId;             // 任务唯一 ID
        int userId;                     // 所属用户 ID
        TaskStatus status;              // 任务最终状态
        std::string input;              // 输入数据（用于回溯）
        std::string output;             // 输出结果
        std::string errorMessage;       // 错误信息
        std::time_t createTime;         // 创建时间
        std::time_t completeTime;       // 完成时间
    };

    // 用户任务数据结构，包含任务的完整生命周期信息
    struct UserTask {
        std::string taskId;             // 任务唯一 ID
        UserTaskType type;              // 任务类型
        std::time_t createTime;         // 任务创建时间
        std::string source;             // 任务来源（如客户端 IP、模块名称等）
        std::string input;              // 任务输入数据
        TaskStatus status;              // 当前任务状态
        std::string output;             // 任务执行输出结果
        std::string errorMessage;       // 错误信息（任务失败时填充）
        std::time_t completeTime;       // 任务完成时间
    };

    // 每个用户对应的独立工作线程信息
    struct UserThreadInfo {
        int userId;                             // 用户 ID
        int level;                              // 用户等级/优先级
        std::string name;                       // 用户名称
        std::thread worker;                     // 工作线程对象
        std::atomic<bool> running{true};        // 线程运行标志（默认 true）
        std::vector<UserTask> taskQueue;        // 该用户的任务队列
        std::mutex taskMutex;                   // 任务队列互斥锁
        std::condition_variable taskCV;         // 任务条件变量（用于唤醒等待中的线程）
        std::atomic<int> tasksProcessed{0};     // 已处理任务计数
        std::time_t startTime;                  // 线程启动时间
    };

    // 外部变量
    extern std::map<int, std::unique_ptr<UserThreadInfo>> g_userThreads;    // 用户 ID -> 用户线程信息 映射表
    extern std::mutex g_userThreadsMutex;                                   // 保护 g_userThreads 的互斥锁
    extern std::unordered_map<std::string, TaskResult> g_taskResults;      // 任务 ID -> 任务结果 哈希表
    extern std::mutex g_taskResultsMutex;                                   // 保护 g_taskResults 的互斥锁

    // 外部可使用函数
    std::string Generate_Task_Id();                                         // 生成唯一任务 ID（时间戳+随机数）
    std::string Post_User_Task(int userId, UserTaskType type,              // 向指定用户投递新任务
        const std::string& input, const std::string& source = "");
    void Update_Task_Result(const std::string& taskId, int userId,         // 更新任务执行结果
        TaskStatus status, const std::string& output,
        const std::string& errorMessage = "");
    TaskResult* Get_Task_Result(const std::string& taskId);                 // 根据任务 ID 获取任务结果指针
}

#endif //!USERTHREAD_H