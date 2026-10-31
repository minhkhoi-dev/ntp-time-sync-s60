/*
 * Host unit tests for ntppacket.cpp. Build and run on Linux:
 *   g++ -std=c++98 -Wall -Wextra -Itest/shim -Iinc test/test_ntppacket.cpp src/ntppacket.cpp -o /tmp/t && /tmp/t
 */
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "ntppacket.h"

static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond) do { ++gChecks; if (!(cond)) { ++gFailures; \
	std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static const TInt64 KUs = 1000000;
static const TInt64 KUnixToNtp = 2208988800LL;

static TInt64 NtpUsFromUtc(int y, int mo, int d, int h, int mi, int s)
	{
	struct tm t = {};
	t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
	t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
	return (static_cast<TInt64>(timegm(&t)) + KUnixToNtp) * KUs;
	}

static void Put32(TUint8* p, TUint32 v)
	{
	p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
	}

/* Build the reply an honest server would send. */
static void MakeReply(TUint8* p, const TUint8* aRequest, TInt64 aT2, TInt64 aT3,
                      int li = 0, int vn = 4, int mode = 4, int stratum = 2)
	{
	for (int i = 0; i < KNtpPacketLength; ++i) p[i] = 0;
	p[0] = (li << 6) | (vn << 3) | mode;
	p[1] = stratum;
	for (int i = 0; i < 8; ++i) p[24 + i] = aRequest[40 + i]; // originate = our transmit
	TUint32 s, f;
	NtpUsToTimestamp(aT2, s, f); Put32(p + 32, s); Put32(p + 36, f);
	NtpUsToTimestamp(aT3, s, f); Put32(p + 40, s); Put32(p + 44, f);
	}

static TInt64 Abs(TInt64 v) { return v < 0 ? -v : v; }

int main()
	{
	// Constants line up with the calendar.
	CHECK(KNtpMinValidUs == NtpUsFromUtc(2026, 1, 1, 0, 0, 0));

	// Wire format round trips, including across the 2036 rollover.
	const TInt64 times[] = {
		NtpUsFromUtc(2009, 6, 1, 0, 0, 0) + 1,
		NtpUsFromUtc(2026, 10, 22, 15, 4, 5) + 123456,
		NtpUsFromUtc(2036, 2, 7, 6, 28, 15) + 999999,   // last second of era 0
		NtpUsFromUtc(2036, 2, 7, 6, 28, 16),            // first second of era 1
		NtpUsFromUtc(2050, 1, 1, 0, 0, 0) + 500000 };
	for (unsigned i = 0; i < sizeof(times) / sizeof(times[0]); ++i)
		{
		TUint32 s, f;
		NtpUsToTimestamp(times[i], s, f);
		CHECK(Abs(NtpTimestampToUs(s, f) - times[i]) <= 1);
		}
	{
	TUint32 s, f;
	NtpUsToTimestamp(NtpUsFromUtc(2036, 2, 7, 6, 28, 16), s, f);
	CHECK(s == 0 && f == 0); // the rollover really does wrap to zero on the wire
	NtpUsToTimestamp(NtpUsFromUtc(2026, 1, 1, 0, 0, 0), s, f);
	CHECK(s == 3976214400U);
	}

	TUint8 req[KNtpPacketLength];
	TUint8 rep[KNtpPacketLength];
	TUint32 sentSec, sentFrac;
	TNtpSample smp;

	// Request header: LI 0, VN 4, mode 3.
	const TInt64 server = NtpUsFromUtc(2026, 10, 22, 15, 0, 0);
	{
	const TInt64 t1 = server - 123456789; // phone is 123.456789 s slow
	NtpBuildRequest(req, t1, sentSec, sentFrac);
	CHECK(req[0] == 0x23);
	CHECK(req[1] == 0);
	// Symmetric 40 ms each way, 1 ms in the server.
	const TInt64 t2 = server + 40000;
	const TInt64 t3 = t2 + 1000;
	const TInt64 t4 = t1 + 40000 + 1000 + 40000;
	MakeReply(rep, req, t2, t3);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t4, smp) == ENtpOk);
	CHECK(Abs(smp.iOffsetUs - 123456789) <= 2);
	CHECK(Abs(smp.iDelayUs - 80000) <= 2);
	CHECK(smp.iStratum == 2);

	// Asymmetric path: 30 ms out, 70 ms back -> error is (30-70)/2 = -20 ms.
	const TInt64 t4b = t1 + 30000 + 1000 + 70000;
	const TInt64 t2b = server + 30000;
	MakeReply(rep, req, t2b, t2b + 1000);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t4b, smp) == ENtpOk);
	CHECK(Abs(smp.iOffsetUs - (123456789 - 20000)) <= 2);
	}

	// Phone clock ahead by 4 minutes (the NITZ case) -> negative offset.
	{
	const TInt64 t1 = server + 240 * KUs;
	NtpBuildRequest(req, t1, sentSec, sentFrac);
	MakeReply(rep, req, server + 20000, server + 20500);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1 + 40500, smp) == ENtpOk);
	CHECK(Abs(smp.iOffsetUs - (-240 * KUs)) <= 2);
	}

	// Phone reset to 2009 after a flat battery: still fixable.
	{
	const TInt64 t1 = NtpUsFromUtc(2009, 1, 1, 0, 0, 0);
	NtpBuildRequest(req, t1, sentSec, sentFrac);
	MakeReply(rep, req, server, server);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpOk);
	CHECK(Abs(smp.iOffsetUs - (server - t1)) <= 2);
	}

	// Server time after the 2036 rollover is read as 2037, not 1901.
	{
	const TInt64 t1 = server;
	const TInt64 later = NtpUsFromUtc(2037, 3, 1, 0, 0, 0);
	NtpBuildRequest(req, t1, sentSec, sentFrac);
	MakeReply(rep, req, later, later);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpOk);
	CHECK(Abs(smp.iOffsetUs - (later - t1)) <= 2);
	}

	// Rejections.
	{
	const TInt64 t1 = server;
	NtpBuildRequest(req, t1, sentSec, sentFrac);

	MakeReply(rep, req, server, server);
	CHECK(NtpParseReply(rep, 47, sentSec, sentFrac, t1, t1, smp) == ENtpTooShort);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac + 1, t1, t1, smp) == ENtpWrongOrigin);
	CHECK(NtpParseReply(rep, 68, sentSec, sentFrac, t1, t1, smp) == ENtpOk); // extension bytes ok

	MakeReply(rep, req, server, server, 0, 4, 3);       // client mode echo
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpNotServerMode);
	MakeReply(rep, req, server, server, 0, 2, 4);       // version 2
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpBadVersion);
	MakeReply(rep, req, server, server, 0, 3, 4);       // version 3 is fine
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpOk);

	MakeReply(rep, req, 0, 0, 3, 4, 4, 0);              // kiss-o'-death "RATE"
	rep[12] = 'R'; rep[13] = 'A'; rep[14] = 'T'; rep[15] = 'E';
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpKissOfDeath);
	CHECK(smp.iRefId == 0x52415445U);

	MakeReply(rep, req, server, server, 3);             // alarm: unsynchronized
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpUnsynchronized);
	MakeReply(rep, req, server, server, 0, 4, 4, 16);   // stratum 16
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpUnsynchronized);

	MakeReply(rep, req, server, server);
	for (int i = 40; i < 48; ++i) rep[i] = 0;           // zero transmit stamp
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpBadTimestamp);

	const TInt64 old = NtpUsFromUtc(2025, 12, 31, 23, 59, 59);
	MakeReply(rep, req, old, old);                      // server stuck in the past
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpBadTimestamp);

	MakeReply(rep, req, server + 1000, server);         // transmit before receive
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpBadTimestamp);

	MakeReply(rep, req, server, server);
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1 + 11 * KUs, smp) == ENtpBadDelay);
	MakeReply(rep, req, server, server + 500000);       // server says 0.5 s inside, we saw 0
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpBadDelay);
	MakeReply(rep, req, server, server + 50000);        // 50 ms tick jitter is tolerated
	CHECK(NtpParseReply(rep, 48, sentSec, sentFrac, t1, t1, smp) == ENtpOk);
	CHECK(smp.iDelayUs == 0);
	}

	std::printf("%d checks, %d failures\n", gChecks, gFailures);
	return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	}
