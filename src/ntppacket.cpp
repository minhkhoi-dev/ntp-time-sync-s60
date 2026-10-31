/*
 * ntppacket.cpp - SNTP (RFC 4330) packet building and reply validation.
 */
#include "ntppacket.h"

namespace
	{
	const TInt64 KUsPerSec = 1000000;
	const TInt64 KEraSeconds = static_cast<TInt64>(1) << 32;

	// Allow this much negative delay before rejecting: the phone clock only
	// ticks every few milliseconds, so T4 - T1 can come out a little short.
	const TInt64 KDelayToleranceUs = 100000;
	// Anything slower than this is not a usable sample.
	const TInt64 KMaxDelayUs = 10 * KUsPerSec;

	TUint32 Get32(const TUint8* aP)
		{
		return (static_cast<TUint32>(aP[0]) << 24) | (static_cast<TUint32>(aP[1]) << 16)
		     | (static_cast<TUint32>(aP[2]) << 8)  |  static_cast<TUint32>(aP[3]);
		}

	void Put32(TUint8* aP, TUint32 aV)
		{
		aP[0] = static_cast<TUint8>(aV >> 24);
		aP[1] = static_cast<TUint8>(aV >> 16);
		aP[2] = static_cast<TUint8>(aV >> 8);
		aP[3] = static_cast<TUint8>(aV);
		}
	}

void NtpUsToTimestamp(TInt64 aUs, TUint32& aSec, TUint32& aFrac)
	{
	if (aUs < 0)
		{
		aUs = 0;
		}
	const TInt64 sec = aUs / KUsPerSec;
	const TUint64 rem = static_cast<TUint64>(aUs % KUsPerSec);
	aSec = static_cast<TUint32>(sec & 0xFFFFFFFF);
	aFrac = static_cast<TUint32>((rem << 32) / static_cast<TUint64>(KUsPerSec));
	}

TInt64 NtpTimestampToUs(TUint32 aSec, TUint32 aFrac)
	{
	TInt64 sec = aSec;
	if ((aSec & 0x80000000U) == 0)
		{
		sec += KEraSeconds; // era 1: 2036-02-07 onwards
		}
	const TInt64 fracUs = static_cast<TInt64>(
		(static_cast<TUint64>(aFrac) * static_cast<TUint64>(KUsPerSec)) >> 32);
	return sec * KUsPerSec + fracUs;
	}

void NtpBuildRequest(TUint8* aPacket, TInt64 aT1, TUint32& aSentSec, TUint32& aSentFrac)
	{
	for (TInt i = 0; i < KNtpPacketLength; ++i)
		{
		aPacket[i] = 0;
		}
	aPacket[0] = (0 << 6) | (4 << 3) | 3; // LI 0, version 4, mode 3 (client)
	NtpUsToTimestamp(aT1, aSentSec, aSentFrac);
	Put32(aPacket + 40, aSentSec);
	Put32(aPacket + 44, aSentFrac);
	}

TNtpVerdict NtpParseReply(const TUint8* aPacket, TInt aLength,
                          TUint32 aSentSec, TUint32 aSentFrac,
                          TInt64 aT1, TInt64 aT4, TNtpSample& aSample)
	{
	if (aLength < KNtpPacketLength)
		{
		return ENtpTooShort;
		}

	const TInt li = aPacket[0] >> 6;
	const TInt version = (aPacket[0] >> 3) & 7;
	const TInt mode = aPacket[0] & 7;
	aSample.iStratum = aPacket[1];
	aSample.iRefId = Get32(aPacket + 12);

	if (mode != 4)
		{
		return ENtpNotServerMode;
		}
	if (version < 3 || version > 4)
		{
		return ENtpBadVersion;
		}
	if (aSample.iStratum == 0)
		{
		return ENtpKissOfDeath;
		}
	if (li == 3 || aSample.iStratum > 15)
		{
		return ENtpUnsynchronized;
		}

	// The server copies our transmit stamp into its originate field.
	if (Get32(aPacket + 24) != aSentSec || Get32(aPacket + 28) != aSentFrac)
		{
		return ENtpWrongOrigin;
		}

	const TUint32 rxSec = Get32(aPacket + 32);
	const TUint32 rxFrac = Get32(aPacket + 36);
	const TUint32 txSec = Get32(aPacket + 40);
	const TUint32 txFrac = Get32(aPacket + 44);
	if ((rxSec == 0 && rxFrac == 0) || (txSec == 0 && txFrac == 0))
		{
		return ENtpBadTimestamp;
		}

	const TInt64 t2 = NtpTimestampToUs(rxSec, rxFrac);
	const TInt64 t3 = NtpTimestampToUs(txSec, txFrac);
	if (t3 < t2 || t3 < KNtpMinValidUs)
		{
		return ENtpBadTimestamp;
		}

	TInt64 delay = (aT4 - aT1) - (t3 - t2);
	if (delay < -KDelayToleranceUs || delay > KMaxDelayUs)
		{
		return ENtpBadDelay;
		}
	if (delay < 0)
		{
		delay = 0;
		}

	aSample.iDelayUs = delay;
	aSample.iOffsetUs = ((t2 - aT1) + (t3 - aT4)) / 2;
	return ENtpOk;
	}

const char* NtpVerdictName(TNtpVerdict aVerdict)
	{
	switch (aVerdict)
		{
		case ENtpOk:             return "ok";
		case ENtpTooShort:       return "reply too short";
		case ENtpNotServerMode:  return "not a server reply";
		case ENtpBadVersion:     return "bad NTP version";
		case ENtpKissOfDeath:    return "kiss-o'-death";
		case ENtpUnsynchronized: return "server not synchronized";
		case ENtpWrongOrigin:    return "reply does not match request";
		case ENtpBadTimestamp:   return "bad server timestamp";
		case ENtpBadDelay:       return "bad round-trip delay";
		}
	return "unknown";
	}
