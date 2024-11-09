#include "HttpParse.h"

namespace HSLL
{
    void RequestInfo::Reset()
    {
        url.clear();
        data.clear();
        bodySize = -1;
        cType = CONNECTION_TYPE_Not_Set;
        state = PARSE_STATE_Type;
    }

    int HttpConnect::epollfd = -1;
    SqlPool *HttpConnect::sqlPool = nullptr;

    void HttpConnect::Parse() // 解析http请求
    {
        int index;
        unsigned char *pRead;

        switch (info.state)
        {
        case PARSE_STATE_Finished:
            goto Sign_Type;
        case PARSE_STATE_Type:
            goto Sign_Type;
        case PARSE_STATE_Url:
            goto Sign_Url;
        case PARSE_STATE_Url_Body:
            goto Sign_Url_Body;
        case PARSE_STATE_Version:
            goto Sign_Version;
        case PARSE_STATE_Key:
            goto Sign_Key;
        case PARSE_STATE_Value:
            goto Sign_Value;
        case PARSE_STATE_Body:
            goto Sign_Body;
        }

    Sign_Type:

        PARSE_NOT_FIND_RETURN(" ", 4)

        if (bRequest.MemiEql((const unsigned char *)"PO", 2))
            info.rType = REQUEST_TYPE_Post;
        else if (bRequest.MemiEql((const unsigned char *)"GE", 2))
            info.rType = REQUEST_TYPE_Get;
        else
            PARSE_ERROR_RETURN

        bRequest.SubmitRead(index + 1);
        info.state = PARSE_STATE_Url;

    Sign_Url:

        if (info.rType == REQUEST_TYPE_Get)
        {
            index = bRequest.Find("?");
            if (index == -1)
            {
                PARSE_NOT_FIND_RETURN(" ", 256)
                bRequest.StringSet(info.url, index);
            }
            else
            {
                bRequest.StringSet(info.url, index);
                bRequest.SubmitRead(index + 1);
                info.state = PARSE_STATE_Url_Body;
            Sign_Url_Body:
                PARSE_NOT_FIND_RETURN(" ", 256)
                bRequest.StringSet(info.data, index);
            }
        }
        else
        {
            PARSE_NOT_FIND_RETURN(" ", 256)
            bRequest.StringSet(info.url, index);
        }

        bRequest.SubmitRead(index + 1);
        info.state = PARSE_STATE_Version;
    Sign_Version:

        PARSE_NOT_FIND_RETURN("\r\n", 8)
        if (bRequest.MemiEql((const unsigned char *)"HTTP/1.0", 8))
            info.vType = VERSION_TYPE_10;
        else if (bRequest.MemiEql((const unsigned char *)"HTTP/1.1", 8))
            info.vType = VERSION_TYPE_11;
        else
            PARSE_ERROR_RETURN

        bRequest.SubmitRead(index + 2);
        info.state = PARSE_STATE_Key;
    Sign_Key:

        if (bRequest.Find("\r\n") == 0)
        {
            bRequest.SubmitRead(2);
            if (info.rType == REQUEST_TYPE_Get)
                PARSE_FINISHED_RETURN
            else if (info.bodySize == -1) // 未设置BodySize大小
                PARSE_ERROR_RETURN
            else
                goto Sign_Body;
        }
        PARSE_NOT_FIND_RETURN(": ", 128)

        if (index == 14 && bRequest.MemiEql((const unsigned char *)"Content-Length", 14))
            info.kType = KEY_TYPE_Content_Length;
        else if (index == 10 && bRequest.MemiEql((const unsigned char *)"Connection", 10))
            info.kType = KEY_TYPE_Keep_Alive;
        else
            info.kType = KEY_TYPE_Normal;

        bRequest.SubmitRead(index + 2);
        info.state = PARSE_STATE_Value;
    Sign_Value:

        PARSE_NOT_FIND_RETURN("\r\n", 512)

        if (info.kType == KEY_TYPE_Content_Length)
            info.bodySize = bRequest.StrToInt(index);
        else if (info.kType == KEY_TYPE_Keep_Alive)
        {
            if (bRequest.MemiEql((const unsigned char *)"close", 5))
                info.cType = CONNECTION_TYPE_Close;
            else if (bRequest.MemiEql((const unsigned char *)"keep-alive", 10))
                info.cType = CONNECTION_TYPE_Close;
        }

        bRequest.SubmitRead(index + 2);
        info.state = PARSE_STATE_Key;
        goto Sign_Key; // 回到上一步

    Sign_Body:

        while (info.bodySize)
        {
            unsigned int rSize = bRequest.StringSet(info.data, info.bodySize);
            if (rSize == 0) //
                return;
            info.bodySize -= rSize;
            bRequest.SubmitRead(rSize);
        }

        info.state = PARSE_STATE_Finished;
        return;
    }

    Generator<false, RESPONSE_RESULT> HttpConnect::Response(HttpConnect *pConnect) // 响应器
    {
        while (true)
        {
            pConnect->Parse(); // 解析一个请求

            if (pConnect->info.state == PARSE_STATE_Error) // 错误请求
                co_return RESPONSE_RESULT_Error;
            else if (pConnect->info.state != PARSE_STATE_Finished) // 请求不完整
            {
                co_yield RESPONSE_RESULT_Wait;
                if (pConnect->coRet)
                    co_return RESPONSE_RESULT_Error;
                continue;
            }

            int rRead;
            int size;
            unsigned char *pBuf;
            unsigned char *pWrite;

            TaskBase *pt = nullptr;

            auto WriteHead = [&](const char *contentType, unsigned int typeLength) // 正常请求
            {
                unsigned int offset = 0;
                pConnect->bSend.GetWritePointer(&pWrite);
                memcpy(pWrite, "HTTP/1.1 200 OK\r\n", 17); // 写入line
                offset += 17;
                memcpy(&pWrite[offset], "Connection: keep-alive\r\n", 24); // 写入长连接
                offset += 24;
                memcpy(&pWrite[offset], contentType, typeLength); // 写入content格式
                offset += typeLength;
                pConnect->bSend.SubmitWrite(offset);
            };

            auto serverError = [&]()
            {
                static const char *str =
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 21\r\n"
                    "\r\n"
                    "Internal Server Error";

                pConnect->bSend.GetWritePointer(&pWrite);
                memcpy(pWrite, str, 105);
                pConnect->bSend.SubmitWrite(105);
                size = pConnect->bSend.GetReadPointer(&pBuf);
            };

            auto invalidParams = [&]()
            {
                static const char *str =
                    "HTTP/1.1 400 The request has invalid parameters.\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 35\r\n"
                    "\r\n"
                    "The request has invalid parameters.";

                pConnect->bSend.GetWritePointer(&pWrite);
                memcpy(pWrite, str, 133);
                pConnect->bSend.SubmitWrite(133);
                size = pConnect->bSend.GetReadPointer(&pBuf);
            };

            auto notFound = [&]()
            {
                static const char *str =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 48\r\n"
                    "\r\n"
                    "The requested page or resource is not supported.";
                pConnect->bSend.GetWritePointer(&pWrite);
                memcpy(pWrite, str, 120);
                pConnect->bSend.SubmitWrite(120);
                size = pConnect->bSend.GetReadPointer(&pBuf);
            };

            if (!pConnect->info.url.compare("/"))
            {
                WriteHead(HttpString::format_html, HttpString::format_html_str_length);
                pConnect->bSend.GetWritePointer(&pWrite);
                pt = new TaskFile("Root/judge.html", &pConnect->bSend);
                rRead = HttpString::GetContentLengthAndEnd(pWrite, pt->fileSize);
                pConnect->bSend.SubmitWrite(rRead);
                pt->GetBuffer(&pBuf, &size);
            }
            else if (!pConnect->info.url.compare("/Root/page.jpg"))
            {
                WriteHead(HttpString::format_jpge, HttpString::format_jpge_str_length);
                pConnect->bSend.GetWritePointer(&pWrite);
                pt = new TaskFile("Root/page.jpg", &pConnect->bSend);
                rRead = HttpString::GetContentLengthAndEnd(pWrite, pt->fileSize);
                pConnect->bSend.SubmitWrite(rRead);
                pt->GetBuffer(&pBuf, &size);
            }
            else if (!pConnect->info.url.compare("/0"))
            {
                static const char *succeed = "{\"success\": true, \"message\": \"Invalid credentials\"}";
                static const char *failed = "{\"success\": false, \"message\": \"Invalid credentials\"}";

                std::vector<std::string> vLogin = {"username", "password"};
                if (!parseBody(pConnect->info.data, vLogin))
                {
                    invalidParams();
                }
                else
                {
                    MYSQL *pMysql = pConnect->sqlPool->WaitConnect();
                    std::string query = "SELECT COUNT(*) FROM user WHERE username = '" + vLogin[0] +
                                        "' AND password = '" + vLogin[1] + "'";

                    if (mysql_query(pMysql, query.c_str()))
                    {
                        fprintf(stderr, "Query Error: %s\n", mysql_error(pMysql));
                        pConnect->sqlPool->FreeConnect(pMysql);
                        serverError();
                    }
                    else
                    {
                        MYSQL_RES *res = mysql_store_result(pMysql);
                        if (res)
                        {
                            MYSQL_ROW row = mysql_fetch_row(res);

                            if (row && atoi(row[0]) > 0)
                                pt = new TaskFile("Root/video.html", &pConnect->bSend);
                            else
                                pt = new TaskFile("Root/jugeError.html", &pConnect->bSend);

                            mysql_free_result(res);
                            WriteHead(HttpString::format_html, HttpString::format_html_str_length);
                            pConnect->bSend.GetWritePointer(&pWrite);
                            rRead = HttpString::GetContentLengthAndEnd(pWrite, pt->fileSize);
                            pConnect->bSend.SubmitWrite(rRead);
                            pt->GetBuffer(&pBuf, &size);
                        }
                        else
                        {
                            serverError();
                        }
                    }
                    pConnect->sqlPool->FreeConnect(pMysql);
                }
            }
            else if (!pConnect->info.url.compare("/Root/sakura.mp4"))
            {
                WriteHead(HttpString::format_mp4, HttpString::format_mp4_str_length);
                pConnect->bSend.GetWritePointer(&pWrite);
                pt = new TaskFile("Root/sakura.mp4", &pConnect->bSend);
                rRead = HttpString::GetContentLengthAndEnd(pWrite, pt->fileSize);
                pConnect->bSend.SubmitWrite(rRead);
                pt->GetBuffer(&pBuf, &size);
            }
            else
                notFound();

            WR_RESULT result;

            while (true)
            {
                if (size == 0)
                {
                    if (pt)
                    {
                        pConnect->bSend.Reset();
                        bool last = pt->GetBuffer(&pBuf, &size);
                        if (last)
                        {
                            delete pt;
                            pt = nullptr;
                            if (pConnect->info.cType == CONNECTION_TYPE_Close)
                                co_return RESPONSE_RESULT_Close;
                            else if (pConnect->info.cType == CONNECTION_TYPE_Not_Set && pConnect->info.vType == VERSION_TYPE_10)
                                co_return RESPONSE_RESULT_Close;
                            break;
                        }
                    }
                    else
                    {
                        if (pConnect->info.cType == CONNECTION_TYPE_Close)
                            co_return RESPONSE_RESULT_Close;
                        else if (pConnect->info.cType == CONNECTION_TYPE_Not_Set && pConnect->info.vType == VERSION_TYPE_10)
                            co_return RESPONSE_RESULT_Close;
                        break;
                    }
                }
                result = pConnect->Write(pBuf, size, &rRead);
                if (result == WR_RESULT_Ok)
                {
                    size -= rRead;
                    pBuf += rRead;
                }
                else if (result == WR_RESULT_bSend_Full) // 暂停
                {
                    co_yield RESPONSE_RESULT_NoSpace;
                    if (pConnect->coRet)
                    {
                        if (pt)
                            delete pt;
                        co_return RESPONSE_RESULT_Error;
                    }
                    continue;
                }
                else
                {
                    if (pt)
                        delete pt;
                    co_return RESPONSE_RESULT_Close;
                }
            }

            pConnect->bSend.Reset(); // 重置bSend
            pConnect->info.Reset();  // 重置Info

            co_yield RESPONSE_RESULT_Ok;
            if (pConnect->coRet)
                co_return RESPONSE_RESULT_Error;
        }
    };

    RESPONSE_RESULT HttpConnect::DealRequests()
    {
        while (true)
        {
            responser.Resume();
            RESPONSE_RESULT result = responser.Value().value();
            if (result != RESPONSE_RESULT_Ok)
                return result;
        }
    }

    void HttpConnect::Close()
    {
        if (responser.HandleInvalid() && !responser.hasDone()) // 结束协程
        {
            coRet = true;
            responser.Resume();
            coRet = false;
        }
        if (fd != -1)
        {
            epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
            close(fd);
            fd = -1;
        }
    }

    void HttpConnect::Reset() // 重置
    {
        ip.clear();
        info.Reset();
        bSend.Reset();
        bRequest.Reset();
        Close();
    }

    HttpConnect::~HttpConnect()
    {
        Close();
    }

    void HttpConnect::Init(int fd, const char *ip)
    {
        this->fd = fd;
        this->ip.append(ip);
        responser = Response(this);
    }

    WR_RESULT HttpConnect::Read()
    {
        int rSize;
        unsigned char *pWrite;
        unsigned int bSize;
        while (true)
        {
            bSize = bRequest.GetWritePointer(&pWrite);
            if (bSize == 0) // 没有可写空间
                return WR_RESULT_bRequest_Full;

            rSize = recv(fd, pWrite, bSize, 0);
            if (rSize == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    return WR_RESULT_Close;
                else
                    return WR_RESULT_Ok;
            }
            else if (rSize == 0)
                return WR_RESULT_Close;

            bRequest.SubmitWrite(rSize);
        }
    }

    WR_RESULT HttpConnect::Write(unsigned char *buf, unsigned int size, int *rSize)
    {
        *rSize = send(fd, buf, size, MSG_NOSIGNAL);
        if (*rSize == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                return WR_RESULT_Close;
            else
                return WR_RESULT_bSend_Full;
        }
        else if (*rSize == 0)
            return WR_RESULT_Close;
        return WR_RESULT_Ok;
    }

    REQUEST_RESULT HttpConnect::DealRead() // 返回false则关闭连接
    {
        while (true)
        {
            WR_RESULT result = Read();

            if (result == WR_RESULT_Close)
                return REQUEST_RESULT_Success_Close;

            RESPONSE_RESULT rResult = DealRequests();

            if (rResult == RESPONSE_RESULT_Error) // 出现错误
            {
                return REQUEST_RESULT_Error;
            }
            else if (rResult == RESPONSE_RESULT_Close) // 响应完成请求关闭，短连接
            {
                return REQUEST_RESULT_Success_Close;
            }
            else if (rResult == RESPONSE_RESULT_NoSpace) // send缓冲区已满未完成,重置Out事件
            {
                SetOneShot(ONESHOT_TYPE_Out);
            }
            else // 已经完成所有请求(处于wait状态)
            {
                if (result == WR_RESULT_bRequest_Full) // 重新尝试写入
                    continue;
                SetOneShot(ONESHOT_TYPE_In); // 请求处理完毕重置in事件
            }
            return REQUEST_RESULT_Success;
        }
    }

    REQUEST_RESULT HttpConnect::DeadlWrite()
    {
        RESPONSE_RESULT reuslt = DealRequests();

        switch (reuslt)
        {
        case RESPONSE_RESULT_Error: // 出错
            return REQUEST_RESULT_Error;
        case RESPONSE_RESULT_Close: // 请求关闭连接
            return REQUEST_RESULT_Success_Close;
        case RESPONSE_RESULT_NoSpace: // send缓冲区已满未完成,继续监听out事件
            SetOneShot(ONESHOT_TYPE_Out);
            break;
        default: // 已经完成所有请求(处于wait状态)
            SetOneShot(ONESHOT_TYPE_In);
            break;
        }
        return REQUEST_RESULT_Success;
    }
}