/*
 * syncsession.h - one NTP sync: get a Wi-Fi connection, resolve a server,
 * take up to 3 samples and report the one with the shortest round trip.
 */
#ifndef SYNCSESSION_H
#define SYNCSESSION_H

#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>
#include <commdbconnpref.h>

#include "ntppacket.h"
#include "config.h"

class CLogger;
class CGuardTimer;

class MSyncObserver
	{
public:
	/** aSample and aServer are valid only when aError is KErrNone. */
	virtual void SyncFinished(TInt aError, const TNtpSample& aSample, const TDesC& aServer) = 0;
	/** The user chose this Wi-Fi access point from the system picker (interactive syncs only). */
	virtual void AccessPointPicked(TUint32 aId, const TDesC& aName) = 0;
	};

class CSyncSession : public CActive
	{
public:
	static CSyncSession* NewL(RSocketServ& aSocketServ, CLogger& aLog, MSyncObserver& aObserver);
	~CSyncSession();

	/**
	 * Starts a sync. aConfig and aWlanIaps must stay unchanged until the
	 * observer is called, which always happens later from RunL.
	 * An interactive sync with no iap= enabled lets the user pick the access point.
	 */
	void Start(const CNtpConfig& aConfig, const RArray<TWlanIap>& aWlanIaps, TBool aInteractive);
	TBool IsBusy() const { return iState != EIdle; }

	/** About the last sync, valid once the observer has been called. */
	TBool UsedPicker() const { return iPickerUsed; }
	TBool NoWifi() const { return iNoWifi; }
	/** Access point the last sync used, 0 = unknown. */
	TUint32 UsedIapId() const { return iUsedIap; }
	/** Host name of the best server of the last sync, without the address. */
	const TDesC& BestServerName() const { return iBestName; }

	/** Called by the guard timer when the current step takes too long. */
	void OnGuardTimeout();

private:
	enum TState
		{
		EIdle,
		EConnecting,
		EPicking,
		EResolving,
		ESending,
		EReceiving,
		EPausing,
		EFinishing
		};

	CSyncSession(RSocketServ& aSocketServ, CLogger& aLog, MSyncObserver& aObserver);
	void ConstructL();

	// From CActive
	void RunL();
	void DoCancel();
	TInt RunError(TInt aError);

	TBool AttachToActiveWlan(const RArray<TWlanIap>& aWlanIaps);
	void StartPicker();
	void StartNextIap();
	void TryNextConnection();
	void OnConnected();
	void ResolveNextServer();
	void OnResolved();
	void SendSample();
	void OnSent();
	void OnReceived();
	void SampleDone();
	void Finish(TInt aError);
	void CloseAll();
	void Wait(TState aState, TInt aGuardSeconds);
	TInt64 NowNtpUs() const;

private:
	RSocketServ& iSocketServ;
	CLogger& iLog;
	MSyncObserver& iObserver;
	const CNtpConfig* iConfig;

	TState iState;
	CGuardTimer* iGuard;
	TBool iTimedOut;
	RTimer iPause;

	RConnection iConnection;
	TCommDbConnPref iPref;
	TBool iAttached;
	TInt iIapIndex;
	TBool iPickerUsed;
	TBool iPicked;
	TBool iNoWifi;
	TUint32 iUsedIap;

	RHostResolver iResolver;
	TNameEntry iNameEntry;
	TInt iServerIndex;
	TInetAddr iServerAddr;
	TBuf<40> iServerIp;

	RSocket iSocket;
	TInetAddr iFromAddr;
	TBuf8<KNtpPacketLength> iTxBuf;
	TBuf8<256> iRxBuf;
	TUint32 iSentSec;
	TUint32 iSentFrac;
	TInt64 iT1;

	TInt iAttempts;
	TInt iGoodSamples;
	TBool iHaveBest;
	TNtpSample iBest;
	TBuf<200> iBestServer;
	TBuf<80> iBestName;
	TInt iLastError;
	TInt64 iNtpEpoch;
	};

#endif // SYNCSESSION_H
