#include <Windows.h>
#include "../include/LogHelper.h"

namespace VFS
{
    inline std::string GetCurrentPath()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);

        auto szBuffer = std::string(buffer);
        auto pos = szBuffer.find_last_of("\\/");
        return szBuffer.substr(0, pos);
    }

    static void LogErrorHandler(const std::string & szMessage)
    {
        Logf(CUSTOM_LOG_FILENAME, "Log error handled: %s\n", szMessage.c_str());
    }


    CVFSLog::CVFSLog(const std::string & szLoggerName, const std::string & szFileName) :
        m_szLoggerName(szLoggerName), m_szFileName(szFileName)
    {
        try
        {
            auto sinks = std::vector<spdlog::sink_ptr>();

            sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(m_szFileName.c_str()));

            m_logger = std::make_shared<spdlog::logger>(m_szLoggerName.c_str(), sinks.begin(), sinks.end());
            m_logger->set_error_handler(LogErrorHandler);

            spdlog::register_logger(m_logger);
        }
        catch (const spdlog::spdlog_ex & ex)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on InitLogger (spdlog::spdlog_ex): %s\n", ex.what());
            abort();
        }
        catch (DWORD dwNumber)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on InitLogger (w/ number): %p\n", dwNumber);
            abort();
        }
        catch (...)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on InitLogger (w/o information!)\n");
            abort();
        }
    }

    void CVFSLog::Log(const char* c_szFunction, int iLevel, const char* c_szFormat, ...)
    {
//        std::lock_guard <std::recursive_mutex> __lock(m_mutex);

	    auto logger = spdlog::get("VFSLog");
        if (!logger)
            return;

        char pTmpString[8192] = { 0 };
        va_list vaArgList;
        va_start(vaArgList, c_szFormat);
        vsprintf_s(pTmpString, c_szFormat, vaArgList);
        va_end(vaArgList);

        char pFinalString[9000] = { 0 };
        if (strlen(c_szFunction))
            sprintf_s(pFinalString, "%s | %s", c_szFunction, pTmpString);
        else
            strcpy_s(pFinalString, pTmpString);

        try
        {
            switch (iLevel)
            {
                case LL_SYS: logger->info(pFinalString); break;
                case LL_CRI: logger->critical(pFinalString); break;
                case LL_ERR: logger->error(pFinalString); break;
                case LL_DEV: logger->debug(pFinalString); break;
                case LL_TRACE: logger->trace(pFinalString); break;
                case LL_WARN: logger->warn(pFinalString); break;
            }
            logger->flush();
        }
        catch (const spdlog::spdlog_ex & ex)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on sys_log (spdlog::spdlog_ex %s\n", ex.what());
            abort();
        }
        catch (DWORD dwNumber)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on sys_log (w/ number): %p\n", dwNumber);
            abort();
        }
        catch (...)
        {
            Logf(CUSTOM_LOG_FILENAME, "Exception throw on sys_log (w/o information!");
            abort();
        }
    }
}
