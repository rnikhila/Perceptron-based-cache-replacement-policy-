#ifndef REPL_STATE_H
#define REPL_STATE_H
 
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"
#include <iostream>

using namespace std;

#define training_threshold 	78
#define reuse_threshold 	79
#define bypass_threshold	3
#define sat_cnt     16
#define DANSHIP	
// Replacement Policies Supported
typedef enum 
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUstackposition;
    unsigned char reuse;
    UINT32 RRPV;
    UINT32  outcome;
    UINT64  signature_m;
     // CONTESTANTS: Add extra state per cache line here

} LINE_REPLACEMENT_STATE;

typedef struct
{
	unsigned int valid;
	unsigned int lru_status ;
	unsigned long long int partial_tag;
	signed int y_out;
	unsigned long int pc1_table_index;
	unsigned long int pc2_table_index;
	unsigned long long int pc3_table_index;
	unsigned long long int current_pc_table_index;
	unsigned long long int tag_4_table_index ;
	unsigned long long int tag_7_table_index ;
	UINT32 RRPV;
        UINT32  outcome;
        UINT64  signature_m;
		
} sampler;

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
    LINE_REPLACEMENT_STATE   **repl;
    sampler **sampler_array ;
    UINT32 * SHCT;
    UINT32 * SHCT_sampler;
    unsigned long long int default_repl_called;
    unsigned long int false_positives; 
//    int temp_random_counter =0;

//weight tables
   signed int pc1_table[256];
    signed int pc2_table[256];
    signed int pc3_table[256];
    signed int curr_pc_table[256];
    signed int tag_4_table[256];
    signed int tag_7_table[256];
//last 3 pc accesses to the LLc
    UINT64 pc1_LLC;
    UINT64 pc2_LLC;
    UINT64 pc3_LLC;
    UINT64 curr_PC_LLC;

	
  private:

    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;
    int	offset_bits, index_bits,tagshift_bits;
    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here

  public:
    ostream & PrintStats(ostream &out);

    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );

    INT32 GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType );

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID );

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; } 
    void   IncrementTimer() { mytimer++; } 

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit, UINT64 address );

    void   update_sampler(UINT32 sampler_set_index, INT32 updateWayID, UINT64 address, Addr_t PC, signed int y_out,UINT64 tag,UINT64 pc1_hash, UINT64 pc2_hash, UINT64 pc3_hash, UINT64 current_PC_hash, UINT64 tag_4_hash, UINT64 tag_7_hash,bool hit_mispredictio,bool miss_misprediction);	

    void Update_sampler_LRU(UINT32 setIndex, INT32 WayID );
    INT32 Get_sampler_LRU_Victim( UINT32 setIndex );
    INT32  Get_Sampler_RRIP_Victim( UINT32 setIndex );

    INT32 Get_RRIP_Victim( UINT32 setIndex );	
    void Update_RRIP(UINT32 setIndex, INT32 updateWayID, bool cacheHit,Addr_t PC );
    void Update_sampler_RRIP(UINT32 setIndex, INT32 updateWayID, bool SamplerHit, Addr_t PC );

    ~CACHE_REPLACEMENT_STATE(void);

  private:
    
    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    INT32  Get_My_Victim( UINT32 setIndex, UINT64 address,Addr_t PC,UINT32 accessType );
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );
    void   UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID, UINT64 address,Addr_t PC,UINT32 accessType, bool cahehit );
    	
};

#endif
