/*
 * ntpsettinglist.cpp - the settings screen: a standard S60 settings list.
 */
#include "ntpsettinglist.h"

#include <akntextsettingpage.h>
#include <eikfrlb.h>
#include <txtetext.h>

#include "ntpsyncui.hrh"
#include "ntpuisettings.h"

namespace
	{
	// The choices, in the order of the texts in the resource file.
	const TInt KIntervalMinutes[] = { 15, 30, 60, 180, 360, 720, 1440, 10080 };
	const TInt KThresholdMs[] = { 100, 500, 1000, 2000, 5000 };

	TInt NearestIndex(const TInt* aChoices, TInt aCount, TInt aValue)
		{
		TInt best = 0;
		TInt bestDistance = KMaxTInt;
		for (TInt i = 0; i < aCount; ++i)
			{
			const TInt distance = Abs(aChoices[i] - aValue);
			if (distance < bestDistance)
				{
				bestDistance = distance;
				best = i;
				}
			}
		return best;
		}
	}

TInt NtpIntervalChoiceCount()
	{
	return sizeof(KIntervalMinutes) / sizeof(KIntervalMinutes[0]);
	}

TInt NtpIntervalMinutes(TInt aIndex)
	{
	return KIntervalMinutes[Min(Max(aIndex, 0), NtpIntervalChoiceCount() - 1)];
	}

TInt NtpIntervalIndex(TInt aMinutes)
	{
	return NearestIndex(KIntervalMinutes, NtpIntervalChoiceCount(), aMinutes);
	}

TInt NtpThresholdChoiceCount()
	{
	return sizeof(KThresholdMs) / sizeof(KThresholdMs[0]);
	}

TInt NtpThresholdMs(TInt aIndex)
	{
	return KThresholdMs[Min(Max(aIndex, 0), NtpThresholdChoiceCount() - 1)];
	}

TInt NtpThresholdIndex(TInt aMs)
	{
	return NearestIndex(KThresholdMs, NtpThresholdChoiceCount(), aMs);
	}

TNtpSettingData::TNtpSettingData()
	: iInterval(0), iThreshold(0), iWatch(ETrue), iLanguage(ENtpLangVietnamese)
	{
	}

TBool TNtpSettingData::Equals(const TNtpSettingData& aOther) const
	{
	return iIapsText == aOther.iIapsText
		&& iInterval == aOther.iInterval
		&& iThreshold == aOther.iThreshold
		&& !iWatch == !aOther.iWatch
		&& iServers == aOther.iServers
		&& iLanguage == aOther.iLanguage;
	}

TBool NtpIsServerSeparator(TText aChar)
	{
	return aChar == ',' || aChar == ';' || aChar == ' ' || aChar == '\t'
		|| aChar == '\n' || aChar == '\r'
		|| aChar == CEditableText::EParagraphDelimiter
		|| aChar == CEditableText::ELineBreak;
	}

namespace
	{
	// The page of the servers: the Enter key starts a new line instead of accepting it.
	class CNtpServersSettingPage : public CAknTextSettingPage
		{
	public:
		CNtpServersSettingPage(TInt aResourceId, TDes& aText, TInt aFlags)
			: CAknTextSettingPage(aResourceId, aText, aFlags)
			{
			}

		TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
			{
			if (aType == EEventKey && aKeyEvent.iCode == EKeyEnter)
				{
				return TextControl()->OfferKeyEventL(aKeyEvent, aType);
				}
			return CAknTextSettingPage::OfferKeyEventL(aKeyEvent, aType);
			}
		};

	// The servers row: one server per line in the page, joined with ", " in the list.
	class CNtpServersSettingItem : public CAknTextSettingItem
		{
	public:
		CNtpServersSettingItem(TInt aId, TDes& aText)
			: CAknTextSettingItem(aId, aText)
			{
			}

		~CNtpServersSettingItem()
			{
			delete iShown;
			}

		const TDesC& SettingTextL()
			{
			const TDesC& base = CAknTextSettingItem::SettingTextL();
			TBool hasSeparator = EFalse;
			for (TInt i = 0; i < base.Length(); ++i)
				{
				if (NtpIsServerSeparator(base[i]))
					{
					hasSeparator = ETrue;
					break;
					}
				}
			if (!hasSeparator)
				{
				return base;
				}
			// Each run of separators becomes ", "; none at the start or the end.
			HBufC* shown = HBufC::NewL(base.Length() * 2 + 2);
			TPtr out(shown->Des());
			TBool pending = EFalse;
			for (TInt i = 0; i < base.Length(); ++i)
				{
				if (NtpIsServerSeparator(base[i]))
					{
					pending = out.Length() > 0;
					}
				else
					{
					if (pending)
						{
						out.Append(_L(", "));
						pending = EFalse;
						}
					out.Append(base[i]);
					}
				}
			if (out.Length() == 0)
				{
				delete shown;
				return base;
				}
			delete iShown;
			iShown = shown;
			return *iShown;
			}

		// Like CAknTextSettingItem::EditItemL, with the page that takes line breaks.
		void EditItemL(TBool /*aCalledFromMenu*/)
			{
			CNtpServersSettingPage* page = new (ELeave) CNtpServersSettingPage(
				SettingPageResourceId(), InternalTextPtr(),
				SettingPageFlags() | CAknTextSettingPage::EZeroLengthAllowed);
			SetSettingPage(page);
			SettingPage()->SetSettingPageObserver(this);
			SetUpStandardSettingPageL();
			SettingPage()->ExecuteLD(CAknSettingPage::EUpdateWhenChanged);
			SetSettingPage(NULL);
			}

	private:
		HBufC* iShown;
		};
	}

CNtpSettingList::CNtpSettingList(MNtpSettingObserver& aObserver)
	: iObserver(aObserver)
	{
	}

// Each row is bound to its value in iData; the resource (r_ntp_setting_list) gives the
// names, the pages and the texts of the values.
CAknSettingItem* CNtpSettingList::CreateSettingItemL(TInt aId)
	{
	switch (aId)
		{
		case ENtpSetIaps:
			return new (ELeave) CAknTextSettingItem(aId, iData.iIapsText);
		case ENtpSetInterval:
			return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iData.iInterval);
		case ENtpSetThreshold:
			return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iData.iThreshold);
		case ENtpSetWatch:
			return new (ELeave) CAknBinaryPopupSettingItem(aId, iData.iWatch);
		case ENtpSetServers:
			return new (ELeave) CNtpServersSettingItem(aId, iData.iServers);
		case ENtpSetLanguage:
			return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iData.iLanguage);
		default:
			return NULL;
		}
	}

// The selection key on a row (an on/off row just toggles), or Options > Change. The
// access points row opens no page here: the app UI is asked to edit it. For the others
// the new value goes to iData, and to the app UI if it differs from the old one.
void CNtpSettingList::EditItemL(TInt aIndex, TBool aCalledFromMenu)
	{
	CAknSettingItemArray* items = SettingItemArray();
	const TInt index = items->ItemIndexFromVisibleIndex(aIndex);
	if (index < 0 || index >= items->Count())
		{
		return;
		}
	CAknSettingItem* item = items->At(index);
	if (item->Identifier() == ENtpSetIaps)
		{
		iObserver.SettingChangedL(ENtpSetIaps);
		return;
		}
	const TNtpSettingData before(iData);
	iEditing = ETrue;
	TRAPD(err, CAknSettingItemList::EditItemL(aIndex, aCalledFromMenu));
	iEditing = EFalse;
	User::LeaveIfError(err);
	item->StoreL();
	if (!iData.Equals(before))
		{
		iObserver.SettingChangedL(item->Identifier());
		}
	}

void CNtpSettingList::EditCurrentL()
	{
	const TInt current = ListBox()->CurrentItemIndex();
	if (current >= 0)
		{
		EditItemL(current, ETrue);
		}
	}

TInt CNtpSettingList::CurrentRow()
	{
	return ListBox() ? ListBox()->CurrentItemIndex() : -1;
	}

void CNtpSettingList::SetCurrentRow(TInt aRow)
	{
	if (ListBox() && aRow >= 0 && aRow < SettingItemArray()->Count())
		{
		ListBox()->SetCurrentItemIndex(aRow);
		ListBox()->ScrollToMakeItemVisible(aRow);
		}
	}

void CNtpSettingList::RefreshL()
	{
	if (iEditing)
		{
		return; // the page that is open has the row's value; it comes back through EditItemL
		}
	CAknSettingItemArray* items = SettingItemArray();
	for (TInt i = 0; i < items->Count(); ++i)
		{
		CAknSettingItem* item = items->At(i);
		item->LoadL();
		item->UpdateListBoxTextL();
		}
	ListBox()->DrawDeferred();
	}

void CNtpSettingList::SizeChanged()
	{
	if (ListBox())
		{
		ListBox()->SetRect(Rect());
		}
	}
