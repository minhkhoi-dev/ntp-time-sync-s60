/*
 * ntpuisettings.h - the app's own settings file, C:\private\E7A15D02\ui.ini.
 * Today it holds the language of the app's screens: "lang=vi" or "lang=en".
 */
#ifndef NTPUISETTINGS_H
#define NTPUISETTINGS_H

#include <e32base.h>
#include <f32file.h>

enum TNtpUiLanguage
	{
	ENtpLangVietnamese = 0,
	ENtpLangEnglish
	};

/** Reads the language; Vietnamese if the file or the line is missing. Never leaves. */
TInt NtpUiReadLanguage(RFs& aFs);

/** Writes ui.ini with the language, replacing the file. */
void NtpUiWriteLanguageL(RFs& aFs, TInt aLanguage);

#endif // NTPUISETTINGS_H
