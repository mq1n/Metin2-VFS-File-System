#pragma once
#include "BasicLog.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace VFS
{
    static const auto CUSTOM_LOG_FILENAME = "VFSPro.log";

    enum EVFSLogLevels
    {
        LL_SYS,
        LL_ERR,
        LL_CRI,
        LL_WARN,
        LL_DEV,
        LL_TRACE
    };

    class CVFSLog
    {
        public:
			virtual ~CVFSLog() noexcept = default;
			CVFSLog(const CVFSLog&) = delete;
			CVFSLog(CVFSLog&&) noexcept = delete;
			CVFSLog& operator=(const CVFSLog&) = delete;
			CVFSLog& operator=(CVFSLog&&) noexcept = delete;
            
        public:
            CVFSLog() = default;
            CVFSLog(const std::string & szLoggerName, const std::string & szFileName);

            void Log(const char* c_szFunction, int iLevel, const char* c_szFormat, ...);

        private:
            mutable std::recursive_mutex		m_mutex;

            std::shared_ptr <spdlog::logger>	m_logger;
            std::string							m_szLoggerName;
            std::string							m_szFileName;
    };
}
