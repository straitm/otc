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
#include "rover_cont.h"
#include "rover_root.h"
#include "rover_progress.cpp"

static void printhelp()
{
  cout << 
  "ROVER, the ROOT I/O Outer Veto Reconstructor\n"
  "           ^        ^     ^^   ^\n"
  "Woof!\n"
  "\n"
  "Basic syntax: rover -o [output file] [one or more muon.root files]\n"
  "\n"
  "-c: Overwrite existing output file\n"
  "-l: Use RecoOV's default likelihoods instead of generating them from "
      "the input\n"
  "-n [number] Process at most this many events\n"
  "-r: Do not copy OVHitInfoTree to output file\n"
  "-h: This help text\n";
}

/** Parses the command line and returns the position of the first file
name (i.e. the first argument not parsed). */
static int handle_cmdline(int argc, char ** argv, bool & skiplike,
                          bool & copyhits, bool & clobber, 
                          unsigned int & nevents, char * & outfile)
{
  const char * opts = "lro:chn:";
  bool done = false;
 
  while(!done){
    char whatwegot;
    switch(whatwegot = getopt(argc, argv, opts)){
      case -1:
        done = true;
        break;
      case 'l':
        skiplike = true;
        break;
      case 'r':
        copyhits = false;
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

/** Somewhere something is causing ROVER to exit silently on a seg fault
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

static int min(const unsigned int a, const unsigned int b)
{
  return a > b ? b : a;
}

static vector<float> setup_likes(const bool skiplike,
                                 const unsigned int nevent,
                                 TH1 * & h_loglike)
{
  if(skiplike) return vector<float>();

  cout << "Building table of likelihoods..." << endl;
  vector<unsigned int> signal, bground;
  unsigned int n_likeinit = min(nevent, 1500000);

  initprogressindicator(n_likeinit, 6);
  // NOTE: Do not attempt to start anywhere but on event zero.
  // For better performance, we don't allow random seeks.
  for(unsigned int i = 0; i < n_likeinit; i++){
    rover_input_event inevent = get_event(i);
    if(RecoOV_likebuild(signal, bground, *(inevent.event_for_reco))){
      cerr << "Aaaaahh! Something went wrong in RecoOV_likebuild.\n";
      exit(1);
    }
    progressindicator(i);
  }

  vector<float> likes = RecoOV_likeinit(signal, bground, h_loglike);
  if(likes.size() == 0)
    cerr << "Warning: insufficient statistics.  Will fall back on "
            "default table of likelihoods." << endl;
  return likes;
}

static void reconstruct(const unsigned int nevent, 
                        const vector<float> & likelihoods)
{
  cout << "Reconstructing..." << endl;
  initprogressindicator(nevent, 6);

  // NOTE: Do not attempt to start anywhere but on event zero.
  // For better performance, we don't allow random seeks.
  for(unsigned int i = 0; i < nevent; i++){
    vector<OVTrackFromReco> tracks;
    vector<OVXYFromReco> xys;
    rover_input_event inevent = get_event(i);
    if(RecoOV(tracks, xys, *(inevent.event_for_reco), likelihoods))
      cerr << "Aaaahh! RecoOV failed. Not writing event " << i << endl;
    else  
      write_event(tracks, xys, inevent.EventID);
    progressindicator(i);
  }
  cout << "All done reconstructing." << endl;
}

int main(int argc, char ** argv)
{
  signal(SIGSEGV, on_segv_or_bus);
  signal(SIGBUS,  on_segv_or_bus);
  signal(SIGINT, endearly);
  signal(SIGHUP, endearly);

  char * outfile = NULL;
  bool clobber = false, // Whether to overwrite existing output.
       copyhits = true, // Whether to copy OVHitInfoTree to output file.
       skiplike = false; // Whether to skip the likelihood loop and 
                         // pass a zero sized array to RecoOV.
  unsigned int maxevent = 0;
  const int file1 = handle_cmdline(argc, argv, skiplike, copyhits, 
                                   clobber, maxevent, outfile);

  unsigned int nevent = root_init(maxevent, copyhits, clobber, outfile, 
                                  argv + file1, argc - file1);

  TH1 * h_loglike = NULL;
  vector<float> likelihoods = setup_likes(skiplike, nevent, h_loglike);

  reconstruct(nevent, likelihoods);

  root_finish(h_loglike);
  
  return 0;
}
