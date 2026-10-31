/*
 * config.cpp - settings from C:\Data\ntpsyncd.ini and the Wi-Fi access point list.
 */
#include "config.h"
#include "logger.h"

#include <commdb.h>
#include <wlancdbcols.h>
#include <utf.h>

_LIT(KIniFile, "C:\\Data\\ntpsyncd.ini");

const TInt KMaxServers = 5;
const TInt KMaxServerNameLen = 128;
const TInt KMaxIniSize = 8 * 1024;
const TInt KDefaultIntervalMinutes = 360;
const TInt KMinIntervalMinutes = 15;
const TInt KMaxIntervalMinutes = 7 * 24 * 60;
const TInt KDefaultThresholdMs = 500;
const TInt KMaxThresholdMs = 600000;

// ---------------------------------------------------------------------------
// Access point list

void ListWlanIapsL(RArray<TWlanIap>& aIaps)
	{
	aIaps.Reset();
	CCommsDatabase* db = CCommsDatabase::NewL(EDatabaseTypeIAP);
	CleanupStack::PushL(db);
	// The WLAN bearer set matches every IAP that uses the LAN service table,
	// which on these phones means the Wi-Fi access points.
	CCommsDbTableView* view = db->OpenIAPTableViewMatchingBearerSetLC(
		ECommDbBearerWLAN, ECommDbConnectionDirectionOutgoing);
	for (TInt err = view->GotoFirstRecord(); err == KErrNone; err = view->GotoNextRecord())
		{
		TWlanIap iap;
		iap.iId = 0;
		view->ReadUintL(TPtrC(COMMDB_ID), iap.iId);
		TRAP_IGNORE(view->ReadTextL(TPtrC(COMMDB_NAME), iap.iName));
		aIaps.AppendL(iap);
		}
	CleanupStack::PopAndDestroy(2, db); // view, db
	}

TBool IsWlanIap(const RArray<TWlanIap>& aIaps, TUint32 aIapId)
	{
	for (TInt i = 0; i < aIaps.Count(); ++i)
		{
		if (aIaps[i].iId == aIapId)
			{
			return ETrue;
			}
		}
	return EFalse;
	}

// Reads the SSID of a Wi-Fi access point from the S60 WLAN service table
// (WLANServiceTable, key column ServiceID = the IAP's IAPService, text column
// WlanSSID). Leaves with KErrNotFound if the access point has no such record.
static void ReadWlanSsidL(TUint32 aIapId, TDes& aSsid)
	{
	CCommsDatabase* db = CCommsDatabase::NewL(EDatabaseTypeIAP);
	CleanupStack::PushL(db);

	TUint32 service = 0;
	CCommsDbTableView* iapView = db->OpenViewMatchingUintLC(TPtrC(IAP), TPtrC(COMMDB_ID), aIapId);
	User::LeaveIfError(iapView->GotoFirstRecord());
	iapView->ReadUintL(TPtrC(IAP_SERVICE), service);
	CleanupStack::PopAndDestroy(iapView);

	CCommsDbTableView* view = db->OpenViewMatchingUintLC(TPtrC(WLAN_SERVICE),
		TPtrC(WLAN_SERVICE_ID), service);
	User::LeaveIfError(view->GotoFirstRecord());
	TRAPD(err, view->ReadTextL(TPtrC(NU_WLAN_SSID), aSsid));
	if (err != KErrNone)
		{
		// Some builds keep the SSID in a long text column.
		HBufC* longText = view->ReadLongTextLC(TPtrC(NU_WLAN_SSID));
		aSsid.Copy(longText->Left(aSsid.MaxLength()));
		CleanupStack::PopAndDestroy(longText);
		}
	CleanupStack::PopAndDestroy(2, db); // view, db
	}

TBool IsEasyWlanIap(TUint32 aIapId, const TDesC& aName)
	{
	TBuf<64> ssid;
	TRAPD(err, ReadWlanSsidL(aIapId, ssid));
	if (err == KErrNone)
		{
		return ssid.Length() == 0;
		}
	_LIT(KEasyWlan, "Easy WLAN");
	return aName.CompareF(KEasyWlan) == 0;
	}

// ---------------------------------------------------------------------------
// Settings

CNtpConfig* CNtpConfig::NewL(RFs& aFs, CLogger& aLog)
	{
	CNtpConfig* self = new (ELeave) CNtpConfig(aFs, aLog);
	CleanupStack::PushL(self);
	self->ResetL();
	CleanupStack::Pop(self);
	return self;
	}

CNtpConfig::CNtpConfig(RFs& aFs, CLogger& aLog)
	: iFs(aFs), iLog(aLog)
	{
	}

CNtpConfig::~CNtpConfig()
	{
	iServers.ResetAndDestroy();
	iIaps.Close();
	}

void CNtpConfig::ResetL()
	{
	iServers.ResetAndDestroy();
	iIaps.Reset();
	iIntervalMinutes = KDefaultIntervalMinutes;
	iThresholdMs = KDefaultThresholdMs;
	iWatch = ETrue;
	iEnabled = ETrue;
	}

void CNtpConfig::LoadL(const RArray<TWlanIap>& aWlanIaps, TBool aFilterIaps)
	{
	ResetL();

	RFile file;
	TInt err = file.Open(iFs, KIniFile, EFileRead | EFileShareReadersOnly);
	if (err == KErrNotFound || err == KErrPathNotFound)
		{
		AddServerL(_L8("vn.pool.ntp.org"));
		AddServerL(_L8("asia.pool.ntp.org"));
		AddServerL(_L8("pool.ntp.org"));
		TRAPD(werr, SaveL(aWlanIaps));
		if (werr == KErrNone)
			{
			iLog.Log(_L("Wrote default settings to C:\\Data\\ntpsyncd.ini"));
			}
		else
			{
			iLog.Log(_L("Could not write C:\\Data\\ntpsyncd.ini (%d)"), werr);
			}
		}
	else if (err != KErrNone)
		{
		iLog.Log(_L("Could not open settings (%d), using defaults"), err);
		}
	else
		{
		CleanupClosePushL(file);
		TInt size = 0;
		User::LeaveIfError(file.Size(size));
		if (size > KMaxIniSize)
			{
			size = KMaxIniSize;
			}
		HBufC8* text = HBufC8::NewLC(size);
		TPtr8 ptr = text->Des();
		User::LeaveIfError(file.Read(ptr, size));
		ParseL(*text);
		CleanupStack::PopAndDestroy(2, &file); // text, file
		}

	if (iServers.Count() == 0)
		{
		AddServerL(_L8("vn.pool.ntp.org"));
		AddServerL(_L8("asia.pool.ntp.org"));
		AddServerL(_L8("pool.ntp.org"));
		}

	if (aFilterIaps)
		{
		FilterIaps(aWlanIaps);
		}
	}

// Keeps only iap= IDs that are Wi-Fi access points on this phone and not
// Easy WLAN, so a packet data ID or a deleted access point is never started.
void CNtpConfig::FilterIaps(const RArray<TWlanIap>& aWlanIaps)
	{
	TInt i = 0;
	while (i < iIaps.Count())
		{
		const TUint32 id = iIaps[i];
		TInt found = KErrNotFound;
		for (TInt j = 0; j < aWlanIaps.Count(); ++j)
			{
			if (aWlanIaps[j].iId == id)
				{
				found = j;
				break;
				}
			}
		if (found == KErrNotFound)
			{
			iLog.Log(_L("Settings: iap=%u is not a Wi-Fi access point on this phone, ignored"), id);
			iIaps.Remove(i);
			}
		else if (IsEasyWlanIap(id, aWlanIaps[found].iName))
			{
			iLog.Log(_L("Settings: iap=%u is Easy WLAN, ignored"), id);
			iIaps.Remove(i);
			}
		else
			{
			++i;
			}
		}
	}

void CNtpConfig::SaveIapL(TUint32 aId, const TDesC& aName)
	{
	RFile file;
	TBool utf16 = EFalse;
	TBool needNewline = EFalse;
	TInt err = file.Open(iFs, KIniFile, EFileRead | EFileWrite | EFileShareExclusive);
	if (err == KErrNotFound || err == KErrPathNotFound)
		{
		TInt mkErr = iFs.MkDirAll(KIniFile);
		if (mkErr != KErrNone && mkErr != KErrAlreadyExists)
			{
			User::Leave(mkErr);
			}
		err = file.Create(iFs, KIniFile, EFileWrite | EFileShareExclusive);
		}
	User::LeaveIfError(err);
	CleanupClosePushL(file);

	TInt size = 0;
	User::LeaveIfError(file.Size(size));
	if (size > 0)
		{
		TBuf8<2> head;
		User::LeaveIfError(file.Read(0, head, 2));
		utf16 = head.Length() == 2 && head[0] == 0xFF && head[1] == 0xFE;
		const TInt tailLen = utf16 ? 2 : 1;
		if (size >= tailLen)
			{
			TBuf8<2> tail;
			User::LeaveIfError(file.Read(size - tailLen, tail, tailLen));
			if (utf16)
				{
				needNewline = !(tail.Length() == 2 && tail[0] == '\n' && tail[1] == 0);
				}
			else
				{
				needNewline = !(tail.Length() == 1 && tail[0] == '\n');
				}
			}
		}

	TBuf<160> line;
	if (needNewline)
		{
		line.Copy(_L("\r\n"));
		}
	line.AppendFormat(_L("iap=%u    ; %S (picked from the menu)\r\n"), aId, &aName);

	TInt pos = 0;
	User::LeaveIfError(file.Seek(ESeekEnd, pos));
	if (utf16)
		{
		User::LeaveIfError(file.Write(TPtrC8(reinterpret_cast<const TUint8*>(line.Ptr()), line.Size())));
		}
	else
		{
		TBuf8<480> line8;
		CnvUtfConverter::ConvertFromUnicodeToUtf8(line8, line);
		User::LeaveIfError(file.Write(line8));
		}
	CleanupStack::PopAndDestroy(&file);
	}

void CNtpConfig::ParseL(const TDesC8& aText)
	{
	TPtrC8 text(aText);

	// Editors on the phone sometimes save UTF-16; keep the low bytes.
	HBufC8* narrowed = NULL;
	if (text.Length() >= 2 && text[0] == 0xFF && text[1] == 0xFE)
		{
		narrowed = HBufC8::NewLC(text.Length() / 2);
		TPtr8 n = narrowed->Des();
		for (TInt i = 2; i + 1 < text.Length(); i += 2)
			{
			n.Append(text[i]);
			}
		text.Set(*narrowed);
		}
	else if (text.Length() >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF)
		{
		text.Set(text.Mid(3)); // UTF-8 byte order mark
		}

	while (text.Length() > 0)
		{
		TInt eol = text.Locate('\n');
		TPtrC8 line(eol < 0 ? text : text.Left(eol));
		ParseLineL(line);
		text.Set(eol < 0 ? TPtrC8() : text.Mid(eol + 1));
		}

	if (narrowed)
		{
		CleanupStack::PopAndDestroy(narrowed);
		}
	}

void CNtpConfig::ParseLineL(const TDesC8& aLine)
	{
	TBuf8<160> line;
	line.Copy(aLine.Left(line.MaxLength()));
	TInt cut = line.Locate(';');
	if (cut >= 0)
		{
		line.SetLength(cut);
		}
	cut = line.Locate('#');
	if (cut >= 0)
		{
		line.SetLength(cut);
		}
	line.Trim(); // also drops the '\r' of CRLF files
	if (line.Length() == 0)
		{
		return;
		}

	const TInt eq = line.Locate('=');
	if (eq <= 0)
		{
		TBuf<160> wide;
		wide.Copy(line);
		iLog.Log(_L("Settings: ignoring line \"%S\""), &wide);
		return;
		}

	TBuf8<32> key;
	key.Copy(line.Left(Min(eq, key.MaxLength())));
	key.Trim();
	TBuf8<128> value;
	value.Copy(line.Mid(eq + 1).Left(value.MaxLength()));
	value.Trim();

	TLex8 lex(value);
	if (key.CompareF(_L8("server")) == 0)
		{
		if (value.Length() > 0)
			{
			AddServerL(value);
			}
		}
	else if (key.CompareF(_L8("iap")) == 0)
		{
		TUint32 id = 0;
		if (lex.Val(id, EDecimal) == KErrNone && id != 0 && iIaps.Find(id) == KErrNotFound)
			{
			iIaps.AppendL(id);
			}
		}
	else if (key.CompareF(_L8("interval")) == 0)
		{
		TInt v = 0;
		if (lex.Val(v) == KErrNone)
			{
			iIntervalMinutes = Max(KMinIntervalMinutes, Min(v, KMaxIntervalMinutes));
			}
		}
	else if (key.CompareF(_L8("threshold")) == 0)
		{
		TInt v = 0;
		if (lex.Val(v) == KErrNone)
			{
			iThresholdMs = Max(0, Min(v, KMaxThresholdMs));
			}
		}
	else if (key.CompareF(_L8("watch")) == 0)
		{
		TInt v = 0;
		if (lex.Val(v) == KErrNone)
			{
			iWatch = (v != 0);
			}
		}
	else if (key.CompareF(_L8("enabled")) == 0)
		{
		TInt v = 0;
		if (lex.Val(v) == KErrNone)
			{
			iEnabled = (v != 0);
			}
		}
	else
		{
		TBuf<32> wide;
		wide.Copy(key);
		iLog.Log(_L("Settings: unknown key \"%S\""), &wide);
		}
	}

void CNtpConfig::AddServerL(const TDesC8& aName)
	{
	if (iServers.Count() >= KMaxServers)
		{
		return;
		}
	HBufC* name = HBufC::NewLC(aName.Length());
	name->Des().Copy(aName);
	iServers.AppendL(name);
	CleanupStack::Pop(name);
	}

void CNtpConfig::AddServerL(const TDesC& aName)
	{
	if (aName.Length() == 0 || iServers.Count() >= KMaxServers)
		{
		return;
		}
	const TPtrC cut(aName.Left(KMaxServerNameLen));
	HBufC* name = cut.AllocLC();
	iServers.AppendL(name);
	CleanupStack::Pop(name);
	}

void CNtpConfig::ClearServers()
	{
	iServers.ResetAndDestroy();
	}

void CNtpConfig::SetIapsL(const RArray<TUint32>& aIaps)
	{
	iIaps.Reset();
	for (TInt i = 0; i < aIaps.Count(); ++i)
		{
		if (aIaps[i] != 0 && iIaps.Find(aIaps[i]) == KErrNotFound)
			{
			iIaps.AppendL(aIaps[i]);
			}
		}
	}

void CNtpConfig::SetIntervalMinutes(TInt aMinutes)
	{
	iIntervalMinutes = Max(KMinIntervalMinutes, Min(aMinutes, KMaxIntervalMinutes));
	}

void CNtpConfig::SetThresholdMs(TInt aMs)
	{
	iThresholdMs = Max(0, Min(aMs, KMaxThresholdMs));
	}

// Writes aText as UTF-8.
static void WriteWideL(RFile& aFile, const TDesC& aText)
	{
	TBuf8<600> text8;
	CnvUtfConverter::ConvertFromUnicodeToUtf8(text8, aText);
	User::LeaveIfError(aFile.Write(text8));
	}

static void WriteIapLineL(RFile& aFile, const TDesC& aPrefix, TUint32 aId,
	const RArray<TWlanIap>& aWlanIaps)
	{
	TPtrC name;
	for (TInt i = 0; i < aWlanIaps.Count(); ++i)
		{
		if (aWlanIaps[i].iId == aId)
			{
			name.Set(aWlanIaps[i].iName);
			break;
			}
		}
	TBuf<200> line;
	line.Format(_L("%Siap=%u    ; %S\r\n"), &aPrefix, aId, &name);
	WriteWideL(aFile, line);
	}

void CNtpConfig::SaveL(const RArray<TWlanIap>& aWlanIaps)
	{
	_LIT(KNewFile, "C:\\Data\\ntpsyncd.ini.new");
	TInt err = iFs.MkDirAll(KIniFile);
	if (err != KErrNone && err != KErrAlreadyExists)
		{
		User::Leave(err);
		}

	RFile file;
	User::LeaveIfError(file.Replace(iFs, KNewFile, EFileWrite | EFileShareExclusive));
	CleanupClosePushL(file);

	User::LeaveIfError(file.Write(_L8(
		"; ntpsyncd settings. Changes apply at the next sync, or right away when saved\r\n"
		"; from the NTP Time Sync app. Log: C:\\Data\\ntpsyncd.log\r\n"
		"; Saving settings in the NTP Time Sync app rewrites this file; other comments are not kept.\r\n"
		"\r\n"
		"; NTP servers, tried in order (up to 5).\r\n")));

	TBuf<200> line;
	for (TInt i = 0; i < iServers.Count(); ++i)
		{
		const TDesC* server = iServers[i];
		line.Format(_L("server=%S\r\n"), server);
		WriteWideL(file, line);
		}

	User::LeaveIfError(file.Write(_L8(
		"\r\n"
		"; Wi-Fi access points ntpsyncd may switch on by itself to sync.\r\n"
		"; With none enabled it only syncs while a Wi-Fi connection is\r\n"
		"; already up (checked every 10 minutes until it succeeds).\r\n"
		"; Remove the ';' in front of the ones to use. IDs that are not\r\n"
		"; Wi-Fi access points, and Easy WLAN, are skipped.\r\n")));

	_LIT(KEnabledPrefix, "");
	_LIT(KDisabledPrefix, ";");
	for (TInt i = 0; i < iIaps.Count(); ++i)
		{
		WriteIapLineL(file, KEnabledPrefix, iIaps[i], aWlanIaps);
		}
	for (TInt i = 0; i < aWlanIaps.Count(); ++i)
		{
		const TUint32 id = aWlanIaps[i].iId;
		if (iIaps.Find(id) == KErrNotFound && !IsEasyWlanIap(id, aWlanIaps[i].iName))
			{
			WriteIapLineL(file, KDisabledPrefix, id, aWlanIaps);
			}
		}
	if (aWlanIaps.Count() == 0 && iIaps.Count() == 0)
		{
		User::LeaveIfError(file.Write(_L8("; (no Wi-Fi access points were found)\r\n")));
		}

	TBuf8<64> num;
	User::LeaveIfError(file.Write(_L8(
		"\r\n"
		"; Minutes between syncs.\r\n")));
	num.Format(_L8("interval=%d\r\n"), iIntervalMinutes);
	User::LeaveIfError(file.Write(num));
	User::LeaveIfError(file.Write(_L8(
		"\r\n"
		"; Leave the clock alone if it is off by less than this (milliseconds).\r\n")));
	num.Format(_L8("threshold=%d\r\n"), iThresholdMs);
	User::LeaveIfError(file.Write(num));
	User::LeaveIfError(file.Write(_L8(
		"\r\n"
		"; 1 = re-sync soon when something else (network operator time,\r\n"
		"; another app) changes the clock. 0 = only on schedule.\r\n")));
	num.Format(_L8("watch=%d\r\n"), iWatch ? 1 : 0);
	User::LeaveIfError(file.Write(num));
	User::LeaveIfError(file.Write(_L8(
		"\r\n"
		"; 1 = sync in the background. 0 = the daemon stops (set from the NTP Time Sync app).\r\n")));
	num.Format(_L8("enabled=%d\r\n"), iEnabled ? 1 : 0);
	User::LeaveIfError(file.Write(num));

	User::LeaveIfError(file.Flush());
	CleanupStack::PopAndDestroy(&file);
	err = iFs.Replace(KNewFile, KIniFile);
	if (err != KErrNone)
		{
		iFs.Delete(KNewFile);
		User::Leave(err);
		}
	}
