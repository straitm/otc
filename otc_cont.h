/// Largest number of XY overlaps and tracks that RecoOV should return
/// for one event. Rather than contaminate most of the source here with
/// DCRecoOV.hh and everything that it depends on (ROOT!), just copy
/// this here.
#define OTC_MAX_RECO_OV_OBJ 64
#define OTC_MAXXYHIT 16

struct otc_input_event {
  // All the hits
  OVEventForReco hits;

  // Number of XY overlaps in the Outer Veto
  int nxy;

  // number of hits in each xy overlap
  int xy_nhit[OTC_MAX_RECO_OV_OBJ];

  // the hit indices 
  int xy_hits[OTC_MAX_RECO_OV_OBJ][OTC_MAXXYHIT];
};

struct otc_output_event {

  // How many 16ns clock ticks OTC thinks we should move this event
  // forward to reduce the effect of accidentals.
  int recommended_forward;

  // The largest gap between hits.  This is an exclusive gap, i.e.
  // |   HIT   | NOTHING |   HIT   | --> 1
  int gap;

  // The total length of the event in clock cycles. This length is
  // inclusive, such that an event with all the hits in the same cycle
  // has length 1.
  int length;

  // Number of clock cycles after the first hit of the biggest hit
  int biggest_forward;
 
  // If there was some sort of pathology in the data or the processing
  // thereof.  Currently this means that there were un-time-ordered
  // hits in the input.
  bool error;
};
