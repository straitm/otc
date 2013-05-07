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

static otc_output_event doit(const OVEventForReco * inevent)
{
  otc_output_event out;
  memset(&out, 0, sizeof(out));
  
  if(inevent->nhit == 0) return out;
 
  int lasttime = inevent->Time[0];
  for(unsigned int i = 0; i < inevent->nhit; i++){
    const int gap = inevent->Time[i] - lasttime;
    if(gap < 0)
      printf("Hits %d and %d of %d out of order with times %d and %d\n",
             i, i-1, inevent->nhit, lasttime, inevent->Time[i]);
    if(gap > out.clocks_forward) out.clocks_forward = gap;
    lasttime = inevent->Time[i];
  }
  printf("%d\n", out.clocks_forward);

  return out;
}

static void doit_loop(const unsigned int nevent)
{
  cout << "Working..." << endl;
  initprogressindicator(nevent, 6);

  // NOTE: Do not attempt to start anywhere but on event zero.
  // For better performance, we don't allow random seeks.
  for(unsigned int i = 0; i < nevent; i++){
    otc_input_event inevent = get_event(i);
    const otc_output_event out = doit(inevent.event_for_reco);
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
