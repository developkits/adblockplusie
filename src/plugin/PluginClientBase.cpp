#include "PluginStdAfx.h"

// Internet / FTP
#include <wininet.h>

// IP adapter
#include <iphlpapi.h>

#include "PluginSettings.h"
#include "PluginSystem.h"
#include "PluginDictionary.h"
#include "PluginHttpRequest.h"
#include "PluginMutex.h"
#include "PluginClass.h"

#include "PluginClientBase.h"

// IP adapter
#pragma comment(lib, "IPHLPAPI.lib")

// IE functions
#pragma comment(lib, "iepmapi.lib")

// Internet / FTP
#pragma comment(lib, "wininet.lib")


CComAutoCriticalSection CPluginClientBase::s_criticalSectionLocal;

std::vector<CPluginError> CPluginClientBase::s_pluginErrors;

bool CPluginClientBase::s_isErrorLogging = false;


CPluginClientBase::CPluginClientBase()
{
}


CPluginClientBase::~CPluginClientBase()
{
}


bool CPluginClientBase::IsValidDomain(const CString& domain)
{
  return domain != ABPDOMAIN &&
    domain != USERS_HOST &&
    domain != L"about:blank" &&
    domain != L"about:tabs" &&
    domain.Find(L"javascript:") != 0 &&
    !domain.IsEmpty();
}


CString CPluginClientBase::ExtractDomain(const CString& url)
{
  int pos = 0;
  CString http = url.Find('/',pos) >= 0 ? url.Tokenize(L"/", pos) : L"";
  CString domain = url.Tokenize(L"/", pos);

  domain.Replace(L"www.", L"");
  domain.Replace(L"www1.", L"");
  domain.Replace(L"www2.", L"");

  domain.MakeLower();

  return domain;
}


CString& CPluginClientBase::UnescapeUrl(CString& url)
{
  CString unescapedUrl;
  DWORD cb = 2048;

  if (SUCCEEDED(::UrlUnescape(url.GetBuffer(), unescapedUrl.GetBufferSetLength(cb), &cb, 0)))
  {
    unescapedUrl.ReleaseBuffer();
    unescapedUrl.Truncate(cb);

    url.ReleaseBuffer();
    url = unescapedUrl;
  }

  return url;
}


void CPluginClientBase::SetLocalization()
{
  CPluginSystem* system = CPluginSystem::GetInstance();
  CString browserLanguage = system->GetBrowserLanguage();

  CPluginDictionary* dic = CPluginDictionary::GetInstance();
  dic->SetLanguage(browserLanguage);

  CPluginSettings* settings = CPluginSettings::GetInstance();
  if (settings->IsMainProcess() && settings->IsMainThread() && !settings->Has(SETTING_LANGUAGE))
  {
    // TODO: We might want to set this to "en" if browserLanguage is not in filterLanguagesList
    settings->SetString(SETTING_LANGUAGE, browserLanguage);
    settings->Write();
  }
}


void CPluginClientBase::LogPluginError(DWORD errorCode, int errorId, int errorSubid, const CString& description, bool isAsync, DWORD dwProcessId, DWORD dwThreadId)
{
  // Prevent circular references
  if (CPluginSettings::HasInstance() && isAsync)
  {
    DEBUG_ERROR_CODE_EX(errorCode, description, dwProcessId, dwThreadId);

    CString pluginError;
    pluginError.Format(L"%2.2d%2.2d", errorId, errorSubid);

    CString pluginErrorCode;
    pluginErrorCode.Format(L"%u", errorCode);

    CPluginSettings* settings = CPluginSettings::GetInstance();

    settings->AddError(pluginError, pluginErrorCode);
  }

  // Post error to client for later submittal
  if (!isAsync)
  {
    CPluginClientBase::PostPluginError(errorId, errorSubid, errorCode, description);
  }
}


void CPluginClientBase::PostPluginError(int errorId, int errorSubid, DWORD errorCode, const CString& errorDescription)
{
  s_criticalSectionLocal.Lock();
  {
    CPluginError pluginError(errorId, errorSubid, errorCode, errorDescription);

    s_pluginErrors.push_back(pluginError);
  }
  s_criticalSectionLocal.Unlock();
}


bool CPluginClientBase::PopFirstPluginError(CPluginError& pluginError)
{
  bool hasError = false;

  s_criticalSectionLocal.Lock();
  {
    std::vector<CPluginError>::iterator it = s_pluginErrors.begin();
    if (it != s_pluginErrors.end())
    {
      pluginError = *it;

      hasError = true;

      s_pluginErrors.erase(it);
    }
  }
  s_criticalSectionLocal.Unlock();

  return hasError;
}

// ============================================================================
// Whitelisting
// ============================================================================

#ifdef SUPPORT_WHITELIST

void CPluginClientBase::CacheWhiteListedUrl(const CString& url, bool isWhitelisted)
{
  m_criticalSectionWhitelist.Lock();
  {
    m_cacheWhitelistedUrls[url] = isWhitelisted;
  }
  m_criticalSectionWhitelist.Unlock();
}

bool CPluginClientBase::IsUrlWhiteListed(const CString& url)
{
  if (url.IsEmpty())
  {
    return false;
  }

  bool isWhitelisted = false;
  bool isCached = false;

  m_criticalSectionWhitelist.Lock();
  {
    std::map<CString,bool>::iterator it = m_cacheWhitelistedUrls.find(url);

    isCached = it != m_cacheWhitelistedUrls.end();
    if (isCached)
    {
      isWhitelisted = it->second;
    }
  }
  m_criticalSectionWhitelist.Unlock();

  if (!isCached)
  {
    int pos = 0;
    CString http = url.Find('/',pos) >= 0 ? url.Tokenize(L"/", pos) : L"";
    CString domain = ExtractDomain(url);
    if (http == L"res:" || http == L"file:")
    {
      isWhitelisted = true;
    }
    else
    {
      isWhitelisted = CPluginSettings::GetInstance()->IsWhiteListedDomain(domain);
    }

    CacheWhiteListedUrl(url, isWhitelisted);
  }

  return isWhitelisted;
}

void CPluginClientBase::ClearWhiteListCache()
{
  m_criticalSectionWhitelist.Lock();
  {
    m_cacheWhitelistedUrls.clear();
  }
  m_criticalSectionWhitelist.Unlock();
}


#endif // SUPPORT_WHITELIST