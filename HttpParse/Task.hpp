#ifndef HSLL_TASK
#define HSLL_TASK

#include "Utils.h"

namespace HSLL
{

    class TaskBase
    {
    public:
        unsigned int fileSize;
        virtual void Free() = 0;
        virtual bool GetBuffer(unsigned char **pBuf, int *size) = 0;
    };

    class TaskFile : public TaskBase
    {
        int fd;
        bool finished;
        unsigned int offset;
        unsigned int alignedMapSize;
        FixedBuffer *fb;

    public:
        unsigned char *pBuffer = nullptr;

    public:
        TaskFile(const char *fileName, FixedBuffer *fb) : finished(false), offset(0)
        {
            fd = open(fileName, O_RDONLY);
            fileSize = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET);
            this->fb = fb;
        }

        bool GetBuffer(unsigned char **pBuf, int *size) override
        {
            if (!finished)
            {
                if (pBuffer)
                {
                    munmap(pBuffer, alignedMapSize);
                    pBuffer = nullptr;
                }

                long pageSize = sysconf(_SC_PAGESIZE);
                unsigned int alignOffset = (offset / pageSize) * pageSize;
                unsigned int pageOffset = offset % pageSize;

                unsigned char *pWrite;
                int rSize = fb->GetWritePointer(&pWrite);
                unsigned int mapSize = fileSize - offset > rSize ? rSize : fileSize - offset;
                alignedMapSize = ((pageOffset + mapSize + pageSize - 1) / pageSize) * pageSize;
                pBuffer = (unsigned char *)mmap(nullptr, alignedMapSize, PROT_READ, MAP_SHARED, fd, alignOffset);

                memcpy(pWrite, pBuffer + pageOffset, mapSize);
                offset += mapSize;
                fb->SubmitWrite(mapSize);

                if (offset == fileSize)
                    finished = true;
                *size = fb->GetReadPointer(pBuf);
                return false;
            }
            return true;
        }

        void Free() override
        {
            close(fd);
            if (pBuffer)
            munmap(pBuffer, alignedMapSize);
        }
    };

}

#endif