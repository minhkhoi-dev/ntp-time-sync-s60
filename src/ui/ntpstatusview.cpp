/*
 * ntpstatusview.cpp - the main screen: read-only rows with what ntpsyncd reports.
 */
#include "ntpstatusview.h"

#include <aknlists.h>
#include <eikenv.h>
#include <eikclbd.h>
#include <eiksbfrm.h>
#include <ntpsyncui.rsg>

namespace
	{
	// A text of the resource file, in the language loaded now.
	HBufC* ReadTextLC(TInt aResourceId)
		{
		return CEikonEnv::Static()->AllocReadResourceLC(aResourceId);
		}

	void AddRowL(CDesCArray& aItems, TInt aTitleId, const TDesC& aValue)
		{
		HBufC* title = ReadTextLC(aTitleId);
		TBuf<300> item;
		item.Format(_L("\t%S\t%S\t"), title, &aValue);
		CleanupStack::PopAndDestroy(title);
		aItems.AppendL(item);
		}

	void AddRowTextL(CDesCArray& aItems, TInt aTitleId, TInt aValueId)
		{
		HBufC* value = ReadTextLC(aValueId);
		AddRowL(aItems, aTitleId, *value);
		CleanupStack::PopAndDestroy(value);
		}

	// "HH:MM" on today's date (local time), else "DD/MM HH:MM".
	void FormatTime(TDes& aOut, TInt64 aUtc)
		{
		const TTimeIntervalSeconds offset = User::UTCOffset();
		TTime when(aUtc);
		when += offset;
		TTime now;
		now.UniversalTime();
		now += offset;
		const TDateTime d = when.DateTime();
		const TDateTime n = now.DateTime();
		if (d.Year() == n.Year() && d.Month() == n.Month() && d.Day() == n.Day())
			{
			aOut.Format(_L("%02d:%02d"), d.Hour(), d.Minute());
			}
		else
			{
			aOut.Format(_L("%02d/%02d %02d:%02d"), d.Day() + 1, TInt(d.Month()) + 1,
				d.Hour(), d.Minute());
			}
		}

	// "+2.322 s" or "-0.215 s".
	void FormatOffset(TDes& aOut, TInt64 aUs)
		{
		const TUint sign = aUs < 0 ? '-' : '+';
		const TInt64 a = aUs < 0 ? -aUs : aUs;
		const TInt64 whole = a / 1000000;
		const TInt ms = static_cast<TInt>((a % 1000000) / 1000);
		aOut.Format(_L("%c%Ld.%03d s"), sign, whole, ms);
		}
	}

TNtpShownState NtpShownState(const TNtpStatus& aStatus, TBool aDaemonRunning)
	{
	if (!aDaemonRunning || aStatus.iState == ENtpStateStopped)
		{
		return ENtpShownStopped;
		}
	return aStatus.iState == ENtpStateSyncing ? ENtpShownSyncing : ENtpShownRunning;
	}

CNtpStatusView* CNtpStatusView::NewL(const TRect& aRect, MNtpStatusViewObserver& aObserver)
	{
	CNtpStatusView* self = new (ELeave) CNtpStatusView(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	CleanupStack::Pop(self);
	return self;
	}

CNtpStatusView::CNtpStatusView(MNtpStatusViewObserver& aObserver)
	: iObserver(aObserver)
	{
	}

void CNtpStatusView::ConstructL(const TRect& aRect)
	{
	CreateWindowL();
	iItems = new (ELeave) CDesCArrayFlat(8);
	iList = new (ELeave) CAknDoubleStyleListBox;
	iList->SetContainerWindowL(*this);
	iList->ConstructL(this, EAknListBoxSelectionList);
	iList->SetListBoxObserver(this);
	iList->Model()->SetItemTextArray(iItems);
	iList->Model()->SetOwnershipType(ELbmDoesNotOwnItemArray);
	iList->CreateScrollBarFrameL(ETrue);
	iList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff,
		CEikScrollBarFrame::EAuto);
	SetRect(aRect);
	ActivateL();
	}

CNtpStatusView::~CNtpStatusView()
	{
	delete iList;
	delete iItems;
	}

void CNtpStatusView::ShowStatusL(const TNtpStatus& aStatus, TBool aDaemonRunning)
	{
	const TNtpShownState shown = NtpShownState(aStatus, aDaemonRunning);
	iItems->Reset();
	TBuf<200> value;
	TBuf<40> time;
	TBuf<40> offset;

	// 1. background sync
	TInt stateId = R_NTP_STR_VAL_RUNNING;
	if (shown == ENtpShownStopped)
		{
		stateId = R_NTP_STR_VAL_STOPPED;
		}
	else if (shown == ENtpShownSyncing)
		{
		stateId = R_NTP_STR_VAL_SYNCING;
		}
	AddRowTextL(*iItems, R_NTP_STR_ROW_STATE, stateId);

	// 2. last good sync
	if (aStatus.iLastSuccessUtc == 0)
		{
		AddRowTextL(*iItems, R_NTP_STR_ROW_LAST, R_NTP_STR_VAL_NONE);
		}
	else
		{
		FormatTime(time, aStatus.iLastSuccessUtc);
		FormatOffset(offset, aStatus.iOffsetUs);
		if (aStatus.iClockResult == ENtpClockSetFailed)
			{
			HBufC* fmt = ReadTextLC(R_NTP_STR_VAL_SET_FAILED);
			value.Format(*fmt, &time, aStatus.iSetError);
			CleanupStack::PopAndDestroy(fmt);
			}
		else
			{
			const TInt id = (aStatus.iClockResult == ENtpClockCorrected)
				? R_NTP_STR_VAL_CORRECTED : R_NTP_STR_VAL_RIGHT;
			HBufC* fmt = ReadTextLC(id);
			value.Format(*fmt, &time, &offset);
			CleanupStack::PopAndDestroy(fmt);
			}
		AddRowL(*iItems, R_NTP_STR_ROW_LAST, value);
		}

	// 3. last attempt, only when it failed (decided by the error, not by the times)
	if (aStatus.iLastError != KErrNone && aStatus.iLastAttemptUtc != 0)
		{
		FormatTime(time, aStatus.iLastAttemptUtc);
		HBufC* fmt = NULL;
		if (aStatus.iNoWifi)
			{
			fmt = ReadTextLC(R_NTP_STR_VAL_NO_WIFI);
			value.Format(*fmt, &time);
			}
		else if (aStatus.iLastError == KErrCancel)
			{
			fmt = ReadTextLC(R_NTP_STR_VAL_CANCELLED);
			value.Format(*fmt, &time);
			}
		else
			{
			fmt = ReadTextLC(R_NTP_STR_VAL_FAILED);
			value.Format(*fmt, &time, aStatus.iLastError);
			}
		CleanupStack::PopAndDestroy(fmt);
		AddRowL(*iItems, R_NTP_STR_ROW_ATTEMPT, value);
		}

	if (aStatus.iLastSuccessUtc != 0)
		{
		// 4. server
		const TPtrC server(aStatus.Server());
		if (server.Length() > 0)
			{
			AddRowL(*iItems, R_NTP_STR_ROW_SERVER, server);
			}
		else
			{
			AddRowTextL(*iItems, R_NTP_STR_ROW_SERVER, R_NTP_STR_VAL_UNKNOWN);
			}

		// 5. Wi-Fi
		const TPtrC iapName(aStatus.IapName());
		if (aStatus.iIapId == 0)
			{
			AddRowTextL(*iItems, R_NTP_STR_ROW_WIFI, R_NTP_STR_VAL_UNKNOWN);
			}
		else if (iapName.Length() > 0)
			{
			AddRowL(*iItems, R_NTP_STR_ROW_WIFI, iapName);
			}
		else
			{
			HBufC* fmt = ReadTextLC(R_NTP_STR_VAL_IAP_ID);
			value.Format(*fmt, aStatus.iIapId);
			CleanupStack::PopAndDestroy(fmt);
			AddRowL(*iItems, R_NTP_STR_ROW_WIFI, value);
			}
		}

	// 6. next sync
	if (shown != ENtpShownStopped)
		{
		if (aStatus.iNextSyncUtc == 0)
			{
			AddRowTextL(*iItems, R_NTP_STR_ROW_NEXT, R_NTP_STR_VAL_NOT_SCHEDULED);
			}
		else
			{
			FormatTime(time, aStatus.iNextSyncUtc);
			AddRowL(*iItems, R_NTP_STR_ROW_NEXT, time);
			}
		}

	TInt current = iList->CurrentItemIndex();
	iList->HandleItemRemovalL();
	iList->HandleItemAdditionL();
	if (current < 0 || current >= iItems->Count())
		{
		current = 0;
		}
	iList->SetCurrentItemIndex(current);
	iList->DrawDeferred();
	}

TKeyResponse CNtpStatusView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
	{
	return iList->OfferKeyEventL(aKeyEvent, aType);
	}

TInt CNtpStatusView::CountComponentControls() const
	{
	return 1;
	}

CCoeControl* CNtpStatusView::ComponentControl(TInt /*aIndex*/) const
	{
	return iList;
	}

void CNtpStatusView::SizeChanged()
	{
	if (iList)
		{
		iList->SetRect(Rect());
		}
	}

void CNtpStatusView::FocusChanged(TDrawNow aDrawNow)
	{
	if (iList)
		{
		iList->SetFocus(IsFocused(), aDrawNow);
		}
	}

void CNtpStatusView::HandleListBoxEventL(CEikListBox* /*aListBox*/, TListBoxEvent aEventType)
	{
	if (aEventType == EEventEnterKeyPressed || aEventType == EEventItemDoubleClicked)
		{
		iObserver.StatusSelectedL();
		}
	}
