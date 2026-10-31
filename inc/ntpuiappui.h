/*
 * ntpuiappui.h - the NTP Time Sync AppUi: the status screen, the Options menu and the
 * commands it sends to ntpsyncd.
 */
#ifndef NTPUIAPPUI_H
#define NTPUIAPPUI_H

#include <aknappui.h>

#include "ntpsettinglist.h"
#include "ntpshared.h"
#include "ntpstatusview.h"

class CPropertyWatcher;

class CNtpUiAppUi : public CAknAppUi, public MNtpStatusViewObserver,
	public MNtpSettingObserver
	{
public:
	void ConstructL();
	~CNtpUiAppUi();

private:
	// From CAknAppUi
	void HandleCommandL(TInt aCommand);
	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane);
	void HandleStatusPaneSizeChange();
	void HandleForegroundEventL(TBool aForeground);
	void HandleResourceChangeL(TInt aType);
	// From MNtpStatusViewObserver
	void StatusSelectedL();
	// From MNtpSettingObserver
	void SettingChangedL(TInt aId);

	// The texts: the framework loads ntpsyncui.rsc (Vietnamese); UseLanguageL swaps it
	// for ntpsyncui_en.rsc before any screen is made.
	void UseLanguageL(TInt aLanguage);
	void ResourceFile(TInt aLanguage, TDes& aName) const;
	void ShowSoftkeysL();
	void ShowTitleL();

	// Notes: with a text, from a resource, or from a resource with one %d.
	enum TNoteKind { ENoteInfo, ENoteConfirm, ENoteError };
	void NoteL(TNoteKind aKind, const TDesC& aText);
	void NoteResL(TNoteKind aKind, TInt aResourceId);
	void NoteNumberL(TNoteKind aKind, TInt aResourceId, TInt aNumber);

	// The settings screen, in front of the status screen while iSettings is set.
	void ShowSettingsL(TBool aShow);
	CNtpSettingList* NewSettingListL();
	void LoadSettingsL(TNtpSettingData& aData);
	void RefreshSettingsL();
	void ApplySettingL(TInt aId);
	TBool PickIapsL(RArray<TUint32>& aIaps);
	static TInt LanguageChanged(TAny* aSelf);
	void ApplyLanguageL();

	// The daemon's state, read again and shown.
	void RefreshL();
	void Refresh();
	static TInt StatusChanged(TAny* aSelf);
	static TBool DaemonRunning();

	// Commands
	void SyncNow();
	void SendCommand(TInt aCommand);
	void ShowLogL();
	void StopL();
	void StartL();
	void SaveEnabled(TBool aEnabled);
	void SaveEnabledL(TBool aEnabled);

private:
	CNtpStatusView* iView;
	CNtpSettingList* iSettings;
	CAsyncCallBack* iLanguageCallBack;
	CPropertyWatcher* iWatcher;
	TNtpStatus iStatus;
	TBool iDaemonRunning;
	TInt iUiLanguage;
	};

#endif // NTPUIAPPUI_H
