/*
 * syncsession.cpp - one NTP sync over Wi-Fi.
 *
 * Connection order:
 *   1. a Wi-Fi connection some other app already has up (attach, no radio cost)
 *   2. each iap= from the settings, started without any prompt
 * On each connection the servers are tried in order. A server is done once
 * it gives 3 good samples or 4 attempts; the shortest round trip wins.
 */
#include "syncsession.h"
#include "logger.h"

#include <es_enum.h>

namespace
	{
	const TInt KConnectTimeoutSec = 45;
	const TInt KPickTimeoutSec = 120; // the user is choosing
	const TInt KResolveTimeoutSec = 20;
	const TInt KSendTimeoutSec = 5;
	const TInt KReplyTimeoutSec = 3;
	const TInt KSamplesWanted = 3;
	const TInt KMaxAttemptsPerServer = 4;
	const TInt KPauseBetweenSamplesUs = 2000000; // be polite to the pool
	}

// ---------------------------------------------------------------------------
// CGuardTimer: cancels whatever the session is waiting for if it takes too long.

class CGuardTimer : public CTimer
	{
public:
	static CGuardTimer* NewL(CSyncSession& aOwner)
		{
		CGuardTimer* self = new (ELeave) CGuardTimer(aOwner);
		CleanupStack::PushL(self);
		self->ConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	void StartSeconds(TInt aSeconds)
		{
		Cancel();
		After(TTimeIntervalMicroSeconds32(aSeconds * 1000000));
		}
private:
	CGuardTimer(CSyncSession& aOwner) : CTimer(EPriorityStandard), iOwner(aOwner)
		{
		CActiveScheduler::Add(this);
		}
	void RunL()
		{
		if (iStatus.Int() == KErrNone)
			{
			iOwner.OnGuardTimeout();
			}
		}
private:
	CSyncSession& iOwner;
	};

// ---------------------------------------------------------------------------

CSyncSession* CSyncSession::NewL(RSocketServ& aSocketServ, CLogger& aLog, MSyncObserver& aObserver)
	{
	CSyncSession* self = new (ELeave) CSyncSession(aSocketServ, aLog, aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CSyncSession::CSyncSession(RSocketServ& aSocketServ, CLogger& aLog, MSyncObserver& aObserver)
	: CActive(EPriorityStandard), iSocketServ(aSocketServ), iLog(aLog), iObserver(aObserver),
	  iState(EIdle)
	{
	CActiveScheduler::Add(this);
	}

void CSyncSession::ConstructL()
	{
	iGuard = CGuardTimer::NewL(*this);
	User::LeaveIfError(iPause.CreateLocal());
	iNtpEpoch = TTime(TDateTime(1900, EJanuary, 0, 0, 0, 0, 0)).Int64();
	}

CSyncSession::~CSyncSession()
	{
	Cancel();
	CloseAll();
	iPause.Close();
	delete iGuard;
	}

TInt64 CSyncSession::NowNtpUs() const
	{
	TTime now;
	now.UniversalTime();
	return now.Int64() - iNtpEpoch;
	}

void CSyncSession::Start(const CNtpConfig& aConfig, const RArray<TWlanIap>& aWlanIaps,
	TBool aInteractive)
	{
	if (iState != EIdle)
		{
		return;
		}
	iConfig = &aConfig;
	iLastError = KErrNotFound;
	iIapIndex = 0;
	iAttached = EFalse;
	iHaveBest = EFalse;
	iBestServer.Zero();
	iBestName.Zero();
	iPickerUsed = EFalse;
	iPicked = EFalse;
	iNoWifi = EFalse;
	iUsedIap = 0;

	if (aInteractive && aConfig.Iaps().Count() == 0)
		{
		StartPicker();
		return;
		}
	if (AttachToActiveWlan(aWlanIaps))
		{
		OnConnected();
		}
	else
		{
		StartNextIap();
		}
	}

TBool CSyncSession::AttachToActiveWlan(const RArray<TWlanIap>& aWlanIaps)
	{
	if (iConnection.Open(iSocketServ) != KErrNone)
		{
		return EFalse;
		}
	TUint count = 0;
	if (iConnection.EnumerateConnections(count) == KErrNone)
		{
		for (TUint i = 1; i <= count; ++i)
			{
			TConnectionInfoBuf info;
			if (iConnection.GetConnectionInfo(i, info) != KErrNone)
				{
				continue;
				}
			const TUint32 iap = info().iIapId;
			if (!IsWlanIap(aWlanIaps, iap))
				{
				continue;
				}
			if (iConnection.Attach(info, RConnection::EAttachTypeNormal) == KErrNone)
				{
				iLog.Log(_L("Using the Wi-Fi connection already up (access point %u)"), iap);
				iAttached = ETrue;
				iUsedIap = iap;
				return ETrue;
				}
			}
		}
	iConnection.Close();
	return EFalse;
	}

void CSyncSession::StartPicker()
	{
	iPickerUsed = ETrue;
	const TInt err = iConnection.Open(iSocketServ);
	if (err != KErrNone)
		{
		iLog.Log(_L("Could not ask for an access point (%d)"), err);
		iLastError = err;
		Finish(err);
		return;
		}
	iPref = TCommDbConnPref();
	iPref.SetDialogPreference(ECommDbDialogPrefPrompt);
	iPref.SetBearerSet(ECommDbBearerWLAN);
	iPref.SetDirection(ECommDbConnectionDirectionOutgoing);
	iLog.Log(_L("Asking which Wi-Fi access point to use"));
	iConnection.Start(iPref, iStatus);
	Wait(EPicking, KPickTimeoutSec);
	}

void CSyncSession::StartNextIap()
	{
	CloseAll();
	const RArray<TUint32>& iaps = iConfig->Iaps();
	if (iIapIndex >= iaps.Count())
		{
		if (iaps.Count() == 0)
			{
			iLog.Log(_L("No Wi-Fi connection is up and no iap= is enabled in the settings"));
			iNoWifi = ETrue;
			}
		Finish(iLastError);
		return;
		}

	const TUint32 iap = iaps[iIapIndex];
	TInt err = iConnection.Open(iSocketServ);
	if (err != KErrNone)
		{
		iLastError = err;
		++iIapIndex;
		StartNextIap();
		return;
		}
	iPref.SetIapId(iap);
	iPref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
	iPref.SetDirection(ECommDbConnectionDirectionOutgoing);
	iLog.Log(_L("Starting Wi-Fi access point %u"), iap);
	iConnection.Start(iPref, iStatus);
	Wait(EConnecting, KConnectTimeoutSec);
	}

void CSyncSession::TryNextConnection()
	{
	if (iPicked)
		{
		Finish(iLastError); // the picked access point is not in the configured list
		return;
		}
	if (iAttached)
		{
		iAttached = EFalse; // fall back to the configured access points
		}
	else
		{
		++iIapIndex;
		}
	StartNextIap();
	}

void CSyncSession::OnConnected()
	{
	TInt err = iResolver.Open(iSocketServ, KAfInet, KProtocolInetUdp, iConnection);
	if (err == KErrNone)
		{
		err = iSocket.Open(iSocketServ, KAfInet, KSockDatagram, KProtocolInetUdp, iConnection);
		}
	if (err != KErrNone)
		{
		iLog.Log(_L("Could not open a socket on the connection (%d)"), err);
		iLastError = err;
		TryNextConnection();
		return;
		}
	iServerIndex = 0;
	ResolveNextServer();
	}

void CSyncSession::ResolveNextServer()
	{
	if (iServerIndex >= iConfig->ServerCount())
		{
		TryNextConnection();
		return;
		}
	iResolver.GetByName(iConfig->Server(iServerIndex), iNameEntry, iStatus);
	Wait(EResolving, KResolveTimeoutSec);
	}

void CSyncSession::OnResolved()
	{
	iServerAddr = TInetAddr::Cast(iNameEntry().iAddr);
	iServerAddr.SetPort(KNtpPort);
	iServerIp.Zero();
	iServerAddr.Output(iServerIp);
	iAttempts = 0;
	iGoodSamples = 0;
	SendSample();
	}

void CSyncSession::SendSample()
	{
	iT1 = NowNtpUs();
	iTxBuf.SetLength(KNtpPacketLength);
	NtpBuildRequest(&iTxBuf[0], iT1, iSentSec, iSentFrac);
	iSocket.SendTo(iTxBuf, iServerAddr, 0, iStatus);
	Wait(ESending, KSendTimeoutSec);
	}

void CSyncSession::OnSent()
	{
	iRxBuf.Zero();
	iSocket.RecvFrom(iRxBuf, iFromAddr, 0, iStatus);
	Wait(EReceiving, KReplyTimeoutSec);
	}

void CSyncSession::OnReceived()
	{
	const TInt64 t4 = NowNtpUs();
	TNtpSample sample;
	const TNtpVerdict verdict = NtpParseReply(iRxBuf.Ptr(), iRxBuf.Length(),
		iSentSec, iSentFrac, iT1, t4, sample);
	const TDesC& name = iConfig->Server(iServerIndex);

	if (verdict == ENtpWrongOrigin)
		{
		// A late answer to an earlier attempt (or junk): keep waiting for ours.
		iRxBuf.Zero();
		iSocket.RecvFrom(iRxBuf, iFromAddr, 0, iStatus);
		Wait(EReceiving, KReplyTimeoutSec);
		return;
		}

	if (verdict == ENtpOk)
		{
		++iGoodSamples;
		if (!iHaveBest || sample.iDelayUs < iBest.iDelayUs)
			{
			iBest = sample;
			iHaveBest = ETrue;
			iBestServer.Format(_L("%S (%S)"), &name, &iServerIp);
			iBestName.Copy(name.Left(iBestName.MaxLength()));
			}
		}
	else
		{
		TBuf<40> reason;
		reason.Copy(TPtrC8(reinterpret_cast<const TUint8*>(NtpVerdictName(verdict))));
		if (verdict == ENtpKissOfDeath)
			{
			TBuf<4> code;
			for (TInt shift = 24; shift >= 0; shift -= 8)
				{
				const TUint c = (sample.iRefId >> shift) & 0xFF;
				code.Append((c >= 0x20 && c < 0x7F) ? TChar(c) : TChar('?'));
				}
			iLog.Log(_L("%S (%S): %S %S, skipping it"), &name, &iServerIp, &reason, &code);
			iAttempts = KMaxAttemptsPerServer; // stop asking this server
			}
		else
			{
			iLog.Log(_L("%S (%S): %S"), &name, &iServerIp, &reason);
			}
		iLastError = KErrCorrupt;
		}
	SampleDone();
	}

void CSyncSession::SampleDone()
	{
	++iAttempts;
	if (iGoodSamples >= KSamplesWanted || iAttempts >= KMaxAttemptsPerServer)
		{
		if (iGoodSamples > 0)
			{
			Finish(KErrNone);
			}
		else
			{
			++iServerIndex;
			ResolveNextServer();
			}
		return;
		}
	iPause.After(iStatus, TTimeIntervalMicroSeconds32(KPauseBetweenSamplesUs));
	Wait(EPausing, 0);
	}

void CSyncSession::Wait(TState aState, TInt aGuardSeconds)
	{
	iState = aState;
	iTimedOut = EFalse;
	SetActive();
	if (aGuardSeconds > 0)
		{
		iGuard->StartSeconds(aGuardSeconds);
		}
	}

void CSyncSession::OnGuardTimeout()
	{
	if (!IsActive())
		{
		return;
		}
	iTimedOut = ETrue;
	switch (iState)
		{
		case EConnecting:
		case EPicking:
			iConnection.Close(); // completes the pending Start with an error
			break;
		case EResolving:
			iResolver.Cancel();
			break;
		case ESending:
			iSocket.CancelSend();
			break;
		case EReceiving:
			iSocket.CancelRecv();
			break;
		default:
			break;
		}
	}

void CSyncSession::RunL()
	{
	iGuard->Cancel();
	const TInt status = iStatus.Int();
	const TInt err = (iTimedOut && status != KErrNone) ? KErrTimedOut : status;

	switch (iState)
		{
		case EConnecting:
			if (err == KErrNone)
				{
				iUsedIap = iConfig->Iaps()[iIapIndex];
				iLog.Log(_L("Wi-Fi access point %u connected"), iUsedIap);
				OnConnected();
				}
			else
				{
				iLog.Log(_L("Wi-Fi access point %u did not connect (%d)"),
					iConfig->Iaps()[iIapIndex], err);
				iLastError = err;
				TryNextConnection();
				}
			break;

		case EPicking:
			if (err == KErrNone)
				{
				TUint32 id = 0;
				TBuf<KCommsDbSvrMaxFieldLength> name;
				const TInt idErr = iConnection.GetIntSetting(_L("IAP\\Id"), id);
				if (iConnection.GetDesSetting(_L("IAP\\Name"), name) != KErrNone)
					{
					name.Zero();
					}
				iPicked = ETrue;
				if (idErr == KErrNone)
					{
					iUsedIap = id;
					iLog.Log(_L("Wi-Fi access point %u (%S) picked"), id, &name);
					iObserver.AccessPointPicked(id, name);
					}
				else
					{
					iLog.Log(_L("Wi-Fi access point connected, but its id is unknown (%d)"), idErr);
					}
				OnConnected();
				}
			else
				{
				if (err == KErrCancel)
					{
					iLog.Log(_L("No access point picked (%d)"), err);
					}
				else
					{
					iLog.Log(_L("Picked access point did not connect (%d)"), err);
					}
				iLastError = err;
				Finish(err);
				}
			break;

		case EResolving:
			if (err == KErrNone)
				{
				OnResolved();
				}
			else
				{
				iLog.Log(_L("Could not look up %S (%d)"), &iConfig->Server(iServerIndex), err);
				iLastError = err;
				++iServerIndex;
				ResolveNextServer();
				}
			break;

		case ESending:
			if (err == KErrNone)
				{
				OnSent();
				}
			else
				{
				iLog.Log(_L("Send to %S failed (%d)"), &iServerIp, err);
				iLastError = err;
				SampleDone();
				}
			break;

		case EReceiving:
			if (err == KErrNone)
				{
				OnReceived();
				}
			else
				{
				iLog.Log(_L("No reply from %S (%S) (%d)"),
					&iConfig->Server(iServerIndex), &iServerIp, err);
				iLastError = err;
				SampleDone();
				}
			break;

		case EPausing:
			SendSample();
			break;

		case EFinishing:
			iState = EIdle;
			iObserver.SyncFinished(status, iBest, iBestServer);
			break;

		default:
			break;
		}
	}

TInt CSyncSession::RunError(TInt aError)
	{
	iLog.Log(_L("Sync error %d"), aError);
	Finish(aError);
	return KErrNone;
	}

void CSyncSession::DoCancel()
	{
	iGuard->Cancel();
	switch (iState)
		{
		case EConnecting:
		case EPicking:
			iConnection.Close();
			break;
		case EResolving:
			iResolver.Cancel();
			break;
		case ESending:
			iSocket.CancelSend();
			break;
		case EReceiving:
			iSocket.CancelRecv();
			break;
		case EPausing:
			iPause.Cancel();
			break;
		default:
			break; // EFinishing is already complete
		}
	iState = EIdle;
	}

void CSyncSession::Finish(TInt aError)
	{
	CloseAll();
	// Report from RunL so the caller of Start() is never re-entered.
	iState = EFinishing;
	iStatus = KRequestPending;
	SetActive();
	TRequestStatus* status = &iStatus;
	User::RequestComplete(status, iHaveBest ? KErrNone : (aError == KErrNone ? KErrGeneral : aError));
	}

void CSyncSession::CloseAll()
	{
	iSocket.Close();
	iResolver.Close();
	iConnection.Close(); // only drops our use; Wi-Fi stays up if others use it
	}
