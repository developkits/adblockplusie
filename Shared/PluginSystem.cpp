#include "PluginStdAfx.h"

// Internet / FTP
#include <wininet.h>

// IP adapter
#include <iphlpapi.h>

#include "PluginSystem.h"
#include "PluginClient.h"
#include "PluginSha1.h"
#include "PluginSettings.h"


// IP adapter
#pragma comment(lib, "IPHLPAPI.lib")

// IE functions
#pragma comment(lib, "iepmapi.lib")

// Internet / FTP
#pragma comment(lib, "wininet.lib")

CPluginSystem* CPluginSystem::s_instance = NULL;
CComAutoCriticalSection CPluginSystem::s_criticalSection;

CPluginSystem::CPluginSystem()
{
	s_instance = NULL;
}


CPluginSystem::~CPluginSystem()
{
	s_instance = NULL;
}



CPluginSystem* CPluginSystem::GetInstance()
{
    CPluginSystem* system;

    s_criticalSection.Lock();
    {
	    if (!s_instance)
	    {
		    // We cannot copy the client directly into the instance variable
		    // If the constructor throws we do not want to alter instance
		    CPluginSystem* systemInstance = new CPluginSystem();

		    s_instance = systemInstance;
	    }
	    
	    system = s_instance;
    }
    s_criticalSection.Unlock();

	return system;
}

CString CPluginSystem::GetBrowserLanguage() const
{
    CString browserLanguage;

	LANGID lcid = ::GetUserDefaultLangID();
	TCHAR language[128];
	memset(language, 0, sizeof(language));

	int res = ::GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, language, 127);
	if (res == 0)
	{
		DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_BROWSER_LANGUAGE, "System::GetBrowserLang - Failed");
	}
	else
	{
		browserLanguage = language;
	}

	return browserLanguage;
}


CString CPluginSystem::GetBrowserVersion() const
{
	CString browserVersion;

	HKEY hKey;
	DWORD res;

	// Open the handler
	if ((res = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Internet Explorer", 0, KEY_QUERY_VALUE, &hKey)) == ERROR_SUCCESS)
	{
		TCHAR buf[255];
		DWORD dwBufSize = sizeof(buf);
		DWORD dwType = REG_SZ;

		// Do the processing, find the version
		if ((res = ::RegQueryValueEx(hKey, L"Version", 0, &dwType, (BYTE*)buf, &dwBufSize)) == ERROR_SUCCESS)
		{
			browserVersion = buf;
			int pos = 0;
			if ((pos = browserVersion.Find('.')) >= 0)
			{
				browserVersion = browserVersion.Left(pos);
			}
		}
		else
		{
			DEBUG_ERROR_LOG(res, PLUGIN_ERROR_OS_VERSION, PLUGIN_ERROR_OS_VERSION_REG_QUERY_VALUE, L"Client::GetBrowserVer - Failed reg query value");
		}

		// Close the handler
		::RegCloseKey(hKey);
	}
	else
	{
		DEBUG_ERROR_LOG(res, PLUGIN_ERROR_OS_VERSION, PLUGIN_ERROR_OS_VERSION_REG_OPEN_KEY, L"Client::GetBrowserVer - Failed reg open");
	}

	return browserVersion;
}


CString CPluginSystem::GetUserName() const
{
    CString userName;
        
    TCHAR name[UNLEN + 1];
    DWORD length = UNLEN + 1;
    
    int res = ::GetUserName(name, &length);
    if (res != 0)
    {
        userName = name;
    }
    else
    {
	    DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_USER_NAME, L"Client::GetUserName - Failed");
    }

    return userName;
}


CString CPluginSystem::GetComputerName() const
{
    CString computerName;
        
    TCHAR name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
    
    if (::GetComputerName(name, &length))
    {
        computerName = name;
    }
    else
    {
        DWORD dwError = ::GetLastError();

	    DEBUG_ERROR_LOG(dwError, PLUGIN_ERROR_SYSINFO, PLUGIN_ERROR_SYSINFO_COMPUTER_NAME, L"Client::GetComputerName - Failed");

        computerName.Format(L"err %u", dwError);
    }
    
    return computerName;
}


CString CPluginSystem::GetPluginId()
{
    CString pluginId;
    
    s_criticalSection.Lock();
    {
#ifdef CONFIG_IN_REGISTRY
			DWORD dwResult = NULL; 
			HKEY hKey;
			RegOpenKey(HKEY_CURRENT_USER, L"SOFTWARE\\SimpleAdblock", &hKey);
			DWORD type = 0;
			WCHAR pid[250];
			DWORD cbData;
			dwResult = ::RegQueryValueEx(hKey, L"PluginId", NULL, &type, (BYTE*)pid, &cbData);
			if (dwResult == ERROR_SUCCESS)
			{
				CString pluginId = pid;
				::RegCloseKey(hKey);
				m_pluginId = pluginId;
			}
#endif
	    if (m_pluginId.IsEmpty())
	    { 
#ifdef CONFIG_IN_REGISTRY
			RegOpenKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\SimpleAdblock", &hKey);
			type = 0;
			WCHAR pid[250];
			DWORD cbData;
			dwResult = ::RegQueryValueEx(hKey, L"PluginId", NULL, &type, (BYTE*)pid, &cbData);
			if (dwResult == ERROR_SUCCESS)
			{
				CString pluginId = pid;
				::RegCloseKey(hKey);
				m_pluginId = pluginId;
			}
#endif

		    if (m_pluginId.IsEmpty())
			{ 

				CPluginSettings* settings = CPluginSettings::GetInstance();
				m_pluginId = settings->GetString(SETTING_PLUGIN_ID, L"");
				if (m_pluginId.IsEmpty())
				{
					m_pluginId = GeneratePluginId();
				}
			}
	    }

        pluginId = m_pluginId;
    }    
    s_criticalSection.Unlock();

	return pluginId;
}

void CPluginSystem::SetPluginId(CString pluginId)
{
	m_pluginId = pluginId;
#ifdef CONFIG_IN_REGISTRY
			DWORD dwResult = NULL; 
			HKEY hKey;
			RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\SimpleAdblock", &hKey);
			DWORD type = 0;
//			MessageBox(NULL, pluginId, L"testdemo", MB_OK);
			dwResult = ::RegSetValueEx(hKey, L"PluginId", NULL, REG_SZ, (const BYTE*)pluginId.GetBuffer(), (pluginId.GetLength() * 2) + 1);

			RegCreateKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\SimpleAdblock", &hKey);
			type = 0;
//			MessageBox(NULL, pluginId, L"testdemo", MB_OK);
			dwResult = ::RegSetValueEx(hKey, L"PluginId", NULL, REG_SZ, (const BYTE*)pluginId.GetBuffer(), (pluginId.GetLength() * 2) + 1);

#endif

}


#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) ::HeapAlloc(::GetProcessHeap(), 0, (x))
#define FREE(x) ::HeapFree(::GetProcessHeap(), 0, (x))

CString CPluginSystem::GetMacId(bool addSeparator) const
{
    CString id;

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
    IP_ADAPTER_PREFIX *pPrefix = NULL;

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do 
    {
        pAddresses = (IP_ADAPTER_ADDRESSES *) MALLOC(outBufLen);
        if (pAddresses) 
        {
			dwRetVal = ::GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
			if (dwRetVal == ERROR_BUFFER_OVERFLOW) 
			{
				FREE(pAddresses);
				pAddresses = NULL;
			} 
			else 
			{
				break;
			}
		}

        Iterations++;

	} while (dwRetVal == ERROR_BUFFER_OVERFLOW && Iterations < MAX_TRIES);

    if (dwRetVal == NO_ERROR) 
    {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;

		int macCount = 0;

        while (pCurrAddresses && macCount < 2) 
        {
            if (pCurrAddresses->PhysicalAddressLength != 0) 
            {
                CString buffer;
                
                bool isValid = false;
                
                for (i = 0; i < (int) pCurrAddresses->PhysicalAddressLength; i++) 
                {
                    if (!buffer.IsEmpty())
                    {
                        buffer += ':';
                    }
                    
                    int value = pCurrAddresses->PhysicalAddress[i];
                    if (value != 0)
                    {
                        isValid = true;
                    }

                    CString part;
                    part.Format(L"%2.2x", value);
                    
                    buffer += part;
                }
                
                if (isValid)
                {

                    if (!id.IsEmpty() && addSeparator)
                    {
		                id += '-';
                    }
                    id += buffer;
                    
                    macCount++;
                }
            }

            pCurrAddresses = pCurrAddresses->Next;
        }
    } 
    else
    {
		DEBUG_ERROR_LOG(dwRetVal, PLUGIN_ERROR_MAC_ID, PLUGIN_ERROR_MAC_ID_RETRIEVAL_EX, L"Client::GetMacId - Failed GetAdaptersAddresses");
    }

    if (pAddresses) 
    {
        FREE(pAddresses);
    }

    return id;
}


CString CPluginSystem::GeneratePluginId()
{
	CString id = GetMacId() + GetComputerName();
    
	// Generate SHA1 encryption
	TCHAR szReport[100];

	CStringA idA = id;

	CSHA1 sha1;
	sha1.Update((unsigned char*)idA.GetBuffer(), idA.GetLength());
	sha1.Final();

	sha1.ReportHash(szReport, CSHA1::REPORT_HEX_SHORT);
    
	id = szReport;
	id.MakeLower();
	
    return id;
}
