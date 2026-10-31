/*
 * logger.cpp - appends timestamped lines to C:\Data\ntpsyncd.log.
 * When the log passes 64 KB it is moved to ntpsyncd.old.log.
 */
#include "logger.h"
#include <utf.h>

_LIT(KLogDir, "C:\\Data\\");
_LIT(KLogFile, "C:\\Data\\ntpsyncd.log");
_LIT(KOldLogFile, "C:\\Data\\ntpsyncd.old.log");
const TInt KMaxLogSize = 64 * 1024;

namespace
	{
	/** Silently cut lines that are too long instead of panicking. */
	class TTruncate : public TDes16Overflow
		{
	public:
		void Overflow(TDes16& /*aDes*/) {}
		};
	}

CLogger* CLogger::NewL(RFs& aFs)
	{
	CLogger* self = new (ELeave) CLogger(aFs);
	aFs.MkDirAll(KLogDir); // usually exists already
	return self;
	}

CLogger::CLogger(RFs& aFs)
	: iFs(aFs)
	{
	}

void CLogger::Log(TRefByValue<const TDesC> aFmt, ...)
	{
	TBuf<320> line;
	TTime now;
	now.HomeTime();
	const TDateTime dt = now.DateTime();
	line.Format(_L("%04d-%02d-%02d %02d:%02d:%02d  "),
		dt.Year(), TInt(dt.Month()) + 1, dt.Day() + 1,
		dt.Hour(), dt.Minute(), dt.Second());

	VA_LIST list;
	VA_START(list, aFmt);
	TTruncate overflow;
	line.AppendFormatList(aFmt, list, &overflow);
	VA_END(list);

	TBuf8<1000> out;
	CnvUtfConverter::ConvertFromUnicodeToUtf8(out, line);
	if (out.Length() > out.MaxLength() - 2)
		{
		out.SetLength(out.MaxLength() - 2);
		}
	out.Append(_L8("\r\n"));
	Write(out);
	}

void CLogger::Write(const TDesC8& aData)
	{
	TEntry entry;
	if (iFs.Entry(KLogFile, entry) == KErrNone && entry.iSize > KMaxLogSize)
		{
		iFs.Replace(KLogFile, KOldLogFile);
		}

	RFile file;
	TInt err = file.Open(iFs, KLogFile, EFileWrite | EFileShareAny);
	if (err == KErrNotFound)
		{
		err = file.Create(iFs, KLogFile, EFileWrite | EFileShareAny);
		}
	if (err != KErrNone)
		{
		return;
		}
	TInt pos = 0;
	file.Seek(ESeekEnd, pos);
	file.Write(aData);
	file.Close();
	}
