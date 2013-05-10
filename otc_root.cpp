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
#include "TSystem.h"
#include "TChain.h"
#include "TFile.h"
#include "TError.h"
#include "TClonesArray.h"
#include "DCRecoOV.hh" 
#include "otc_cont.h"


namespace {
  otc_input_event inevent;
  otc_output_event outevent;

  // These are needed to get the ADC counts and clock ticks out of the
  // muon.root files before we cast them to integers and put them in
  // inevent.
  double floatingQ[MAXOVHITS], floatingTime[MAXOVHITS];
  vector<TTree *> hitchain;
  vector<uint64_t> hitchain_entries;
  vector<TTree *> recochain;
  vector<uint64_t> recochain_entries;
  bool inputismc = false;

  // Needed for writing the output file
  TFile * outfile;
  TTree * recotree;
}; 

static void get_hits(const uint64_t current_event)
{
  // Go through some contortions for speed. Favor TBranch::GetEntry over
  // TTree::GetEntry, which loops through unused branches on every call.
  // Avoid using TChain to find the TTrees' branches on every call.
  static TBranch * qbr = 0, * timebr = 0, * chbr = 0, 
                 * statbr = 0;

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

    curtree = hitchain[++curtreeindex];
    nextbreak = hitchain_entries[curtreeindex+1];
    offset = hitchain_entries[curtreeindex];

    curtree->SetMakeClass(1);
    chbr   = curtree->GetBranch("OVHitInfoBranch.fChNum");
    statbr = curtree->GetBranch("OVHitInfoBranch.fStatus");
    qbr    = curtree->GetBranch("OVHitInfoBranch.fQ");
    timebr = curtree->GetBranch("OVHitInfoBranch.fTime");
    int dummy;
    curtree->SetBranchAddress("OVHitInfoBranch", &dummy);
    curtree->SetBranchAddress("OVHitInfoBranch.fChNum", inevent.hits.ChNum);
    curtree->SetBranchAddress("OVHitInfoBranch.fStatus",inevent.hits.Status);
    curtree->SetBranchAddress("OVHitInfoBranch.fQ",     floatingQ);
    curtree->SetBranchAddress("OVHitInfoBranch.fTime",  floatingTime);
  }

  const uint64_t localentry = current_event - offset;

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
  inevent.hits.nhit = chbr->GetEntry(localentry)/sizeof(unsigned int) - 1;

  // It's awkward to avoid having already filled an array at the first
  // moment that nhit is known. If the number of events is truly huge,
  // it will segfault. However, if it's only a little too large, since
  // inevent.hits.ChNum comes before several other arrays in OVEventForReco,
  // we will usually survive and can abort more cleanly here.
  if(inevent.hits.nhit > MAXOVHITS){
    fprintf(stderr, "Crazy event with %d hits! I just ran "
            "off the end of some arrays, so I'm bailing out!\n",
             inevent.hits.nhit);
    exit(1);
  }
  else if(inevent.hits.nhit == 0 && !inputismc){
    fprintf(stderr, "Event with no hits. Unexpected in data. Is this Monte "
            "Carlo missing OVHitThInfoTree?\n");
  }

  statbr->GetEntry(localentry);

  qbr->GetEntry(localentry);
  for(unsigned int i = 0; i < inevent.hits.nhit; i++)
    inevent.hits.Q[i] = int(floatingQ[i]); 

  timebr->GetEntry(localentry);
  for(unsigned int i = 0; i < inevent.hits.nhit; i++)
    inevent.hits.Time[i] = int(floatingTime[i]);
}

void get_reco(const uint64_t current_event)
{
  static TBranch * nhitbr = 0, * hitsbr = 0;

  static uint64_t offset = 0, nextbreak = 0;

  if(current_event == 0 || current_event == nextbreak){
    static TTree * curtree = 0;
    static int curtreeindex = -1;
    if(current_event == 0){
      offset = nextbreak = 0;
      curtreeindex = -1;
    }

    curtree = recochain[++curtreeindex];
    nextbreak = recochain_entries[curtreeindex+1];
    offset = recochain_entries[curtreeindex];

    curtree->SetMakeClass(1);
    nhitbr = curtree->GetBranch("xy.nhit");
    hitsbr = curtree->GetBranch("xy.hits[16]");
    int dummy;
    curtree->SetBranchAddress("xy", &dummy),
    curtree->SetBranchAddress("xy.nhit", inevent.xy_nhit),
    curtree->SetBranchAddress("xy.hits[16]", inevent.xy_hits);
  }

  const uint64_t localentry = current_event - offset;
  inevent.nxy = nhitbr->GetEntry(localentry)/sizeof(int) - 1;

  if(inevent.nxy > OTC_MAX_RECO_OV_OBJ){
    fprintf(stderr, "AAAAahhhh %d XY overlaps!\n", inevent.nxy);
    exit(1);
  }

  hitsbr->GetEntry(localentry);
}


/** Make inevent the eventn'th event in the chain. */
otc_input_event get_event(const uint64_t current_event)
{
  memset(&inevent, 0, sizeof(inevent));
  get_hits(current_event);
  get_reco(current_event);

  return inevent;
}

void write_event(const otc_output_event & out)
{
  outevent = out;
  recotree->Fill();
}

static uint64_t root_init_input(const char * const * const filenames,
                                const int nfiles)
{
  TChain mctestchain("OVHitThInfoTree");

  uint64_t totentries_hit = 0, totentries_reco = 0;

  for(int i = 0; i < nfiles; i++){
    const char * const fname = filenames[i];
    if(strlen(fname) < 9){
      fprintf(stderr, "%s doesn't have the form *muon*.root\n", fname);
      _exit(1);
    }
    if(!strstr(fname, "muon")){
      fprintf(stderr, "File name %s does not contain \"muon\"\n", fname);
      _exit(1);
    }
    if(strstr(fname, ".root") != fname + strlen(fname) - 5){
      fprintf(stderr, "File name %s does not end in \".root\"\n", fname);
      _exit(1);
    }

    TFile * inputfile = TFile::Open(fname, "read");
    if(!inputfile || inputfile->IsZombie()){
      fprintf(stderr, "%s became a zombie when ROOT tried to read it.\n",fname);
      _exit(1);
    }

    TTree * temp=dynamic_cast<TTree*>(inputfile->Get("OVHitInfoTree"));
    if(!temp){
      fprintf(stderr, "%s does not have an OVHitInfoTree tree\n", fname);
      _exit(1);
    }

    hitchain_entries.push_back(totentries_hit);
    hitchain.push_back(temp);
    totentries_hit += temp->GetEntries();

    temp = dynamic_cast<TTree*>(inputfile->Get("RecoOVInfoTree"));
    if(!temp){
      fprintf(stderr, "%s does not have a RecoOVInfoTree tree\n", fname);
      _exit(1);
    }

    recochain_entries.push_back(totentries_reco);
    recochain.push_back(temp);
    totentries_reco += temp->GetEntries();

    {
      const int old_geil = gErrorIgnoreLevel;
      gErrorIgnoreLevel = kFatal;
      mctestchain.Add(fname);
      if(mctestchain.GetEntries() != 0) inputismc = true;
      gErrorIgnoreLevel = old_geil; 
    }

    printf("Loaded %s\n", fname);
  }

  if(totentries_hit != totentries_reco){
    fprintf(stderr, "ERROR: hit tree has %ld entries, but reco tree has %ld\n",
            totentries_hit, totentries_reco);
    exit(1);
  }

  return totentries_hit;
}

static void root_init_output(const bool clobber,
                             const char * const outfilename)
{
  outfile = new TFile(outfilename, clobber?"RECREATE":"CREATE");

  if(!outfile || outfile->IsZombie()){
    fprintf(stderr, "Could not open output file %s. Does it already exist?  "
            "Use -c to overwrite existing output.\n", outfilename);
    exit(1);
  }

  // Name and title same as in old EnDep code
  recotree = new TTree("otc", "OV time correction tree tree tree");

  recotree->Branch("recommended_forward", &outevent.recommended_forward);
  recotree->Branch("biggest_forward", &outevent.biggest_forward);
  recotree->Branch("length", &outevent.length);
  recotree->Branch("gap", &outevent.gap);
  recotree->Branch("error", &outevent.error);
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
                   const char * const outfilenm,
                   const char * const * const infiles, const int nfiles)
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
