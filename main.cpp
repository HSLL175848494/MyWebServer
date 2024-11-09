#include "WebServer.h"
using namespace HSLL;

int main()
{
    {
        /*
        Log log(true);
        FALSE_PRINT_RETURN(log.Init(), "invalid log path");
        */
        Config config;
        memcpy(config.databaseName, "BaseWebServer", sizeof("BaseWebServer") + 1);
        memcpy(config.userName, "liu", sizeof("liu") + 1);
        memcpy(config.password, "@175848494", sizeof("@175848494") + 1);
        //HSLL::WebServer ws(&config,&log);
        HSLL::WebServer ws(&config, nullptr);
        FALSE_RETURN(ws.Init());
        FALSE_RETURN(ws.StartListen());
        ws.EventLoop();
    }

    return 0;
}
