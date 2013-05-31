#include "stdafx.h"

#include "../shared/AutoHandle.h"
#include "../shared/Communication.h"
#include "Debug.h"
#include "Utils.h"

namespace
{
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine;

  std::string ToUtf8String(std::wstring str)
  {
    size_t length = str.size();
    if (length == 0)
      return std::string();

    DWORD utf8StringLength = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, 0, 0, 0, 0);
    if (utf8StringLength == 0)
      throw std::runtime_error("Failed to determine the required buffer size");

    std::string utf8String(utf8StringLength, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), length, &utf8String[0], utf8StringLength, 0, 0);
    return utf8String;
  }

  void WriteStrings(Communication::OutputBuffer& response,
      const std::vector<std::string> strings)
  {
    int32_t count = strings.size();
    response << count;
    for (int32_t i = 0; i < count; i++)
      response << strings[i];
  }

  void WriteSubscriptions(Communication::OutputBuffer& response,
      const std::vector<AdblockPlus::SubscriptionPtr> subscriptions)
  {
    int32_t count = subscriptions.size();
    response << count;
    for (int32_t i = 0; i < count; i++)
    {
      AdblockPlus::SubscriptionPtr subscription = subscriptions[i];
      response << subscription->GetProperty("url")->AsString()
               << subscription->GetProperty("title")->AsString()
               << subscription->GetProperty("specialization")->AsString()
               << subscription->IsListed();
    }
  }

  Communication::OutputBuffer HandleRequest(Communication::InputBuffer& request)
  {
    Communication::OutputBuffer response;

    std::string procedureName;
    request >> procedureName;
    if (procedureName == "Matches")
    {
      std::string url;
      std::string type;
      std::string documentUrl;
      request >> url >> type >> documentUrl;
      response << filterEngine->Matches(url, type, documentUrl);
    }
    else if (procedureName == "GetElementHidingSelectors")
    {
      std::string domain;
      request >> domain;
      WriteStrings(response, filterEngine->GetElementHidingSelectors(domain));
    }
    else if (procedureName == "FetchAvailableSubscriptions")
    {
      WriteSubscriptions(response, filterEngine->FetchAvailableSubscriptions());
    }
    else if (procedureName == "GetListedSubscriptions")
    {
      WriteSubscriptions(response, filterEngine->GetListedSubscriptions());
    }
    else if (procedureName == "SetSubscription")
    {
      std::string url;
      request >> url;

      std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->GetListedSubscriptions();
      for (size_t i = 0, count = subscriptions.size(); i < count; i++)
        subscriptions[i]->RemoveFromList();

      filterEngine->GetSubscription(url)->AddToList();
    }
    else if (procedureName == "UpdateAllSubscriptions")
    {
      std::vector<AdblockPlus::SubscriptionPtr> subscriptions = filterEngine->GetListedSubscriptions();
      for (size_t i = 0, count = subscriptions.size(); i < count; i++)
        subscriptions[i]->UpdateFilters();
    }
    else if (procedureName == "GetExceptionDomains")
    {
      std::vector<AdblockPlus::FilterPtr> filters = filterEngine->GetListedFilters();
      std::vector<std::string> domains;
      for (size_t i = 0, count = filters.size(); i < count; i++)
      {
        AdblockPlus::FilterPtr filter = filters[i];
        if (filter->GetType() == AdblockPlus::Filter::TYPE_EXCEPTION)
        {
          std::string text = filter->GetProperty("text")->AsString();

          //@@||example.com^$document
          const char prefix[] = "@@||";
          const char suffix[] = "^$document";
          const int prefixLen = strlen(prefix);
          const int suffixLen = strlen(suffix);
          if (!text.compare(0, prefixLen, prefix) &&
              !text.compare(text.size() - suffixLen, suffixLen, suffix))
          {
            domains.push_back(text.substr(prefixLen, text.size() - prefixLen - suffixLen));
          }
        }
      }

      WriteStrings(response, domains);
    }
    else if (procedureName == "AddFilter")
    {
      std::string text;
      request >> text;

      filterEngine->GetFilter(text)->AddToList();
    }
    return response;
  }

  DWORD WINAPI ClientThread(LPVOID param)
  {
    std::auto_ptr<Communication::Pipe> pipe(static_cast<Communication::Pipe*>(param));

    try
    {
      Communication::InputBuffer message = pipe->ReadMessage();
      Communication::OutputBuffer response = HandleRequest(message);
      pipe->WriteMessage(response);
    }
    catch (const std::exception& e)
    {
      DebugException(e);
    }

    // TODO: Keep the pipe open until the client disconnects

    return 0;
  }
}

std::auto_ptr<AdblockPlus::FilterEngine> CreateFilterEngine()
{
  // TODO: Pass appInfo in, which should be sent by the client
  AdblockPlus::JsEnginePtr jsEngine = AdblockPlus::JsEngine::New();
  std::string dataPath = ToUtf8String(GetAppDataPath());
  dynamic_cast<AdblockPlus::DefaultFileSystem*>(jsEngine->GetFileSystem().get())->SetBasePath(dataPath);
  std::auto_ptr<AdblockPlus::FilterEngine> filterEngine(new AdblockPlus::FilterEngine(jsEngine));
  return filterEngine;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  // TODO: Attempt to create the pipe first, and exit immediately if this
  //       fails. Since multiple instances of the engine could be running,
  //       this may need named mutices to avoid race conditions.
  //       Note that as soon as the pipe is created first, we can reduce the
  //       client timeout after CreateProcess(), but should increase the one
  //       in WaitNamedPipe().

  filterEngine = CreateFilterEngine();

  for (;;)
  {
    try
    {
      Communication::Pipe* pipe = new Communication::Pipe(Communication::pipeName,
            Communication::Pipe::MODE_CREATE);

      // TODO: Count established connections, kill the engine when none are left
      AutoHandle thread(CreateThread(0, 0, ClientThread, static_cast<LPVOID>(pipe), 0, 0));
      if (!thread.get())
      {
        delete pipe;
        DebugLastError("CreateThread failed");
        return 1;
      }
    }
    catch (std::runtime_error e)
    {
      DebugException(e);
      return 1;
    }
  }

  return 0;
}
