#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <vector>
using std::vector;
#include <algorithm>

const bool USECOLOR = isatty(1);

// Size of character buffers that we're going to snprintf into. This is
// way more than is actually needed, so we're not going to check whether
// snprintf output was truncated. In the extremely unlikely case that it
// is, the only consequence is that the user doesn't get to see the end
// of the string.
static const int CHARMAX = 256;

static inline int max(const int a, const int b)
{
  return a > b? a : b;
}

// Given a double greater than 1, round to 2 digits or less, or the
// number of digits given in sf
static int sigfigs(const double in, const int sf=2)
{
  // Could/should convert to uint64_t
  if(in >= 2.147483648e9){
    fprintf(stderr, 
            "I'm not going to be able to store %f in an int!\n", in);
    return 2147483647;
  }

  int ttsf;
  switch(sf){
    case 1: ttsf = 10; break;
    case 2: ttsf = 100; break;
    case 3: ttsf = 1000; break;
    case 4: ttsf = 10000; break;
    case 5: ttsf = 100000; break;
    case 6: ttsf = 1000000; break;
    case 7: ttsf = 10000000; break;
    case 8: ttsf = 100000000; break;
    case 9: ttsf = 1000000000; break;
    default: 
      fprintf(stderr, "%d is an unreasonable number of sigfigs\n", sf);
      return int(in+0.5);
  }

  int n = int(in+0.5);
  if(n > ttsf){
    int divided = 0;
    int lastdig = n%10;
    while((n /= 10) > ttsf){
      lastdig = n%10;
      divided++;
    }
    if(lastdig >= 5) n++;
    for(int i = 0; i <= divided; i++) n *= 10;
  }
  return n;
}

// Translate a number of seconds into a human-readable time with four
// significant figures. Return the number of seconds that the format
// translates to.
static int formatestimate4(char * answer, const int sec, const bool wow)
{
  if(sec < 60){ // up to 59 sec
    snprintf(answer, CHARMAX, "%2ds", sec);
    return sec;
  }
  else if(sec < 3599){ // up to 59m59s
    snprintf(answer, CHARMAX, "%2dm%02ds", sec/60, sec%60);
    return sec;
  }
  else if(sec < 35995){ // up to 9h59m50s
    int ts = ((sec+5)/10) * 10;
    snprintf(answer, CHARMAX, "%dh%02dm%02ds", 
             ts/3600, (ts%3600)/60, ts%60);
    return ts;
  }
  else if(sec < 88370){ // up to 23h59m 
    int tm = (sec + 30)/60;
    snprintf(answer, CHARMAX, "%dh%02dm", tm/60, tm%60);
    return tm*60;
  }
  else if(sec < 863700){ // up to 9d23h50m
    int tm = ((sec + 300)/600) * 10;
    snprintf(answer, CHARMAX, "%dd%02dh%02dm%s",
             tm/1440, (tm%1440)/60, tm%60, wow?" (!)":"");
    return tm*60;
  }
  else if(sec < 8638200){ // up to 99d23h
    int th = ((sec + 1800)/3600);
    snprintf(answer, CHARMAX, "%dd%02dh%s", th/24, th%24,
             wow?" (!!!)":"");
    return th*3600;
  }
  else if(sec < 86400*1000){ // up to 999d20h
    int th = ((sec + 18000)/36000) * 10;
    snprintf(answer, CHARMAX, "%dd%02dh%s", th/24, th%24,
             wow?" (!!!!!)":"");
    return th*3600;
  }
  else{ // Over 1000d
    int td = sigfigs(double(sec)/86400, 4);
    snprintf(answer, CHARMAX, "%dd%s", td, wow?" (!!!!!!!)":"");
    return td*86400;
  }
}

// Translate a number of seconds into a human-readable time with three
// significant figures. Return the number of seconds that the format
// translates to.
static int formatestimate3(char * answer, const int sec, const bool wow)
{
  if(sec < 60){ // up to 59 sec
    snprintf(answer, CHARMAX, "%2ds", sec);
    return sec;
  }
  else if(sec < 599){ // up to 9m59s
    snprintf(answer, CHARMAX, "%dm%02ds", sec/60, sec%60);
    return sec;
  }
  else if(sec < 3595){ // up to 59m50s (59m55s)
    int ts = ((sec + 5)/10) * 10;
    snprintf(answer, CHARMAX, "%dm%02ds", ts/60, ts%60);
    return ts;
  }
  else if(sec < 35970){ // up to 9h59m
    int tm = ((sec + 30)/60);
    snprintf(answer, CHARMAX, "%dh%02dm", tm/60, tm%60);
    return tm*60;
  }
  else if(sec < 84100){ // up to 23h50m 
    int tm = (((sec + 30)/60) / 10) * 10;
    snprintf(answer, CHARMAX, "%dh%dm", tm/60, tm%60);
    return tm*60;
  }
  else if(sec < 856800){ // up to 9d23h
    int th = (sec + 1800)/3600;
    snprintf(answer, CHARMAX, "%dd%02dh%s", th/24, th%24,
             wow?" (!)":"");
    return th*3600;
  }
  else if(sec < 8640000){ // up to 99d20h
    int th = (((sec + 1800)/3600) / 10) * 10;
    snprintf(answer, CHARMAX, "%dd%02dh%s", th/24, th%24,
             wow?" (!!!)":"");
    return th*3600;
  }
  else{ // Over 100d
    int td = sigfigs(double(sec)/86400, 3);
    snprintf(answer, CHARMAX, "%dd%s", td, wow?" (!!!!!)":"");
    return td*86400;
  }
}

// Translate a number of seconds into a human-readable time with two
// significant figures. Return the number of seconds that the format
// translates to.
static int formatestimate2(char * answer, const int sec, const bool wow)
{
  if(sec < 55){ // 1-55 sec
    snprintf(answer, CHARMAX, "%2ds", sec);
    return sec;
  }
  else if(sec < 570){ // 1 minute to 9m30s
    int tm = (sec+5)/60, ts = (sec+5)%60/10*10;
    snprintf(answer, CHARMAX, "%dm%02ds", tm, ts);
    return ts + tm*60;
  }
  else if(sec < 3570){ // 10 minutes to 59 minutes
    int tm = (sec+30)/60;
    snprintf(answer, CHARMAX, "%dm", tm);
    return tm*60;
  }
  else if(sec < 34200){ // 1 hour to 9h50m
    int th = (sec+300)/3600, tm = (sec+300)%3600/600*10;
    snprintf(answer, CHARMAX, "%dh%02dm", th, tm);
    return th*3600 + tm*60;
  }
  else if(sec < 84600){ // 10 hours to 23 hours
    int th = (sec+1800)/3600;
    snprintf(answer, CHARMAX, "%dh", th);
    return th*3600;
  }
  else if(sec < 856800){ 
    // this one is weird. what is two sig figs between 1 and 10 days?
    // well, 0.1 day is about 2 hours, so let's use that: 1 day to 9d22h
    int td = (sec+3600)/86400, th = (sec+3600)%86400/7200*2;

    snprintf(answer,CHARMAX, "%dd%02dh%s", td, th, wow?" (!)":"");

    return td*86400 + th*3600;
  }
  else{
    int td = sigfigs(double(sec)/86400, 2);
    snprintf(answer, CHARMAX, "%dd%s", td, wow?" (!!!)":"");
    return td*86400;
  }
}

// Translate a number of seconds into a human-readable time with one
// significant figure. Return the number of seconds that the format
// translates to.
static int formatestimate1(char * answer, const int sec, const bool wow)
{
  if(sec < 5){
    snprintf(answer, CHARMAX, "%2ds", sec);
    return sec;
  }
  if(sec < 55){ // 6-55 sec
    int ts = ((sec+5)/10)*10;

    // special case: Add a digit if the answer would other wise be "10s"
    if(ts == 10) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%2ds", ts);
    return ts;
  }
  else if(sec < 570){ // 1 minute to 9m30s
    int tm = (sec+30)/60;

    // special case: Add a digit if the answer would other wise be "1m"
    if(tm == 1) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%2dm", tm);
    return tm*60;
  }
  else if(sec < 3570){ // 10 minutes to 59 minutes
    int tm = ((sec+300)/600) * 10;

    // special case: Add a digit if the answer would other wise be "10m"
    if(tm == 10) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%dm", tm);
    return tm*60;
  }
  else if(sec < 34200){ // 1 hour to 9h50m
    int th = (sec+1800)/3600;

    // special case: Add a digit if the answer would other wise be "1h"
    if(th == 1) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%2dh", th);
    return th*3600;
  }
  else if(sec < 84600){ // 10 hours to 23 hours
    int th = ((sec+1800)/36000) * 10;

    // special case: Add a digit if the answer would other wise be "10h"
    if(th == 10) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%dh", th);
    return th*3600;
  }
  else if(sec < 856800){ 
    int td = (sec+43200)/86400;

    // special case: Add a digit if the answer would other wise be "1d"
    if(td == 1) return formatestimate2(answer, sec, wow);

    snprintf(answer, CHARMAX, "%dd%s", td, wow?" (!)":"");
    return td*86400;
  }
  else{
    int td = sigfigs(double(sec)/86400, 1);
    snprintf(answer, CHARMAX, "%dd%s", td, wow?" (!!!)":"");
    return td*86400;
  }
}

// Put the formatted time, etasec, in answer. Uses sigfig significant
// figures, or the closest implemented number. If wow is true, put
// "(!)" or "(!!!)" for very long times.
// 
// The largest unit used is days. Easy enough to extend to weeks, etc.
// if you like. If my programs want to run that long, I either rewrite
// them, use a bigger cluster, or change my goals.
//
// Return the number of seconds that the format translates to.
static int formatestimate(char * answer, const int etasec, 
                          const bool wow, const int sigfig)
{
  switch(sigfig){
    case 1:  return formatestimate1(answer, etasec, wow);
    case 2:  return formatestimate2(answer, etasec, wow);
    case 3:  return formatestimate3(answer, etasec, wow);
    case 4:  return formatestimate4(answer, etasec, wow);
    default: return formatestimate4(answer, etasec, wow);
  }
}

// * ince, the estimated time remaining under the assumption that the
//   program will run at its average speed since the last report.
//
// * tote, the estimated time remaining under the assumption that the
//   program will run at its average speed since the beginning.
// 
// * frac, the fraction of the way through the program that we are.
// 
// If the program is less than 1/2 done, return the harmonic mean of
// ince & tote.
//
// Otherwise, return a weighted average of ince & tote, with ince
// getting more weight as the program progresses.
static int etasec(const double ince, const double tote, 
                  const double frac)
{ 
  if(ince == -1) return int(round(tote));

  if(frac < 0.5) return int(round(sqrt(tote*ince)));

  const double N = 0.75; // maximum weight of ince

  return int(round( (1-N + (2*N-1)*frac)*ince + 
                    (  N - (2*N-1)*frac)*tote)   );
}

// Returns a string describing the estimated time left. Returns a
// pointer to a string representing the time to be printed. Caller must
// free the string when done with it.
static char * eta(int & dispeta, const double ince, const double tote, 
                  const double frac)
{
  char * answer = (char*)malloc(CHARMAX);
  dispeta = formatestimate(answer, etasec(ince, tote, frac), false, 
                 frac < 0.1 ? 1 : 2); 
  return answer;
}

// Give a total time so far and the estimated time to completion,
// determine how many significant digits their sum has. (The total time
// so far is exact, while the ETA can be considered to have one or two
// sig figs.)
static int sfofetot(const int eta, const int tot)
{
  if(eta <= 0) return 9;
  if(tot <= 0) return 1;

  if(eta > 10*tot) return 1;

  int totdig = int(log10(tot)), etadig = int(log10(eta));

  // ETA has two sigfigs if we are more than 1/11th done, one otherwise
  // (That's totally arbitrary.)
  const int etasf = 2;

  // If the ETA is smaller than the total, the total can start to 
  // dominate the sum, giving more significant figures.  e.g.
  // 12345 total +  670 eta --> 5 - 3 + 2 = 4 sig figs as compared to
  //  9876 total + 7700 eta --> 4 - 4 + 2 = 2 sig figs or
  //    99 total +  870 eta --> max(2 - 3 + 2 == 1, 2) = 2 sig figs
  return max(totdig - etadig + etasf, etasf);
}

// If the ETA improves by 20% or more, sets status to 1. If it
// gets worse by 25% or more, sets status to -1. Otherwise, sets it to
// 0. These can be used to color the output. This coloring method is
// vulnerable to a slow boil problem, wherein the ETA slowly increases,
// but never fast enough to trigger the status change. However, this
// isn't a typical occurance (usually things get better or worse
// abruptly when other jobs seize or release resources, or you run out
// of buffer space, or whatnot), so I'm not going to protect against it.
static bool nopreviousestimate = true; 
static int findstatus(int dispeta, double inctime)
{
  static int previousprint;

  int status;

  if(nopreviousestimate) status = 0;
  else if(dispeta < 2) status = 0;
  else if(dispeta+inctime < 0.75*previousprint) status = 1;
  else if(dispeta+inctime > 1.33*previousprint) status = -1;
  else status = 0;

  previousprint = dispeta;
  nopreviousestimate = false;

  return status;
}

// Returns a string describing the estimated total running time. Returns
// a pointer to a string representing the time to be printed. Caller
// must free the string when done with it. 
static char * etotal(const double tottime, const double ince,
                     const double tote, const double frac)
{
  char * answer = (char*)malloc(CHARMAX);

  int eta = etasec(ince, tote, frac); // estimate
  int tot = int(tottime); // exact
  int current = eta + tot;

  formatestimate(answer, current, true, sfofetot(eta, tot)); 

  return answer;
}

// Returns a pointer to a string representing the time to be printed.
// Caller must free the string when done with it.
static char * disptime(const double ttime)
{
  char * buf = (char*)malloc(CHARMAX);

  if(ttime >= 2.147483648e9){
    fprintf(stderr, "I'm not going to be able to store %f in an int!\n",
            ttime);
    snprintf(buf, CHARMAX, "more than 78 years");
    return buf;
  }

  int t = int(ttime);
  bool reqzero = false, showseconds = true;
  int printed = 0; // keep track of how many characters have been used

  if(t >= 86400){
    printed += snprintf(buf, CHARMAX, "%dd ",  t/86400); 
    t %= 86400;
    reqzero = true;
    showseconds = false;
  }

  if(t >= 3600 || reqzero){
    printed += snprintf(buf+printed, CHARMAX-printed, 
                        "%0*dh%s", reqzero?2:1, t/3600, reqzero?"":" ");
    t %= 3600;
    reqzero = true;
  }

  if(t >= 60 || reqzero){
    printed += snprintf(buf+printed, CHARMAX-printed, 
                        "%0*dm", reqzero?2:1, t/60);
    t %= 60;
    reqzero = true;
  }

  if(showseconds)
    snprintf(buf+printed, CHARMAX-printed, "%0*ds", reqzero?2:1, t);

  return buf;
} 


// Not meant to have any generality. Just a helper function for
// generateprintpoints.
static unsigned int iexp10(const int ep)
{
  switch(ep){
    case 2: return 100;
    case 3: return 1000;
    case 4: return 10000;
    case 5: return 100000;
    case 6: return 1000000;
    case 7: return 10000000;
    case 8: return 100000000;
    case 9: return 1000000000;
    default: 
      fprintf(stderr, "Bad exponent in iexp10()\n");
      return 1;
  }
}

/* Given the total number of events and the most digits to print in the
reports, generate the events on which progress should be reported. */
vector<unsigned int> generateprintpoints(const unsigned int total,
                                         const int maxe)
{
  vector<unsigned int> ppoints;

  // First three, so you can see the program is not stuck
  // (But not zero, see below.)
  for(int i = 0; i <= 2; i++) ppoints.push_back(i);

  // Last one, to get a report of total time
  ppoints.push_back(total-1);

  // Makes 10% - 90% print. Parentheses required around (total/10) to
  // make this work for numbers bigger than 0xffffffff/10.
  for(unsigned int i = 1; i <= 9; i++) ppoints.push_back(i*(total/10));

  // Makes 1%-9% and 91%-99%, 0.1%-0.9% and 99.1%-99.9%, etc.
  for(int ep = 2; ep <= maxe; ep++){
    for(unsigned int i = 1; i <= 9; i++){
      ppoints.push_back(i * (total/iexp10(ep)));
      ppoints.push_back(total - i * (total/iexp10(ep)));
    }
  }

  // cat ppoints | sort | uniq
  sort(ppoints.begin(), ppoints.end());
  ppoints.resize(unique(ppoints.begin(),ppoints.end())-ppoints.begin());

  // 0 is problematic since the user will randomly start with 0 or 1
  // and if the user starts with 1 and we were expecting 0, we'll never
  // print anything. Also, there's no way to have a good time estimate
  // on the first iteration anyway.
  if(ppoints[0] == 0) ppoints.erase(ppoints.begin());

  return ppoints;
}

// Each time, new is set to the current time. old is set to the current
// time the first time, then subsequently is set to new at the bottom
static double firsttime, oldtime;
static vector<unsigned int> ppoints; // the values of sofar to print
static unsigned int nextprint = 0xffffffff;
static double lastfrac;
static unsigned int total;

static void printprogress(const unsigned int sofar,
                          const char * const taskname)
{
  // we're never going to find this one or any one before it again,
  // so remove them to save time in future searches.
  if(ppoints.size()){
    ppoints.erase(ppoints.begin());
    nextprint = ppoints[0];
  }

  double frac = double(sofar)/total;

  struct timeval brokennewtime;
  gettimeofday(&brokennewtime, NULL); 
  double newtime = brokennewtime.tv_sec + 1e-6 * brokennewtime.tv_usec;
  double tottime = newtime - firsttime;
  double inctime = newtime - oldtime;

  // Don't print anything until N seconds have passed since the first
  // time since often programs do their first few iterations slowly due
  // to opening files for the first time, etc. Don't print a report if
  // we've printed one in the last M seconds either. As a special case,
  // *do* print if this is the last iteration. This gets the total time
  // on the screen.
  if(sofar != total-1 && (tottime < 4 || inctime < 2)) return;

  // Divide-by-zero protection. frac == 0 can happen if the user
  // initializes us and then spends a long time doing something
  // else before starting the loop AND does the first call with 0.
  // frac-lastfrac can happen if the user improperly calls us with the
  // same number twice.
  if(frac == 0 || frac-lastfrac == 0) return;

  double tote = tottime/frac - tottime;
  double ince = frac-lastfrac > 0? (1-frac)*inctime/(frac-lastfrac): -1;

  int ep;
  // Force last call to be 100%, not 99.98% or something silly like that
  if     (sofar == total - 1) ep = 0; 
  // Otherwise, set the number of sigfigs to the regime we're in.
  else if(frac < 0.0000000099 || frac > 0.99999999) ep = 8;
  else if(frac < 0.000000099  || frac > 0.9999999)  ep = 7;
  else if(frac < 0.00000099   || frac > 0.999999)   ep = 6;
  else if(frac < 0.0000099    || frac > 0.99999)    ep = 5;
  else if(frac < 0.000099     || frac > 0.9999)     ep = 4;
  else if(frac < 0.00099      || frac > 0.999)      ep = 3;
  else if(frac < 0.0099       || frac > 0.99)       ep = 2;
  else if(frac < 0.099        || frac > 0.9)        ep = 1;
  else                                              ep = 0;

  char * dispelapsed = disptime(tottime);
  int ndispeta;
  char * dispeta = eta(ndispeta, ince, tote, frac);
  int status = findstatus(ndispeta, inctime);
  char * disptot = etotal(tottime, ince, tote, frac);
  if(sofar == total - 1) disptot[0] = '\0';

  // If your background wasn't black, this makes it black; You'll
  // have to say 'reset' or 'ls --color=auto' or something like that
  // afterwards if you don't like black. 37=white, 31=red, 32=green
  if(USECOLOR)
    printf("*"
           "%s: %7.*f%% "
           "So far: %9s "
           "%c[%s;%s;40mEst total: %9s "
           "ETA: %9s%c[0;37;40m  "
           "*\n",

           taskname,
 
           /* The percentage */
           ep-1 > 0? ep-1: 0, 
           round(pow(10, ep+1)*frac)/pow(10, ep-1),

           dispelapsed,

           0x1b,
           status == 0?"0":"1",
           status == 0?"37":status == 1?"32":"31",
           disptot, 

           dispeta,
           0x1b
    );
  else
    printf("*"
           "%s: %7.*f%% "
           "So far: %9s  "
           "Est total: %9s "
           "ETA: %9s  "
           "*\n",

           taskname,

           /* The percentage */
           ep-1 > 0? ep-1: 0, 
           round(pow(10, ep+1)*frac)/pow(10, ep-1),
           dispelapsed, disptot, dispeta
    ); 
 
  fflush(stdout);

  free(dispelapsed);
  free(dispeta);
  free(disptot);

  oldtime = newtime;
  lastfrac = frac;
}

void initprogressindicator(const unsigned int totin, const int maxe)
{
  if     (maxe > 9) fprintf(stderr, "maxe may not be > 9. Using 9\n");
  else if(maxe < 1) fprintf(stderr, "maxe may not be < 1. Using 1\n");
    
  nopreviousestimate = true;
  total = totin;
  ppoints = generateprintpoints(total, maxe>9? 9: maxe<1? 1: maxe);
  nextprint = ppoints[0];

  struct timeval brokenfirsttime;
  gettimeofday(&brokenfirsttime, NULL); 
  firsttime = brokenfirsttime.tv_sec + 1e-6 * brokenfirsttime.tv_usec;

  oldtime = firsttime;
  lastfrac = 0;
}

// Print progress report if appropriate.  For best results, include this
// In your loop *after* the work of the loop is done and call it on 
// every iteration.
// 
// If this does not print, all it does is compares two integers and then
// returns. The exception to this is if it is not printing because not
// enough time has elapsed since the previous print. This is about as
// rare as the prints themselves, so not a performance concern.
//
// inline makes a serious impact (factor of 10 improvement), at least if
// using -O2 and if this cpp file is included directly rather than via
// the header. (If using the header, this file is compiled separately
// and so *can't* be inlined.)
//
// Without inline, with or without including this file directly, it is
// a factor of 2 speed improvement to have printprogress as a separate
// function, presumably because we don't invalidate as much of the cache
// or pipeline or something by having a big dangling function body that
// turns out to be unused.
#ifdef PROGRESS_INDICATOR_HEADER_USED
  void progressindicator(const unsigned int sofar,
                         const char * const taskname)
#else
  inline void progressindicator(const unsigned int sofar,
                                const char * const taskname)
#endif
{
  if(sofar == nextprint) printprogress(sofar, taskname);
}
