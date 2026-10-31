/*
 * logger.h - appends timestamped lines to C:\Data\ntpsyncd.log.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <e32base.h>
#include <f32file.h>

class CLogger : public CBase
	{
public:
	static CLogger* NewL(RFs& aFs);

	/** printf-style line, prefixed with local date and time. Never leaves. */
	void Log(TRefByValue<const TDesC> aFmt, ...);

private:
	explicit CLogger(RFs& aFs);
	void Write(const TDesC8& aData);

private:
	RFs& iFs;
	};

#endif // LOGGER_H
