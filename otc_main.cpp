/**
  \author Matthew Strait
  \brief User interaction and unorganized functionality.
*/

using namespace std;

#include <signal.h>
#include <errno.h>
#include <vector>
#include "otc_cont.h"
#include "otc_root.h"
#include "otc_progress.cpp"

#include "zcont.h"
extern zdrawstrip ** striplinesabs;

static void printhelp()
{
  printf(
  "OTC: The Outer Veto Event Time Corrector\n"
  "\n"
  "Basic syntax: otc -o [output file] [one or more muon.root files]\n"
  "\n"
  "-c: Overwrite existing output file\n"
  "-n [number] Process at most this many events\n"
  "-h: This help text\n");
}

/** Parses the command line and returns the position of the first file
name (i.e. the first argument not parsed). */
static int handle_cmdline(int argc, char ** argv, bool & clobber,
                          unsigned int & nevents, char * & outfile)
{
  const char * const opts = "o:chn:";
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
    fprintf(stderr, "You must give an output file name with -o\n");
    printhelp();
    exit(1);
  }

  if(argc <= optind){
    fprintf(stderr, "Please give at least one muon.root file.\n\n");
    printhelp();
    exit(1);
  }
  return optind;
}

static void on_segv_or_bus(const int signal)
{
  fprintf(stderr, "Got %s. Exiting.\n", signal==SIGSEGV? "SEGV": "BUS");
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
  fprintf(stderr, "Got Ctrl-C or similar.  Exiting.\n");
  _exit(1); // See comment above
}

static cart makecart(const double x, const double y, const double z)
{
  cart a;
  a.x = x;
  a.y = y;
  a.z = z;
  return a;
}

static cart stpcenter(const unsigned int ch,
                      const unsigned short status,
                      const bool uselowiftrig)
{
  // If this is not an ADC hit, assume it is a trigger box hit (it is)
  const zhit hit(ch, 0, 0, status == 2? normal:
                       uselowiftrig?edgelow:edgehigh);
  const zdrawstrip strip = striplinesabs[hit.mod][hit.stp];
  return makecart((strip.x1+strip.x2)/2,
                  (strip.y1+strip.y2)/2,
                  strip.z);
}

static void lastpos(otc_output_event & __restrict__ out,
                    const OVEventForReco & __restrict__ hits)
{
  double farthest = 0;

  unsigned int i = 0;
  while(hits.Time[i] != hits.Time[hits.nhit-1]) i++;

  for(; i < hits.nhit; i++){
    const cart sc = stpcenter(hits.ChNum[i], hits.Status[i], false);
    const double dist = sqrt(sc.x*sc.x + sc.y*sc.y);
    if(dist > farthest){
      farthest = dist;
      out.lastx = sc.x;
      out.lasty = sc.y;
      out.lastz = sc.z;
    }

    if(hits.Status[i] != 2){
      const cart sc = stpcenter(hits.ChNum[i], hits.Status[i], true);
      const double dist = sqrt(sc.x*sc.x + sc.y*sc.y);
      if(dist > farthest){
        farthest = dist;
        out.lastx = sc.x;
        out.lasty = sc.y;
        out.lastz = sc.z;
      }
    }
  }
}

static void do_hits_stuff(otc_output_event & __restrict__ out,
                          const OVEventForReco & __restrict__ hits)
{
  // Should not happen for data, but can happen in Monte Carlo
  if(hits.nhit == 0) return;
 
  for(unsigned int i = 1; i < hits.nhit; i++){
    if(hits.Time[i] < hits.Time[i-1]){
      printf("Hits %d and %d of %d out of order with times %d and %d\n",
             i, i-1, hits.nhit, hits.Time[i-1], hits.Time[i]);
      out.error = true;
    }
  }
  out.length = hits.Time[hits.nhit-1] - hits.Time[0] + 1;

  if(!out.error) lastpos(out, hits);
}

static otc_output_event doit(const otc_input_event & inevent)
{
  otc_output_event out;
  memset(&out, 0, sizeof(out));
  
  do_hits_stuff(out, inevent.hits);

  return out;
}

static void doit_loop(const unsigned int nevent)
{
  printf("Working...\n");
  initprogressindicator(nevent, 4);

  // NOTE: Do not attempt to start anywhere but on event zero.
  // For better performance, we don't allow random seeks.
  for(unsigned int i = 0; i < nevent; i++)
    write_event(doit(get_event(i))), // never do this
    progressindicator(i, "OTC");
  printf("All done working.\n");
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
