/*
 * ntpsyncui.cpp - NTP Time Sync app entry point.
 */
#include <eikstart.h>

#include "ntpuiapplication.h"

LOCAL_C CApaApplication* NewApplication()
	{
	return new CNtpUiApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
