#include "Utils.h"

namespace HSLL
{
    // 请至少留31字节空间
    unsigned int HttpString::GetContentLengthAndEnd(unsigned char *buffer, unsigned int size)
    {
        unsigned int i = 0;
        unsigned char tail[15] = "          \r\n\r\n";
        while (size)
        {
            tail[9 - i] = size % 10 + 48;
            size /= 10;
            i++;
        }
        memcpy(buffer, "Content-Length: ", 16);
        memcpy(buffer + 16, tail + 10 - i, 4 + i);
        return 20 + i;
    }

    void FixedBuffer::Reset()
    {
        nowSize = 0;
        head = 0;
        tail = 0;
    }

    int FixedBuffer::Find(const char *str)
    {
        unsigned int len = strlen(str);
        if (len == 0)
            return -1;
        unsigned int nowLen = nowSize;
        unsigned int index = tail;
        while (nowLen >= len)
        {
            for (int i = 0; i < len; i++)
            {
                if (buffer[(index + i) % MAX_USER_WR_BSIZE] != str[i])
                    break;
                if (i == len - 1)
                    return nowSize - nowLen;
            }
            nowLen -= 1;
            index = (index + 1) % MAX_USER_WR_BSIZE;
        }
        return -1;
    }

    unsigned int FixedBuffer::GetWritePointer(unsigned char **from)
    {
        if (nowSize == MAX_USER_WR_BSIZE)
            return 0;
        *from = buffer + head;
        unsigned int size = MAX_USER_WR_BSIZE - head;
        return MAX_USER_WR_BSIZE - nowSize > size ? size : MAX_USER_WR_BSIZE - nowSize;
    }

    unsigned int FixedBuffer::GetReadPointer(unsigned char **from)
    {
        if (nowSize == 0)
            return 0;
        *from = buffer + tail;
        unsigned int size = MAX_USER_WR_BSIZE - tail;
        return nowSize > size ? size : nowSize;
    }

    bool FixedBuffer::SubmitRead(unsigned int size)
    {
        if (size > nowSize)
            return false;
        tail = (tail + size) % MAX_USER_WR_BSIZE;
        nowSize -= size;
        return true;
    }

    bool FixedBuffer::SubmitWrite(unsigned int size)
    {
        if (size > MAX_USER_WR_BSIZE - nowSize)
            return false;
        head = (head + size) % MAX_USER_WR_BSIZE;
        nowSize += size;
        return true;
    }

    unsigned int FixedBuffer::StringSet(std::string &s, unsigned int size)
    {
        if (nowSize == 0 || size == 0)
            return 0;
        if (nowSize < size)
            size = nowSize;

        unsigned int lenToEnd = nowSize > MAX_USER_WR_BSIZE - tail ? MAX_USER_WR_BSIZE - tail : nowSize;
        if (lenToEnd >= size)
        {
            s.append((const char *)&buffer[tail], size);
        }
        else
        {
            s.append((const char *)&buffer[tail], lenToEnd);
            s.append((const char *)&buffer[0], size - lenToEnd);
        }
        return size;
    }

    bool FixedBuffer::MemiEql(const unsigned char *str, unsigned int size)
    {
        if (nowSize < size)
            return false;
        for (unsigned int i = 0; i < size; i++)
        {
            unsigned int index = (i + tail) % MAX_USER_WR_BSIZE;
            if (buffer[index] == str[i] || ((buffer[index] > 64 && buffer[index] < 91 && buffer[index] == str[i] - 32) || (str[i] > 64 && str[i] < 91 && buffer[index] == str[i] + 32)))
                continue;
            else
                return false;
        }
        return true;
    }

    int FixedBuffer::StrToInt(unsigned int size)
    {
        if (nowSize < size)
            return -1;
        int convert = 0;
        unsigned int lenToEnd = nowSize > MAX_USER_WR_BSIZE - tail ? MAX_USER_WR_BSIZE - tail : nowSize;
        if (lenToEnd >= size)
        {
            for (unsigned int i = 0; i < size; i++)
            {
                if (buffer[tail + i] > 47 && buffer[tail + i] < 58)
                {
                    convert = convert * 10 + buffer[tail + i] - 48;
                    continue;
                }
                return -1;
            }
        }
        else
        {
            for (unsigned int i = 0; i < lenToEnd; i++)
            {
                if (buffer[tail + i] > 47 && buffer[tail + i] < 58)
                {
                    convert = convert * 10 + buffer[tail + i] - 48;
                    continue;
                }
                return -1;
            }
            for (unsigned int i = 0; i < size - lenToEnd; i++)
            {
                if (buffer[i] > 47 && buffer[i] < 58)
                {
                    convert = convert * 10 + buffer[i] - 48;
                    continue;
                }
                return -1;
            }
        }
        return convert;
    }

    bool parseBody(const std::string &data, std::vector<std::string> &vParams)
    {
        if (vParams.empty())
            return false;

        std::string s;

        for (size_t i = 0; i < vParams.size(); i++)
        {
            s += vParams[i];
            if (i == vParams.size() - 1)
                s.append( "=([^&]*)");
            else
               s.append("=([^&]*)&");
        }

        std::regex pattern(s);
        std::smatch matches;

        if (std::regex_search(data, matches, pattern))
        {
            for (unsigned int i = 0; i < vParams.size(); i++)
                vParams[i] = matches[i + 1].str();
            return true;
        }

        return false;
    }
}