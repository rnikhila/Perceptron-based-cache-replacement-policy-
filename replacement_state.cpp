#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <map>
#include <iostream>

using namespace std;

#include "replacement_state.h"

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

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;
    
    	int i, m = numsets , c = -1;
	for (i=0; m; i++) {
		m /= 2;
		c++;
	}
        index_bits = c;
	offset_bits = 6;
	tagshift_bits = (c+6) ;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here
    
    return out;

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways

    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state

    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
	    repl[ setIndex ][ way ].RRPV = 7;
            repl[ setIndex ][ way ].outcome = 0;
            repl[ setIndex ][ way ].signature_m = 0;
	
        }
    }

    if (replPolicy != CRC_REPL_CONTESTANT) return;

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE
    UINT32 num_sampler_sets = (numsets/64);

//initializing the sampler array and lru bits // ?? shld i initialize addresses and all too ?
    sampler_array = new sampler* [num_sampler_sets];
    SHCT = new UINT32 [16*1024]; 
    SHCT_sampler = new UINT32 [256];
    assert(sampler_array);
    
    for(UINT32 i = 0; i< num_sampler_sets ; i++)
    {
	sampler_array[i] = new sampler[assoc];

	for(UINT32 j = 0; j<assoc; j++)
	{
		sampler_array[i][j].lru_status = j;
		sampler_array[i][j].RRPV = 3;
           	sampler_array[i][j].outcome = 0;
           	sampler_array[i][j].signature_m = 0;

	}

    }

    for(int i =0; i<(16*1024) ; i++)
        {
                SHCT[i] = 0;
        }
    for(int i =0; i<(256) ; i++)
        {
                SHCT_sampler[i] = 0;
        }
	
//initializing the weight tables 
    for(UINT32 k = 0; k<256 ; k++)
    {
	pc1_table[k] = 0;
	pc2_table[k] = 0;
	pc3_table[k] = 0;
	curr_pc_table[k] = 0;
	tag_4_table[k] = 0;
        tag_7_table[k] = 0;
    }
default_repl_called = 0;
false_positives = 0;	
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType ) {
    // If no invalid lines, then replace based on replacement policy
    if( replPolicy == CRC_REPL_LRU ) 
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE

	return Get_My_Victim (setIndex, paddr ,PC,accessType);
    }

    // We should never here here

    assert(0);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState( 
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit, UINT64 address )
{
	//fprintf (stderr, "ain't I a stinker? %lld\n", get_cycle_count ());
	//fflush (stderr);
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU ) 
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR UPDATE REPLACEMENT STATE FUNCTION HERE
        // Feel free to use any of the input parameters to make
        // updates to your replacement policy

	UpdateMyPolicy( setIndex, updateWayID, address,PC,accessType,cacheHit); 
	
	
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
	// Get pointer to replacement state of current set

	LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
	INT32   lruWay   = 0;

	// Search for victim whose stack position is assoc-1

	for(UINT32 way=0; way<assoc; way++) {
		if (replSet[way].LRUstackposition == (assoc-1)) {
			lruWay = way;
			break;
		}
	}

	// return lru way

	return lruWay;
}

INT32 CACHE_REPLACEMENT_STATE::Get_sampler_LRU_Victim( UINT32 setIndex )
{
	//get pointer to replacement state of currnt set
	
	sampler *sampler_set = sampler_array[setIndex];
	INT32 sampler_lruway =0;

	// search for victim whose stack position is assoc-1
	for(UINT32 way=0; way<assoc; way++) {
		if (sampler_set[way].lru_status == (assoc-1))
		{
			sampler_lruway = way;
			break;
		}
	}

	//returning lru way of sampler
	
	return sampler_lruway;
}

INT32 CACHE_REPLACEMENT_STATE::Get_RRIP_Victim( UINT32 setIndex ) {
        // return first way always
//      printf(" IN get my victim or set %d \n", setIndex);
        unsigned int i;
        label: for(i= 0; i<assoc;i++)
        {
                if(repl[ setIndex ][ i ].RRPV == 3)
                break;
        }
        if(i!= assoc)
        {
               if(repl[ setIndex ][i].outcome == 0)
        {
                if(SHCT[repl[ setIndex ][i].signature_m] > 0) SHCT[repl[ setIndex ][i].signature_m] --;
        }
 
  //      printf(" victim found in  set %d and for wayid %d \n", setIndex,i);
        return i;
        }
        for(i=0; i<assoc; i++)
        {
//      printf(" victim not found, so incrementing all the ways ... currently at i = %d",i);    
        repl[ setIndex ][ i ].RRPV ++;
        }
        goto label; 
}

INT32 CACHE_REPLACEMENT_STATE::Get_Sampler_RRIP_Victim( UINT32 setIndex ) {
        // return first way always
//      printf(" IN get my victim or set %d \n", setIndex);
	sampler *sampler_set = sampler_array[setIndex];

        unsigned int way;
        label: for(way= 0; way<assoc;way++)
        {
                if(sampler_set[way].RRPV == 3)
                break;
        }
        if(way!= assoc)
        {
       		if(sampler_set[way].outcome == 0)
		{
			if(SHCT_sampler[sampler_set[way].signature_m] > 0) SHCT_sampler[sampler_set[way].signature_m] --;
		} 
  //      printf(" victim found in  set %d and for wayid %d \n", setIndex,i);
        return way;
        }
        for(way=0; way<assoc; way++)
        {
//      printf(" victim not found, so incrementing all the ways ... currently at i = %d",i);    
        sampler_set[way].RRPV ++;
        }
        goto label; 
}
	
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);
    
    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
	// Determine current LRU stack position
	UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<assoc; way++) {
		if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) {
			repl[setIndex][way].LRUstackposition++;
		}
	}

	// Set the LRU stack position of new line to be zero
	repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}
void CACHE_REPLACEMENT_STATE::Update_sampler_LRU(UINT32 setIndex, INT32 WayID )
{
	UINT32 currLRUstackposition = sampler_array[setIndex][WayID].lru_status;
	
	for(UINT32 way=0; way<assoc; way++) {
		if( sampler_array[setIndex][way].lru_status < currLRUstackposition)
		{
			sampler_array[setIndex][way].lru_status++;
		}
	}

	sampler_array[setIndex][WayID].lru_status = 0;
}
	
void CACHE_REPLACEMENT_STATE::Update_RRIP(UINT32 setIndex, INT32 updateWayID, bool cacheHit,Addr_t PC )
{
        UINT64 signature;
        signature = PC % 0x3FFF ;

        if(cacheHit)
        {
        if( repl[ setIndex ][ updateWayID ].RRPV != 0) repl[ setIndex ][ updateWayID ].RRPV = 0;
	        repl[ setIndex ][ updateWayID ].outcome = 1;
        if(SHCT[repl[ setIndex ][ updateWayID ].signature_m] < 4) SHCT[repl[ setIndex ][ updateWayID ].signature_m]++ ;

//      printf("its a cache hit , RRPV of [%d][%d] = %d \n",setIndex, updateWayID, repl[ setIndex ][ updateWayID ].RRPV);
        }
        else
        {
        repl[ setIndex ][ updateWayID ].outcome = 0;
        repl[ setIndex ][ updateWayID ].signature_m = signature;
        if(SHCT[signature] == 0) repl[ setIndex ][ updateWayID ].RRPV = 3;
        else repl[ setIndex ][ updateWayID ].RRPV = 2;

//      printf("its a cache MISS ## , RRPV of [%d][%d] = %d \n",setIndex, updateWayID, repl[ setIndex ][ updateWayID ].RRPV);
        }
}

 
void CACHE_REPLACEMENT_STATE::Update_sampler_RRIP(UINT32 setIndex, INT32 updateWayID, bool SamplerHit, Addr_t PC )
{       
        UINT64 signature;
        signature = PC % 0xFF ;
 

        if(SamplerHit)
        {
        if( sampler_array[ setIndex ][ updateWayID ].RRPV != 0) sampler_array[ setIndex ][ updateWayID ].RRPV = 0;
	sampler_array[ setIndex ][ updateWayID ].outcome = 1;
	if(SHCT_sampler[sampler_array[ setIndex ][ updateWayID ].signature_m] < 4)SHCT_sampler[sampler_array[ setIndex ][ updateWayID ].signature_m]++ ; 
//      printf("its a cache hit , RRPV of [%d][%d] = %d \n",setIndex, updateWayID, repl[ setIndex ][ updateWayID ].RRPV);
        }
        else
        {
	sampler_array[ setIndex ][ updateWayID ].outcome = 0;
	sampler_array[ setIndex ][ updateWayID ].signature_m = signature;
	if(SHCT_sampler[signature] == 0) sampler_array[ setIndex ][ updateWayID ].RRPV =3;
        sampler_array[ setIndex ][ updateWayID ].RRPV = 2;
//      printf("its a cache MISS ## , RRPV of [%d][%d] = %d \n",setIndex, updateWayID, repl[ setIndex ][ updateWayID ].RRPV);
        }
}

INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim( UINT32 setIndex , UINT64 address,Addr_t PC,UINT32 accessType)
{
	if(accessType != ACCESS_PREFETCH)
	{
	// calculate the hashes of the 6 features of the incoming block 

	UINT64 pc1_hash = ((pc1_LLC>>1) ^ PC) & 0xFF ;
	UINT64 pc2_hash = ((pc2_LLC>>2) ^ PC) & 0xFF ;
	UINT64 pc3_hash = ((pc3_LLC>>3) ^ PC) & 0xFF ;
	UINT64 tag_4_hash = (((address >> tagshift_bits) >> 4) ^ PC ) & 0xFF ;
	UINT64 tag_7_hash = (((address >> tagshift_bits) >> 7) ^ PC ) & 0xFF ;
	UINT64 current_pc_hash = ((curr_PC_LLC >> 2)^PC)&0x0FF;

	// calculate the y_out of the incoming block using the indexed weights in the 6 tables
	signed int y_out = curr_pc_table[current_pc_hash] + pc1_table[pc1_hash] + pc2_table[pc2_hash] + pc3_table[pc3_hash] + tag_4_table[tag_4_hash] + tag_7_hash[tag_7_table] ;

	// if dead on arrival, return -1
	if (y_out > bypass_threshold ) return -1;

	// else find a block to replace
	
	// if its not a sampled set, find a dead block to replace
//	if ((setIndex + 1) % 64 != 0)
//	{
	unsigned int i;
	// find the dead block
	for(i=0;i<assoc;i++)
	{
		if (repl[setIndex][i].reuse == 0)
         	break;
	}
	if(i != assoc) // dead_blok found
	return i;
//	}
	// when it is reached here, either a dead block is not found or its in a sampled set , in both cases we have to find a aLRU in the set to evict
		default_repl_called ++ ;
//	return Get_LRU_Victim(setIndex) ;
	return Get_RRIP_Victim( setIndex );
	}
	else return -1 ;
}

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID, UINT64 address,Addr_t PC,UINT32 accessType,bool cacheHit) 
{	
	bool hit_misprediction = false;
	bool miss_misprediction = false;	

	if(accessType != ACCESS_PREFETCH)
	{
	// updating last 3 PC addresses. 
	pc3_LLC = pc2_LLC;
	pc2_LLC = pc1_LLC;
	pc1_LLC = curr_PC_LLC;
	curr_PC_LLC = PC;

	// updating the LRU bit
//	UpdateLRU( setIndex, updateWayID );
	Update_RRIP( setIndex, updateWayID,cacheHit, PC);

	// computing tag
	UINT64 tag = (address >> tagshift_bits) ;

	// getting hashes of all the features
	UINT64 pc1_hash = ((pc1_LLC>>1) ^ PC) & 0xFF ;
	UINT64 pc2_hash = ((pc2_LLC>>2) ^ PC) & 0xFF ;
	UINT64 pc3_hash = ((pc3_LLC>>3) ^ PC) & 0xFF ;
	UINT64 current_pc_hash = ((PC >> 2)^PC)&0x0FF;
	UINT64 tag_4_hash = ((tag >> 4)^PC) & 0x0FF ;
	UINT64 tag_7_hash = ((tag >> 7)^PC) & 0x0FF ;
	
	
	// computing y_out
	signed int y_out = pc1_table[pc1_hash] + pc2_table[pc2_hash] + pc3_table[pc3_hash] + curr_pc_table[current_pc_hash] + tag_4_table[tag_4_hash] + tag_7_table[tag_7_hash];

	//seeing if its a mis prediction in case of both hit and miss
	if(cacheHit)
	{
		if ((y_out > bypass_threshold) ) // incorrectly predicted as not reused 
		{
			hit_misprediction = true;
			false_positives ++;
		}
	}
	if(!cacheHit) //incase of miss
	{
		if((y_out < reuse_threshold) ) miss_misprediction = true ;
	}
	
	//training the weight tables on misprediction 
// new optimization tried, instead of training only the mispredictions from sampled set, tried updating the prediction tables on every misprediction of a hit ... got this idea from 4.2.3 of sampling dead block predictor where it says false positives are more dangerous than false negatives. best performance achieved wen decremented only once -------- ##### include this in report #### 
	if(hit_misprediction) 
	{
			if(curr_pc_table[current_pc_hash] > -32) curr_pc_table[current_pc_hash] --;
			if(pc1_table[pc1_hash] > -32 ) pc1_table[pc1_hash]--;
			if(pc2_table[pc2_hash] > -32) pc2_table[pc2_hash]--;
		        if(pc3_table[pc3_hash] > -32) pc3_table[pc3_hash]--;
			if(tag_4_table[tag_4_hash] > -32) tag_4_table[tag_4_hash]--;
			if(tag_7_table[tag_7_hash] > -32) tag_7_table[tag_7_hash]--;
	}
// not doing it for victim block because doing it only for the sampled sets.
/*
	if(miss_misprediction)  
	{
			if(curr_pc_table[current_pc_hash] < 32) curr_pc_table[current_pc_hash] ++;
			if(pc1_table[pc1_hash] < 32 ) pc1_table[pc1_hash]++;
			if(pc2_table[pc2_hash] < 32) pc2_table[pc2_hash]++;
		        if(pc3_table[pc3_hash] < 32) pc3_table[pc3_hash]++;
			if(tag_4_table[tag_4_hash] < 32) tag_4_table[tag_4_hash]++;
			if(tag_7_table[tag_7_hash] < 32) tag_7_table[tag_7_hash]++;
	}			
	*/
	// making prediction and updating the reuse bit 
	if(y_out <  reuse_threshold) repl[ setIndex ][ updateWayID ].reuse = 1;
	else repl[ setIndex ][ updateWayID ].reuse = 0;

	// see if the set accessed is in the sampler, if yes update it
	if ((setIndex + 1) % 64 == 0) 
	{	
		UINT32 sampler_set_index = ((setIndex + 1)/64) - 1;
		update_sampler(sampler_set_index,updateWayID,address,PC,y_out,tag,pc1_hash,pc2_hash,pc3_hash,current_pc_hash,tag_4_hash,tag_7_hash, hit_misprediction, miss_misprediction);
	}
	}	
//	printf("curr_pc = %lld , pc1 = %lld , pc2 = %lld , pc3 = %lld ...\n ",curr_PC_LLC,pc1_LLC,pc2_LLC,pc3_LLC);
//	printf("pc1_hash = %lld , pc2_hash = %lld , pc3_hash = %lld ... \n", pc1_hash,pc2_hash,pc3_hash);
		
}

void CACHE_REPLACEMENT_STATE::update_sampler(UINT32 sampler_set_index, INT32 updateWayID, UINT64 address, Addr_t PC, signed int y_out, UINT64 tag, UINT64 pc1_hash, UINT64 pc2_hash, UINT64 pc3_hash, UINT64 current_PC_hash, UINT64 tag_4_hash, UINT64 tag_7_hash,bool hit_misprediction,bool miss_misprediction)
{
	




	// see if the tag is there in the sampler set
	unsigned int i;
	for(i = 0; i<assoc; i++)
	{
		if ((sampler_array[sampler_set_index][i].partial_tag )== (tag & 0XFFFF))
		break;
	}
	if(i == assoc) // tag not found // sampler miss 
	{
		UINT32 victim_way = assoc;
		// find an invalid block in sampler 
		for(i = 0; i<assoc; i++)
		{
		if(sampler_array[sampler_set_index][i].valid == 0)
		{	
			victim_way = i;
			sampler_array[sampler_set_index][i].valid = 1; 
			break;
		}
		}
		//if invalid block not found, getting the LRU victim

		if(victim_way == assoc)
		{  
//		victim_way = Get_sampler_LRU_Victim(sampler_set_index); 
		victim_way = Get_Sampler_RRIP_Victim (sampler_set_index);
		}
		// use the victim's features to update the weights 
		
		if((sampler_array[sampler_set_index][victim_way].y_out < (training_threshold))|| miss_misprediction)
		{
		if(curr_pc_table[sampler_array[sampler_set_index][victim_way].current_pc_table_index] < sat_cnt) curr_pc_table[sampler_array[sampler_set_index][victim_way].current_pc_table_index]++;
		if(pc1_table[sampler_array[sampler_set_index][victim_way].pc1_table_index] < sat_cnt) pc1_table[sampler_array[sampler_set_index][victim_way].pc1_table_index]++;
		if(pc2_table[sampler_array[sampler_set_index][victim_way].pc2_table_index]< sat_cnt) pc2_table[sampler_array[sampler_set_index][victim_way].pc2_table_index]++;
		if(pc3_table[sampler_array[sampler_set_index][victim_way].pc3_table_index]  < sat_cnt)pc3_table[sampler_array[sampler_set_index][victim_way].pc3_table_index]++;
		if(tag_4_table[sampler_array[sampler_set_index][victim_way].tag_4_table_index] < sat_cnt) tag_4_table[sampler_array[sampler_set_index][victim_way].tag_4_table_index]++;
		if(tag_7_table[sampler_array[sampler_set_index][victim_way].tag_7_table_index] < sat_cnt) tag_7_table[sampler_array[sampler_set_index][victim_way].tag_7_table_index]++;
		}			 
			sampler_array[sampler_set_index][victim_way].pc1_table_index = pc1_hash;
			sampler_array[sampler_set_index][victim_way].pc2_table_index = pc2_hash;
			sampler_array[sampler_set_index][victim_way].pc3_table_index = pc3_hash;
			sampler_array[sampler_set_index][victim_way].current_pc_table_index = current_PC_hash;
			sampler_array[sampler_set_index][victim_way].tag_4_table_index = tag_4_hash;
			sampler_array[sampler_set_index][victim_way].tag_7_table_index = tag_7_hash;
			sampler_array[sampler_set_index][victim_way].y_out = y_out;
	               	sampler_array[sampler_set_index][victim_way].partial_tag = (tag & 0XFFFF);
		
//		Update_sampler_LRU( sampler_set_index, victim_way );
		Update_sampler_RRIP(sampler_set_index,victim_way,1,PC); // 0 is for sampler_hit = 0
	}

	if(i!= assoc) // sampler hit
	{
		if(sampler_array[sampler_set_index][i].y_out > ((-1)*( training_threshold))||(y_out > bypass_threshold))  // less than threshold or misprediction then update tables 
		{
			//decrement the weights
			if(curr_pc_table[sampler_array[sampler_set_index][i].current_pc_table_index] > -sat_cnt) curr_pc_table[sampler_array[sampler_set_index][i].current_pc_table_index] --;
			if(pc1_table[sampler_array[sampler_set_index][i].pc1_table_index] > -sat_cnt ) pc1_table[sampler_array[sampler_set_index][i].pc1_table_index]--;
			if(pc2_table[sampler_array[sampler_set_index][i].pc2_table_index] > -sat_cnt) pc2_table[sampler_array[sampler_set_index][i].pc2_table_index]--;
		        if(pc3_table[sampler_array[sampler_set_index][i].pc3_table_index] > -sat_cnt) pc3_table[sampler_array[sampler_set_index][i].pc3_table_index]--;
			if(tag_4_table[sampler_array[sampler_set_index][i].tag_4_table_index] > -sat_cnt) tag_4_table[sampler_array[sampler_set_index][i].tag_4_table_index]--;
			if(tag_7_table[sampler_array[sampler_set_index][i].tag_7_table_index] > -sat_cnt) tag_7_table[sampler_array[sampler_set_index][i].tag_7_table_index]--;
		}

			sampler_array[sampler_set_index][i].pc1_table_index = pc1_hash;
			sampler_array[sampler_set_index][i].pc2_table_index = pc2_hash;
			sampler_array[sampler_set_index][i].pc3_table_index = pc3_hash;
			sampler_array[sampler_set_index][i].current_pc_table_index = current_PC_hash;
			sampler_array[sampler_set_index][i].tag_4_table_index = tag_4_hash;
			sampler_array[sampler_set_index][i].tag_7_table_index = tag_7_hash;
			sampler_array[sampler_set_index][i].y_out = y_out;
			sampler_array[sampler_set_index][i].partial_tag = (tag & 0XFFFF);	
			
//		Update_sampler_LRU( sampler_set_index, i );	
		Update_sampler_RRIP(sampler_set_index,i,1,PC); // 1 is for sampler_hit = 1

	}

	
}

 CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE (void) {
}
