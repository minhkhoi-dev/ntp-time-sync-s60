/*
 * ntppacket.h - SNTP (RFC 4330) packet building and reply validation.
 *
 * Pure arithmetic, no Symbian services, so it can be unit-tested on a PC.
 *
 * All times here are "NTP microseconds": microseconds since
 * 1900-01-01 00:00:00 UTC, held in a signed 64-bit integer.
 */
#ifndef NTPPACKET_H
#define NTPPACKET_H

#include <e32def.h>

const TInt KNtpPacketLength = 48;
const TUint KNtpPort = 123;

/** One accepted reply from a server. */
struct TNtpSample
	{
	TInt64  iOffsetUs;  ///< Add this to the local UTC clock to get server UTC.
	TInt64  iDelayUs;   ///< Network round-trip delay.
	TInt    iStratum;   ///< Server stratum (1..15).
	TUint32 iRefId;     ///< Reference ID (kiss code when stratum is 0).
	};

/** Why a reply was rejected, or ENtpOk. */
enum TNtpVerdict
	{
	ENtpOk = 0,
	ENtpTooShort,       ///< Fewer than 48 bytes.
	ENtpNotServerMode,  ///< Mode field is not 4 (server).
	ENtpBadVersion,     ///< Version is not 3 or 4.
	ENtpKissOfDeath,    ///< Stratum 0: server says go away (RATE, DENY, RSTR...).
	ENtpUnsynchronized, ///< Leap indicator 3 or stratum above 15.
	ENtpWrongOrigin,    ///< Not an answer to the request we just sent.
	ENtpBadTimestamp,   ///< Zero, out of order, or before KNtpMinValidUs.
	ENtpBadDelay        ///< Negative or absurd round-trip delay.
	};

/** Earliest server time we believe: 2026-01-01 00:00:00 UTC. */
const TInt64 KNtpMinValidUs = static_cast<TInt64>(3976214400U) * 1000000;

/** Convert NTP microseconds to the 32.32 wire format (wraps in 2036 as NTP does). */
void NtpUsToTimestamp(TInt64 aUs, TUint32& aSec, TUint32& aFrac);

/**
 * Convert a 32.32 wire timestamp to NTP microseconds.
 * Seconds values with the top bit clear are taken to be in NTP era 1
 * (after 2036-02-07), so the result is right from 1968 to 2104.
 */
TInt64 NtpTimestampToUs(TUint32 aSec, TUint32 aFrac);

/**
 * Fill a 48-byte client request. aT1 is the local send time; it is put in
 * the transmit field and returned in wire form so the reply can be matched.
 */
void NtpBuildRequest(TUint8* aPacket, TInt64 aT1, TUint32& aSentSec, TUint32& aSentFrac);

/**
 * Validate a reply and compute offset and delay.
 * aT1 = local time the request was sent, aT4 = local time the reply arrived.
 */
TNtpVerdict NtpParseReply(const TUint8* aPacket, TInt aLength,
                          TUint32 aSentSec, TUint32 aSentFrac,
                          TInt64 aT1, TInt64 aT4, TNtpSample& aSample);

/** Short name of a verdict, for the log. */
const char* NtpVerdictName(TNtpVerdict aVerdict);

#endif // NTPPACKET_H
