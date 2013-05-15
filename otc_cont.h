const unsigned int MAXOVHITS = 64*60;

struct OVEventForReco {
  /// Number of hits in this event
  unsigned int nhit;

  /// Global channel numbers. In OVHitInfo, this is a DC::ChNum, which
  /// is a typedef for unsigned int. I'm matching the length here for
  /// efficiency, but generically it's the caller's responsiblity to
  /// make sure that the input is translated if it doesn't start out as
  /// four bytes.
  unsigned int ChNum[MAXOVHITS];

  /// The type of hit. Equal to 2 for ordinary hits and 4 for edge
  /// triggers.  See comments on size in ChNum.
  unsigned short Status[MAXOVHITS];

  /// The integrated ADC counts. As of this writing, these are stored
  /// in muon.root files as doubles (typedef DC::PE), but this does
  /// not make sense because they are strictly integer valued. For
  /// the moment, let's take an efficiency hit to translate them to
  /// integers with an eye towards having them be integers in the files
  /// in the future.  This way (new) RecoOV can be written to handle
  /// integers from now on.
  int Q[MAXOVHITS];

  /// The number of 16ns clock ticks since the last rollover. The clock
  /// rolls over every 2^29 ticks. This is currently stored in muon.root
  /// files as a double, which doesn't make sense, and represented by
  /// typedef DC::T_ns, which also doesn't make sense, because it does
  /// not represent nanoseconds, but rather counts of 16ns. As above,
  /// I'm taking a stand and forcing conversion to integers here.
  int Time[MAXOVHITS];
};

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
