/*
 * main.cpp - ntpsyncd entry point.
 *
 * First copy: becomes the background daemon.
 * Any later launch (menu icon, file manager): asks the running daemon to
 * sync now, then exits.
 */
#include <e32base.h>
#include <e32property.h>

#include "daemon.h"
#include "ntpsyncd.hrh"

_LIT(KSingleInstance, "ntpsyncd.single");

LOCAL_C void MainL()
	{
	RMutex single;
	const TInt err = single.CreateGlobal(KSingleInstance);
	if (err == KErrAlreadyExists)
		{
		RProperty::Set(TUid::Uid(KNtpSyncdUid3), KNtpSyncdSyncNowKey,
			static_cast<TInt>(User::NTickCount()));
		return;
		}
	User::LeaveIfError(err);
	CleanupClosePushL(single);

	CActiveScheduler* scheduler = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(scheduler);
	CActiveScheduler::Install(scheduler);

	CNtpDaemon* daemon = CNtpDaemon::NewLC();
	if (daemon->Start())
		{
		CActiveScheduler::Start(); // runs until the process is killed or told to stop
		}

	CleanupStack::PopAndDestroy(3, &single); // daemon, scheduler, single
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		{
		return KErrNoMemory;
		}
	TRAPD(err, MainL());
	delete cleanup;
	return err;
	}
