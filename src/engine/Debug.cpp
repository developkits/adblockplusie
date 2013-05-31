#include "stdafx.h"

#include <fstream>
#include <stdio.h>

#include "Debug.h"
#include "Utils.h"

#ifdef _DEBUG

namespace
{
  class CriticalSection
  {
  public:
    CriticalSection()
    {
      InitializeCriticalSection(&section);
    }

    ~CriticalSection()
    {
      DeleteCriticalSection(&section);
    }

    class Lock
    {
    public:
      Lock(CriticalSection& cs)
          : section(&cs.section)
      {
        EnterCriticalSection(section);
      }

      ~Lock()
      {
        LeaveCriticalSection(section);
      }
    private:
      LPCRITICAL_SECTION section;
    };
  private:
    CRITICAL_SECTION section;
  };

  static CriticalSection debugLock;
}

void Debug(const std::string& text)
{
  SYSTEMTIME st;
  ::GetSystemTime(&st);

  char timeBuf[14];
  _snprintf_s(timeBuf, _TRUNCATE, "%02i:%02i:%02i.%03i", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

  std::wstring filePath = GetAppDataPath() + L"\\debug_engine.txt";

  CriticalSection::Lock lock(debugLock);
  std::ofstream out(filePath, std::ios::app);
  out << timeBuf << " - " << text << std::endl;
  out.flush();
}

void DebugLastError(const std::string& message)
{
  std::stringstream stream;
  stream << message << " (Error code: " << GetLastError() << ")";
  Debug(stream.str());
}

void DebugException(const std::exception& exception)
{
  Debug(std::string("An exception occurred: ") + exception.what());
}

#endif // _DEBUG
