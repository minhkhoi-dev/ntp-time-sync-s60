/*
 * daemon.cpp - decides when to sync and applies the result to the clock.
 *
 * Schedule:
 *   - first sync 60 s after boot (3 s when started by hand)
 *   - then every `interval` minutes
 *   - after a failure: every 10 min if no iap= is set (no radio cost, it only
 *     looks for Wi-Fi that is already up), otherwise 10, 20, 40... min
 *   - if something else moves the clock: re-sync 30 s later, at most once
 *     per 15 min, so we never fight network operator time in a loop
 *   - every 5 min it compares the clock with the tick counter and logs jumps
 */
#include "daemon.h"
#include "logger.h"
#include "ntpsyncd.hrh"

#include <aknglobalnote.h>
#include <bacntf.h>
#include <hal.h>

namespace
	{
	_LIT(KVersion, "2.0.3");
	const TInt KBootDelaySec = 60;
	const TInt KManualStartDelaySec = 3;
	const TInt KBootWindowSec = 300;
	const TInt KNoWifiPollSec = 10 * 60;
	const TInt KRetryBaseSec = 10 * 60;
	const TInt KJumpCheckSec = 5 * 60;
	const TInt64 KJumpMinUs = 200000;
	const TInt64 KSetCheckUs = 50000;
	const TInt KExternalResyncDelaySec = 30;
	const TInt KExternalResyncGapSec = 15 * 60;
	const TInt KMaxChunkSec = 30 * 60; // RTimer::After tops out near 35 min

	_LIT_SECURITY_POLICY_PASS(KReadPolicy);
	_LIT_SECURITY_POLICY_S0(KWritePolicy, KNtpSyncdUid3);
	_LIT_SECURITY_POLICY_S0(KAppWritePolicy, KNtpSyncUiUid3);

	TInt64 UtcNow()
		{
		TTime now;
		now.UniversalTime();
		return now.Int64();
		}

	TInt64 AbsUs(TInt64 aUs)
		{
		return aUs < 0 ? -aUs : aUs;
		}

	// Defines one of our keys; it is fine if it is already there.
	void DefineKeyL(TUint aKey, TInt aType, const TSecurityPolicy& aWrite, TInt aSize)
		{
		const TInt err = RProperty::Define(KNtpSyncdCategory, aKey, aType,
			KReadPolicy, aWrite, aSize);
		if (err != KErrNone && err != KErrAlreadyExists)
			{
			User::Leave(err);
			}
		}

	void FormatSeconds(TDes& aOut, TInt64 aUs)
		{
		const TUint sign = aUs < 0 ? '-' : '+';
		const TInt64 a = aUs < 0 ? -aUs : aUs;
		const TInt64 whole = a / 1000000;
		const TInt ms = static_cast<TInt>((a % 1000000) / 1000);
		aOut.Format(_L("%c%Ld.%03d s"), sign, whole, ms);
		}

	// Feedback for syncs the user started. Never leaves; a missing notifier is ignored.
	void ShowNoteL(TAknGlobalNoteType aType, const TDesC& aText)
		{
		CAknGlobalNote* note = CAknGlobalNote::NewLC();
		note->ShowNoteL(aType, aText);
		CleanupStack::PopAndDestroy(note);
		}

	void ShowNote(TAknGlobalNoteType aType, const TDesC& aText)
		{
		TRAP_IGNORE(ShowNoteL(aType, aText));
		}
	}

// ---------------------------------------------------------------------------
// CChunkTimer

CChunkTimer* CChunkTimer::NewL(const TCallBack& aCallBack)
	{
	CChunkTimer* self = new (ELeave) CChunkTimer(aCallBack);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CChunkTimer::CChunkTimer(const TCallBack& aCallBack)
	: CTimer(EPriorityStandard), iCallBack(aCallBack)
	{
	CActiveScheduler::Add(this);
	}

void CChunkTimer::StartSeconds(TInt aSeconds)
	{
	Cancel();
	iSecondsLeft = Max(aSeconds, 1);
	ArmChunk();
	}

TInt CChunkTimer::SecondsLeft() const
	{
	return IsActive() ? iSecondsLeft : KMaxTInt;
	}

void CChunkTimer::ArmChunk()
	{
	// Relative timers ignore clock changes, which is what we want here.
	iChunk = Min(iSecondsLeft, KMaxChunkSec);
	After(TTimeIntervalMicroSeconds32(iChunk * 1000000));
	}

void CChunkTimer::RunL()
	{
	iSecondsLeft -= iChunk;
	if (iStatus.Int() == KErrNone && iSecondsLeft > 0)
		{
		ArmChunk();
		return;
		}
	iSecondsLeft = 0;
	iCallBack.CallBack();
	}

// ---------------------------------------------------------------------------
// CNtpDaemon

CNtpDaemon* CNtpDaemon::NewLC()
	{
	CNtpDaemon* self = new (ELeave) CNtpDaemon;
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}

CNtpDaemon::CNtpDaemon()
	{
	}

void CNtpDaemon::ConstructL()
	{
	RProcess().SetPriority(EPriorityBackground);

	User::LeaveIfError(iFs.Connect());
	User::LeaveIfError(iSocketServ.Connect());
	iLog = CLogger::NewL(iFs);
	iConfig = CNtpConfig::NewL(iFs, *iLog);
	iSession = CSyncSession::NewL(iSocketServ, *iLog, *this);
	iTimer = CChunkTimer::NewL(TCallBack(TimerCallBack, this));
	iJumpCheck = CChunkTimer::NewL(TCallBack(JumpCheckCallBack, this));

	// Category is our own SID. Only another copy of ntpsyncd may ask for a sync
	// or write the status; only the NTP Time Sync app may send commands.
	DefineKeyL(KNtpSyncdSyncNowKey, RProperty::EInt, KWritePolicy, 0);
	DefineKeyL(KNtpSyncdStatusKey, RProperty::EByteArray, KWritePolicy, sizeof(TNtpStatus));
	DefineKeyL(KNtpSyncdCommandKey, RProperty::EInt, KAppWritePolicy, 0);

	Mem::FillZ(&iShared, sizeof(iShared));
	iShared.iLayout = KNtpStatusLayout;
	iShared.iState = ENtpStateIdle;
	iShared.iLastError = KErrNone;

	iSyncNow = CPropertyWatcher::NewL(KNtpSyncdCategory, KNtpSyncdSyncNowKey, TCallBack(SyncNowCallBack, this));
	iCommands = CPropertyWatcher::NewL(KNtpSyncdCategory, KNtpSyncdCommandKey, TCallBack(CommandCallBack, this));
	iEnvironment = CEnvironmentChangeNotifier::NewL(CActive::EPriorityStandard,
		TCallBack(EnvironmentCallBack, this));

	if (HAL::Get(HALData::ENanoTickPeriod, iNanoTickUs) != KErrNone || iNanoTickUs <= 0)
		{
		iNanoTickUs = 1000;
		}
	}

CNtpDaemon::~CNtpDaemon()
	{
	delete iEnvironment;
	delete iCommands;
	delete iSyncNow;
	delete iJumpCheck;
	delete iTimer;
	delete iSession;
	delete iConfig;
	iWlanIaps.Close();
	delete iLog;
	iSocketServ.Close();
	iFs.Close();
	}

TBool CNtpDaemon::Start()
	{
	_LIT(KAtBoot, "at boot");
	_LIT(KByHand, "by hand");
	_LIT(KFromApp, "from the app");
	_LIT(KAppArg, "app");
	iEnvironment->Start();
	TBuf<16> args;
	User::CommandLine(args);
	// Started from the app counts as by hand, even right after boot.
	iFirstSyncFromApp = args.CompareF(KAppArg) == 0;
	const TBool atBoot = !iFirstSyncFromApp && SecondsSince(0) < KBootWindowSec;
	const TPtrC how(atBoot ? KAtBoot() : (iFirstSyncFromApp ? KFromApp() : KByHand()));
	iLog->Log(_L("ntpsyncd %S started %S"), &KVersion, &how);

	// Write the default settings file now so it is there to edit. This load is
	// only for that; iap= is checked at each sync (no duplicate "ignored" lines here).
	TRAP_IGNORE(ListWlanIapsL(iWlanIaps));
	TRAP_IGNORE(iConfig->LoadL(iWlanIaps, EFalse));

	if (!iConfig->Enabled())
		{
		iLog->Log(_L("Background sync is off (enabled=0 in the settings), exiting"));
		iShared.iState = ENtpStateStopped;
		iShared.iNextSyncUtc = 0;
		PublishStatus();
		return EFalse;
		}

	iFirstSyncByHand = !atBoot;
	iUtcOffsetSec = User::UTCOffset().Int();
	Rebase();
	iJumpCheck->StartSeconds(KJumpCheckSec);
	ScheduleIn(atBoot ? KBootDelaySec : KManualStartDelaySec);
	return ETrue;
	}

void CNtpDaemon::PublishStatus()
	{
	RProperty::Set(KNtpSyncdCategory, KNtpSyncdStatusKey, TPckgC<TNtpStatus>(iShared));
	}

void CNtpDaemon::StopDaemon(const TDesC& aWhy)
	{
	iLog->Log(_L("Stopped (%S)"), &aWhy);
	iTimer->Cancel();
	iJumpCheck->Cancel();
	if (iSession->IsBusy())
		{
		iSession->Cancel();
		}
	iShared.iState = ENtpStateStopped;
	iShared.iNextSyncUtc = 0;
	PublishStatus();
	CActiveScheduler::Stop();
	}

void CNtpDaemon::LoadSettings()
	{
	TRAPD(err, ListWlanIapsL(iWlanIaps));
	if (err != KErrNone)
		{
		iLog->Log(_L("Could not read access points (%d)"), err);
		}
	const TBool haveWlanList = (err == KErrNone);
	TRAP(err, iConfig->LoadL(iWlanIaps, haveWlanList));
	if (err != KErrNone)
		{
		iLog->Log(_L("Could not read settings (%d)"), err);
		}
	}

TInt CNtpDaemon::SecondsSince(TUint32 aNanoTicks) const
	{
	const TUint32 elapsed = User::NTickCount() - aNanoTicks; // wraps safely
	return static_cast<TInt>((TInt64(elapsed) * iNanoTickUs) / 1000000);
	}

void CNtpDaemon::BeginSync(const TDesC& aReason, TBool aInteractive, TBool aNotes)
	{
	if (iSession->IsBusy())
		{
		return;
		}
	iTimer->Cancel();
	iFirstSyncByHand = EFalse;
	iNotes = aNotes;

	LoadSettings();
	if (!iConfig->Enabled())
		{
		StopDaemon(_L("enabled=0 in the settings"));
		return;
		}

	iLog->Log(_L("Sync (%S)"), &aReason);
	// With no iap= saved the system picker is about to show; no note on top of it.
	if (iNotes && iConfig->Iaps().Count() > 0)
		{
		ShowNote(EAknGlobalInformationNote, _L("ntpsyncd: syncing the clock"));
		}
	iShared.iState = ENtpStateSyncing;
	iShared.iLastAttemptUtc = UtcNow();
	iShared.iNextSyncUtc = 0;
	PublishStatus();
	iSession->Start(*iConfig, iWlanIaps, aInteractive);
	}

void CNtpDaemon::AccessPointPicked(TUint32 aId, const TDesC& aName)
	{
	if (!IsWlanIap(iWlanIaps, aId))
		{
		return;
		}
	if (IsEasyWlanIap(aId, aName))
		{
		iLog->Log(_L("Easy WLAN was picked: used for this sync, not saved"));
		return;
		}
	TRAPD(err, iConfig->SaveIapL(aId, aName));
	if (err == KErrNone)
		{
		iLog->Log(_L("Saved access point %u to C:\\Data\\ntpsyncd.ini"), aId);
		}
	else
		{
		iLog->Log(_L("Could not save access point %u to C:\\Data\\ntpsyncd.ini (%d)"), aId, err);
		}
	}

void CNtpDaemon::SyncFinished(TInt aError, const TNtpSample& aSample, const TDesC& aServer)
	{
	const TBool recheck = iRecheckAfterSync;
	iRecheckAfterSync = EFalse;
	iShared.iState = ENtpStateIdle;
	iShared.iLastError = aError;
	iShared.iNoWifi = (aError != KErrNone && iSession->NoWifi());
	if (aError == KErrNone)
		{
		iFailures = 0;
		ApplySample(aSample, aServer);
		iShared.iLastSuccessUtc = UtcNow();
		iShared.iOffsetUs = aSample.iOffsetUs;
		iShared.iStratum = aSample.iStratum;
		iShared.iDelayMs = static_cast<TInt>(aSample.iDelayUs / 1000);
		iShared.SetServer(iSession->BestServerName());
		iShared.iIapId = iSession->UsedIapId();
		iShared.SetIapName(KNullDesC);
		for (TInt i = 0; i < iWlanIaps.Count(); ++i)
			{
			if (iWlanIaps[i].iId == iShared.iIapId)
				{
				iShared.SetIapName(iWlanIaps[i].iName);
				break;
				}
			}
		// If the clock moved while we were measuring, the sample may be off.
		ScheduleIn(recheck ? KExternalResyncDelaySec : iConfig->IntervalSeconds());
		return;
		}

	++iFailures;
	TInt delay = KNoWifiPollSec;
	if (iConfig->Iaps().Count() > 0)
		{
		delay = KRetryBaseSec << Min(iFailures - 1, 8);
		delay = Min(delay, iConfig->IntervalSeconds());
		}
	iLog->Log(_L("Sync failed (%d)"), aError);
	if (iNotes)
		{
		TBuf<100> text;
		if (iSession->UsedPicker() && aError == KErrCancel)
			{
			text.Copy(_L("No Wi-Fi access point picked"));
			}
		else if (iSession->NoWifi())
			{
			text.Copy(_L("No Wi-Fi connection"));
			}
		else
			{
			text.Format(_L("Sync failed (%d). See C:\\Data\\ntpsyncd.log"), aError);
			}
		ShowNote(EAknGlobalErrorNote, text);
		}
	ScheduleIn(delay);
	}

void CNtpDaemon::ApplySample(const TNtpSample& aSample, const TDesC& aServer)
	{
	TBuf<40> offset;
	FormatSeconds(offset, aSample.iOffsetUs);
	const TInt delayMs = static_cast<TInt>(aSample.iDelayUs / 1000);
	const TInt64 absOffset = aSample.iOffsetUs < 0 ? -aSample.iOffsetUs : aSample.iOffsetUs;

	if (absOffset < iConfig->ThresholdUs())
		{
		iShared.iClockResult = ENtpClockRight;
		iShared.iSetError = KErrNone;
		iLog->Log(_L("Clock is right (%S) - %S, stratum %d, %d ms round trip"),
			&offset, &aServer, aSample.iStratum, delayMs);
		if (iNotes)
			{
			TBuf<80> text;
			text.Format(_L("Clock is right (%S)"), &offset);
			ShowNote(EAknGlobalConfirmationNote, text);
			}
		return;
		}

	TTime target;
	target.UniversalTime();
	target += TTimeIntervalMicroSeconds(aSample.iOffsetUs);

	const TInt err = User::SetUTCTime(target);
	iShared.iClockResult = (err == KErrNone) ? ENtpClockCorrected : ENtpClockSetFailed;
	iShared.iSetError = err;
	if (err == KErrNone)
		{
		const TInt64 off = UtcNow() - target.Int64();
		if (AbsUs(off) > KSetCheckUs)
			{
			TBuf<40> diff;
			FormatSeconds(diff, off);
			iLog->Log(_L("Clock reads %S off right after it was set"), &diff);
			}
		Rebase();
		iLastSetTick = User::NTickCount();
		iHaveSet = ETrue;
		iLog->Log(_L("Clock corrected by %S - %S, stratum %d, %d ms round trip"),
			&offset, &aServer, aSample.iStratum, delayMs);
		if (iNotes)
			{
			TBuf<80> text;
			text.Format(_L("Clock corrected by %S"), &offset);
			ShowNote(EAknGlobalConfirmationNote, text);
			}
		}
	else if (err == KErrPermissionDenied)
		{
		iLog->Log(_L("Could not set the clock (%d): WriteDeviceData capability missing"), err);
		}
	else
		{
		iLog->Log(_L("Could not set the clock (%d)"), err);
		}
	if (err != KErrNone && iNotes)
		{
		TBuf<80> text;
		text.Format(_L("Could not set the clock (%d)"), err);
		ShowNote(EAknGlobalErrorNote, text);
		}
	}

void CNtpDaemon::ScheduleIn(TInt aSeconds)
	{
	iTimer->StartSeconds(aSeconds);
	iShared.iNextSyncUtc = UtcNow() + TInt64(aSeconds) * 1000000;
	PublishStatus();
	TTime when;
	when.HomeTime();
	when += TTimeIntervalSeconds(aSeconds);
	const TDateTime dt = when.DateTime();
	if (aSeconds >= 120)
		{
		iLog->Log(_L("Next sync around %02d:%02d"), dt.Hour(), dt.Minute());
		}
	}

void CNtpDaemon::ScheduleNoLaterThan(TInt aSeconds)
	{
	if (iTimer->SecondsLeft() > aSeconds)
		{
		ScheduleIn(aSeconds);
		}
	}

void CNtpDaemon::HandleEnvironmentChange(TInt aChanges)
	{
	if (!iEnvironmentPrimed)
		{
		iEnvironmentPrimed = ETrue; // the first report lists every change type
		return;
		}
	CheckUtcOffset();
	if (aChanges < 0 || !(aChanges & EChangesSystemTime))
		{
		return;
		}
	// We rebase right after our own correction, so that one reads as no jump.
	const TInt64 jump = ClockVsTickUs();
	if (AbsUs(jump) < KJumpMinUs)
		{
		return;
		}
	ClockMoved(jump, ETrue);
	}

void CNtpDaemon::Rebase()
	{
	iRefUtc = UtcNow();
	iRefTick = User::NTickCount();
	}

TInt64 CNtpDaemon::ClockVsTickUs() const
	{
	const TInt64 now = UtcNow();
	const TUint32 elapsed = User::NTickCount() - iRefTick; // wraps safely
	return now - (iRefUtc + TInt64(elapsed) * iNanoTickUs);
	}

void CNtpDaemon::CheckUtcOffset()
	{
	const TInt now = User::UTCOffset().Int();
	if (now != iUtcOffsetSec)
		{
		iLog->Log(_L("UTC offset changed from %d to %d min"), iUtcOffsetSec / 60, now / 60);
		iUtcOffsetSec = now;
		}
	}

void CNtpDaemon::JumpCheck()
	{
	const TInt64 jump = ClockVsTickUs();
	if (AbsUs(jump) >= KJumpMinUs)
		{
		ClockMoved(jump, EFalse);
		}
	else
		{
		Rebase(); // so slow drift between the two clocks never adds up
		}
	CheckUtcOffset();
	iJumpCheck->StartSeconds(KJumpCheckSec);
	}

void CNtpDaemon::ClockMoved(TInt64 aJumpUs, TBool aNotified)
	{
	TBuf<40> amount;
	FormatSeconds(amount, aJumpUs);
	TBuf<120> line;
	if (aNotified)
		{
		line.Format(_L("Clock moved by %S by something else"), &amount);
		}
	else
		{
		line.Format(_L("Clock moved by %S with no time-change notice"), &amount);
		}
	if (iHaveSet)
		{
		TBuf<40> tail;
		tail.Format(_L(", %d s after ntpsyncd last set it"), SecondsSince(iLastSetTick));
		line.Append(tail);
		}
	iLog->Log(_L("%S"), &line);
	Rebase();

	if (!iConfig->WatchClock())
		{
		return;
		}
	if (iSession->IsBusy())
		{
		iRecheckAfterSync = ETrue;
		return;
		}
	if (iExternalResyncDone)
		{
		const TInt since = SecondsSince(iExternalResyncTick);
		if (since < KExternalResyncGapSec)
			{
			ScheduleNoLaterThan(Max(KExternalResyncGapSec - since, KExternalResyncDelaySec));
			return;
			}
		}
	iExternalResyncDone = ETrue;
	iExternalResyncTick = User::NTickCount();
	ScheduleNoLaterThan(KExternalResyncDelaySec);
	}

TInt CNtpDaemon::JumpCheckCallBack(TAny* aSelf)
	{
	static_cast<CNtpDaemon*>(aSelf)->JumpCheck();
	return 0;
	}

TInt CNtpDaemon::TimerCallBack(TAny* aSelf)
	{
	CNtpDaemon* self = static_cast<CNtpDaemon*>(aSelf);
	if (self->iFirstSyncByHand)
		{
		if (self->iFirstSyncFromApp)
			{
			self->BeginSync(_L("started from the app"), ETrue, EFalse);
			}
		else
			{
			self->BeginSync(_L("started by hand"), ETrue, ETrue);
			}
		}
	else
		{
		self->BeginSync(_L("scheduled"), EFalse, EFalse);
		}
	return 0;
	}

TInt CNtpDaemon::SyncNowCallBack(TAny* aSelf)
	{
	static_cast<CNtpDaemon*>(aSelf)->BeginSync(_L("asked by hand"), ETrue, ETrue);
	return 0;
	}

TInt CNtpDaemon::CommandCallBack(TAny* aSelf)
	{
	CNtpDaemon* self = static_cast<CNtpDaemon*>(aSelf);
	TInt value = 0;
	if (self->iCommands->Property().Get(value) == KErrNone)
		{
		self->HandleCommand(NtpCommandCode(value));
		}
	return 0;
	}

void CNtpDaemon::HandleCommand(TInt aCommand)
	{
	switch (aCommand)
		{
		case ENtpCmdSyncNow:
			BeginSync(_L("asked from the app"), ETrue, EFalse); // the app shows the result
			break;
		case ENtpCmdReload:
			if (iSession->IsBusy())
				{
				break; // the next sync re-reads the settings
				}
			LoadSettings();
			iLog->Log(_L("Settings reloaded (asked from the app)"));
			if (!iConfig->Enabled())
				{
				StopDaemon(_L("enabled=0 in the settings"));
				}
			else
				{
				if (iTimer->SecondsLeft() > iConfig->IntervalSeconds())
					{
					ScheduleIn(iConfig->IntervalSeconds());
					}
				PublishStatus();
				}
			break;
		case ENtpCmdStop:
			StopDaemon(_L("asked from the app"));
			break;
		default:
			break;
		}
	}

TInt CNtpDaemon::EnvironmentCallBack(TAny* aSelf)
	{
	CNtpDaemon* self = static_cast<CNtpDaemon*>(aSelf);
	self->HandleEnvironmentChange(self->iEnvironment->Change());
	return 0;
	}
