/*
 * ntpshared.h - what the daemon publishes and what the NTP Time Sync app may send.
 */
#ifndef NTPSHARED_H
#define NTPSHARED_H

#include <e32std.h>

#include "ntpsyncd.hrh"

/** P&S category: the daemon's SID, which defines the keys. */
const TUid KNtpSyncdCategory = { KNtpSyncdUid3 };

enum TNtpDaemonState
	{
	ENtpStateIdle = 0,
	ENtpStateSyncing,
	ENtpStateStopped
	};

enum TNtpClockResult
	{
	ENtpClockRight = 0,
	ENtpClockCorrected,
	ENtpClockSetFailed
	};

enum TNtpCommand
	{
	ENtpCmdSyncNow = 1,
	ENtpCmdReload = 2,
	ENtpCmdStop = 3
	};

/** The tick part makes every Set a new value, so subscribers are always woken. */
inline TInt NtpCommandValue(TInt aCommand)
	{
	return aCommand | static_cast<TInt>(User::NTickCount() << 4);
	}

inline TInt NtpCommandCode(TInt aValue)
	{
	return aValue & 0xF;
	}

const TInt KNtpStatusLayout = 2;
const TInt KNtpStatusServerMax = 64;
const TInt KNtpStatusIapNameMax = 50;

// Plain data, no TBuf: it is copied as raw bytes, and zeroing would wipe a TBuf's header.
struct TNtpStatus
	{
	TInt iLayout;            // KNtpStatusLayout; readers ignore any other value
	TInt iState;             // TNtpDaemonState
	TInt64 iLastAttemptUtc;  // TTime::Int64() when the last sync started, 0 = none yet
	TInt iLastError;         // result of the last finished sync, KErrNone = success
	TBool iNoWifi;           // the last sync failed because no Wi-Fi was usable
	TInt64 iLastSuccessUtc;  // when the last good sync finished, 0 = none since the daemon started
	TInt64 iOffsetUs;        // the rest describe the last good sync
	TInt iClockResult;       // TNtpClockResult
	TInt iSetError;          // User::SetUTCTime error with ENtpClockSetFailed
	TInt iStratum;
	TInt iDelayMs;
	TInt iServerLength;      // characters used in iServer
	TText iServer[KNtpStatusServerMax]; // host name, e.g. "vn.pool.ntp.org", cut to fit
	TUint32 iIapId;          // access point used, 0 = unknown
	TInt iIapNameLength;     // characters used in iIapName
	TText iIapName[KNtpStatusIapNameMax];
	TInt64 iNextSyncUtc;     // when the next sync is due, 0 = none scheduled

	void SetServer(const TDesC& aText)
		{
		iServerLength = Min(aText.Length(), KNtpStatusServerMax);
		Mem::Copy(iServer, aText.Ptr(), iServerLength * sizeof(TText));
		}
	TPtrC Server() const
		{
		return TPtrC(iServer, Min(Max(iServerLength, 0), KNtpStatusServerMax));
		}
	void SetIapName(const TDesC& aText)
		{
		iIapNameLength = Min(aText.Length(), KNtpStatusIapNameMax);
		Mem::Copy(iIapName, aText.Ptr(), iIapNameLength * sizeof(TText));
		}
	TPtrC IapName() const
		{
		return TPtrC(iIapName, Min(Max(iIapNameLength, 0), KNtpStatusIapNameMax));
		}
	};

__ASSERT_COMPILE(sizeof(TNtpStatus) <= 512); // RProperty byte array limit

#endif // NTPSHARED_H
