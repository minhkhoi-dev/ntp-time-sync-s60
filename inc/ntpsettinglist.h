/*
 * ntpsettinglist.h - the settings screen: a standard S60 settings list of the values in
 * ntpsyncd.ini and the language of the app. The AppUi fills the data and applies a change.
 */
#ifndef NTPSETTINGLIST_H
#define NTPSETTINGLIST_H

#include <aknsettingitemlist.h>

/** The choices of the interval and threshold rows, and the nearest one to any value. */
TInt NtpIntervalChoiceCount();
TInt NtpIntervalMinutes(TInt aIndex);
TInt NtpIntervalIndex(TInt aMinutes);
TInt NtpThresholdChoiceCount();
TInt NtpThresholdMs(TInt aIndex);
TInt NtpThresholdIndex(TInt aMs);

/** A character that ends a server name in the servers text: , ; space, tab, any line break. */
TBool NtpIsServerSeparator(TText aChar);

/** The values the rows show and edit (the rows of TNtpSettingId). */
class TNtpSettingData
	{
public:
	TNtpSettingData();
	TBool Equals(const TNtpSettingData& aOther) const;

public:
	TBuf<256> iIapsText;   // the names of the enabled access points; read only in the list
	TInt iInterval;        // index of the interval choices
	TInt iThreshold;       // index of the threshold choices
	TBool iWatch;
	TBuf<256> iServers;    // the servers, one per paragraph (CEditableText::EParagraphDelimiter)
	TInt iLanguage;        // TNtpUiLanguage
	};

class MNtpSettingObserver
	{
public:
	/**
	 * aId (TNtpSettingId) was changed in the list and its new value is in the data. For
	 * ENtpSetIaps it is the request to edit the row: the list opens no page for it.
	 */
	virtual void SettingChangedL(TInt aId) = 0;
	};

class CNtpSettingList : public CAknSettingItemList
	{
public:
	explicit CNtpSettingList(MNtpSettingObserver& aObserver);
	TNtpSettingData& Data() { return iData; }
	/** Shows the data again (after the app UI changed it). Does nothing while a page is open. */
	void RefreshL();
	/** Options > Change: the page of the focused row. */
	void EditCurrentL();
	/** The focused row (-1 when none), and the focus put on a row. */
	TInt CurrentRow();
	void SetCurrentRow(TInt aRow);

	// From CAknSettingItemList
	void EditItemL(TInt aIndex, TBool aCalledFromMenu);

private:
	CAknSettingItem* CreateSettingItemL(TInt aId);
	void SizeChanged();

private:
	MNtpSettingObserver& iObserver;
	TNtpSettingData iData;
	TBool iEditing;
	};

#endif // NTPSETTINGLIST_H
