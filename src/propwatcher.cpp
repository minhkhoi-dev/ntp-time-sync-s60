/*
 * propwatcher.cpp - calls back when a Publish & Subscribe property changes.
 */
#include "propwatcher.h"

CPropertyWatcher* CPropertyWatcher::NewL(TUid aCategory, TUint aKey, const TCallBack& aCallBack)
	{
	CPropertyWatcher* self = new (ELeave) CPropertyWatcher(aCallBack);
	CleanupStack::PushL(self);
	self->ConstructL(aCategory, aKey);
	CleanupStack::Pop(self);
	return self;
	}

CPropertyWatcher::CPropertyWatcher(const TCallBack& aCallBack)
	: CActive(EPriorityStandard), iCallBack(aCallBack)
	{
	CActiveScheduler::Add(this);
	}

void CPropertyWatcher::ConstructL(TUid aCategory, TUint aKey)
	{
	User::LeaveIfError(iProperty.Attach(aCategory, aKey));
	Subscribe();
	}

CPropertyWatcher::~CPropertyWatcher()
	{
	Cancel();
	iProperty.Close();
	}

void CPropertyWatcher::Subscribe()
	{
	iProperty.Subscribe(iStatus);
	SetActive();
	}

void CPropertyWatcher::RunL()
	{
	const TInt status = iStatus.Int();
	Subscribe();
	if (status == KErrNone)
		{
		iCallBack.CallBack();
		}
	}

void CPropertyWatcher::DoCancel()
	{
	iProperty.Cancel();
	}
