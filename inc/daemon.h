/*
 * daemon.h - decides when to sync and applies the result to the clock.
 */
#ifndef DAEMON_H
#define DAEMON_H

#include <e32base.h>
#include <e32property.h>
#include <f32file.h>
#include <es_sock.h>

#include "syncsession.h"
#include "config.h"
#include "ntpshared.h"
#include "propwatcher.h"

class CLogger;
class CEnvironmentChangeNotifier;

/** Fires a callback after any number of seconds, in chunks short enough for RTimer. */
class CChunkTimer : public CTimer
	{
public:
	static CChunkTimer* NewL(const TCallBack& aCallBack);
	void StartSeconds(TInt aSeconds);
	/** Upper bound on the seconds left, or KMaxTInt when idle. */
	TInt SecondsLeft() const;
private:
	explicit CChunkTimer(const TCallBack& aCallBack);
	void RunL();
	void ArmChunk();
private:
	TCallBack iCallBack;
	TInt iSecondsLeft;
	TInt iChunk;
	};

class CNtpDaemon : public CBase, public MSyncObserver
	{
public:
	static CNtpDaemon* NewLC();
	~CNtpDaemon();
	/** Returns EFalse if the daemon should exit at once (enabled=0). */
	TBool Start();

	// From MSyncObserver
	void SyncFinished(TInt aError, const TNtpSample& aSample, const TDesC& aServer);
	void AccessPointPicked(TUint32 aId, const TDesC& aName);

private:
	CNtpDaemon();
	void ConstructL();
	void LoadSettings();
	void BeginSync(const TDesC& aReason, TBool aInteractive, TBool aNotes);
	void ApplySample(const TNtpSample& aSample, const TDesC& aServer);
	void PublishStatus();
	void StopDaemon(const TDesC& aWhy);
	void HandleCommand(TInt aCommand);
	void ScheduleIn(TInt aSeconds);
	void ScheduleNoLaterThan(TInt aSeconds);
	TInt SecondsSince(TUint32 aNanoTicks) const;
	void HandleEnvironmentChange(TInt aChanges);
	void Rebase();
	TInt64 ClockVsTickUs() const;
	void ClockMoved(TInt64 aJumpUs, TBool aNotified);
	void CheckUtcOffset();
	void JumpCheck();

	static TInt JumpCheckCallBack(TAny* aSelf);
	static TInt TimerCallBack(TAny* aSelf);
	static TInt SyncNowCallBack(TAny* aSelf);
	static TInt CommandCallBack(TAny* aSelf);
	static TInt EnvironmentCallBack(TAny* aSelf);

private:
	RFs iFs;
	RSocketServ iSocketServ;
	CLogger* iLog;
	CNtpConfig* iConfig;
	RArray<TWlanIap> iWlanIaps;
	CSyncSession* iSession;
	CChunkTimer* iTimer;
	CChunkTimer* iJumpCheck;
	CPropertyWatcher* iSyncNow;
	CPropertyWatcher* iCommands;
	CEnvironmentChangeNotifier* iEnvironment;

	TInt iNanoTickUs;
	TInt iFailures;
	TBool iEnvironmentPrimed;
	TInt64 iRefUtc;
	TUint32 iRefTick;
	TBool iHaveSet;
	TUint32 iLastSetTick;
	TInt iUtcOffsetSec;
	TBool iExternalResyncDone;
	TUint32 iExternalResyncTick;
	TBool iRecheckAfterSync;
	TBool iFirstSyncByHand;
	TBool iFirstSyncFromApp;
	TBool iNotes;
	TNtpStatus iShared;
	};

#endif // DAEMON_H
