rover_input_event get_event(const uint64_t current_event);
uint64_t root_init(const uint64_t maxevent, const bool copyhits,
                   const bool clobber, char * outfile, 
                   char ** infiles, const int nfiles);
void write_event(const std::vector<OVTrackFromReco> & tracks,
                 const std::vector<OVXYFromReco> & xys,
                 const unsigned int EventID);

void root_finish(TH1 * h_loglike);
