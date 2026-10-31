/*
 * ntpuiappui.cpp - the NTP Time Sync AppUi: the status screen, the Options menu and the
 * commands it sends to ntpsyncd.
 */
#include "ntpuiappui.h"

#include <akncheckboxsettingpage.h>
#include <akntitle.h>
#include <aknnotewrappers.h>
#include <aknquerydialog.h>
#include <aknmessagequerydialog.h>
#include <avkon.hrh>
#include <bautils.h>
#include <eikapp.h>
#include <eikbtgpc.h>
#include <eikmenup.h>
#include <eikspane.h>
#include <e32property.h>
#include <txtetext.h>
#include <utf.h>
#include <ntpsyncui.rsg>

#include "config.h"
#include "logger.h"
#include "ntpsyncd.hrh"
#include "ntpsyncui.hrh"
#include "ntpuisettings.h"
#include "propwatcher.h"

namespace
	{
	// The resource files of the two languages, on the drive the app is installed on.
	_LIT(KResourceVi, "\\resource\\apps\\ntpsyncui.rsc");
	_LIT(KResourceEn, "\\resource\\apps\\ntpsyncui_en.rsc");

	_LIT(KDaemonMutex, "ntpsyncd.single"); // the daemon's single-instance mutex (main.cpp)
	_LIT(KDaemonExe, "ntpsyncd.exe");
	_LIT(KDaemonArg, "app");
	_LIT(KLogFile, "C:\\Data\\ntpsyncd.log");

	const TInt KLogTailBytes = 6 * 1024;

	// Line order reversed (newest first), lines joined with LF; empty lines dropped.
	HBufC* ReverseLinesLC(const TDesC& aText)
		{
		HBufC* out = HBufC::NewLC(aText.Length() + 2);
		TPtr o = out->Des();
		TInt end = aText.Length();
		while (end > 0)
			{
			// skip the line end that follows the line
			if (aText[end - 1] == '\n' || aText[end - 1] == '\r')
				{
				--end;
				continue;
				}
			TInt start = end;
			while (start > 0 && aText[start - 1] != '\n')
				{
				--start;
				}
			TPtrC line(aText.Mid(start, end - start));
			if (o.Length() > 0)
				{
				o.Append('\n');
				}
			o.Append(line);
			end = start;
			}
		return out;
		}
	}

void CNtpUiAppUi::ConstructL()
	{
	BaseConstructL(EAknEnableSkin);
	iLanguageCallBack = new (ELeave) CAsyncCallBack(TCallBack(LanguageChanged, this),
		CActive::EPriorityStandard);

	// The texts in the saved language, before anything shows them: the framework loaded
	// ntpsyncui.rsc (Vietnamese) and made the softkeys from it.
	if (NtpUiReadLanguage(iCoeEnv->FsSession()) == ENtpLangEnglish)
		{
		TRAPD(err, UseLanguageL(ENtpLangEnglish));
		if (err == KErrNone)
			{
			ShowSoftkeysL();
			}
		}
	ShowTitleL();

	Mem::FillZ(&iStatus, sizeof(iStatus));
	iView = CNtpStatusView::NewL(ClientRect(), *this);
	iView->SetMopParent(this);
	AddToStackL(iView);
	iView->SetFocus(ETrue);

	// Without the watcher the screen is still right, it is only refreshed less often.
	TRAP_IGNORE(iWatcher = CPropertyWatcher::NewL(KNtpSyncdCategory, KNtpSyncdStatusKey,
		TCallBack(StatusChanged, this)));
	RefreshL();
	}

CNtpUiAppUi::~CNtpUiAppUi()
	{
	delete iWatcher;
	delete iLanguageCallBack;
	if (iSettings)
		{
		RemoveFromStack(iSettings);
		delete iSettings;
		}
	if (iView)
		{
		RemoveFromStack(iView); // harmless when the settings screen has taken it off
		delete iView;
		}
	}

void CNtpUiAppUi::HandleCommandL(TInt aCommand)
	{
	switch (aCommand)
		{
		case EEikCmdExit:
		case EAknSoftkeyExit:
			Exit();
			return;
		case ENtpUiCmdSyncNow:
			SyncNow();
			return;
		case ENtpUiCmdLog:
			ShowLogL();
			return;
		case ENtpUiCmdStop:
			StopL();
			return;
		case ENtpUiCmdStart:
			StartL();
			return;
		case ENtpUiCmdSettings:
			ShowSettingsL(ETrue);
			return;
		case EAknSoftkeyBack:
			ShowSettingsL(EFalse);
			return;
		case ENtpUiCmdChange:
			if (iSettings)
				{
				iSettings->EditCurrentL();
				}
			return;
		default:
			return;
		}
	}

// Hides what does not apply to the state on the screen.
void CNtpUiAppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
	{
	if (aResourceId != R_NTP_MENU)
		{
		return;
		}
	const TBool onSettings = iSettings != NULL;
	const TNtpShownState shown = NtpShownState(iStatus, iDaemonRunning);
	aMenuPane->SetItemDimmed(ENtpUiCmdSyncNow, onSettings || shown != ENtpShownRunning);
	aMenuPane->SetItemDimmed(ENtpUiCmdSettings, onSettings);
	aMenuPane->SetItemDimmed(ENtpUiCmdLog, onSettings);
	aMenuPane->SetItemDimmed(ENtpUiCmdStop, onSettings || shown == ENtpShownStopped);
	aMenuPane->SetItemDimmed(ENtpUiCmdStart, onSettings || shown != ENtpShownStopped);
	aMenuPane->SetItemDimmed(ENtpUiCmdChange, !onSettings);
	}

void CNtpUiAppUi::HandleStatusPaneSizeChange()
	{
	if (iView)
		{
		iView->SetRect(ClientRect());
		}
	if (iSettings)
		{
		iSettings->SetRect(ClientRect());
		}
	}

void CNtpUiAppUi::HandleResourceChangeL(TInt aType)
	{
	CAknAppUi::HandleResourceChangeL(aType);
	if (aType == KEikDynamicLayoutVariantSwitch)
		{
		HandleStatusPaneSizeChange();
		}
	}

void CNtpUiAppUi::HandleForegroundEventL(TBool aForeground)
	{
	CAknAppUi::HandleForegroundEventL(aForeground);
	if (aForeground && iView)
		{
		Refresh();
		}
	}

// The selection key on the list.
void CNtpUiAppUi::StatusSelectedL()
	{
	SyncNow();
	}

// ---------------------------------------------------------------------------
// Texts

void CNtpUiAppUi::UseLanguageL(TInt aLanguage)
	{
	if (aLanguage == iUiLanguage)
		{
		return;
		}
	TFileName name;
	ResourceFile(aLanguage, name);
	if (!BaflUtils::FileExists(iCoeEnv->FsSession(), name))
		{
		User::Leave(KErrNotFound); // the texts stay as they are
		}
	TFileName old;
	ResourceFile(iUiLanguage, old);
	// Both files have the same NAME, so the same offset and IDs; the application frees
	// the one loaded now at the exit, as it would have the first.
	iCoeEnv->DeleteResourceFile(Application()->ResourceFileOffset());
	TRAPD(err, iCoeEnv->AddResourceFileL(name));
	if (err != KErrNone)
		{
		iCoeEnv->AddResourceFileL(old); // the texts of before
		User::Leave(err);
		}
	iUiLanguage = aLanguage;
	}

void CNtpUiAppUi::ResourceFile(TInt aLanguage, TDes& aName) const
	{
	const TFileName exe(Application()->AppFullName());
	TParsePtrC app(exe);
	aName.Copy(app.Drive());
	aName.Append(aLanguage == ENtpLangEnglish ? KResourceEn() : KResourceVi());
	}

void CNtpUiAppUi::ShowSoftkeysL()
	{
	CEikButtonGroupContainer* cba = CEikButtonGroupContainer::Current();
	if (cba)
		{
		cba->SetCommandSetL(iSettings ? R_NTP_SOFTKEYS_OPTIONS_BACK
			: R_NTP_SOFTKEYS_OPTIONS_EXIT);
		cba->DrawDeferred();
		}
	}

void CNtpUiAppUi::ShowTitleL()
	{
	CAknTitlePane* pane = static_cast<CAknTitlePane*>(
		StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle)));
	HBufC* title = iEikonEnv->AllocReadResourceLC(iSettings ? R_NTP_STR_SETTINGS_TITLE
		: R_NTP_STR_TITLE);
	pane->SetTextL(*title);
	CleanupStack::PopAndDestroy(title);
	}

void CNtpUiAppUi::NoteL(TNoteKind aKind, const TDesC& aText)
	{
	CAknResourceNoteDialog* note = NULL;
	switch (aKind)
		{
		case ENoteConfirm:
			note = new (ELeave) CAknConfirmationNote(EFalse);
			break;
		case ENoteError:
			note = new (ELeave) CAknErrorNote(EFalse);
			break;
		default:
			note = new (ELeave) CAknInformationNote(EFalse);
			break;
		}
	note->ExecuteLD(aText);
	}

void CNtpUiAppUi::NoteResL(TNoteKind aKind, TInt aResourceId)
	{
	HBufC* text = iEikonEnv->AllocReadResourceLC(aResourceId);
	NoteL(aKind, *text);
	CleanupStack::PopAndDestroy(text);
	}

void CNtpUiAppUi::NoteNumberL(TNoteKind aKind, TInt aResourceId, TInt aNumber)
	{
	HBufC* fmt = iEikonEnv->AllocReadResourceLC(aResourceId);
	TBuf<160> text;
	text.Format(*fmt, aNumber);
	CleanupStack::PopAndDestroy(fmt);
	NoteL(aKind, text);
	}

// ---------------------------------------------------------------------------
// The settings screen

namespace
	{
	// Appends as much of aText as fits.
	void AppendFit(TDes& aTo, const TDesC& aText)
		{
		aTo.Append(aText.Left(Min(aText.Length(), aTo.MaxLength() - aTo.Length())));
		}

	void DestroySelectionItems(TAny* aItems)
		{
		CSelectionItemList* items = static_cast<CSelectionItemList*>(aItems);
		items->ResetAndDestroy();
		delete items;
		}

	TBool IsServerSeparator(TText aChar)
		{
		return NtpIsServerSeparator(aChar);
		}
	}

// The settings screen in front, with Back on the right softkey, or the status screen
// again. The one in front is on the control stack, the other is hidden.
void CNtpUiAppUi::ShowSettingsL(TBool aShow)
	{
	if (aShow == (iSettings != NULL))
		{
		return;
		}
	if (aShow)
		{
		CNtpSettingList* list = NewSettingListL();
		CleanupStack::PushL(list);
		AddToStackL(list);
		CleanupStack::Pop(list);
		RemoveFromStack(iView);
		iView->MakeVisible(EFalse);
		iView->SetFocus(EFalse);
		iSettings = list;
		iSettings->SetFocus(ETrue);
		}
	else
		{
		RemoveFromStack(iSettings);
		delete iSettings;
		iSettings = NULL;
		iView->SetRect(ClientRect());
		iView->MakeVisible(ETrue);
		AddToStackL(iView);
		iView->SetFocus(ETrue);
		}
	ShowSoftkeysL();
	ShowTitleL();
	if (iSettings)
		{
		iSettings->DrawDeferred();
		}
	else
		{
		Refresh();
		}
	}

// The settings list from the resource file loaded now, its rows filled from the disk,
// not yet on the control stack.
CNtpSettingList* CNtpUiAppUi::NewSettingListL()
	{
	CNtpSettingList* list = new (ELeave) CNtpSettingList(*this);
	CleanupStack::PushL(list);
	LoadSettingsL(list->Data()); // the rows take their values when they are made
	list->SetMopParent(this);
	list->ConstructFromResourceL(R_NTP_SETTING_LIST);
	list->SetRect(ClientRect());
	list->ActivateL();
	CleanupStack::Pop(list);
	return list;
	}

// The settings as they are on the disk now.
void CNtpUiAppUi::LoadSettingsL(TNtpSettingData& aData)
	{
	RFs& fs = iCoeEnv->FsSession();
	CLogger* log = CLogger::NewL(fs);
	CleanupStack::PushL(log);
	CNtpConfig* config = CNtpConfig::NewL(fs, *log);
	CleanupStack::PushL(config);
	RArray<TWlanIap> wlan;
	CleanupClosePushL(wlan);
	TRAPD(listErr, ListWlanIapsL(wlan));
	config->LoadL(wlan, listErr == KErrNone);

	aData.iIapsText.Zero();
	for (TInt i = 0; i < config->Iaps().Count(); ++i)
		{
		const TUint32 id = config->Iaps()[i];
		if (i > 0)
			{
			AppendFit(aData.iIapsText, _L(", "));
			}
		TInt found = -1;
		for (TInt j = 0; j < wlan.Count(); ++j)
			{
			if (wlan[j].iId == id)
				{
				found = j;
				break;
				}
			}
		if (found >= 0 && wlan[found].iName.Length() > 0)
			{
			AppendFit(aData.iIapsText, wlan[found].iName);
			}
		else
			{
			TBuf<16> num;
			num.Num(id);
			AppendFit(aData.iIapsText, num);
			}
		}
	if (aData.iIapsText.Length() == 0)
		{
		HBufC* none = iEikonEnv->AllocReadResourceLC(R_NTP_STR_SET_IAPS_NONE);
		AppendFit(aData.iIapsText, *none);
		CleanupStack::PopAndDestroy(none);
		}

	aData.iInterval = NtpIntervalIndex(config->IntervalMinutes());
	aData.iThreshold = NtpThresholdIndex(config->ThresholdMs());
	aData.iWatch = config->WatchClock();
	aData.iServers.Zero();
	for (TInt s = 0; s < config->ServerCount(); ++s)
		{
		if (s > 0)
			{
			const TText delimiter = CEditableText::EParagraphDelimiter;
			AppendFit(aData.iServers, TPtrC(&delimiter, 1));
			}
		AppendFit(aData.iServers, config->Server(s));
		}
	aData.iLanguage = NtpUiReadLanguage(fs);
	CleanupStack::PopAndDestroy(3, log); // wlan, config, log
	}

// The rows from the disk again (after a change, or one that failed).
void CNtpUiAppUi::RefreshSettingsL()
	{
	if (iSettings)
		{
		LoadSettingsL(iSettings->Data());
		iSettings->RefreshL();
		}
	}

// A row was changed (or, for the access points, asked to be edited). Always ends with
// the rows read from the disk; a failure is told.
void CNtpUiAppUi::SettingChangedL(TInt aId)
	{
	TRAPD(err, ApplySettingL(aId));
	if (err != KErrNone)
		{
		TRAP_IGNORE(NoteNumberL(ENoteError, R_NTP_STR_NOTE_SAVE_FAILED, err));
		}
	TRAP_IGNORE(RefreshSettingsL());
	}

// Saves the one setting that changed, on top of the ini as it is now (the daemon may
// have added an iap= line since the screen was filled), and tells the daemon.
void CNtpUiAppUi::ApplySettingL(TInt aId)
	{
	RFs& fs = iCoeEnv->FsSession();
	if (aId == ENtpSetLanguage)
		{
		NtpUiWriteLanguageL(fs, iSettings->Data().iLanguage);
		iLanguageCallBack->CallBack(); // after the list's EditItemL has returned
		return;
		}

	// The page for the access points, before the file is read.
	RArray<TUint32> picked;
	CleanupClosePushL(picked);
	if (aId == ENtpSetIaps && !PickIapsL(picked))
		{
		CleanupStack::PopAndDestroy(&picked);
		return;
		}

	const TNtpSettingData& d = iSettings->Data();
	CLogger* log = CLogger::NewL(fs);
	CleanupStack::PushL(log);
	CNtpConfig* config = CNtpConfig::NewL(fs, *log);
	CleanupStack::PushL(config);
	RArray<TWlanIap> wlan;
	CleanupClosePushL(wlan);
	TRAPD(listErr, ListWlanIapsL(wlan));
	config->LoadL(wlan, listErr == KErrNone);
	switch (aId)
		{
		case ENtpSetIaps:
			config->SetIapsL(picked);
			break;
		case ENtpSetInterval:
			config->SetIntervalMinutes(NtpIntervalMinutes(d.iInterval));
			break;
		case ENtpSetThreshold:
			config->SetThresholdMs(NtpThresholdMs(d.iThreshold));
			break;
		case ENtpSetWatch:
			config->SetWatch(d.iWatch);
			break;
		case ENtpSetServers:
			{
			// split on commas, semicolons and spaces; the first five
			const TInt KMaxServers = 5;
			config->ClearServers();
			TInt added = 0;
			TInt pos = 0;
			const TInt len = d.iServers.Length();
			while (pos < len && added < KMaxServers)
				{
				while (pos < len && IsServerSeparator(d.iServers[pos]))
					{
					++pos;
					}
				const TInt start = pos;
				while (pos < len && !IsServerSeparator(d.iServers[pos]))
					{
					++pos;
					}
				if (pos > start)
					{
					config->AddServerL(d.iServers.Mid(start, pos - start));
					++added;
					}
				}
			break;
			}
		default:
			break;
		}
	config->SaveL(wlan);
	CleanupStack::PopAndDestroy(4, &picked); // wlan, config, log, picked
	if (DaemonRunning())
		{
		SendCommand(ENtpCmdReload);
		}
	}

// The page of the phone's Wi-Fi access points (not Easy WLAN), those enabled now
// checked. True with the checked ones, in the order of the list, if the user accepts.
TBool CNtpUiAppUi::PickIapsL(RArray<TUint32>& aIaps)
	{
	RFs& fs = iCoeEnv->FsSession();
	CLogger* log = CLogger::NewL(fs);
	CleanupStack::PushL(log);
	CNtpConfig* config = CNtpConfig::NewL(fs, *log);
	CleanupStack::PushL(config);
	RArray<TWlanIap> wlan;
	CleanupClosePushL(wlan);
	TRAPD(listErr, ListWlanIapsL(wlan));
	config->LoadL(wlan, listErr == KErrNone);

	// the choices: no Easy WLAN
	RArray<TWlanIap> choices;
	CleanupClosePushL(choices);
	if (listErr == KErrNone)
		{
		for (TInt i = 0; i < wlan.Count(); ++i)
			{
			if (!IsEasyWlanIap(wlan[i].iId, wlan[i].iName))
				{
				choices.AppendL(wlan[i]);
				}
			}
		}
	if (choices.Count() == 0)
		{
		CleanupStack::PopAndDestroy(4, log); // choices, wlan, config, log
		NoteResL(ENoteInfo, R_NTP_STR_NOTE_NO_IAPS);
		return EFalse;
		}

	CSelectionItemList* items = new (ELeave) CSelectionItemList(8);
	CleanupStack::PushL(TCleanupItem(DestroySelectionItems, items));
	for (TInt i = 0; i < choices.Count(); ++i)
		{
		TBuf<KCommsDbSvrMaxFieldLength> name(choices[i].iName);
		if (name.Length() == 0)
			{
			name.Num(choices[i].iId);
			}
		CSelectableItem* item = new (ELeave) CSelectableItem(name,
			config->Iaps().Find(choices[i].iId) != KErrNotFound);
		CleanupStack::PushL(item);
		item->ConstructL(); // copies the name
		items->AppendL(item);
		CleanupStack::Pop(item);
		}

	CAknCheckBoxSettingPage* page = new (ELeave) CAknCheckBoxSettingPage(R_NTP_IAPS_PAGE, items);
	const TBool accepted = page->ExecuteLD(CAknSettingPage::EUpdateWhenAccepted);
	if (accepted)
		{
		for (TInt i = 0; i < items->Count(); ++i)
			{
			if (items->At(i)->SelectionStatus())
				{
				aIaps.AppendL(choices[i].iId);
				}
			}
		}
	CleanupStack::PopAndDestroy(5, log); // items, choices, wlan, config, log
	return accepted;
	}

// The Language row was changed and written (after the list's EditItemL has returned).
TInt CNtpUiAppUi::LanguageChanged(TAny* aSelf)
	{
	TRAP_IGNORE(static_cast<CNtpUiAppUi*>(aSelf)->ApplyLanguageL());
	return 0;
	}

// The texts swapped, then the screens made again with them: the softkeys, the title, the
// settings list on the same row, and the status screen. The menu reads its texts each
// time it opens.
void CNtpUiAppUi::ApplyLanguageL()
	{
	UseLanguageL(NtpUiReadLanguage(iCoeEnv->FsSession()));
	ShowSoftkeysL();
	ShowTitleL();
	if (iSettings)
		{
		// the new list first, so that a failure leaves the old one there
		CNtpSettingList* list = NewSettingListL();
		const TInt row = iSettings->CurrentRow();
		RemoveFromStack(iSettings);
		delete iSettings;
		iSettings = list;
		iSettings->SetCurrentRow(row);
		AddToStackL(iSettings);
		iSettings->SetFocus(ETrue);
		iSettings->DrawDeferred();
		}
	Refresh();
	}

// ---------------------------------------------------------------------------
// The daemon's state

TBool CNtpUiAppUi::DaemonRunning()
	{
	RMutex mutex;
	if (mutex.OpenGlobal(KDaemonMutex) != KErrNone)
		{
		return EFalse;
		}
	mutex.Close();
	return ETrue;
	}

void CNtpUiAppUi::RefreshL()
	{
	TPckgBuf<TNtpStatus> buf;
	if (RProperty::Get(KNtpSyncdCategory, KNtpSyncdStatusKey, buf) == KErrNone
		&& buf().iLayout == KNtpStatusLayout)
		{
		iStatus = buf();
		}
	else
		{
		Mem::FillZ(&iStatus, sizeof(iStatus));
		}
	iDaemonRunning = DaemonRunning();
	iView->ShowStatusL(iStatus, iDaemonRunning);
	}

void CNtpUiAppUi::Refresh()
	{
	TRAP_IGNORE(RefreshL());
	}

TInt CNtpUiAppUi::StatusChanged(TAny* aSelf)
	{
	static_cast<CNtpUiAppUi*>(aSelf)->Refresh();
	return 0;
	}

// ---------------------------------------------------------------------------
// Commands

void CNtpUiAppUi::SyncNow()
	{
	if (NtpShownState(iStatus, iDaemonRunning) == ENtpShownRunning)
		{
		SendCommand(ENtpCmdSyncNow);
		}
	}

// The rows show the result; only a failure to send is told.
void CNtpUiAppUi::SendCommand(TInt aCommand)
	{
	const TInt err = RProperty::Set(KNtpSyncdCategory, KNtpSyncdCommandKey,
		NtpCommandValue(aCommand));
	if (err != KErrNone)
		{
		TRAP_IGNORE(NoteNumberL(ENoteError, R_NTP_STR_NOTE_COMMAND_FAILED, err));
		}
	Refresh();
	}

// The last 6 KB of the log, newest line first.
void CNtpUiAppUi::ShowLogL()
	{
	RFs& fs = iCoeEnv->FsSession();
	RFile file;
	// the daemon appends with EFileShareAny
	if (file.Open(fs, KLogFile, EFileRead | EFileShareReadersOrWriters) != KErrNone)
		{
		NoteResL(ENoteInfo, R_NTP_STR_NOTE_NO_LOG);
		return;
		}
	CleanupClosePushL(file);
	TInt size = 0;
	User::LeaveIfError(file.Size(size));
	const TInt pos = Max(0, size - KLogTailBytes);
	HBufC8* raw = HBufC8::NewLC(KLogTailBytes);
	TPtr8 rawPtr = raw->Des();
	User::LeaveIfError(file.Read(pos, rawPtr, KLogTailBytes));
	TPtrC8 bytes(rawPtr);
	if (pos > 0)
		{
		// the first line is cut: drop it
		const TInt eol = bytes.Locate('\n');
		bytes.Set(eol == KErrNotFound ? TPtrC8() : bytes.Mid(eol + 1));
		}
	HBufC* wide = HBufC::NewLC(bytes.Length() + 1);
	TPtr widePtr = wide->Des();
	CnvUtfConverter::ConvertToUnicodeFromUtf8(widePtr, bytes);
	HBufC* text = ReverseLinesLC(*wide);
	if (text->Length() == 0)
		{
		CleanupStack::PopAndDestroy(4, &file); // text, wide, raw, file
		NoteResL(ENoteInfo, R_NTP_STR_NOTE_NO_LOG);
		return;
		}

	HBufC* title = iEikonEnv->AllocReadResourceLC(R_NTP_STR_LOG_TITLE);
	CAknMessageQueryDialog* dialog = CAknMessageQueryDialog::NewL(*text);
	dialog->PrepareLC(R_NTP_LOG_QUERY);
	dialog->SetHeaderTextL(*title);
	dialog->RunLD();
	CleanupStack::PopAndDestroy(5, &file); // title, text, wide, raw, file
	}

void CNtpUiAppUi::StopL()
	{
	CAknQueryDialog* query = CAknQueryDialog::NewL();
	if (!query->ExecuteLD(R_NTP_STOP_QUERY))
		{
		return;
		}
	SaveEnabled(EFalse);
	SendCommand(ENtpCmdStop);
	}

void CNtpUiAppUi::StartL()
	{
	SaveEnabled(ETrue);
	RProcess daemon;
	const TInt err = daemon.Create(KDaemonExe, KDaemonArg);
	if (err == KErrNone)
		{
		daemon.Resume();
		daemon.Close();
		NoteResL(ENoteConfirm, R_NTP_STR_NOTE_STARTED);
		}
	else
		{
		NoteNumberL(ENoteError, R_NTP_STR_NOTE_START_FAILED, err);
		}
	Refresh();
	}

// enabled=0/1 in ntpsyncd.ini, so the daemon stays off or comes back at the next boot.
// A failure is told, and the command goes on.
void CNtpUiAppUi::SaveEnabled(TBool aEnabled)
	{
	TRAPD(err, SaveEnabledL(aEnabled));
	if (err != KErrNone)
		{
		TRAP_IGNORE(NoteNumberL(ENoteError, R_NTP_STR_NOTE_SAVE_FAILED, err));
		}
	}

void CNtpUiAppUi::SaveEnabledL(TBool aEnabled)
	{
	RFs& fs = iCoeEnv->FsSession();
	CLogger* log = CLogger::NewL(fs);
	CleanupStack::PushL(log);
	CNtpConfig* config = CNtpConfig::NewL(fs, *log);
	CleanupStack::PushL(config);
	RArray<TWlanIap> iaps;
	CleanupClosePushL(iaps);
	TRAPD(listErr, ListWlanIapsL(iaps));
	config->LoadL(iaps, listErr == KErrNone);
	config->SetEnabled(aEnabled);
	config->SaveL(iaps);
	CleanupStack::PopAndDestroy(3, log); // iaps, config, log
	}
