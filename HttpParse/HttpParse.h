#ifndef HSLL_HTTPPARSE
#define HSLL_HTTPPARSE

#include <map>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "Task.hpp"
#include "../SqlPool/SqlPool.h"
#include "../Coroutine/Coroutine.hpp"

namespace HSLL
{
#define PARSE_ERROR_RETURN              \
    {                                   \
        info.state = PARSE_STATE_Error; \
        return;                         \
    }
#define PARSE_FINISHED_RETURN              \
    {                                      \
        info.state = PARSE_STATE_Finished; \
        return;                            \
    }

#define PARSE_NOT_FIND_RETURN(str, max) \
    index = bRequest.Find(str);         \
    if (index == -1)                    \
    {                                   \
        if (bRequest.nowSize > max)     \
            PARSE_ERROR_RETURN return;  \
    }                                   \
    else if (index > max)               \
    PARSE_ERROR_RETURN

    enum ONESHOT_TYPE
    {
        ONESHOT_TYPE_In = EPOLLIN,   // 监听可读
        ONESHOT_TYPE_Out = EPOLLOUT, // 监听可写
        ONESHOT_TYPE_Close = 0
    };

    enum VERSION_TYPE
    {
        VERSION_TYPE_10, // 1.0
        VERSION_TYPE_11, // 1.1
    };

    enum REQUEST_TYPE
    {
        REQUEST_TYPE_Get,  // get请求
        REQUEST_TYPE_Post, // post请求
    };

    enum CONNECTION_TYPE
    {
        CONNECTION_TYPE_Close,     // 立刻关闭连接
        CONNECTION_TYPE_Not_Set,   // 未设置
        CONNECTION_TYPE_Keep_Alive // 长连接
    };

    enum REQUEST_RESULT
    {
        REQUEST_RESULT_Error,        // 出错
        REQUEST_RESULT_Success,      // 成功
        REQUEST_RESULT_Success_Close // 成功，但由于连接是一次性的keep-alive:close
    };

    enum RESPONSE_RESULT
    {
        RESPONSE_RESULT_Wait,    // 需要等待数据到达
        RESPONSE_RESULT_NoSpace, // send缓冲区已满
        RESPONSE_RESULT_Close,   // 连接关闭
        RESPONSE_RESULT_Error,   // 出现错误
        RESPONSE_RESULT_Ok       // 完成请求
    };

    enum WR_RESULT
    {
        WR_RESULT_Ok,           // 完成
        WR_RESULT_Close,        // 连接已关闭
        WR_RESULT_bSend_Full,   // send缓冲区已满
        WR_RESULT_bRequest_Full // 接收缓冲区空
    };

    enum KEY_TYPE
    {
        KEY_TYPE_Normal,         // 普通key
        KEY_TYPE_Keep_Alive,     // 连接模式
        KEY_TYPE_Content_Length, // body长度
    };

    enum PARSE_STATE // 解析状态
    {
        PARSE_STATE_Type,     // 正在解析请求类型
        PARSE_STATE_Url,      // 正在解析请求url
        PARSE_STATE_Url_Body, // 正处于接收urlBody中（get请求）
        PARSE_STATE_Version,  // 正在解析http版本
        PARSE_STATE_Key,      // 正在解析某一行的key，如（Content-Length: 27 中的“Content-Length”）
        PARSE_STATE_Value,    // 正在解析某一行的value，如（Content-Length: 27 中的27）
        PARSE_STATE_Body,     // 正在解析body
        PARSE_STATE_Finished, // 完成
        PARSE_STATE_Error,    // 解析出错,应当立刻关闭连接
    };

    struct RequestInfo
    {
        int bodySize = -1;                               // body的大小
        std::string url;                                 // 域名
        std::string data;                                // 用户数据
        KEY_TYPE kType;                                  // 当前解读行项key类型
        VERSION_TYPE vType;                              // http版本
        REQUEST_TYPE rType;                              // 请求类型
        CONNECTION_TYPE cType = CONNECTION_TYPE_Not_Set; // 保持连接类型
        PARSE_STATE state = PARSE_STATE_Type;            // 当前解析状态

        void Reset();
    };

    class HttpConnect
    {
        friend class WebServer;

    private:
        static int epollfd;
        static SqlPool *sqlPool;

        int fd = -1;
        bool coRet = false;
        std::string ip;                              // ip
        RequestInfo info;                            // 当前解析
        FixedBuffer bSend;                           // 未完成发送的数据
        FixedBuffer bRequest;                        // http请求
        Generator<false, RESPONSE_RESULT> responser; // 响应器

        void Parse();                                                             // 解析函数
        void SetOneShot(ONESHOT_TYPE type);                                       // 重置oneshot
        RESPONSE_RESULT DealRequests();                                           // 处理请求
        static Generator<false, RESPONSE_RESULT> Response(HttpConnect *pConnect); // 响应函数

    public:
        ~HttpConnect(); // 析构
        void Close();//关闭连接
        void Reset();   // 重置状态
        void Init(int fd, const char *ip);
        WR_RESULT Read(); // 读取请求
        WR_RESULT Write(unsigned char *buf, unsigned int size, int *rSize);
        REQUEST_RESULT DealRead();   // 返回false则关闭连接
        REQUEST_RESULT DeadlWrite(); // 返回false则关闭连接
    };
}

#endif