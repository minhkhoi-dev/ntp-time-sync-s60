/*
 * ntpstatusview.h - the main screen: read-only rows with what ntpsyncd reports.
 */
#ifndef NTPSTATUSVIEW_H
#define NTPSTATUSVIEW_H

#include <coecntrl.h>
#include <eiklbo.h>
#include <badesca.h>

#include "ntpshared.h"

class CAknDoubleStyleListBox;

/** What the "background sync" row shows. */
enum TNtpShownState
	{
	ENtpShownStopped = 0,
	ENtpShownSyncing,
	ENtpShownRunning
	};

/** Stopped if the daemon is not running or says so; else syncing or running. */
TNtpShownState NtpShownState(const TNtpStatus& aStatus, TBool aDaemonRunning);

/** Told when the selection key is pressed on the list. */
class MNtpStatusViewObserver
	{
public:
	virtual void StatusSelectedL() = 0;
	};

class CNtpStatusView : public CCoeControl, public MEikListBoxObserver
	{
public:
	static CNtpStatusView* NewL(const TRect& aRect, MNtpStatusViewObserver& aObserver);
	~CNtpStatusView();

	/** Rebuilds the rows from the status the daemon published. */
	void ShowStatusL(const TNtpStatus& aStatus, TBool aDaemonRunning);

	// From CCoeControl
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

private:
	explicit CNtpStatusView(MNtpStatusViewObserver& aObserver);
	void ConstructL(const TRect& aRect);

	// From CCoeControl
	TInt CountComponentControls() const;
	CCoeControl* ComponentControl(TInt aIndex) const;
	void SizeChanged();
	void FocusChanged(TDrawNow aDrawNow);
	// From MEikListBoxObserver
	void HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType);

private:
	MNtpStatusViewObserver& iObserver;
	CAknDoubleStyleListBox* iList;
	CDesCArrayFlat* iItems;
	};

#endif // NTPSTATUSVIEW_H
