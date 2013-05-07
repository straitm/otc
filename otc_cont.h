struct otc_input_event {
  unsigned int EventID;
  OVEventForReco * event_for_reco;
};

struct otc_output_event {

  // How many 16ns clock ticks OTC thinks we should move this event
  // forward to reduce the effect of accidentals.
  int clocks_forward;

  // If there was some sort of pathology in the data or the processing
  // thereof.  Currently this means that there were un-time-ordered
  // hits in the input.
  bool error;
};
