#include "Log.h"

namespace HSLL
{
	Log::Log(bool mulThread, unsigned int maxSize) : mulThread(mulThread), maxSize(maxSize)
	{
		this->logPath[0] = 0;
		strcpy(this->logPath, "ServerLog//log0000.txt");
		LOG_CREATEDIRECTORY;
	}

	Log::Log(const char *directoryPath, bool mulThread, unsigned int maxSize) : mulThread(mulThread), maxSize(maxSize)
	{
		logPath[0] = 0;
		strcpy(logPath, directoryPath);
		strcat(logPath, "//log0000.txt");
	}

	bool Log::Init()
	{
		unsigned int len = strlen(logPath);
		char *ptr = logPath + len - 1;
	reopen:
		if (fstream.is_open())
			fstream.close();

		fstream.open(logPath, std::ios::out | std::ios::app);

		if (fstream.is_open())
		{
			fileSize = fstream.tellg();
			if (fileSize > maxSize)
			{
				while (*ptr == 57)
					ptr--;
				(*ptr)++;
				goto reopen;
			}
		}
		return fstream.is_open();
	}

	void Log::WriteLog(const char *str)
	{
		if (mulThread)
			mtx.lock();

		unsigned int len = strlen(str);
		fileSize += len;

		if (fileSize > maxSize)
			Init();

		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm now_tm = *std::localtime(&now_time_t);

		char buffer[21];
		std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d\n",
					  now_tm.tm_year + 1900,
					  now_tm.tm_mon + 1,
					  now_tm.tm_mday,
					  now_tm.tm_hour,
					  now_tm.tm_min,
					  now_tm.tm_sec);
		std::string log(buffer);
		log.append(str);
		log.append("\n\n");

		fstream.write(log.c_str(), log.size());
		fstream.flush();
		
		if (mulThread)
			mtx.unlock();
	}

	Log::~Log()
	{
		if (fstream.is_open())
			fstream.close();
	}
}
