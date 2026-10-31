/*
 * propwatcher.h - calls back when a Publish & Subscribe property changes.
 */
#ifndef PROPWATCHER_H
#define PROPWATCHER_H

#include <e32base.h>
#include <e32property.h>

class CPropertyWatcher : public CActive
	{
public:
	static CPropertyWatcher* NewL(TUid aCategory, TUint aKey, const TCallBack& aCallBack);
	~CPropertyWatcher();
	/** For the owner to read the value in its callback. */
	RProperty& Property() { return iProperty; }
private:
	CPropertyWatcher(const TCallBack& aCallBack);
	void ConstructL(TUid aCategory, TUint aKey);
	void Subscribe();
	void RunL();
	void DoCancel();
private:
	TCallBack iCallBack;
	RProperty iProperty;
	};

#endif // PROPWATCHER_H
