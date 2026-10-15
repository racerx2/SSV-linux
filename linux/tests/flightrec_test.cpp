/****************************************************************************
  Space Shuttle Vessel - Linux port, modified by racerx2, 2026
  Licensed under the GNU General Public License version 2 or any later version (see SSV-LICENSE.txt)
  **************************************************************************/
// not upstream: tests of the flight recorder support (design ssv-flightrec), with Orbiter's recorder calls stubbed
#include "FlightRecorder.h"
#include "CrewControls.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; fprintf (stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); } } while (0)

// the Orbiter calls FlightRecorder.cpp makes
static bool recording = false, playback = false;
static std::vector<std::pair<std::string, std::string>> events;
static int logs = 0;

VESSEL::VESSEL (OBJHANDLE, int) : vessel (nullptr), flightmodel (1), version (0) {}
bool VESSEL::Recording () const { return recording; }
bool VESSEL::Playback () const { return playback; }
void VESSEL::RecordEvent (const char *event_type, const char *event) const { events.emplace_back (event_type, event); }
char *VESSEL::GetName () const { static char name[] = "test"; return name; }
double oapiRand () { return 0.25; }
void oapiWriteLogV (const char *, ...) { logs++; }

static void Tokens ()
{
	const std::string line = flightrec::Quote ("Panel A6U") + " " + flightrec::Quote ("SENSE") + " \t[1]";
	const char *p = line.c_str ();
	std::string a, b;
	CHECK (flightrec::NextToken (p, a) && a == "Panel A6U");
	CHECK (flightrec::NextToken (p, b) && b == "SENSE");
	CHECK (!strcmp (flightrec::SkipBlanks (p), "[1]"));
	CHECK (flightrec::NextToken (p, a) && a == "[1]" && !flightrec::NextToken (p, a));
	p = "  GEAR\tARM";
	CHECK (flightrec::NextToken (p, a) && a == "GEAR" && flightrec::NextToken (p, b) && b == "ARM");
	p = "\"\" x";
	CHECK (flightrec::NextToken (p, a) && a.empty () && flightrec::NextToken (p, b) && b == "x");
	p = "\"open";
	CHECK (!flightrec::NextToken (p, a));
	p = " \t";
	CHECK (!flightrec::NextToken (p, a));
	CHECK (flightrec::OneLine ("a\nb\rc\td\x7f" "e") == "a b c d e");
}

static void Recorder ()
{
	VESSEL v (nullptr), w (nullptr), x (nullptr);
	flightrec::Draw a (0.8);
	{
		flightrec::Recorder r (&v);
		r.Step ();
		CHECK (events.empty () && !r.JustStarted ());
		r.Record (flightrec::CMD, "GEAR ARM");
		CHECK (events.empty ());

		recording = true;
		r.Step ();
		CHECK (!r.JustStarted () && events.empty ());
		r.Step ();
		CHECK (r.JustStarted () && events.size () == 2 && events[0].first == "SSV_NOMINAL" && events[0].second == "2");
		CHECK (events[1].first == "SSV_DRAWS" && events[1].second == "1 0 0.80000000000000004");
		r.Step ();
		CHECK (!r.JustStarted () && events.size () == 2);
		r.Record (flightrec::CMD, "PLBCAM\n1 0 0 0");
		CHECK (events.size () == 3 && events[2].first == "SSV_CMD" && events[2].second == "PLBCAM 1 0 0 0");
		recording = false;
		r.Step ();
		recording = true;
		r.Step ();
		r.Step ();
		CHECK (r.JustStarted () && events.size () == 5 && events[3].first == "SSV_NOMINAL" && events[4].first == "SSV_DRAWS");
		recording = false;
		events.clear ();

		CHECK (r.OnPlaybackEvent ("SSV_NOMINAL", "1") && !r.Locked () && (a.Get () == 0.8));
		CHECK (flightrec::Rand () == 0.25 && a.Get () == 0.8);

		playback = true;
		flightrec::Recorder s (&w), t (&x);
		CHECK (a.Get () == 0.8);
		CHECK (r.OnPlaybackEvent ("SSV_NOMINAL", " 1") && r.Locked () && (a.Get () == 0.5));
		CHECK (r.OnPlaybackEvent ("SSV_NOMINAL", "1") && r.Locked ());
		CHECK (s.OnPlaybackEvent ("ssv_nominal", "1") && s.Locked ());
		CHECK (t.OnPlaybackEvent ("SSV_NOMINAL", "3") && !t.Locked ());
		CHECK (flightrec::Rand () == 0.5 && a.Get () == 0.5);
		CHECK (r.OnPlaybackEvent ("SSV_DRAWS", "1 0 0.25") && r.OnPlaybackEvent ("SSV_DRAW", "0 0.25") && a.Get () == 0.5);
		CHECK (r.OnPlaybackEvent ("SSV_EVENT", "LIFTOFF") && r.OnPlaybackEvent ("SSV_CW", "x") && r.OnPlaybackEvent ("SSV_MA", "ON"));
		CHECK (!r.OnPlaybackEvent ("SSV_VC", "\"A\" \"B\" [1]") && !r.OnPlaybackEvent ("SSV_CMD", "GEAR ARM"));

		playback = false;
		r.Step ();
		CHECK (!r.Locked () && s.Locked () && (a.Get () == 0.5));
	}
	CHECK ((a.Get () == 0.8) && flightrec::Rand () == 0.25);
}

// the Sensor_Press[X] = Sensor (0, 5000) pattern
struct Holder
{
	flightrec::Draw d;
	explicit Holder (double u = 0.5) : d (u) {}
};

static const std::pair<std::string, std::string> *Find (const char *type, size_t n = 0)
{
	for (const auto &e : events) if ((e.first == type) && (n-- == 0)) return &e;
	return nullptr;
}

static int Values (const std::string &s)
{
	int n = -2;
	for (const char *p = s.c_str (); *p; p++) n += ((*p != ' ') && ((p == s.c_str ()) || (p[-1] == ' ')));
	return n;
}

static void Draws ()
{
	VESSEL v (nullptr);
	recording = playback = false;
	events.clear ();

	flightrec::Draw d0 (0.125);
	auto gone = std::make_unique<flightrec::Draw> (0.25);
	flightrec::Draw d2 (0.375);
	gone.reset ();
	Holder h[2];
	h[1] = Holder (0.625);
	flightrec::Draw c (d2);
	c = d0;
	CHECK (d0.Get () == 0.125 && d2.Get () == 0.375 && h[1].d.Get () == 0.625 && c.Get () == 0.125);
	{
		flightrec::Recorder r (&v);
		recording = true;
		r.Step ();
		r.Step ();
		CHECK (events.size () == 2 && Find ("SSV_DRAWS") && Find ("SSV_DRAWS")->second == "5 0 0.125 0.375 0.5 0.625 0.125");
		d2.Redraw (r);
		CHECK (d2.Get () == 0.25 && events.size () == 3 && events[2].first == "SSV_DRAW" && events[2].second == "1 0.25");
		recording = false;
		d2.Redraw (r);
		CHECK (d2.Get () == 0.25 && events.size () == 3);
	}

	std::vector<std::unique_ptr<flightrec::Draw>> more;
	for (int i = 0; i < 20; i++) more.push_back (std::make_unique<flightrec::Draw> (0.75));
	events.clear ();
	{
		flightrec::Recorder r (&v);
		recording = true;
		r.Step ();
		r.Step ();
		const auto *l0 = Find ("SSV_DRAWS", 0), *l1 = Find ("SSV_DRAWS", 1);
		CHECK (events.size () == 3 && l0 && l1 && !Find ("SSV_DRAWS", 2));
		CHECK (l0 && l0->second.compare (0, 5, "25 0 ") == 0 && Values (l0->second) == 16);
		CHECK (l1 && l1->second.compare (0, 6, "25 16 ") == 0 && Values (l1->second) == 9);
		recording = false;
	}

	std::string line0 = "25 0", line1 = "25 16";
	for (int i = 0; i < 16; i++) line0 += " 0.875";
	for (int i = 16; i < 25; i++) line1 += " 0.875";
	events.clear ();
	logs = 0;
	{
		flightrec::Recorder q (&v);
		playback = true;
		CHECK (d0.Get () == 0.125);
		q.Step ();
		CHECK (d0.Get () == 0.5 && more[19]->Get () == 0.5);
		CHECK (q.OnPlaybackEvent ("SSV_DRAWS", line0.c_str ()) && d0.Get () == 0.5);
		CHECK (q.OnPlaybackEvent ("SSV_NOMINAL", "2") && q.Locked () && d0.Get () == 0.5);
		d2.Redraw (q);
		CHECK (d2.Get () == 0.5 && events.empty ());
		CHECK (q.OnPlaybackEvent ("SSV_DRAWS", line0.c_str ()) && d0.Get () == 0.5 && more[19]->Get () == 0.5);
		CHECK (q.OnPlaybackEvent ("SSV_DRAWS", line1.c_str ()) && d0.Get () == 0.875 && d2.Get () == 0.875 && more[19]->Get () == 0.875);
		d0.Redraw (q);
		CHECK (q.OnPlaybackEvent ("SSV_DRAW", "0 0.0625") && d0.Get () == 0.0625);
		d0.Redraw (q);
		CHECK (d0.Get () == 0.0625 && events.empty () && flightrec::Rand () == 0.5);
		CHECK (q.OnPlaybackEvent ("SSV_DRAW", "25 0.5") && q.OnPlaybackEvent ("SSV_DRAW", "x"));

		playback = false;
		q.Step ();
		CHECK (!q.Locked () && d0.Get () == 0.0625 && d2.Get () == 0.875);
		const int before = logs;

		playback = true;
		q.Step ();
		CHECK (q.OnPlaybackEvent ("SSV_NOMINAL", "2") && q.Locked () && d2.Get () == 0.5);
		CHECK (q.OnPlaybackEvent ("SSV_DRAWS", "24 0 0.25") && d2.Get () == 0.5 && logs == before + 2);
		CHECK (q.OnPlaybackEvent ("SSV_DRAWS", line1.c_str ()) && q.OnPlaybackEvent ("SSV_DRAW", "1 0.25") && d2.Get () == 0.5 && logs == before + 2);
		playback = false;
		q.Step ();
		CHECK (d0.Get () == 0.0625 && d2.Get () == 0.875);
	}
	CHECK (d0.Get () == 0.0625);
	events.clear ();
}

static void Log ()
{
	logs = 0;
	flightrec::LogUnknown ("SSV_OV", "SSV_VC", "\"A\" \"B\"");
	flightrec::LogUnknown ("SSV_OV", "SSV_VC", "\"A\" \"B\"");
	flightrec::LogUnknown ("SSV_OV", "SSV_CMD", "\"A\" \"B\"");
	CHECK (logs == 2);
}

static bool Same (const CrewControls &a, const CrewControls &b)
{
	return !memcmp (a.rhc, b.rhc, sizeof (a.rhc)) && !memcmp (a.thc, b.thc, sizeof (a.thc)) && !memcmp (a.trim, b.trim, sizeof (a.trim)) &&
		a.rpta == b.rpta && !memcmp (a.sbtc, b.sbtc, sizeof (a.sbtc)) && a.sbtcto[0] == b.sbtcto[0] && a.sbtcto[1] == b.sbtcto[1] &&
		!memcmp (a.brake, b.brake, sizeof (a.brake)) && !memcmp (a.rmsrhc, b.rmsrhc, sizeof (a.rmsrhc)) &&
		!memcmp (a.rmsthc, b.rmsthc, sizeof (a.rmsthc)) && a.rmssj == b.rmssj;
}

static void Controls ()
{
	CrewControls c;
	c.rhc[0][0] = 0.25; c.rhc[1][1] = -0.5; c.rhc[2][2] = 1.0;
	c.thc[0][1] = -1; c.thc[1][2] = 1;
	c.trim[1][0] = 1;
	c.rpta = -0.125;
	c.sbtc[0] = 0.75; c.sbtcto[1] = true;
	c.brake[0][1] = 0.5; c.brake[1][0] = 1.0;
	c.rmsrhc[2] = -0.25; c.rmsthc[0] = 0.5;
	c.rmssj = -1;
	const std::string s = FormatCrewControls (c);
	int n = 0;
	for (const char *p = s.c_str (); *p; p++) n += (*p == ' ');
	CHECK (n == 34 && s.find (',') == std::string::npos);
	CrewControls d;
	CHECK (ParseCrewControls (s.c_str (), d) && Same (c, d));
	CHECK (FormatCrewControls (d) == s);

	CrewControls e = d;
	CHECK (!ParseCrewControls (s.substr (0, s.rfind (' ')).c_str (), e) && Same (e, d));
	CHECK (!ParseCrewControls (("1.5" + s.substr (s.find (' '))).c_str (), e) && Same (e, d));
	CHECK (!ParseCrewControls (nullptr, e) && !ParseCrewControls ("", e));

	CrewControls f;
	f.rhc[0][0] = 0.123456;
	CHECK (ParseCrewControls (FormatCrewControls (f).c_str (), e) && !CrewControlsDiffer (f, e) && e.rhc[0][0] != f.rhc[0][0]);

	CrewControls a, b;
	CHECK (!CrewControlsDiffer (a, b));
	b.rhc[1][2] = 0.004;
	CHECK (!CrewControlsDiffer (a, b));
	b.rhc[1][2] = 0.006;
	CHECK (CrewControlsDiffer (a, b));
	a.rhc[1][2] = 0.003; b.rhc[1][2] = 0.0;
	CHECK (CrewControlsDiffer (a, b));
	a.rhc[1][2] = 0.998; b.rhc[1][2] = 1.0;
	CHECK (CrewControlsDiffer (a, b));
	a.rhc[1][2] = b.rhc[1][2] = 0.0;
	b.sbtc[1] = 0.997;
	CHECK (!CrewControlsDiffer (a, b));
	b.sbtc[1] = 1.0; b.thc[1][0] = 1;
	CHECK (CrewControlsDiffer (a, b));
	b.thc[1][0] = 0; b.sbtcto[0] = true;
	CHECK (CrewControlsDiffer (a, b));
	b.sbtcto[0] = false; b.rmssj = 1;
	CHECK (CrewControlsDiffer (a, b));
}

int main ()
{
	Tokens ();
	Recorder ();
	Draws ();
	Log ();
	Controls ();
	printf ("%d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
