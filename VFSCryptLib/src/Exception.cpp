/*
**  ClanLib SDK
**  Copyright (c) 1997-2016 The ClanLib Team
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
**  Note: Some of the libraries ClanLib may link to may have additional
**  requirements or restrictions.
**
**  File Author(s):
**
**    Magnus Norddahl
*/

#include "../include/Exception.h"

#if defined HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <mutex>
#endif


static int capture_stack_trace(int frames_to_skip, int max_frames, void **out_frames)
{
#ifdef _WIN32
	if (max_frames > 32)
		max_frames = 32;

	unsigned short capturedFrames = 0;

	// RtlCaptureStackBackTrace is only available on Windows XP or newer versions of Windows
	typedef WORD(NTAPI FuncRtlCaptureStackBackTrace)(DWORD, DWORD, PVOID *, PDWORD);
	HMODULE module = LoadLibrary(TEXT("kernel32.dll"));
	if (module)
	{
		FuncRtlCaptureStackBackTrace *ptrRtlCaptureStackBackTrace = (FuncRtlCaptureStackBackTrace *)GetProcAddress(module, "RtlCaptureStackBackTrace");
		if (ptrRtlCaptureStackBackTrace)
			capturedFrames = ptrRtlCaptureStackBackTrace(frames_to_skip + 1, max_frames, out_frames, nullptr);
		FreeLibrary(module);
	}

	return capturedFrames;

#elif defined HAVE_EXECINFO_H
	// Ensure the output is cleared
	memset(out_frames, 0, (sizeof(void *)) * max_frames);

	if (out_hash)
		*out_hash = 0;

	return (backtrace(out_frames, max_frames));
#else
	return 0;
#endif
}


static std::vector<std::string> get_stack_frames_text(void **frames, int num_frames)
{
#ifdef _WIN32
	static std::recursive_mutex mutex;
	std::unique_lock<std::recursive_mutex> mutex_lock(mutex);

	BOOL result = SymInitialize(GetCurrentProcess(), NULL, TRUE);
	if (!result)
		return std::vector<std::string>();

	std::vector<std::string> backtrace_text;
	for (unsigned short i = 0; i < num_frames; i++)
	{
		unsigned char buffer[sizeof(IMAGEHLP_SYMBOL64) + 128];
		IMAGEHLP_SYMBOL64 *symbol64 = reinterpret_cast<IMAGEHLP_SYMBOL64*>(buffer);
		memset(symbol64, 0, sizeof(IMAGEHLP_SYMBOL64) + 128);
		symbol64->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		symbol64->MaxNameLength = 128;

		DWORD64 displacement = 0;
		BOOL result = SymGetSymFromAddr64(GetCurrentProcess(), (DWORD64)frames[i], &displacement, symbol64);
		if (result)
		{
			IMAGEHLP_LINE64 line64;
			DWORD displacement = 0;
			memset(&line64, 0, sizeof(IMAGEHLP_LINE64));
			line64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			result = SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)frames[i], &displacement, &line64);
			if (result)
			{
				char szText[512];
				sprintf_s(szText, "%s (%s, line %d)",
					symbol64->Name, line64.FileName, (int)line64.LineNumber);

				backtrace_text.push_back(szText);
			}
			else
			{
				backtrace_text.push_back(symbol64->Name);
			}
		}
	}

	SymCleanup(GetCurrentProcess());
	return backtrace_text;

#elif defined HAVE_EXECINFO_H

	char **strings;
	strings = backtrace_symbols(frames, num_frames);
	if (!strings)
	{
		return std::vector<std::string>();
	}

	std::vector<std::string> backtrace_text;

	for (int cnt = 0; cnt < num_frames; cnt++)
	{
		// Decode the strings
		char *ptr = strings[cnt];
		char *filename = ptr;
		const char *function = "";

		// Find function name
		while (*ptr)
		{
			if (*ptr == '(')	// Found function name
			{
				*(ptr++) = 0;
				function = ptr;
				break;
			}
			ptr++;
		}

		// Find offset
		if (function[0])	// Only if function was found
		{
			while (*ptr)
			{
				if (*ptr == '+')	// Found function offset
				{
					*(ptr++) = 0;
					break;
				}
				if (*ptr == ')')	// Not found function offset, but found, end of function
				{
					*(ptr++) = 0;
					break;
				}
				ptr++;
			}
		}

		int status;
		char *new_function = abi::__cxa_demangle(function, nullptr, nullptr, &status);
		if (new_function)	// Was correctly decoded
		{
			function = new_function;
		}

		backtrace_text.push_back(string_format("%1 (%2)", function, filename));

		if (new_function)
		{
			free(new_function);
		}
	}

	free(strings);
	return backtrace_text;
#else
	return std::vector<std::string>();
#endif
}



Exception::Exception(const std::string &message) : message(message)
{
	num_frames = capture_stack_trace(1, max_frames, frames);
	for (int i = num_frames; i < max_frames; i++)
		frames[i] = nullptr;
}

const char* Exception::what() const throw()
{
	// Note, buffer is mutable
	buffer = message;
	return buffer.c_str();
}

std::vector<std::string> Exception::get_stack_trace() const
{
	return get_stack_frames_text(frames, num_frames);
}

std::string Exception::get_message_and_stack_trace() const
{
	std::vector<std::string> stack_trace = get_stack_trace();
	std::string text = message;
	for (auto & elem : stack_trace)
	{
#ifdef _WIN32
		text += "\r\n    at ";
#else
		text += "\n    at ";
#endif

		text += elem.c_str();
	}

	return text;
}
