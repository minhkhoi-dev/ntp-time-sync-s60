/*
 * config.h - settings from C:\Data\ntpsyncd.ini and the Wi-Fi access point list.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <e32base.h>
#include <f32file.h>
#include <cdblen.h>

class CLogger;

/** A Wi-Fi access point from the phone's connection settings. */
struct TWlanIap
	{
	TUint32 iId;
	TBuf<KCommsDbSvrMaxFieldLength> iName;
	};

/** Fills aIaps with every Wi-Fi (LAN service) access point. */
void ListWlanIapsL(RArray<TWlanIap>& aIaps);

/** True if aIapId is in aIaps. */
TBool IsWlanIap(const RArray<TWlanIap>& aIaps, TUint32 aIapId);

/**
 * True if the Wi-Fi access point is Easy WLAN (the one that opens a network
 * picker): its WLAN service record has an empty SSID. If that record cannot be
 * read, falls back to the access point name "Easy WLAN".
 */
TBool IsEasyWlanIap(TUint32 aIapId, const TDesC& aName);

class CNtpConfig : public CBase
	{
public:
	static CNtpConfig* NewL(RFs& aFs, CLogger& aLog);
	~CNtpConfig();

	/**
	 * Re-reads the ini file. If it does not exist, writes a commented default
	 * one that lists aWlanIaps so the user can pick which to enable.
	 * With aFilterIaps, iap= IDs not in aWlanIaps, or that are Easy WLAN, are
	 * dropped with a line in the log; pass EFalse when aWlanIaps could not be read.
	 */
	void LoadL(const RArray<TWlanIap>& aWlanIaps, TBool aFilterIaps);

	/**
	 * Appends "iap=<id>" to the ini file (created if missing) without touching the
	 * rest of it. Does not change the settings held in memory.
	 */
	void SaveIapL(TUint32 aId, const TDesC& aName);

	/**
	 * Writes the whole ini file from the settings in memory (to a temporary
	 * file first, then replaces the old one). Other comments are not kept.
	 * aWlanIaps supplies the access point names and the disabled ones listed.
	 */
	void SaveL(const RArray<TWlanIap>& aWlanIaps);

	TInt ServerCount() const { return iServers.Count(); }
	const TDesC& Server(TInt aIndex) const { return *iServers[aIndex]; }
	/** Access points the daemon may switch on by itself. Empty = only use Wi-Fi already up. */
	const RArray<TUint32>& Iaps() const { return iIaps; }
	TInt IntervalSeconds() const { return iIntervalMinutes * 60; }
	TInt64 ThresholdUs() const { return TInt64(iThresholdMs) * 1000; }
	TBool WatchClock() const { return iWatch; }
	TBool Enabled() const { return iEnabled; }
	TInt IntervalMinutes() const { return iIntervalMinutes; }
	TInt ThresholdMs() const { return iThresholdMs; }

	// For the NTP Time Sync app: change the settings in memory, then call SaveL.
	void SetIntervalMinutes(TInt aMinutes);
	void SetThresholdMs(TInt aMs);
	void SetWatch(TBool aWatch) { iWatch = aWatch; }
	void SetEnabled(TBool aEnabled) { iEnabled = aEnabled; }
	void SetIapsL(const RArray<TUint32>& aIaps);
	void ClearServers();
	/** Adds a server unless the list is full or aName is empty. */
	void AddServerL(const TDesC& aName);

private:
	CNtpConfig(RFs& aFs, CLogger& aLog);
	void ResetL();
	void ParseL(const TDesC8& aText);
	void ParseLineL(const TDesC8& aLine);
	void AddServerL(const TDesC8& aName);
	void FilterIaps(const RArray<TWlanIap>& aWlanIaps);

private:
	RFs& iFs;
	CLogger& iLog;
	RPointerArray<HBufC> iServers;
	RArray<TUint32> iIaps;
	TInt iIntervalMinutes;
	TInt iThresholdMs;
	TBool iWatch;
	TBool iEnabled;
	};

#endif // CONFIG_H
