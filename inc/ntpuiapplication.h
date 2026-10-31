/*
 * ntpuiapplication.h - the NTP Time Sync application object.
 */
#ifndef NTPUIAPPLICATION_H
#define NTPUIAPPLICATION_H

#include <aknapp.h>

#include "ntpsyncd.hrh"

const TUid KUidNtpSyncUiApp = { KNtpSyncUiUid3 };

class CNtpUiApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const;
protected:
	CApaDocument* CreateDocumentL();
	};

#endif // NTPUIAPPLICATION_H
