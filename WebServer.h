#ifndef HSLL_WEBSERVER
#define HSLL_WEBSERVER

#include <fcntl.h>
#include<signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "Config.h"
#include "HttpParse/HttpParse.h"
#include "ThreadPool/ThreadPool.hpp"

namespace HSLL
{

static volatile bool server_loop =true;

    class WebServer;

#define MAX_TASK_BSIZE 500
#define MAX_EVENT_BSIZE 10000

    enum TaskEvent_Type
    {
        TaskEvent_Read,
        TaskEvent_Write,
        TaskEvent_Close,
    };

    class TaskEvent
    {
        int fd;
        WebServer *pWebServer;
        TaskEvent_Type type;

    public:
        void Fill(WebServer *pWebServer, TaskEvent_Type type, int fd);
        void execute();
    };

    class WebServer
    {
        friend class TaskEvent;

    private:
        Log *pLog;
        Config *pConfig;
        SqlPool sqlPool;
        HttpConnect *users;
        ThreadPool<TaskEvent> threadPool;
        epoll_event events[MAX_EVENT_BSIZE];

        int epollfd;
        int listenfd;

        void DealConnect();
        void DealClose(int fd);
        void DealWrite(int fd);
        void DealRead(int fd);
        bool AddtoEpoll(int fd, bool oneShot);

    public:
        bool Init();
        bool EventLoop();
        bool StartListen();
        WebServer(Config *pConfig, Log *pLog = nullptr);
        ~WebServer();
    };
}
#endif