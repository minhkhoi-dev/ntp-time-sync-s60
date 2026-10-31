/*
 * ntpuidocument.cpp - the NTP Time Sync document: only makes the AppUi.
 */
#include "ntpuidocument.h"
#include "ntpuiappui.h"

CNtpUiDocument* CNtpUiDocument::NewL(CEikApplication& aApp)
	{
	return new (ELeave) CNtpUiDocument(aApp);
	}

CNtpUiDocument::CNtpUiDocument(CEikApplication& aApp)
	: CAknDocument(aApp)
	{
	}

CEikAppUi* CNtpUiDocument::CreateAppUiL()
	{
	return new (ELeave) CNtpUiAppUi;
	}
