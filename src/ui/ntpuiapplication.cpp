/*
 * ntpuiapplication.cpp - the NTP Time Sync application object.
 */
#include "ntpuiapplication.h"
#include "ntpuidocument.h"

TUid CNtpUiApplication::AppDllUid() const
	{
	return KUidNtpSyncUiApp;
	}

CApaDocument* CNtpUiApplication::CreateDocumentL()
	{
	return CNtpUiDocument::NewL(*this);
	}
