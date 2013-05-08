/**
  \author Matthew Strait
  \brief User interaction and unorganized functionality.
*/

using namespace std;

#include <signal.h>
#include <errno.h>
#include <iostream>
#include <vector>
#include "DCRecoOV.hh"
#include "otc_cont.h"
#include "otc_root.h"
#include "otc_progress.cpp"

// The OV Event Builder requires that within one EnDep (i.e. event) at
// least two hits are strictly larger than this threshold. I'm not clear
// on whether they have to be part of the same muon-like double.
static const int EB_THRESHOLD = 73;

static void printhelp()
{
  cout << 
  "OTC: The Outer Veto Event Time Corrector\n"
  "\n"
  "Basic syntax: otc -o [output file] [one or more muon.root files]\n"
  "\n"
  "-c: Overwrite existing output file\n"
  "-n [number] Process at most this many events\n"
  "-h: This help text\n";
}

/** Parses the command line and returns the position of the first file
name (i.e. the first argument not parsed). */
static int handle_cmdline(int argc, char ** argv, bool & clobber,
                          unsigned int & nevents, char * & outfile)
{
  const char * opts = "o:chn:";
  bool done = false;
 
  while(!done){
    char whatwegot;
    switch(whatwegot = getopt(argc, argv, opts)){
      case -1:
        done = true;
        break;
      case 'n':
        errno = 0;
        char * endptr;
        nevents = strtol(optarg, &endptr, 10);
        if((errno == ERANGE && (nevents == UINT_MAX)) || 
           (errno != 0 && nevents == 0) || 
           endptr == optarg || *endptr != '\0'){
          fprintf(stderr,
            "%s (given with -n) isn't a number I can handle\n", optarg);
          exit(1);
        }
        break;
      case 'o':
        outfile = optarg;
        break;
      case 'c':
        clobber = true;
        break;
      case 'h':
        printhelp();
        exit(0);
      default:
        printhelp();
        exit(1);
    }
  }  

  if(!outfile){
    cerr << "You must give an output file name with -o\n";
    printhelp();
    exit(1);
  }

  if(argc <= optind){
    cerr << "Please give at least one muon.root file.\n\n";
    printhelp();
    exit(1);
  }
  return optind;
}

/** Somewhere something is causing OTC to exit silently on a seg fault
instead of mentioning that it has seg faulted! So I'll explicitly catch
seg faults and tell the user what happened as is normally expected. I
have no idea what happens on a SIGBUS if I don't intervene, so I'll
catch that too, just because it's something that happens occasionally.
*/
static void on_segv_or_bus(const int signal)
{
  cerr << "Got " << (signal==SIGSEGV? "SEGV": "BUS") << ".  Exiting.\n";
  // Use _exit() instead of exit() to avoid calling atexit() functions
  // and/or other signal handlers. Something, presumably in the bowels
  // of ROOT, must be doing one of these since a call to exit() can take
  // several minutes (!) to complete, but _exit() finishes more quickly.
  _exit(1);
}

/** To be called when the user presses Ctrl-C or something similar
happens. */
static void endearly(__attribute__((unused)) int signal)
{
  cerr << "Got Ctrl-C or similar.  Exiting.\n";
  _exit(1); // See comment above
}

static void do_hits_stuff(otc_output_event & out,
                          const OVEventForReco & hits)
{
  // Should not happen for data, but can happen in Monte Carlo
  if(hits.nhit == 0) return;
 
  int lasttime = hits.Time[0];
  int biggest = 0;
  for(unsigned int i = 0; i < hits.nhit; i++){
    const int diff = hits.Time[i] - lasttime;
    if(diff < 0){
      printf("Hits %d and %d of %d out of order with times %d and %d\n",
             i, i-1, hits.nhit, lasttime, hits.Time[i]);
      out.error = true;
    }
    if(diff - 1 > out.gap) out.gap = diff - 1;
    lasttime = hits.Time[i];

    if(hits.Q[i] > biggest){
      biggest = hits.Q[i];
      out.biggest_forward = hits.Time[i] - hits.Time[0];
    }
  }
  out.length = hits.Time[hits.nhit-1] - hits.Time[0] + 1;
}

/*
Find the best estimate of the time difference between the first hit and
the *muon* hit.

The scenario at hand is where an accidental gamma hits the OV right
before a muon. We want to reject the hits from that gamma.

For now, I will claim that I only care about events with XY overlaps.

First, we will discard any hits that are not part of an XY overlap.

Of the remaining XY overlaps, there are three possibilities:

1) Formed by the muon or muon shower fragments.

2) The gamma hit one of the same modules as the muon hit. It hit before
and made the module dead to the muon. The XY overlap has one module from
the muon and the other from the gamma. The true muon position is lost.

3) The gamma hit an adjacent module and formed a separate XY overlap.
Another XY overlap gives the correct muon position.

Since case #1 can validly occur with the hits one clock cycle apart (and
the top one need not be earlier -- the fiber length can easily cancel
that effect), we cannot reject an XY overlap for having hits in two
consecutive clock cycles. Even having a gap of one cycle is possible if
the muon hits at the extreme far end of one module and the extreme near
end of the other. Maybe we really need to look up where the XY overlap
is... Still, having a gap of two cycles is not realistic.

Supposing that there is exactly one additional accidental gamma, it
should be sufficient to simply take the time of the third hit in the
event.

But what if there are multiple correlated gammas? One can imagine a
decay cascade from junk in or near the OV modules. Two such gammas
could even form an XY overlap by themselves. You might think, well, ok,
but then this is just equivalent to the fact that sometimes one much
passes through right before another. Except that this gamma-gamma XY
overlap will typically only be visible --- post-event builder --- if it
is attached to a following muon, and so form an event-builder shoulder
anyway.

(Come to think about it, do unlucky muons with low ADC counts form an
event builder shoulder? Come back to this.)

The solution to a gamma-gamma event that does *not* form an XY overlap
by itself is to discard hits outside of XY overlaps, and then take the
third hit time (to protect against one of the gammas being in case #2 or
#3 above).

But to protect against gamma-gamma events that *do* form XY overlaps, we
have to switch course. What if we accept only the highest-likelihood XY
overlap and take the time of its third hit? I think this covers all the
cases. Of course, it shifts the time later for a large class of events
with no accidental gamma at all, but it does so in a consistent way, and
I think that's ok.

A more extreme solution would be to take the *last* hit time of the
event. I don't think this is a good idea. It would "protect" against
all the cases above, but also causes accidental gammas *after* the muon
make the OV event late. I suppose, though, it is the most conservative
approach if one wishes to discard any slightly suspicious event. The
event length variable that I am saving will let this definition be used.
*/
static void do_xy_stuff(otc_output_event & out, 
                        const otc_input_event & inevent)
{
  if(inevent.nxy == 0) return;

  if(inevent.xy_nhit[0] < 3){
    printf("Weird XY overlap with only %d hits!\n", inevent.xy_nhit[0]);
    return;
  }

  out.recommended_forward = inevent.hits.Time[inevent.xy_hits[0][2]]
                           -inevent.hits.Time[0];
}

static otc_output_event doit(const otc_input_event & inevent)
{
  otc_output_event out;
  memset(&out, 0, sizeof(out));
  
  do_hits_stuff(out, inevent.hits);

  do_xy_stuff(out, inevent); 

  return out;
}

static void doit_loop(const unsigned int nevent)
{
  cout << "Working..." << endl;
  initprogressindicator(nevent, 6);

  // NOTE: Do not attempt to start anywhere but on event zero.
  // For better performance, we don't allow random seeks.
  for(unsigned int i = 0; i < nevent; i++){
    const otc_input_event inevent = get_event(i);
    const otc_output_event out = doit(inevent);
    write_event(out);
    progressindicator(i, "OTC");
  }
  cout << "All done working." << endl;
}

int main(int argc, char ** argv)
{
  signal(SIGSEGV, on_segv_or_bus);
  signal(SIGBUS,  on_segv_or_bus);
  signal(SIGINT, endearly);
  signal(SIGHUP, endearly);

  char * outfile = NULL;
  bool clobber = false; // Whether to overwrite existing output
                         
  unsigned int maxevent = 0;
  const int file1 = handle_cmdline(argc,argv,clobber,maxevent,outfile);

  const unsigned int nevent = root_init(maxevent, clobber, outfile, 
                                        argv + file1, argc - file1);

  doit_loop(nevent);

  root_finish();
  
  return 0;
}
