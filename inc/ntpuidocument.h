/*
 * ntpuidocument.h - the NTP Time Sync document: only makes the AppUi.
 */
#ifndef NTPUIDOCUMENT_H
#define NTPUIDOCUMENT_H

#include <akndoc.h>

class CNtpUiDocument : public CAknDocument
	{
public:
	static CNtpUiDocument* NewL(CEikApplication& aApp);
	CEikAppUi* CreateAppUiL();
private:
	explicit CNtpUiDocument(CEikApplication& aApp);
	};

#endif // NTPUIDOCUMENT_H
