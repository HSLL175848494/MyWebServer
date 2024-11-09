#include "WebServer.h"

namespace HSLL
{
    void TaskEvent::Fill(WebServer *pWebServer, TaskEvent_Type type, int fd)
    {
        this->pWebServer = pWebServer;
        this->type = type;
        this->fd = fd;
    };

    void TaskEvent::execute()
    {
        switch (type)
        {
        case TaskEvent_Read:
            pWebServer->DealRead(fd);
            break;
        case TaskEvent_Write:
            pWebServer->DealWrite(fd);
            break;
        default:
            pWebServer->DealClose(fd);
            break;
        }
    }

    void HttpConnect::SetOneShot(ONESHOT_TYPE type)
    {
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLONESHOT | EPOLLERR | EPOLLHUP | EPOLLRDHUP | type;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    }

    bool WebServer::AddtoEpoll(int fd, bool oneShot) // LT触发
    {
        int new_option = fcntl(fd, F_GETFL) | O_NONBLOCK; // 设置非阻塞
        fcntl(fd, F_SETFL, new_option);
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
        if (oneShot)
            event.events |= EPOLLONESHOT;
        FALSE_LOG_RETURN(0 == epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event), pLog, "add to epoll error");
        return true;
    }

    void WebServer::DealConnect()
    {
        struct sockaddr_in address;
        socklen_t addrLength = sizeof(address);
        while (true)
        {
            int fd = accept(listenfd, (struct sockaddr *)&address, &addrLength);

            if (fd < 0)
                return;

            if (fd >= pConfig->maxUser)
            {
                close(fd);
                return;
            }
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), clientIP, INET_ADDRSTRLEN);
            users[fd].Init(fd, clientIP);
            if (!AddtoEpoll(fd, true))
                close(fd);
        }
    }

    void WebServer::DealClose(int fd)
    {
        users[fd].Reset(); // 重置状态
    }

    void WebServer::DealWrite(int fd)
    {
        REQUEST_RESULT result = users[fd].DeadlWrite();
        if (result == REQUEST_RESULT_Error)
        {
            users[fd].ip.append(" :发送了错误请求");
            pLog->WriteLog(users[fd].ip.c_str());
            users[fd].Reset();
        }
        else if (result == REQUEST_RESULT_Success_Close) // 需要主动关闭
            users[fd].Reset();                           // 重置状态
    }

    void WebServer::DealRead(int fd)
    {
        REQUEST_RESULT result = users[fd].DealRead();
        if (result == REQUEST_RESULT_Error)
        {
            users[fd].ip.append(" :发送了错误请求");
            pLog->WriteLog(users[fd].ip.c_str());
            users[fd].Reset();
        }
        else if (result == REQUEST_RESULT_Success_Close) // 需要主动关闭
            users[fd].Reset();                           // 重置状态
    }

    bool WebServer::StartListen()
    {
        // 初始化
        listenfd = socket(PF_INET, SOCK_STREAM, 0);
        FALSE_LOG_THROW(listenfd >= 0, pLog, "listenfd init error");

        linger lin{1, 2};
        int keepAlive = 1;
        FALSE_LOG_THROW(setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) == 0, // 设置检测非活动连接
                        pLog, "set keep alive error");
        FALSE_LOG_THROW(setsockopt(listenfd, IPPROTO_TCP, TCP_KEEPIDLE, &pConfig->keepAlive_Timeout, sizeof(pConfig->keepAlive_Timeout)) == 0,
                        pLog, "set alive interval error");
        FALSE_LOG_THROW(setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin)) == 0,
                        pLog, "set linger error");

        // 开始监听
        int flag = 1;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(pConfig->port);
        FALSE_LOG_THROW(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == 0, pLog, "set resuse addr error");
        FALSE_LOG_THROW(bind(listenfd, (struct sockaddr *)&address, sizeof(address)) >= 0, pLog, "bind listenfd error");
        FALSE_LOG_THROW(listen(listenfd, SOMAXCONN) == 0, pLog, "start listen error");

        // epoll
        FALSE_LOG_THROW((epollfd = epoll_create(5)) != -1, pLog, "epoll create error")
        AddtoEpoll(listenfd, false);

        // 初始化http相关数据
        HttpConnect::epollfd = epollfd;
        HttpConnect::sqlPool = &sqlPool;

        return true;
    }

    bool WebServer::EventLoop()
    {
        threadPool.Start(pConfig->taskQueueLength, pConfig->threadNum);

        char tasksSpace[sizeof(TaskEvent) * MAX_TASK_BSIZE];
        TaskEvent *tasks = (TaskEvent *)tasksSpace;
        unsigned int TaskNum = 0;

        while (server_loop)
        {
            int sockfd;
            int num = epoll_wait(epollfd, events, MAX_EVENT_BSIZE, -1);
            FALSE_LOG_RETURN(num >= 0 || errno == EINTR, pLog, "epoll error")

            for (int i = 0; i < num; i++)
            {
                sockfd = events[i].data.fd;

                if (TaskNum == MAX_TASK_BSIZE)
                {
                    threadPool.AppendSeveral(tasks, MAX_TASK_BSIZE);
                    TaskNum = 0;
                }

                if (sockfd == listenfd)
                {
                    DealConnect();
                }
                else if (events[i].events & EPOLLIN) // Read event
                {
                    tasks[TaskNum++].Fill(this, TaskEvent_Read, sockfd);
                }
                else if (events[i].events & EPOLLOUT) // Write event
                {
                    tasks[TaskNum++].Fill(this, TaskEvent_Write, sockfd);
                }
                else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) // close
                {
                    tasks[TaskNum++].Fill(this, TaskEvent_Close, sockfd);
                }
            }

            if (TaskNum == 1)
                threadPool.Append(tasks[0]);
            else if (TaskNum >= 1)
                threadPool.AppendSeveral(tasks, TaskNum);

            TaskNum = 0;
        }

        return true;
    }

    void HandleClose(int sig)
    {
        server_loop = true;
    }

    bool WebServer::Init()
    {
        signal(SIGINT, HandleClose);
        FALSE_LOG_RETURN(pConfig, pLog, "config is nullptr");
        users = new HttpConnect[pConfig->maxUser];
        FALSE_LOG_RETURN_FUNC(sqlPool.Init("localhost", pConfig->userName, pConfig->password, pConfig->databaseName, 3306),
                              sqlPool.Release(), pLog, "sqlPools init error");
        return true;
    }

    WebServer::WebServer(Config *pConfig, Log *pLog) : pConfig(pConfig), pLog(pLog), users(nullptr) {};

    WebServer::~WebServer()
    {
        threadPool.Release();
        sqlPool.Release();
        if (users)
            delete[] users;
    }
}