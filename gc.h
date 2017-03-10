typedef struct {
   int64_t next_state_predict_time;
   int complete;
   uint valid_pages; //# of valid pages have to be copied back 
   int64_t gc_predict_complete_time; 
   int64_t write_interval;
   int64_t last_write_time;
   int64_t gc_interval;
   int64_t gc_start_time;
   uint free_pages; //# of pages being freed
   uint below_hard_threshold;
} gc_status;

typedef struct {
   int64_t gc_predict_complete_time;
   uint channel;
} gc_max_status;
