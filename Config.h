#ifndef HSLL_CONFIG
#define HSLL_CONFIG

namespace HSLL
{

#define CONFIG_STR_MAX_LENGTH 128

    struct Config
    {
        unsigned short port = 9054; // 端口号

        unsigned short connectMax = 8; // sql连接池的最大数量

        unsigned short threadNum = 8; // 线程池内的线程数量

        unsigned int maxUser = 65535; // 最大连接数

        unsigned int maxEventBufNum = 10000; // 单次最大事件处理数目

        unsigned int taskQueueLength=50000;//任务队列的长度

        unsigned short close_Timewait = 2; // 主动关闭连接时,数据等待时间（单位:秒）

        unsigned int keepAlive_Timeout = 60; // 非活跃连接检查间隔（单位:秒）

        char userName[CONFIG_STR_MAX_LENGTH]; // 数据库用户名

        char password[CONFIG_STR_MAX_LENGTH]; // 数据库密码

        char databaseName[CONFIG_STR_MAX_LENGTH]; // 数据库名
    };

}

#endif