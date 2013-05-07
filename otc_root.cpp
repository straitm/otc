/**
  \author Matthew Strait
  \brief Handles most ROOT interaction for ROVER.
*/

using namespace std;

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE // for safe basename()
#endif
#include <string.h>
#include <vector>
#include <iostream>
#include "TSystem.h"
#include "TChain.h"
#include "TFile.h"
#include "TError.h"
#include "TClonesArray.h"
#include "DCRecoOV.hh" 
#include "otc_cont.h"

static otc_output_event outevent;

namespace {
  OVEventForReco inevent;

  // These are needed to get the ADC counts and clock ticks out of the
  // muon.root files before we cast them to integers and put them in
  // inevent.
  double floatingQ[MAXOVHITS], floatingTime[MAXOVHITS];
  unsigned int EventID[MAXOVHITS];
  vector<TTree *> fauxchain;
  vector<uint64_t> fauxchain_entries;
  bool inputismc = false;

  // Needed for writing the output file
  TFile * outfile;
  TTree * recotree;
}; 

/** Make inevent the eventn'th event in the chain. */
otc_input_event get_event(const uint64_t current_event)
{
  // Go through some contortions for speed. Favor TBranch::GetEntry over
  // TTree::GetEntry, which loops through unused branches on every call.
  // Avoid using TChain to find the TTrees' branches on every call.
  static TBranch * qbr = 0, * timebr = 0, * chbr = 0, 
                 * statbr = 0, * idbr = 0;

  static uint64_t offset = 0, nextbreak = 0;

  // This allows reading randomly around in the current TTree or
  // resetting to the first TTree, but random reads from one TTree to
  // another will fail catastrophically.
  if(current_event == 0 || current_event == nextbreak){
    static TTree * curtree = 0;
    static int curtreeindex = -1;
    if(current_event == 0){
      offset = nextbreak = 0;
      curtreeindex = -1;
    }

    curtree = fauxchain[++curtreeindex];
    nextbreak = fauxchain_entries[curtreeindex+1];
    offset = fauxchain_entries[curtreeindex];

    curtree->SetMakeClass(1);
    chbr   = curtree->GetBranch("OVHitInfoBranch.fChNum");
    idbr   = curtree->GetBranch("OVHitInfoBranch.fEventID");
    statbr = curtree->GetBranch("OVHitInfoBranch.fStatus");
    qbr    = curtree->GetBranch("OVHitInfoBranch.fQ");
    timebr = curtree->GetBranch("OVHitInfoBranch.fTime");
    int dummy;
    curtree->SetBranchAddress("OVHitInfoBranch", &dummy);
    curtree->SetBranchAddress("OVHitInfoBranch.fChNum", inevent.ChNum);
    curtree->SetBranchAddress("OVHitInfoBranch.fStatus",inevent.Status);
    curtree->SetBranchAddress("OVHitInfoBranch.fQ",     floatingQ);
    curtree->SetBranchAddress("OVHitInfoBranch.fTime",  floatingTime);
    curtree->SetBranchAddress("OVHitInfoBranch.fEventID", EventID);
  }

  uint64_t localentry = current_event - offset;

  // Instead of getting the number of hits via
  // TTree::SetBranchAddress("OVHitInfoBranch", &n), TTree::GetEntry(i),
  // which has the unfortunate side effect of looping through all
  // branches, determine it by the number of bytes read when getting
  // something else.
  //
  // WARNING WARNING WARNING: I have empirically found that the array
  // length is one less than it would seem from taking the number of
  // bytes read. Maybe ROOT reads an end marker after the real data
  // and counts that towards the byte count. In any case, I don't know
  // whether I'm using a real feature here, or if this is just something
  // that works by accident.
  inevent.nhit = chbr->GetEntry(localentry)/sizeof(unsigned int) - 1;

  // It's awkward to avoid having already filled an array at the first
  // moment that nhit is known. If the number of events is truly huge,
  // it will segfault. However, if it's only a little too large, since
  // inevent.ChNum comes before several other arrays in OVEventForReco,
  // we will usually survive and can abort more cleanly here.
  if(inevent.nhit > MAXOVHITS){
    cerr << "Crazy event with " << inevent.nhit << " hits! I just ran "
            "off the end of some arrays, so I'm bailing out!\n";
    exit(1);
  }
  else if(inevent.nhit == 0){
    if(!inputismc)
      cerr << "Event with no hits. Unexpected in data. Is this Monte "
              "Carlo missing OVHitThInfoTree? Setting EventID to zero, "
              "which should be ok.\n";
    EventID[0] = 0;
  }

  idbr->GetEntry(localentry);
  statbr->GetEntry(localentry);

  qbr->GetEntry(localentry);
  for(unsigned int i = 0; i < inevent.nhit; i++)
    inevent.Q[i] = int(floatingQ[i]); 

  timebr->GetEntry(localentry);
  for(unsigned int i = 0; i < inevent.nhit; i++)
    inevent.Time[i] = int(floatingTime[i]);

  otc_input_event tutorloop;
  tutorloop.event_for_reco = &inevent;
  tutorloop.EventID = EventID[0];
  return tutorloop;
}

void write_event(const otc_output_event & out)
{
  outevent = out;
  recotree->Fill();
}

/** Loads the list of filenames into the global TChain "chain". Arranges
for reading of the chain through SetBranchAddress and optimizes with
SetBranchStatus. */
static uint64_t root_init_input(char ** filenames, const int nfiles)
{
  TChain mctestchain("OVHitThInfoTree");

  uint64_t totentries = 0;

  for(int i = 0; i < nfiles; i++){
    const char * fname = filenames[i];
    if(strlen(fname) < 9){
      cerr << fname << " doesn't have the form *muon*.root\n";
      _exit(1);
    }
    if(!strstr(fname, "muon")){
      cerr << "File name " << fname << " does not contain \"muon\"\n";
      _exit(1);
    }
    if(strstr(fname, ".root") != fname + strlen(fname) - 5){
      cerr << "File name " << fname << " does not end in \".root\"\n";
      _exit(1);
    }
    if(gSystem->AccessPathName(fname)!=0) {
      cerr << fname << " does not exist, or you can't read it\n";
      _exit(1);
    }

    TFile * inputfile = TFile::Open(fname, "read");
    if(!inputfile || inputfile->IsZombie()){
      cerr << fname << " became a zombie when ROOT tried to read it.\n";
      _exit(1);
    }

    TTree * temp=dynamic_cast<TTree*>(inputfile->Get("OVHitInfoTree"));
    if(!temp){
      cerr << fname << " does not have an OVHitInfoTree tree\n";
      _exit(1);
    }

    fauxchain_entries.push_back(totentries);
    fauxchain.push_back(temp);
    totentries += temp->GetEntries();

    {
      int old_geil = gErrorIgnoreLevel;
      gErrorIgnoreLevel = kFatal;
      mctestchain.Add(fname);
      if(mctestchain.GetEntries() != 0) inputismc = true;
      gErrorIgnoreLevel = old_geil; 
    }

    cout << "Loaded " << fname << endl;
  }

  return totentries;
}

static void root_init_output(const bool clobber, char * outfilename)
{
  outfile = new TFile(outfilename, clobber?"RECREATE":"CREATE");

  if(!outfile || outfile->IsZombie()){
    cerr << "Could not open output file " << outfilename
         << ".  Does it already exist?  "
            "Use -c to overwrite existing output.\n";
    exit(1);
  }

  // Name and title same as in old EnDep code
  recotree = new TTree("otc", "OV time correction tree tree tree");

  recotree->Branch("clocks_forward/I", &outevent.clocks_forward);
  recotree->Branch("error/O", &outevent.error);
}

void root_finish()
{
  gErrorIgnoreLevel = kError;
  outfile->cd();
  recotree->Write();
  outfile->Close();
}

/* Sets up the ROOT input and output. */
uint64_t root_init(const uint64_t maxevent, const bool clobber,
                   char * outfilenm, char ** infiles, const int nfiles)
{
  // ROOT warnings are usually not helpful to the user, so we'll try
  // to catch warning conditions ourselves.  However, I know of at
  // least one error that can't be caught before a seg fault (!), so
  // let ROOT spew about that.
  gErrorIgnoreLevel = kError; 

  root_init_output(clobber, outfilenm);

  const uint64_t nevents = root_init_input(infiles, nfiles);
  uint64_t neventstouse = nevents;
  if(maxevent && nevents > maxevent) neventstouse = maxevent;

  return neventstouse;
}
