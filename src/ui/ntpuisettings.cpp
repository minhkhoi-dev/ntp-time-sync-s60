/*
 * ntpuisettings.cpp - the app's own settings file, C:\private\E7A15D02\ui.ini.
 */
#include "ntpuisettings.h"

namespace
	{
	_LIT8(KLangKey, "lang");
	_LIT8(KLangVi, "lang=vi\r\n");
	_LIT8(KLangEn, "lang=en\r\n");
	const TInt KMaxIniSize = 512;

	// C:\private\E7A15D02\ui.ini; the private folder is made when aMake is set.
	TInt IniName(RFs& aFs, TDes& aName, TBool aMake)
		{
		_LIT(KIni, "ui.ini");
		if (aMake)
			{
			const TInt err = aFs.CreatePrivatePath(EDriveC);
			if (err != KErrNone && err != KErrAlreadyExists)
				{
				return err;
				}
			}
		TFileName path;
		const TInt err = aFs.PrivatePath(path);
		if (err != KErrNone)
			{
			return err;
			}
		aName.Copy(_L("C:"));
		aName.Append(path);
		aName.Append(KIni);
		return KErrNone;
		}
	}

TInt NtpUiReadLanguage(RFs& aFs)
	{
	TFileName name;
	if (IniName(aFs, name, EFalse) != KErrNone)
		{
		return ENtpLangVietnamese;
		}
	RFile file;
	if (file.Open(aFs, name, EFileRead | EFileShareReadersOrWriters) != KErrNone)
		{
		return ENtpLangVietnamese;
		}
	TBuf8<KMaxIniSize> text;
	const TInt err = file.Read(text);
	file.Close();
	if (err != KErrNone)
		{
		return ENtpLangVietnamese;
		}

	TInt language = ENtpLangVietnamese;
	TInt pos = 0;
	while (pos < text.Length())
		{
		TPtrC8 rest(text.Mid(pos));
		TInt eol = rest.Locate('\n');
		if (eol == KErrNotFound)
			{
			eol = rest.Length();
			}
		TPtrC8 line(rest.Left(eol));
		pos += eol + 1;
		TBuf8<64> work;
		work.Copy(line.Left(work.MaxLength()));
		work.TrimAll();
		const TInt eq = work.Locate('=');
		if (eq <= 0)
			{
			continue;
			}
		TBuf8<16> key;
		key.Copy(work.Left(Min(eq, key.MaxLength())));
		key.TrimAll();
		TBuf8<16> value;
		value.Copy(work.Mid(eq + 1).Left(value.MaxLength()));
		value.TrimAll();
		if (key.CompareF(KLangKey) == 0)
			{
			language = (value.CompareF(_L8("en")) == 0) ? ENtpLangEnglish : ENtpLangVietnamese;
			}
		}
	return language;
	}

void NtpUiWriteLanguageL(RFs& aFs, TInt aLanguage)
	{
	TFileName name;
	User::LeaveIfError(IniName(aFs, name, ETrue));
	RFile file;
	User::LeaveIfError(file.Replace(aFs, name, EFileWrite | EFileShareExclusive));
	CleanupClosePushL(file);
	User::LeaveIfError(file.Write(aLanguage == ENtpLangEnglish ? KLangEn() : KLangVi()));
	CleanupStack::PopAndDestroy(&file);
	}
