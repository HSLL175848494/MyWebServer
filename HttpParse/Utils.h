#ifndef HSLL_FILEMAPPING
#define HSLL_FILEMAPPING

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <optional>
#include <string.h>
#include <vector>
#include <regex>

namespace HSLL
{
    struct HttpString
    {

        static constexpr const unsigned int ok_200_str_length = 17;
        static constexpr const unsigned int head400_str_length = 26;
        static constexpr const unsigned int text400_str_length = 37;
        static constexpr const unsigned int head403_str_length = 24;
        static constexpr const unsigned int text403_str_length = 51;
        static constexpr const unsigned int head404_str_length = 24;
        static constexpr const unsigned int text404_str_length = 50;
        static constexpr const unsigned int head500_str_length = 29;
        static constexpr const unsigned int text500_str_length = 56;
        static constexpr const unsigned int format_html_str_length = 25;
        static constexpr const unsigned int format_jpge_str_length = 26;
        static constexpr const unsigned int format_plain_str_length = 26;
        static constexpr const unsigned int format_json_str_length = 32;
        static constexpr const unsigned int format_mp4_str_length = 25;
        static constexpr const unsigned int connection_close_str_length = 19;
        static constexpr const unsigned int connection_alive_str_length = 24;
        static constexpr const unsigned int content_length_str_length = 16;

        static constexpr const char *ok_200 = "HTTP/1.1 200 OK\r\n";
        static constexpr const char *head400 = "HTTP/1.1 400 Bad Request\r\n";
        static constexpr const char *text400 = "The request has invalid parameters.\r\n";
        static constexpr const char *head403 = "HTTP/1.1 403 Forbidden\r\n";
        static constexpr const char *text403 = "Access is denied due to insufficient permissions.\r\n";
        static constexpr const char *head404 = "HTTP/1.1 404 Not Found\r\n";
        static constexpr const char *text404 = "The requested file was not found on this server.\r\n";
        static constexpr const char *head500 = "HTTP/1.1 500 Internal Error\r\n";
        static constexpr const char *text500 = "There was an unusual problem serving the request file.\r\n";
        static constexpr const char *format_html = "Content-Type: text/html\r\n";
        static constexpr const char *format_jpge = "Content-Type: image/jpeg\r\n";
        static constexpr const char *format_plain = "Content-Type: text/plain\r\n";
        static constexpr const char *format_mp4 = "Content-Type: video/mp4\r\n";
        static constexpr const char *format_json = "Content-Type: application/json\r\n";
        static constexpr const char *connection_close = "Connection: close\r\n";
        static constexpr const char *connection_alive = "Connection: keep-alive\r\n";
        static constexpr const char *content_length = "Content-Length: ";

        // 请至少留30字节空间
        static unsigned int GetContentLengthAndEnd(unsigned char *buffer, unsigned int size);
    };

#define MAX_USER_WR_BSIZE 8192

    struct FixedBuffer
    {
        unsigned char buffer[MAX_USER_WR_BSIZE];
        unsigned int head = 0;
        unsigned int tail = 0;
        unsigned int nowSize = 0;

        void Reset();
        int Find(const char *str);
        int StrToInt(unsigned int size);
        bool MemiEql(const unsigned char *str, unsigned int size);
        bool SubmitRead(unsigned int size);
        bool SubmitWrite(unsigned int size);
        unsigned int GetWritePointer(unsigned char **from);
        unsigned int GetReadPointer(unsigned char **from);
        unsigned int StringSet(std::string &s, unsigned int size);
    };

    bool parseBody(const std::string &data, std::vector<std::string> &vParams);
}

#endif