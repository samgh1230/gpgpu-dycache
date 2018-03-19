// Copyright (c) 2009-2011, Tor M. Aamodt, Tayler Hetherington
// The University of British Columbia
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice, this
// list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
// Neither the name of The University of British Columbia nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GPU_CACHE_H
#define GPU_CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gpu-misc.h"
#include "mem_fetch.h"
#include "../abstract_hardware_model.h"
#include "../tr1_hash_map.h"

#include "addrdec.h"

enum cache_block_state {
    INVALID,
    RESERVED,
    VALID,
    MODIFIED
};

enum cache_request_status {
    HIT = 0,
    HIT_RESERVED,
    MISS,
    RESERVATION_FAIL, 
    NUM_CACHE_REQUEST_STATUS
};

enum cache_event {
    WRITE_BACK_REQUEST_SENT,
    READ_REQUEST_SENT,
    WRITE_REQUEST_SENT
};

const char * cache_request_status_str(enum cache_request_status status); 

struct cache_block_t {
    cache_block_t()
    {
        for(int i=0;i<4;i++)
        {
            m_blk_tag[i]=0;
            m_blk_addr[i]=0;
            m_blk_alloc_time[i]=0;
            m_blk_fill_time[i]=0;
            m_blk_last_access_time[i]=0;
            m_blk_status[i]=INVALID;
            m_fine_grained = false;
        }
        m_referred_chunk.reset();
        m_tag = 0;
        m_addr = 0;
        m_alloc_time = 0;
        m_fill_time = 0;
        m_last_access_time = 0;
        m_status = INVALID;
    }
    void allocate(new_addr_type tag, new_addr_type blk_addr, unsigned time)
    {
        m_tag = tag;
        m_addr = tag;
        m_alloc_time = time;
        m_last_access_time = time;
        m_status = RESERVED; 
    }
    void allocate( new_addr_type c_tag, new_addr_type tag, unsigned time , unsigned sid/*index free start chunck*/, unsigned blksz,unsigned data_size)
    {
        m_common_tag = c_tag;
        switch(blksz){
            case 128:
                assert(sid==0);
                for(int i=0;i<4;i++){
                    m_blk_tag[i]=tag++;//tag for each chunck
                    m_blk_addr[i]=tag++;
                    m_blk_alloc_time[i]=time;
                    m_blk_last_access_time[i]=time;
                    m_blk_fill_time[i]=0;
                    m_blk_status[i]=RESERVED;
                }
            break;
            case 64:
                switch(data_size){
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++)
                        {
                            m_blk_tag[i]=tag++;
                            m_blk_addr[i]=tag++;
                            m_blk_alloc_time[i]=time;
                            m_blk_last_access_time[i]=time;
                            m_blk_fill_time[i]=0;
                            m_blk_status[i]=RESERVED;
                        }
                    break;
                    case 64:
                        assert(sid==0||sid==2);
                        for(int i=0;i<2;i++)
                        {
                            m_blk_tag[sid+i]=tag++;
                            m_blk_addr[sid+i]=tag++;
                            m_blk_alloc_time[sid+i]=time;
                            m_blk_last_access_time[sid+i]=time;
                            m_blk_fill_time[sid+i]=0;
                            m_blk_status[sid+i]=RESERVED;
                        }
                    break;
                    case 32:
                        assert(sid==0||sid==2);
                        //assert(false);
                        for(int i=0;i<2;i++)
                        {
                            m_blk_tag[sid+i]=tag++;
                            m_blk_addr[sid+i]=tag++;
                            m_blk_alloc_time[sid+i]=time;
                            m_blk_last_access_time[sid+i]=time;
                            m_blk_fill_time[sid+i]=0;
                            m_blk_status[sid+i]=RESERVED;
                        }
                    break;
                }
            break;
            case 32:
                switch(data_size){
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++){
                            m_blk_tag[i]=tag++;
                            m_blk_addr[i]=tag++;
                            m_blk_alloc_time[i]=time;
                            m_blk_last_access_time[i]=time;
                            m_blk_fill_time[i]=0;
                            m_blk_status[i]=RESERVED;
                        }
                    break;
                    case 64:
                        //assert(sid==0||sid==2);
                        for(int i=0;i<2;i++){
                            m_blk_tag[sid+i]=tag++;
                            m_blk_addr[sid+i]=tag++;
                            m_blk_alloc_time[sid+i]=time;
                            m_blk_last_access_time[sid+i]=time;
                            m_blk_fill_time[sid+i]=0;
                            m_blk_status[sid+i]=RESERVED;
                        }
                    break;
                    case 32:
                        m_blk_tag[sid]=tag;
                        m_blk_addr[sid]=tag;
                        m_blk_alloc_time[sid]=time;
                        m_blk_last_access_time[sid]=time;
                        m_blk_fill_time[sid]=0;
                        m_blk_status[sid]=RESERVED;
                    break;
                }
            break;
        }
    }
    void fill(unsigned time)
    {
        m_status = VALID;
        m_fill_time = time;
    }
    void fill( unsigned time , unsigned sid, unsigned blksz, unsigned data_size)
    {
        switch(blksz){
            case 128:
                // assert(sid==0);
                for(int i=0;i<4;i++){
                    assert( m_blk_status[i] == RESERVED );
                    m_blk_status[i]=VALID;
                    m_blk_fill_time[i]=time;
                    m_referred_chunk.set(i);
                }
            break;
            case 64:
            switch(data_size)
            {
                case 128:
                    // assert(sid==0);
                    for(int i=0;i<4;i++)
                    {
                        assert(m_blk_status[i]==RESERVED);
                        m_blk_status[i] = VALID;
                        m_blk_fill_time[i]=time;
                        m_referred_chunk.set(i);
                    }
                break;
                case 64:
                    // assert(sid==0||sid==2);
                    for(int i=0;i<2;i++)
                    {
                        assert(m_blk_status[sid+i]==RESERVED);
                        m_blk_status[sid+i]=VALID;
                        m_blk_fill_time[sid+i]=time;
                        m_referred_chunk.set(sid+i);
                    }
                break;
                case 32:
                    // assert(sid==0||sid==2);
                    // assert(false);
                    for(int i=0;i<2;i++)
                    {
                        assert(m_blk_status[sid+i]==RESERVED);
                        m_blk_status[sid+i]=VALID;
                        m_blk_fill_time[sid+i]=time;
                        m_referred_chunk.set(sid+i);
                    }
                break;
            }
            break;
            case 32:
                switch(data_size){
                    case 128:
                    // assert(sid==0);
                    for(int i=0;i<4;i++){
                       assert( m_blk_status[i] == RESERVED );
                        m_blk_status[i]=VALID;
                        m_blk_fill_time[i]=time;
                        m_referred_chunk.set(i);
                    }
                    break;
                    case 64:
                    //assert(sid==0||sid==2);
                    for(int i=0;i<2;i++){
                       assert( m_blk_status[sid+i] == RESERVED );
                        m_blk_status[sid+i]=VALID;
                        m_blk_fill_time[sid+i]=time;
                        m_referred_chunk.set(sid+i);
                    }
                    break;
                    case 32:
                        assert( m_blk_status[sid] == RESERVED );
                        m_blk_status[sid]=VALID;
                        m_blk_fill_time[sid]=time;
                        m_referred_chunk.set(sid);
                    break;
                }
            break;
        }
    }

    void set_last_access_time(unsigned time, unsigned sid, unsigned blksz, unsigned data_size)
    {
        // printf("set last access time: blksz(%d), sid(%d), data_size(%d)\n",blksz,sid,data_size);
        switch(blksz)
        {
            case 128:
                assert(sid==0);
                for(int i=0;i<4;i++){
                    m_blk_last_access_time[i]=time;
                    m_referred_chunk.set(i);
                }
            break;
            case 64:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++){
                            m_blk_last_access_time[i]=time;
                            m_referred_chunk.set(i);
                        }
                    break;
                    case 64:
                    case 32:
                        assert(sid==0||sid==2);
                        for(int i=0;i<2;i++){
                            m_blk_last_access_time[sid+i]=time;
                            m_referred_chunk.set(sid+i);
                        }
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++){
                            m_blk_last_access_time[i]=time;
                            m_referred_chunk.set(i);
                        }
                    break;
                    case 64:
                        //assert(sid==0||sid==2);
                        for(int i=0;i<2;i++){
                            m_blk_last_access_time[sid+i]=time;
                            m_referred_chunk.set(sid+i);
                        }
                    break;
                    case 32:
                        assert(sid<4);
                        m_blk_last_access_time[sid]=time;
                        m_referred_chunk.set(sid);
                    break;
                }
            break;
        }

    }
    void set_alloc_time(unsigned time,unsigned sid,unsigned blksz,unsigned data_size)
    {
        switch(blksz)
        {
            case 128:
                assert(sid==0);
                for(int i=0;i<4;i++)
                    m_blk_alloc_time[i]=time;
            break;
            case 64:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++)
                            m_blk_alloc_time[i]=time;
                    break;
                    case 64:
                    case 32:
                        assert(sid==0||sid==2);
                        for(int i=0;i<2;i++)
                            m_blk_alloc_time[sid+i]=time;
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                        for(int i=0;i<4;i++)
                            m_blk_alloc_time[i]=time;
                    break;
                    case 64:
                        for(int i=0;i<2;i++)
                            m_blk_alloc_time[sid+i]=time;
                    break;
                    case 32:
                        m_blk_alloc_time[sid]=time;
                    break;
                }
            break;
        }
    }
    void set_fill_time(unsigned time,unsigned sid,unsigned blksz,unsigned data_size)
    {
        switch(blksz)
        {
            case 128:
                assert(sid==0);
                for(int i=0;i<4;i++){
                    m_blk_fill_time[i]=time;
                    m_referred_chunk.set(i);
                }
            break;
            case 64:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++){
                            m_blk_fill_time[i]=time;
                            m_referred_chunk.set(i);
                        }
                    break;
                    case 64:
                        assert(sid==0||sid==2);
                        for(int i=0;i<2;i++){
                            m_blk_fill_time[sid+i]=time;
                            m_referred_chunk.set(sid+i);
                        }
                    break;
                    case 32:
                        assert(sid==0||sid==2);
                        //assert(false);
                        for(int i=0;i<2;i++){
                            m_blk_fill_time[sid+i]=time;
                            m_referred_chunk.set(sid+i);
                        }
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++){
                            m_blk_fill_time[i]=time;
                            m_referred_chunk.set(i);
                        }
                    break;
                    case 64:
                        //assert(sid==0||sid==2);
                        for(int i=0;i<2;i++){
                            m_blk_fill_time[sid+i]=time;
                            m_referred_chunk.set(sid+i);
                        }
                    break;
                    case 32:
                        assert(sid<4);
                        m_blk_fill_time[sid]=time;
                        m_referred_chunk.set(sid);
                    break;
                }
            break;
        }
    }
    void set_blk_status(cache_block_state state,unsigned sid,unsigned blksz,unsigned data_size)
    {
        switch(blksz)
        {
            case 128:
                assert(sid==0);
                for(int i=0;i<4;i++)
                    m_blk_status[i]=state;
            break;
            case 64:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++)
                            m_blk_status[i]=state;
                    break;
                    case 64:
                    case 32:
                        assert(sid==0||sid==2);
                        for(int i=0;i<2;i++)
                            m_blk_status[sid+i]=state;
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        for(int i=0;i<4;i++)
                            m_blk_status[i]=state;
                    break;
                    case 64:
                        //assert(sid==0||sid==2);
                        for(int i=0;i<2;i++)
                            m_blk_status[sid+i]=state;
                    break;
                    case 32:
                        assert(sid<4);
                        m_blk_status[sid]=state;
                    break;
                }
            break;
        }
    }
    bool is_modified(unsigned sid,unsigned blksz,unsigned data_size)
    {
        switch(blksz)
        {
            case 128:
            assert(sid==0);
            return (m_blk_status[0]==MODIFIED)||(m_blk_status[1]==MODIFIED)||(m_blk_status[2]==MODIFIED)||(m_blk_status[3]==MODIFIED);
            break;
            case 64:
                assert(sid==0||sid==2);
                switch(data_size)
                {
                    case 128:
                        return (m_blk_status[0]==MODIFIED)||(m_blk_status[1]==MODIFIED)||(m_blk_status[2]==MODIFIED)||(m_blk_status[3]==MODIFIED);
                    break;
                    case 64:
                    case 32:
                        return (m_blk_status[sid]==MODIFIED)||(m_blk_status[sid+1]==MODIFIED);
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                        assert(sid==0);
                        return (m_blk_status[0]==MODIFIED)||(m_blk_status[1]==MODIFIED)||(m_blk_status[2]==MODIFIED)||(m_blk_status[3]==MODIFIED);
                    break;
                    case 64:
                        //assert(sid==0||sid==2);
                        return (m_blk_status[sid]==MODIFIED)||(m_blk_status[sid+1]==MODIFIED);
                    break;
                    case 32:
                        assert(sid<4);
                        return m_blk_status[sid];
                    break;
                }
            break;
        }
    }

    bool chunck_tag_match(new_addr_type chunck_tag, unsigned &sid, unsigned blksz, unsigned data_size)
    {
        new_addr_type ck_tag;
        int i,j;
        switch(blksz){
            case 128:
                // ck_tag = (chunck_tag>>2)<<2;
                /*for(i=0; i<4; i++){
                    if(ck_tag!=m_blk_tag[i]){
                        sid=(unsigned)-1;
                        return false;
                    }
                    else ck_tag++;
                }*/
                if(chunck_tag!=m_blk_tag[0]){
                    sid=(unsigned)-1;
                    return false;
                }
                sid=0;
                return true;
            break;
            case 64:
                switch(data_size){
                    case 128:
                        //ck_tag=(chunck_tag>>2)<<2;
                        
                        for(i=0;i<3;i=i+2)
                        {
                            if(chunck_tag!=m_blk_tag[i]){
                                sid=(unsigned)-1;
                                return false;
                            }
                            else chunck_tag += 2;
                        }
                        sid=0;
                        return true;
                    break;
                    case 64:
                    case 32:
                        chunck_tag=(chunck_tag>>1)<<1;
                        for(i=0;i<4;i=i+2){
                            if(chunck_tag==m_blk_tag[i])
                            {
                                sid = i;
                                return true;
                            }
                        }
                        sid=(unsigned)-1;
                        return false;
                    break;
                }
            break;
            case 32:
                switch(data_size){
                    case 128:
                        //ck_tag=(chunck_tag>>2)<<2;
                        for(i=0;i<4;i++)
                        {
                            if(chunck_tag!=m_blk_tag[i]){
                                sid=(unsigned)-1;
                                return false;
                            }
                            else chunck_tag++;
                        }
                        sid=0;
                        return true;
                    break;
                    case 64:
                        ck_tag=chunck_tag;
                        for(i=0;i<3;i=i+1){
                            for(j=0;j<2;j++){
                                if(ck_tag!=m_blk_tag[i+j])
                                    break;
                                else    
                                    ck_tag++;
                            }
                            if(j==2){
                                sid=i;
                                return true;
                            }
                            ck_tag = chunck_tag;
                        }
                        sid=(unsigned)-1;
                        return false;
                    break;
                    case 32:
                        for(i=0;i<4;i++){
                            if(chunck_tag==m_blk_tag[i]){
                                sid=i;
                                return true;
                            }
                        }
                        sid=(unsigned)-1;
                        return false;
                    break;
                }
            break;
        }
    }
    void set_evicted_blk(unsigned sid, unsigned blksz, unsigned data_size, std::vector<cache_block_t> &evicted)
    {
        switch(blksz)
        {
            case 128:{
                cache_block_t e;
                e.m_evicted_addr = m_blk_addr[0];
                e.m_evicted_size = 128;
                evicted.push_back(e);
            }
            break;
            case 64:
                switch(data_size)
                {
                    case 128:
                        for(int i=0;i<2;i++)
                        {
                            if(m_blk_status[i*2]==MODIFIED)
                            {
                                cache_block_t e;
                                e.m_evicted_addr = m_blk_addr[i*2];
                                e.m_evicted_size = 64;
                                evicted.push_back(e);
                            }
                        }
                    break;
                    case 64:{
                        cache_block_t e;
                        e.m_evicted_addr = m_blk_addr[sid];
                        e.m_evicted_size = 64;
                        evicted.push_back(e);
                    }
                    break;
                    case 32:{
                        if(sid!=0&&sid!=2)
                            sid--;
                        cache_block_t e;
                        e.m_evicted_addr = m_blk_addr[sid];
                        e.m_evicted_size = 64;
                        evicted.push_back(e);
                    }
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                    for(int i=0;i<4;i++)
                    {
                        if(m_blk_status[i]==MODIFIED)
                        {
                            cache_block_t e;
                            e.m_evicted_addr = m_blk_addr[i];
                            e.m_evicted_size = 32;
                            evicted.push_back(e);
                        }
                    }
                    break;
                    case 64:
                        for(int i=0;i<2;i++)
                        {
                            if(m_blk_status[i+sid]==MODIFIED)
                            {
                                cache_block_t e;
                                e.m_evicted_addr = m_blk_addr[i+sid];
                                e.m_evicted_size = 32;
                                evicted.push_back(e);
                            }
                        }
                    break;
                    case 32:{
                        cache_block_t e;
                        e.m_evicted_addr = m_blk_addr[sid];
                        e.m_evicted_size = 32;
                        evicted.push_back(e);
                    }
                    break;
                }
            break;
        }
    }
    //tags for fine&coarse grained management
    new_addr_type    m_common_tag;//high 6-bit
    new_addr_type    m_blk_tag[4];//16-bit
    new_addr_type    m_blk_addr[4];
    unsigned         m_blk_alloc_time[4];
    unsigned         m_blk_last_access_time[4];
    unsigned         m_blk_fill_time[4];
    cache_block_state    m_blk_status[4];
    bool             m_fine_grained;

    std::bitset<4>     m_referred_chunk;
    //unsigned        m_num_chunk_referred;
    
    //tags for 128B management
    new_addr_type   m_tag;
    new_addr_type   m_addr;
    unsigned        m_alloc_time;
    unsigned        m_fill_time;
    unsigned        m_last_access_time;
    cache_block_state   m_status;

    new_addr_type   m_evicted_addr;
    unsigned        m_evicted_size;
};

enum replacement_policy_t {
    LRU,
    FIFO
};

enum write_policy_t {
    READ_ONLY,
    WRITE_BACK,
    WRITE_THROUGH,
    WRITE_EVICT,
    LOCAL_WB_GLOBAL_WT
};

enum allocation_policy_t {
    ON_MISS,
    ON_FILL
};


enum write_allocate_policy_t {
	NO_WRITE_ALLOCATE,
	WRITE_ALLOCATE
};

enum mshr_config_t {
    TEX_FIFO,
    ASSOC // normal cache 
};

enum set_index_function{
    FERMI_HASH_SET_FUNCTION = 0,
    LINEAR_SET_FUNCTION,
    CUSTOM_SET_FUNCTION
};

class cache_config {
public:
    cache_config() 
    { 
        m_valid = false; 
        m_disabled = false;
        m_config_string = NULL; // set by option parser
        m_config_stringPrefL1 = NULL;
        m_config_stringPrefShared = NULL;
        m_data_port_width = 0;
        m_set_index_function = LINEAR_SET_FUNCTION;
    }
    void init(char * config, FuncCache status)
    {
    	cache_status= status;
        assert( config );
        char rp, wp, ap, mshr_type, wap, sif;


        int ntok = sscanf(config,"%u:%u:%u,%c:%c:%c:%c:%c,%c:%u:%u,%u:%u,%u",
                          &m_nset, &m_line_sz, &m_assoc, &rp, &wp, &ap, &wap,
                          &sif,&mshr_type,&m_mshr_entries,&m_mshr_max_merge,
                          &m_miss_queue_size, &m_result_fifo_entries,
                          &m_data_port_width);

        if ( ntok < 11 ) {
            if ( !strcmp(config,"none") ) {
                m_disabled = true;
                return;
            }
            exit_parse_error();
        }
        switch (rp) {
        case 'L': m_replacement_policy = LRU; break;
        case 'F': m_replacement_policy = FIFO; break;
        default: exit_parse_error();
        }
        switch (wp) {
        case 'R': m_write_policy = READ_ONLY; break;
        case 'B': m_write_policy = WRITE_BACK; break;
        case 'T': m_write_policy = WRITE_THROUGH; break;
        case 'E': m_write_policy = WRITE_EVICT; break;
        case 'L': m_write_policy = LOCAL_WB_GLOBAL_WT; break;
        default: exit_parse_error();
        }
        switch (ap) {
        case 'm': m_alloc_policy = ON_MISS; break;
        case 'f': m_alloc_policy = ON_FILL; break;
        default: exit_parse_error();
        }
        switch (mshr_type) {
        case 'F': m_mshr_type = TEX_FIFO; assert(ntok==13); break;
        case 'A': m_mshr_type = ASSOC; break;
        default: exit_parse_error();
        }
        m_line_sz_log2 = LOGB2(m_line_sz);
        m_nset_log2 = LOGB2(m_nset);
        m_valid = true;

        switch(wap){
        case 'W': m_write_alloc_policy = WRITE_ALLOCATE; break;
        case 'N': m_write_alloc_policy = NO_WRITE_ALLOCATE; break;
        default: exit_parse_error();
        }

        // detect invalid configuration 
        if (m_alloc_policy == ON_FILL and m_write_policy == WRITE_BACK) {
            // A writeback cache with allocate-on-fill policy will inevitably lead to deadlock:  
            // The deadlock happens when an incoming cache-fill evicts a dirty
            // line, generating a writeback request.  If the memory subsystem
            // is congested, the interconnection network may not have
            // sufficient buffer for the writeback request.  This stalls the
            // incoming cache-fill.  The stall may propagate through the memory
            // subsystem back to the output port of the same core, creating a
            // deadlock where the wrtieback request and the incoming cache-fill
            // are stalling each other.  
            assert(0 && "Invalid cache configuration: Writeback cache cannot allocate new line on fill. "); 
        }

        // default: port to data array width and granularity = line size 
        if (m_data_port_width == 0) {
            m_data_port_width = m_line_sz; 
        }
        assert(m_line_sz % m_data_port_width == 0); 

        switch(sif){
        case 'H': m_set_index_function = FERMI_HASH_SET_FUNCTION; break;
        case 'C': m_set_index_function = CUSTOM_SET_FUNCTION; break;
        case 'L': m_set_index_function = LINEAR_SET_FUNCTION; break;
        default: exit_parse_error();
        }
    }
    bool disabled() const { return m_disabled;}
    unsigned get_line_sz() const
    {
        assert( m_valid );
        return m_line_sz;
    }
    unsigned get_sid(new_addr_type addr)
    {
        return (addr>>5) & 3;
    }
    unsigned get_num_lines() const
    {
        assert( m_valid );
        return m_nset * m_assoc;
    }

    void print( FILE *fp ) const
    {
        fprintf( fp, "Size = %d B (%d Set x %d-way x %d byte line)\n", 
                 m_line_sz * m_nset * m_assoc,
                 m_nset, m_assoc, m_line_sz );
    }

    virtual unsigned set_index( new_addr_type addr ) const
    {
        if(m_set_index_function != LINEAR_SET_FUNCTION){
            printf("\nGPGPU-Sim cache configuration error: Hashing or "
                    "custom set index function selected in configuration "
                    "file for a cache that has not overloaded the set_index "
                    "function\n");
            abort();
        }
        return(addr >> m_line_sz_log2) & (m_nset-1);
    }

    new_addr_type tag( new_addr_type addr ) const
    {
        // For generality, the tag includes both index and tag. This allows for more complex set index
        // calculations that can result in different indexes mapping to the same set, thus the full
        // tag + index is required to check for hit/miss. Tag is now identical to the block address.

        //return addr >> (m_line_sz_log2+m_nset_log2);
        return addr & ~(m_line_sz-1);
    }
    new_addr_type block_addr( new_addr_type addr ) const
    {
        return addr & ~(m_line_sz-1);
    }
    new_addr_type common_tag(new_addr_type addr)
    {
        return addr >> 27;
    }
    new_addr_type chunck_tag(new_addr_type addr, unsigned blksz, unsigned data_size)
    {
        new_addr_type first_14 = (addr>>13) & (16*1024-1);
        new_addr_type last_2;
        switch(blksz)
        {
            case 128: last_2 = 0; break;
            case 64:
                switch(data_size) 
                {
                    case 128: last_2=0; break;
                    case 64:
                    case 32:
                        last_2 = (addr>>5)&2;
                    break;
                }
            break;
            case 32: 
                switch(data_size){
                    case 128: last_2=0;break;
                    case 64:
                        last_2=(addr>>5)&2;
                    break;
                    case 32:
                        last_2 = (addr>>5)&3;
                    break;
                }
            break;
        }
        first_14 <<= 2;
        return first_14 | last_2;
    }
    FuncCache get_cache_status() {return cache_status;}
    char *m_config_string;
    char *m_config_stringPrefL1;
    char *m_config_stringPrefShared;
    FuncCache cache_status;

protected:
    void exit_parse_error()
    {
        printf("GPGPU-Sim uArch: cache configuration parsing error (%s)\n", m_config_string );
        abort();
    }

    bool m_valid;
    bool m_disabled;
    unsigned m_line_sz;
    unsigned m_line_sz_log2;
    unsigned m_nset;
    unsigned m_nset_log2;
    unsigned m_assoc;

    enum replacement_policy_t m_replacement_policy; // 'L' = LRU, 'F' = FIFO
    enum write_policy_t m_write_policy;             // 'T' = write through, 'B' = write back, 'R' = read only
    enum allocation_policy_t m_alloc_policy;        // 'm' = allocate on miss, 'f' = allocate on fill
    enum mshr_config_t m_mshr_type;

    write_allocate_policy_t m_write_alloc_policy;	// 'W' = Write allocate, 'N' = No write allocate

    union {
        unsigned m_mshr_entries;
        unsigned m_fragment_fifo_entries;
    };
    union {
        unsigned m_mshr_max_merge;
        unsigned m_request_fifo_entries;
    };
    union {
        unsigned m_miss_queue_size;
        unsigned m_rob_entries;
    };
    unsigned m_result_fifo_entries;
    unsigned m_data_port_width; //< number of byte the cache can access per cycle 
    enum set_index_function m_set_index_function; // Hash, linear, or custom set index function

    friend class tag_array;
    friend class baseline_cache;
    friend class read_only_cache;
    friend class tex_cache;
    friend class data_cache;
    friend class l1_cache;
    friend class l2_cache;
};

class l1d_cache_config : public cache_config{
public:
	l1d_cache_config() : cache_config(){}
	virtual unsigned set_index(new_addr_type addr) const;
};

class l2_cache_config : public cache_config {
public:
	l2_cache_config() : cache_config(){}
	void init(linear_to_raw_address_translation *address_mapping);
	virtual unsigned set_index(new_addr_type addr) const;

private:
	linear_to_raw_address_translation *m_address_mapping;
};

class tag_array {
public:
    // Use this constructor
    tag_array(cache_config &config, int core_id, int type_id );
    ~tag_array();

    enum cache_request_status probe( new_addr_type addr, new_addr_type common_tag, new_addr_type chunck_tag, unsigned &idx,unsigned &sid,unsigned blksz,unsigned data_size ) const;
    enum cache_request_status access( new_addr_type addr, new_addr_type common_tag, new_addr_type chunck_tag, unsigned time, unsigned &idx , unsigned &sid,unsigned blksz,unsigned data_size);
    enum cache_request_status access( new_addr_type addr, new_addr_type common_tag, new_addr_type chunck_tag, unsigned time, unsigned &idx, bool &wb, cache_block_t &evicted,unsigned &sid, unsigned blksz, unsigned data_size );

    void fill(new_addr_type addr, new_addr_type common_tag, new_addr_type chunck_tag, unsigned time, unsigned &sid,unsigned blksz,unsigned data_size );
    void fill( unsigned idx, unsigned time ,unsigned &sid,unsigned blksz, unsigned data_size);

    enum cache_request_status probe( new_addr_type addr, unsigned &idx) const;
    enum cache_request_status access( new_addr_type addr, unsigned time, unsigned &idx);
    enum cache_request_status access( new_addr_type addr, unsigned time, unsigned &idx, bool &wb, cache_block_t &evicted);  

    void fill( new_addr_type addr, unsigned time);
    void fill( unsigned idx, unsigned time);

    unsigned size() const { return m_config.get_num_lines();}
    cache_block_t &get_block(unsigned idx) { return m_lines[idx];}

    void flush(); // flash invalidate all entries
    void new_window();

    void print( FILE *stream, unsigned &total_access, unsigned &total_misses ) const;
    float windowed_miss_rate( ) const;
    void get_stats(unsigned &total_access, unsigned &total_misses, unsigned &total_hit_res, unsigned &total_res_fail) const;

	void update_cache_parameters(cache_config &config);

    unsigned words_evicted(unsigned blksz, unsigned data_size){
        switch(blksz){
            case 128:
                return 4;
            break;
            case 64:
                switch(data_size){
                    case 128:
                        return 4;
                    break;
                    case 64:
                    case 32:
                        return 2;
                    break;
                }
            break;
            case 32:
                switch(data_size)
                {
                    case 128:
                    return 4;
                    case 64:
                    return 2;
                    case 32: return 1;
                }
            break;
        }
    }
    unsigned words_referred(unsigned blk_num, unsigned blksz, unsigned data_size, unsigned sid){
        int i,count=0;
        switch(blksz){
            case 128: return m_lines[blk_num].m_referred_chunk.count();
            case 64:
                switch(data_size){
                    case 128: return m_lines[blk_num].m_referred_chunk.count();
                    case 64: 
                    case 32:
                    if(m_lines[blk_num].m_referred_chunk.test(sid))
                        return 2;
                    else return 0;
                }
            case 32:
                switch(data_size){
                    case 128: return m_lines[blk_num].m_referred_chunk.count();
                    case 64:
                    //int count=0;
                    for(i=0;i<2;i++)
                    {
                        if(m_lines[blk_num].m_referred_chunk.test(sid+i))
                            count++;
                    }
                    return count;
                    case 32:
                    if(m_lines[blk_num].m_referred_chunk.test(sid))
                        return 1;
                    else return 0;
                }
        }
    }

    float cache_efficiency(){
        if(m_num_words_evicted&&m_num_words_referred)
            return (float)m_num_words_referred/m_num_words_evicted;
        else return 1;
    }
    float ratio_replace(){
        if(m_num_reqs)
            return (float)m_num_replace/m_num_reqs;
        else return 0;
    }
    unsigned get_num_processed_reqs(){
        return m_num_reqs;
    }
    void reset_words_statistic(){
        m_num_words_referred=0;
        m_num_words_evicted=0;
        m_num_reqs=0;
        m_num_replace=0;
    }
protected:
    // This constructor is intended for use only from derived classes that wish to
    // avoid unnecessary memory allocation that takes place in the
    // other tag_array constructor
    tag_array( cache_config &config,
               int core_id,
               int type_id,
               cache_block_t* new_lines );
    void init( int core_id, int type_id );

protected:

    cache_config &m_config;

    cache_block_t *m_lines; /* nbanks x nset x assoc lines in total */

    unsigned m_access;
    unsigned m_miss;
    unsigned m_pending_hit; // number of cache miss that hit a line that is allocated but not filled
    unsigned m_res_fail;

    // performance counters for calculating the amount of misses within a time window
    unsigned m_prev_snapshot_access;
    unsigned m_prev_snapshot_miss;
    unsigned m_prev_snapshot_pending_hit;

    int m_core_id; // which shader core is using this
    int m_type_id; // what kind of cache is this (normal, texture, constant)

    unsigned m_num_words_evicted;
    unsigned m_num_words_referred;
    unsigned m_num_reqs;
    unsigned m_num_replace;
};

class mshr_table {
public:
    mshr_table( unsigned num_entries, unsigned max_merged )
    : m_num_entries(num_entries),
    m_max_merged(max_merged)
#if (tr1_hash_map_ismap == 0)
    ,m_data(2*num_entries)
#endif
    {
    }

    /// Checks if there is a pending request to the lower memory level already
    bool probe( new_addr_type block_addr ) const;
    /// Checks if there is space for tracking a new memory access
    bool full( new_addr_type block_addr ) const;
    /// Add or merge this access
    void add( new_addr_type block_addr, mem_fetch *mf );
    /// Returns true if cannot accept new fill responses
    bool busy() const {return false;}
    /// Accept a new cache fill response: mark entry ready for processing
    void mark_ready( new_addr_type block_addr, bool &has_atomic );
    /// Returns true if ready accesses exist
    bool access_ready() const {return !m_current_response.empty();}
    /// Returns next ready access
    mem_fetch *next_access();
    void display( FILE *fp ) const;

    void check_mshr_parameters( unsigned num_entries, unsigned max_merged )
    {
    	assert(m_num_entries==num_entries && "Change of MSHR parameters between kernels is not allowed");
    	assert(m_max_merged==max_merged && "Change of MSHR parameters between kernels is not allowed");
    }

private:

    // finite sized, fully associative table, with a finite maximum number of merged requests
    const unsigned m_num_entries;
    const unsigned m_max_merged;

    struct mshr_entry {
        std::list<mem_fetch*> m_list;
        bool m_has_atomic; 
        mshr_entry() : m_has_atomic(false) { }
    }; 
    typedef tr1_hash_map<new_addr_type,mshr_entry> table;
    table m_data;

    // it may take several cycles to process the merged requests
    bool m_current_response_ready;
    std::list<new_addr_type> m_current_response;
};


/***************************************************************** Caches *****************************************************************/
///
/// Simple struct to maintain cache accesses, misses, pending hits, and reservation fails.
///
struct cache_sub_stats{
    unsigned accesses;
    unsigned misses;
    unsigned pending_hits;
    unsigned res_fails;

    unsigned long long port_available_cycles; 
    unsigned long long data_port_busy_cycles; 
    unsigned long long fill_port_busy_cycles; 

    cache_sub_stats(){
        clear();
    }
    void clear(){
        accesses = 0;
        misses = 0;
        pending_hits = 0;
        res_fails = 0;
        port_available_cycles = 0; 
        data_port_busy_cycles = 0; 
        fill_port_busy_cycles = 0; 
    }
    cache_sub_stats &operator+=(const cache_sub_stats &css){
        ///
        /// Overloading += operator to easily accumulate stats
        ///
        accesses += css.accesses;
        misses += css.misses;
        pending_hits += css.pending_hits;
        res_fails += css.res_fails;
        port_available_cycles += css.port_available_cycles; 
        data_port_busy_cycles += css.data_port_busy_cycles; 
        fill_port_busy_cycles += css.fill_port_busy_cycles; 
        return *this;
    }

    cache_sub_stats operator+(const cache_sub_stats &cs){
        ///
        /// Overloading + operator to easily accumulate stats
        ///
        cache_sub_stats ret;
        ret.accesses = accesses + cs.accesses;
        ret.misses = misses + cs.misses;
        ret.pending_hits = pending_hits + cs.pending_hits;
        ret.res_fails = res_fails + cs.res_fails;
        ret.port_available_cycles = port_available_cycles + cs.port_available_cycles; 
        ret.data_port_busy_cycles = data_port_busy_cycles + cs.data_port_busy_cycles; 
        ret.fill_port_busy_cycles = fill_port_busy_cycles + cs.fill_port_busy_cycles; 
        return ret;
    }

    void print_port_stats(FILE *fout, const char *cache_name) const; 
};

///
/// Cache_stats
/// Used to record statistics for each cache.
/// Maintains a record of every 'mem_access_type' and its resulting
/// 'cache_request_status' : [mem_access_type][cache_request_status]
///
class cache_stats {
public:
    cache_stats();
    void clear();
    void inc_stats(int access_type, int access_outcome);
    enum cache_request_status select_stats_status(enum cache_request_status probe, enum cache_request_status access) const;
    unsigned &operator()(int access_type, int access_outcome);
    unsigned operator()(int access_type, int access_outcome) const;
    cache_stats operator+(const cache_stats &cs);
    cache_stats &operator+=(const cache_stats &cs);
    void print_stats(FILE *fout, const char *cache_name = "Cache_stats") const;

    unsigned get_stats(enum mem_access_type *access_type, unsigned num_access_type, enum cache_request_status *access_status, unsigned num_access_status)  const;
    void get_sub_stats(struct cache_sub_stats &css) const;

    void sample_cache_port_utility(bool data_port_busy, bool fill_port_busy); 
private:
    bool check_valid(int type, int status) const;

    std::vector< std::vector<unsigned> > m_stats;

    unsigned long long m_cache_port_available_cycles; 
    unsigned long long m_cache_data_port_busy_cycles; 
    unsigned long long m_cache_fill_port_busy_cycles; 
};

class cache_t {
public:
    virtual ~cache_t() {}
    virtual enum cache_request_status access( new_addr_type addr, mem_fetch *mf, unsigned time, std::list<cache_event> &events ) =  0;

    // accessors for cache bandwidth availability 
    virtual bool data_port_free() const = 0; 
    virtual bool fill_port_free() const = 0; 
};

bool was_write_sent( const std::list<cache_event> &events );
bool was_read_sent( const std::list<cache_event> &events );

/// Baseline cache
/// Implements common functions for read_only_cache and data_cache
/// Each subclass implements its own 'access' function
class baseline_cache : public cache_t {
public:
    baseline_cache( const char *name, cache_config &config, int core_id, int type_id, mem_fetch_interface *memport,
                     enum mem_fetch_status status )
    : m_config(config), m_tag_array(new tag_array(config,core_id,type_id)), 
      m_mshrs(config.m_mshr_entries,config.m_mshr_max_merge), 
      m_bandwidth_management(config) 
    {
        init( name, config, memport, status );
    }

    void init( const char *name,
               const cache_config &config,
               mem_fetch_interface *memport,
               enum mem_fetch_status status )
    {
        m_name = name;
        assert(config.m_mshr_type == ASSOC);
        m_memport=memport;
        m_miss_queue_status = status;
        current_blksz = config.get_line_sz();
    }

    virtual ~baseline_cache()
    {
        delete m_tag_array;
    }

	void update_cache_parameters(cache_config &config)
	{
		m_config=config;
		m_tag_array->update_cache_parameters(config);
		m_mshrs.check_mshr_parameters(config.m_mshr_entries,config.m_mshr_max_merge);
	}

    virtual enum cache_request_status access( new_addr_type addr, mem_fetch *mf, unsigned time, std::list<cache_event> &events ) =  0;
    /// Sends next request to lower level of memory
    void cycle();
    /// Interface for response from lower memory level (model bandwidth restictions in caller)
    virtual void fill( mem_fetch *mf, unsigned time );
    /// Checks if mf is waiting to be filled by lower memory level
    bool waiting_for_fill( mem_fetch *mf );
    /// Are any (accepted) accesses that had to wait for memory now ready? (does not include accesses that "HIT")
    bool access_ready() const {return m_mshrs.access_ready();}
    /// Pop next ready access (does not include accesses that "HIT")
    mem_fetch *next_access(){return m_mshrs.next_access();}
    // flash invalidate all entries in cache
    void flush(){m_tag_array->flush();}
    void print(FILE *fp, unsigned &accesses, unsigned &misses) const;
    void display_state( FILE *fp ) const;

    // Stat collection
    const cache_stats &get_stats() const {
        return m_stats;
    }
    unsigned get_stats(enum mem_access_type *access_type, unsigned num_access_type, enum cache_request_status *access_status, unsigned num_access_status)  const{
        return m_stats.get_stats(access_type, num_access_type, access_status, num_access_status);
    }
    void get_sub_stats(struct cache_sub_stats &css) const {
        m_stats.get_sub_stats(css);
    }

    void change2big_blksz(unsigned blksz)
    {
        current_blksz=blksz;
    }
    void change2small_blksz(unsigned blksz)
    {
        current_blksz=blksz;
    }

    // accessors for cache bandwidth availability 
    bool data_port_free() const { return m_bandwidth_management.data_port_free(); } 
    bool fill_port_free() const { return m_bandwidth_management.fill_port_free(); } 
    cache_config &m_config;
protected:
    // Constructor that can be used by derived classes with custom tag arrays
    baseline_cache( const char *name,
                    cache_config &config,
                    int core_id,
                    int type_id,
                    mem_fetch_interface *memport,
                    enum mem_fetch_status status,
                    tag_array* new_tag_array )
    : m_config(config),
      m_tag_array( new_tag_array ),
      m_mshrs(config.m_mshr_entries,config.m_mshr_max_merge), 
      m_bandwidth_management(config) 
    {
        init( name, config, memport, status );
    }

protected:
    std::string m_name;
    unsigned current_blksz;
    //cache_config &m_config;
    tag_array*  m_tag_array;
    mshr_table m_mshrs;
    std::list<mem_fetch*> m_miss_queue;
    enum mem_fetch_status m_miss_queue_status;
    mem_fetch_interface *m_memport;

    struct extra_mf_fields {
        extra_mf_fields()  { m_valid = false;}
        extra_mf_fields( new_addr_type a, unsigned i, unsigned d ) 
        {
            m_valid = true;
            m_block_addr = a;
            m_cache_index = i;
            m_data_size = d;
        }
        bool m_valid;
        new_addr_type m_block_addr;
        unsigned m_cache_index;
        unsigned m_data_size;
    };

    typedef std::map<mem_fetch*,extra_mf_fields> extra_mf_fields_lookup;

    extra_mf_fields_lookup m_extra_mf_fields;

    cache_stats m_stats;

    /// Checks whether this request can be handled on this cycle. num_miss equals max # of misses to be handled on this cycle
    bool miss_queue_full(unsigned num_miss){
    	  return ( (m_miss_queue.size()+num_miss) >= m_config.m_miss_queue_size );
    }
    /// Read miss handler without writeback
    virtual void send_read_request(new_addr_type addr, new_addr_type block_addr, unsigned cache_index, mem_fetch *mf,
    		unsigned time, bool &do_miss, std::list<cache_event> &events, bool read_only, bool wa);
    /// Read miss handler. Check MSHR hit or MSHR available
    virtual void send_read_request(new_addr_type addr, new_addr_type block_addr, unsigned cache_index, mem_fetch *mf,
    		unsigned time, bool &do_miss, bool &wb, cache_block_t &evicted, std::list<cache_event> &events, bool read_only, bool wa);

    /// Sub-class containing all metadata for port bandwidth management 
    class bandwidth_management 
    {
    public: 
        bandwidth_management(cache_config &config); 

        /// use the data port based on the outcome and events generated by the mem_fetch request 
        void use_data_port(mem_fetch *mf, enum cache_request_status outcome, const std::list<cache_event> &events); 

        /// use the fill port 
        void use_fill_port(mem_fetch *mf); 

        /// called every cache cycle to free up the ports 
        void replenish_port_bandwidth(); 

        /// query for data port availability 
        bool data_port_free() const; 
        /// query for fill port availability 
        bool fill_port_free() const; 
    protected: 
        const cache_config &m_config; 

        int m_data_port_occupied_cycles; //< Number of cycle that the data port remains used 
        int m_fill_port_occupied_cycles; //< Number of cycle that the fill port remains used 
    }; 

    bandwidth_management m_bandwidth_management; 
};

/// Read only cache
class read_only_cache : public baseline_cache {
public:
    read_only_cache( const char *name, cache_config &config, int core_id, int type_id, mem_fetch_interface *memport, enum mem_fetch_status status )
    : baseline_cache(name,config,core_id,type_id,memport,status){}

    /// Access cache for read_only_cache: returns RESERVATION_FAIL if request could not be accepted (for any reason)
    virtual enum cache_request_status access( new_addr_type addr, mem_fetch *mf, unsigned time, std::list<cache_event> &events );

    virtual ~read_only_cache(){}

protected:
    read_only_cache( const char *name, cache_config &config, int core_id, int type_id, mem_fetch_interface *memport, enum mem_fetch_status status, tag_array* new_tag_array )
    : baseline_cache(name,config,core_id,type_id,memport,status, new_tag_array){}
};

/// Data cache - Implements common functions for L1 and L2 data cache
class data_cache : public baseline_cache {
public:
    data_cache( const char *name, cache_config &config,
    			int core_id, int type_id, mem_fetch_interface *memport,
                mem_fetch_allocator *mfcreator, enum mem_fetch_status status,
                mem_access_type wr_alloc_type, mem_access_type wrbk_type )
    			: baseline_cache(name,config,core_id,type_id,memport,status)
    {
        init( mfcreator );
        m_wr_alloc_type = wr_alloc_type;
        m_wrbk_type = wrbk_type;
    }

    virtual ~data_cache() {}

    virtual void init( mem_fetch_allocator *mfcreator )
    {
        m_memfetch_creator=mfcreator;

        // Set read hit function
        m_rd_hit = &data_cache::rd_hit_base;

        // Set read miss function
        m_rd_miss = &data_cache::rd_miss_base;

        // Set write hit function
        switch(m_config.m_write_policy){
        // READ_ONLY is now a separate cache class, config is deprecated
        case READ_ONLY:
            assert(0 && "Error: Writable Data_cache set as READ_ONLY\n");
            break; 
        case WRITE_BACK: m_wr_hit = &data_cache::wr_hit_wb; break;
        case WRITE_THROUGH: m_wr_hit = &data_cache::wr_hit_wt; break;
        case WRITE_EVICT: m_wr_hit = &data_cache::wr_hit_we; break;
        case LOCAL_WB_GLOBAL_WT:
            m_wr_hit = &data_cache::wr_hit_global_we_local_wb;
            break;
        default:
            assert(0 && "Error: Must set valid cache write policy\n");
            break; // Need to set a write hit function
        }

        // Set write miss function
        switch(m_config.m_write_alloc_policy){
        case WRITE_ALLOCATE: m_wr_miss = &data_cache::wr_miss_wa; break;
        case NO_WRITE_ALLOCATE: m_wr_miss = &data_cache::wr_miss_no_wa; break;
        default:
            assert(0 && "Error: Must set valid cache write miss policy\n");
            break; // Need to set a write miss function
        }
    }

    virtual enum cache_request_status access( new_addr_type addr,
                                              mem_fetch *mf,
                                              unsigned time,
                                              std::list<cache_event> &events );
protected:
    data_cache( const char *name,
                cache_config &config,
                int core_id,
                int type_id,
                mem_fetch_interface *memport,
                mem_fetch_allocator *mfcreator,
                enum mem_fetch_status status,
                tag_array* new_tag_array,
                mem_access_type wr_alloc_type,
                mem_access_type wrbk_type)
    : baseline_cache(name, config, core_id, type_id, memport,status, new_tag_array)
    {
        init( mfcreator );
        m_wr_alloc_type = wr_alloc_type;
        m_wrbk_type = wrbk_type;
    }

    mem_access_type m_wr_alloc_type; // Specifies type of write allocate request (e.g., L1 or L2)
    mem_access_type m_wrbk_type; // Specifies type of writeback request (e.g., L1 or L2)

    //! A general function that takes the result of a tag_array probe
    //  and performs the correspding functions based on the cache configuration
    //  The access fucntion calls this function
    virtual enum cache_request_status
        process_tag_probe( bool wr,
                           enum cache_request_status status,
                           new_addr_type addr,
                           unsigned cache_index,
                           mem_fetch* mf,
                           unsigned time,
                           std::list<cache_event>& events );

protected:
    mem_fetch_allocator *m_memfetch_creator;

    // Functions for data cache access
    /// Sends write request to lower level memory (write or writeback)
    void send_write_request( mem_fetch *mf,
                             cache_event request,
                             unsigned time,
                             std::list<cache_event> &events);

    // Member Function pointers - Set by configuration options
    // to the functions below each grouping
    /******* Write-hit configs *******/
    enum cache_request_status
        (data_cache::*m_wr_hit)( new_addr_type addr,
                                 unsigned cache_index,
                                 mem_fetch *mf,
                                 unsigned time,
                                 std::list<cache_event> &events,
                                 enum cache_request_status status );
    /// Marks block as MODIFIED and updates block LRU
    virtual enum cache_request_status
        wr_hit_wb( new_addr_type addr,
                   unsigned cache_index,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-back
    virtual enum cache_request_status
        wr_hit_wt( new_addr_type addr,
                   unsigned cache_index,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-through

    /// Marks block as INVALID and sends write request to lower level memory
    virtual enum cache_request_status
        wr_hit_we( new_addr_type addr,
                   unsigned cache_index,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-evict
    virtual enum cache_request_status
        wr_hit_global_we_local_wb( new_addr_type addr,
                                   unsigned cache_index,
                                   mem_fetch *mf,
                                   unsigned time,
                                   std::list<cache_event> &events,
                                   enum cache_request_status status );
        // global write-evict, local write-back


    /******* Write-miss configs *******/
    enum cache_request_status
        (data_cache::*m_wr_miss)( new_addr_type addr,
                                  unsigned cache_index,
                                  mem_fetch *mf,
                                  unsigned time,
                                  std::list<cache_event> &events,
                                  enum cache_request_status status );
    /// Sends read request, and possible write-back request,
    //  to lower level memory for a write miss with write-allocate
    virtual enum cache_request_status
        wr_miss_wa( new_addr_type addr,
                    unsigned cache_index,
                    mem_fetch *mf,
                    unsigned time,
                    std::list<cache_event> &events,
                    enum cache_request_status status ); // write-allocate
    virtual enum cache_request_status
        wr_miss_no_wa( new_addr_type addr,
                       unsigned cache_index,
                       mem_fetch *mf,
                       unsigned time,
                       std::list<cache_event> &events,
                       enum cache_request_status status ); // no write-allocate

    // Currently no separate functions for reads
    /******* Read-hit configs *******/
    enum cache_request_status
        (data_cache::*m_rd_hit)( new_addr_type addr,
                                 unsigned cache_index,
                                 mem_fetch *mf,
                                 unsigned time,
                                 std::list<cache_event> &events,
                                 enum cache_request_status status );
    virtual enum cache_request_status
        rd_hit_base( new_addr_type addr,
                     unsigned cache_index,
                     mem_fetch *mf,
                     unsigned time,
                     std::list<cache_event> &events,
                     enum cache_request_status status );

    /******* Read-miss configs *******/
    enum cache_request_status
        (data_cache::*m_rd_miss)( new_addr_type addr,
                                  unsigned cache_index,
                                  mem_fetch *mf,
                                  unsigned time,
                                  std::list<cache_event> &events,
                                  enum cache_request_status status );
    virtual enum cache_request_status
        rd_miss_base( new_addr_type addr,
                      unsigned cache_index,
                      mem_fetch*mf,
                      unsigned time,
                      std::list<cache_event> &events,
                      enum cache_request_status status );

};

/// This is meant to model the first level data cache in Fermi.
/// It is write-evict (global) or write-back (local) at
/// the granularity of individual blocks
/// (the policy used in fermi according to the CUDA manual)
class l1_cache : public data_cache {
public:
    l1_cache(const char *name, cache_config &config,
            int core_id, int type_id, mem_fetch_interface *memport,
            mem_fetch_allocator *mfcreator, enum mem_fetch_status status )
            : data_cache(name,config,core_id,type_id,memport,mfcreator,status, L1_WR_ALLOC_R, L1_WRBK_ACC){
                init(mfcreator);
            }

    virtual ~l1_cache(){}
    void init( mem_fetch_allocator *mfcreator )
    {
        //m_memfetch_creator=mfcreator;

        // Set read hit function
        m_rd_hit = &l1_cache::rd_hit_base;

        // Set read miss function
        m_rd_miss = &l1_cache::rd_miss_base;

        // Set write hit function
        switch(m_config.m_write_policy){
        // READ_ONLY is now a separate cache class, config is deprecated
        case READ_ONLY:
            assert(0 && "Error: Writable Data_cache set as READ_ONLY\n");
            break; 
        case WRITE_BACK: m_wr_hit = &l1_cache::wr_hit_wb; break;
        case WRITE_THROUGH: m_wr_hit = &l1_cache::wr_hit_wt; break;
        case WRITE_EVICT: m_wr_hit = &l1_cache::wr_hit_we; break;
        case LOCAL_WB_GLOBAL_WT:
            m_wr_hit = &l1_cache::wr_hit_global_we_local_wb;
            break;
        default:
            assert(0 && "Error: Must set valid cache write policy\n");
            break; // Need to set a write hit function
        }

        // Set write miss function
        switch(m_config.m_write_alloc_policy){
        case WRITE_ALLOCATE: m_wr_miss = &l1_cache::wr_miss_wa; break;
        case NO_WRITE_ALLOCATE: m_wr_miss = &l1_cache::wr_miss_no_wa; break;
        default:
            assert(0 && "Error: Must set valid cache write miss policy\n");
            break; // Need to set a write miss function
        }
    }
    virtual enum cache_request_status
        access( new_addr_type addr,
                mem_fetch *mf,
                unsigned time,
                std::list<cache_event> &events );
    virtual void fill( mem_fetch *mf, unsigned time );

    float cache_efficiency()
    {
        return m_tag_array->cache_efficiency();
    }
    float get_ratio_replace(){
        return m_tag_array->ratio_replace();
    }
    void reset_cache_stat(){
        m_tag_array->reset_words_statistic();
    }
    unsigned get_num_processed_reqs(){
        return m_tag_array->get_num_processed_reqs();
    }
protected:
//! A general function that takes the result of a tag_array probe
    //  and performs the correspding functions based on the cache configuration
    //  The access fucntion calls this function
    virtual enum cache_request_status
        process_tag_probe( bool wr,
                           enum cache_request_status status,
                           new_addr_type addr,
                           unsigned cache_index,
                           unsigned sid,
                           mem_fetch* mf,
                           unsigned time,
                           std::list<cache_event>& events );
    

protected:
    l1_cache( const char *name,
              cache_config &config,
              int core_id,
              int type_id,
              mem_fetch_interface *memport,
              mem_fetch_allocator *mfcreator,
              enum mem_fetch_status status,
              tag_array* new_tag_array )
    : data_cache( name,
                  config,
                  core_id,type_id,memport,mfcreator,status, new_tag_array, L1_WR_ALLOC_R, L1_WRBK_ACC ){}

    // Member Function pointers - Set by configuration options
    // to the functions below each grouping
    /******* Write-hit configs *******/
    enum cache_request_status
        (l1_cache::*m_wr_hit)( new_addr_type addr,
                                 unsigned cache_index,
                                 unsigned sid,
                                 mem_fetch *mf,
                                 unsigned time,
                                 std::list<cache_event> &events,
                                 enum cache_request_status status );
    /// Marks block as MODIFIED and updates block LRU
    virtual enum cache_request_status
        wr_hit_wb( new_addr_type addr,
                   unsigned cache_index,
                   unsigned sid,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-back
    virtual enum cache_request_status
        wr_hit_wt( new_addr_type addr,
                   unsigned cache_index,
                   unsigned sid,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-through

    /// Marks block as INVALID and sends write request to lower level memory
    virtual enum cache_request_status
        wr_hit_we( new_addr_type addr,
                   unsigned cache_index,
                   unsigned sid,
                   mem_fetch *mf,
                   unsigned time,
                   std::list<cache_event> &events,
                   enum cache_request_status status ); // write-evict
    virtual enum cache_request_status
        wr_hit_global_we_local_wb( new_addr_type addr,
                                   unsigned cache_index,
                                   unsigned sid,
                                   mem_fetch *mf,
                                   unsigned time,
                                   std::list<cache_event> &events,
                                   enum cache_request_status status );
        // global write-evict, local write-back


    /******* Write-miss configs *******/
    enum cache_request_status
        (l1_cache::*m_wr_miss)( new_addr_type addr,
                                  unsigned cache_index,
                                  mem_fetch *mf,
                                  unsigned time,
                                  std::list<cache_event> &events,
                                  enum cache_request_status status );
    /// Sends read request, and possible write-back request,
    //  to lower level memory for a write miss with write-allocate
    virtual enum cache_request_status
        wr_miss_wa( new_addr_type addr,
                    unsigned cache_index,
                    mem_fetch *mf,
                    unsigned time,
                    std::list<cache_event> &events,
                    enum cache_request_status status ); // write-allocate
    virtual enum cache_request_status
        wr_miss_no_wa( new_addr_type addr,
                       unsigned cache_index,
                       mem_fetch *mf,
                       unsigned time,
                       std::list<cache_event> &events,
                       enum cache_request_status status ); // no write-allocate

    // Currently no separate functions for reads
    /******* Read-hit configs *******/
    enum cache_request_status
        (l1_cache::*m_rd_hit)( new_addr_type addr,
                                 unsigned cache_index,
                                 unsigned sid,
                                 mem_fetch *mf,
                                 unsigned time,
                                 std::list<cache_event> &events,
                                 enum cache_request_status status );
    virtual enum cache_request_status
        rd_hit_base( new_addr_type addr,
                     unsigned cache_index,
                     unsigned sid,
                     mem_fetch *mf,
                     unsigned time,
                     std::list<cache_event> &events,
                     enum cache_request_status status );

    /******* Read-miss configs *******/
    enum cache_request_status
        (l1_cache::*m_rd_miss)( new_addr_type addr,
                                  unsigned cache_index,
                                  mem_fetch *mf,
                                  unsigned time,
                                  std::list<cache_event> &events,
                                  enum cache_request_status status );
    virtual enum cache_request_status
        rd_miss_base( new_addr_type addr,
                      unsigned cache_index,
                      mem_fetch*mf,
                      unsigned time,
                      std::list<cache_event> &events,
                      enum cache_request_status status );

    /// Read miss handler without writeback
    virtual void send_read_request(new_addr_type addr, new_addr_type block_addr, unsigned cache_index, mem_fetch *mf,
    		unsigned time, bool &do_miss, std::list<cache_event> &events, bool read_only, bool wa);
    /// Read miss handler. Check MSHR hit or MSHR available
    virtual void send_read_request(new_addr_type addr, new_addr_type block_addr, unsigned cache_index, mem_fetch *mf,
    		unsigned time, bool &do_miss, bool &wb, cache_block_t &evicted, std::list<cache_event> &events, bool read_only, bool wa);
    /// Interface for response from lower memory level (model bandwidth restictions in caller)
    
};

/// Models second level shared cache with global write-back
/// and write-allocate policies
class l2_cache : public data_cache {
public:
    l2_cache(const char *name,  cache_config &config,
            int core_id, int type_id, mem_fetch_interface *memport,
            mem_fetch_allocator *mfcreator, enum mem_fetch_status status )
            : data_cache(name,config,core_id,type_id,memport,mfcreator,status, L2_WR_ALLOC_R, L2_WRBK_ACC){}

    virtual ~l2_cache() {}

    virtual enum cache_request_status
        access( new_addr_type addr,
                mem_fetch *mf,
                unsigned time,
                std::list<cache_event> &events );
};

/*****************************************************************************/

// See the following paper to understand this cache model:
// 
// Igehy, et al., Prefetching in a Texture Cache Architecture, 
// Proceedings of the 1998 Eurographics/SIGGRAPH Workshop on Graphics Hardware
// http://www-graphics.stanford.edu/papers/texture_prefetch/
class tex_cache : public cache_t {
public:
    tex_cache( const char *name, cache_config &config, int core_id, int type_id, mem_fetch_interface *memport,
               enum mem_fetch_status request_status, 
               enum mem_fetch_status rob_status )
    : m_config(config), 
    m_tags(config,core_id,type_id), 
    m_fragment_fifo(config.m_fragment_fifo_entries), 
    m_request_fifo(config.m_request_fifo_entries),
    m_rob(config.m_rob_entries),
    m_result_fifo(config.m_result_fifo_entries)
    {
        m_name = name;
        assert(config.m_mshr_type == TEX_FIFO);
        assert(config.m_write_policy == READ_ONLY);
        assert(config.m_alloc_policy == ON_MISS);
        m_memport=memport;
        m_cache = new data_block[ config.get_num_lines() ];
        m_request_queue_status = request_status;
        m_rob_status = rob_status;
    }

    /// Access function for tex_cache
    /// return values: RESERVATION_FAIL if request could not be accepted
    /// otherwise returns HIT_RESERVED or MISS; NOTE: *never* returns HIT
    /// since unlike a normal CPU cache, a "HIT" in texture cache does not
    /// mean the data is ready (still need to get through fragment fifo)
    enum cache_request_status access( new_addr_type addr, mem_fetch *mf, unsigned time, std::list<cache_event> &events );
    void cycle();
    /// Place returning cache block into reorder buffer
    void fill( mem_fetch *mf, unsigned time );
    /// Are any (accepted) accesses that had to wait for memory now ready? (does not include accesses that "HIT")
    bool access_ready() const{return !m_result_fifo.empty();}
    /// Pop next ready access (includes both accesses that "HIT" and those that "MISS")
    mem_fetch *next_access(){return m_result_fifo.pop();}
    void display_state( FILE *fp ) const;

    // accessors for cache bandwidth availability - stubs for now 
    bool data_port_free() const { return true; }
    bool fill_port_free() const { return true; }

    // Stat collection
    const cache_stats &get_stats() const {
        return m_stats;
    }
    unsigned get_stats(enum mem_access_type *access_type, unsigned num_access_type, enum cache_request_status *access_status, unsigned num_access_status) const{
        return m_stats.get_stats(access_type, num_access_type, access_status, num_access_status);
    }

    void get_sub_stats(struct cache_sub_stats &css) const{
        m_stats.get_sub_stats(css);
    }
private:
    std::string m_name;
    const cache_config &m_config;

    struct fragment_entry {
        fragment_entry() {}
        fragment_entry( mem_fetch *mf, unsigned idx, bool m, unsigned d )
        {
            m_request=mf;
            m_cache_index=idx;
            m_miss=m;
            m_data_size=d;
        }
        mem_fetch *m_request;     // request information
        unsigned   m_cache_index; // where to look for data
        bool       m_miss;        // true if sent memory request
        unsigned   m_data_size;
    };

    struct rob_entry {
        rob_entry() { m_ready = false; m_time=0; m_request=NULL;}
        rob_entry( unsigned i, mem_fetch *mf, new_addr_type a ) 
        { 
            m_ready=false; 
            m_index=i;
            m_time=0;
            m_request=mf; 
            m_block_addr=a;
        }
        bool m_ready;
        unsigned m_time; // which cycle did this entry become ready?
        unsigned m_index; // where in cache should block be placed?
        mem_fetch *m_request;
        new_addr_type m_block_addr;
    };

    struct data_block {
        data_block() { m_valid = false;}
        bool m_valid;
        new_addr_type m_block_addr;
    };

    // TODO: replace fifo_pipeline with this?
    template<class T> class fifo {
    public:
        fifo( unsigned size ) 
        { 
            m_size=size; 
            m_num=0; 
            m_head=0; 
            m_tail=0; 
            m_data = new T[size];
        }
        bool full() const { return m_num == m_size;}
        bool empty() const { return m_num == 0;}
        unsigned size() const { return m_num;}
        unsigned capacity() const { return m_size;}
        unsigned push( const T &e ) 
        { 
            assert(!full()); 
            m_data[m_head] = e; 
            unsigned result = m_head;
            inc_head(); 
            return result;
        }
        T pop() 
        { 
            assert(!empty()); 
            T result = m_data[m_tail];
            inc_tail();
            return result;
        }
        const T &peek( unsigned index ) const 
        { 
            assert( index < m_size );
            return m_data[index]; 
        }
        T &peek( unsigned index ) 
        { 
            assert( index < m_size );
            return m_data[index]; 
        }
        T &peek() const
        { 
            return m_data[m_tail]; 
        }
        unsigned next_pop_index() const 
        {
            return m_tail;
        }
    private:
        void inc_head() { m_head = (m_head+1)%m_size; m_num++;}
        void inc_tail() { assert(m_num>0); m_tail = (m_tail+1)%m_size; m_num--;}

        unsigned   m_head; // next entry goes here
        unsigned   m_tail; // oldest entry found here
        unsigned   m_num;  // how many in fifo?
        unsigned   m_size; // maximum number of entries in fifo
        T         *m_data;
    };

    tag_array               m_tags;
    fifo<fragment_entry>    m_fragment_fifo;
    fifo<mem_fetch*>        m_request_fifo;
    fifo<rob_entry>         m_rob;
    data_block             *m_cache;
    fifo<mem_fetch*>        m_result_fifo; // next completed texture fetch

    mem_fetch_interface    *m_memport;
    enum mem_fetch_status   m_request_queue_status;
    enum mem_fetch_status   m_rob_status;

    struct extra_mf_fields {
        extra_mf_fields()  { m_valid = false;}
        extra_mf_fields( unsigned i ) 
        {
            m_valid = true;
            m_rob_index = i;
        }
        bool m_valid;
        unsigned m_rob_index;
    };

    cache_stats m_stats;

    typedef std::map<mem_fetch*,extra_mf_fields> extra_mf_fields_lookup;

    extra_mf_fields_lookup m_extra_mf_fields;
};

typedef  new_addr_type Bound_Reg;
typedef  unsigned long long EWMA_Time;
enum List_Type{
    NONE,
    WORK_LIST,
    VERTEX_LIST,
    EDGE_LIST,
    VISIT_LIST
};
struct EWMA_Unit{
    EWMA_Time work_time;
    EWMA_Time data_time;

    EWMA_Time gen_new_ewma_time(EWMA_Time new_time);
    //ratio reg;
};
enum Prefetch_Mode{
    VERTEX,
    LARGE_NODE
};

class Prefetch_Unit{
public:
    Prefetch_Unit(){init();}
    ~Prefetch_Unit(){}

    void init()
    {
        m_req_q.clear();
        m_max_queue_length=1000;
        m_double_line=false;
        m_worklist_head=m_worklist_tail=-1;
        m_prefetched_vid=-1;
        m_prefetched_el_head=m_prefetched_el_tail=-1;
        m_el_tail_ready=m_el_head_ready=false;
        m_cur_wl_idx=-1;
        m_prefetched_wl_idx=-1;
    }
    
    void reinit()
    {
        init();
    }

    void update_struct_bound(new_addr_type* struct_bound){
        for(unsigned i=0; i<10; i++)
            m_bound_regs[i] = struct_bound[i];
    }

    void set_worklist_band(unsigned long long start, unsigned long long end)
    {
        m_worklist_head = start;
        m_worklist_tail = end;
    }

    void set_cur_wl_idx(unsigned long long idx) {m_cur_wl_idx=idx;}

    bool is_full(){return m_req_q.size()==m_max_queue_length;}

    void new_load_addr(new_addr_type addr)
    {
        // if(!is_full()){
            List_Type type = addr_filter(addr);
            gen_prefetch_requests(addr, type);
        // }
    }
#define ADDRALIGN 0xffffff80;
    void prefetched_data(unsigned long long* pre_data, new_addr_type addr){
        List_Type type = addr_filter(addr);
        new_addr_type el_head_addr, el_tail_addr;
        new_addr_type vid_addr, next_vid_addr, el_addr,prefetch_vl_addr;

        switch(type){
            case WORK_LIST:
                m_prefetched_vid = pre_data[m_prefetched_wl_idx%16];//假设worklist是128B对齐的,那么cacheline的offset就是模16
                el_head_addr = ((m_bound_regs[2] + (m_prefetched_vid/16)*128+ (m_prefetched_vid%16)*8)&ADDRALIGN);//计算需要访问vertex结构的地址，并128B对齐
                el_tail_addr = ((m_bound_regs[2] + ((1+m_prefetched_vid)/16)*128+((1+m_prefetched_vid)%16)*8)&ADDRALIGN);
                if(el_tail_addr == el_head_addr){
                    gen_prefetch_vertexlist(el_head_addr); 
                    m_double_line=false;
                }
                else {
                    gen_prefetch_vertexlist(el_head_addr);
                    gen_prefetch_vertexlist(el_tail_addr);
                    m_double_line=true;
                }
                break;
            case VERTEX_LIST:
                vid_addr = (m_bound_regs[2] +(m_prefetched_vid/16)*128+ (m_prefetched_vid%16)*8) & ADDRALIGN;
                next_vid_addr = (vid_addr + 8) & ADDRALIGN;
                if(m_double_line){
                    if(addr==vid_addr){
                        m_prefetched_el_head = pre_data[m_prefetched_vid%16];
                        m_el_head_ready = true;
                    } else if (addr==next_vid_addr){
                        m_prefetched_el_tail = pre_data[(m_prefetched_vid+1)%16];
                        m_el_tail_ready = true;
                    } else{
                        printf("Error!\n");
                        exit(1);
                    }
                    if(m_el_head_ready & m_el_tail_ready){
                        gen_prefetch_edgelist_on_vertex(m_prefetched_el_head,m_prefetched_el_tail);
                        m_el_head_ready = false;
                        m_el_tail_ready = false;
                        m_double_line=false;
                    }
                } else{
                    m_prefetched_el_head = pre_data[m_prefetched_vid%16];
                    m_prefetched_el_tail = pre_data[m_prefetched_vid%16+1];
                    gen_prefetch_edgelist_on_vertex(m_prefetched_el_head,m_prefetched_el_tail);
                }
            break;
            case EDGE_LIST:
                el_addr = addr;
                for(unsigned i=0; i<16; i++){
                    el_addr = el_addr + i*8;
                    if(el_addr<m_bound_regs[4]+8*m_prefetched_el_tail)
                    {
                        prefetch_vl_addr = m_bound_regs[6] + pre_data[i]*8;
                        gen_prefetch_visitedlist(prefetch_vl_addr);
                    }
                }
            break;
            default:    
            break;
        }
    }

    List_Type addr_filter(new_addr_type addr)
    {
        if((addr>=m_bound_regs[0]&&addr<m_bound_regs[1]))
            return WORK_LIST;
        else if(addr>=m_bound_regs[2] && addr < m_bound_regs[3])
            return VERTEX_LIST;
        else if(addr >= m_bound_regs[4] && addr < m_bound_regs[5])
            return EDGE_LIST;
        else if(addr >= m_bound_regs[6] && addr < m_bound_regs[7])
            return VISIT_LIST;
        else return NONE;
    }

    void gen_prefetch_requests(new_addr_type addr, List_Type type)//统一入口
    {
        switch(type){
            case WORK_LIST: 
                //assert(addr%128==0);
                //printf("current worklist index:%llu\n",m_cur_wl_idx);
                if(addr+8 < m_bound_regs[1]){
                    gen_prefetch_worklist(addr+8);
                    m_prefetched_wl_idx = m_cur_wl_idx+1;
                }
                break;
            // case VERTEX_LIST: gen_prefetch_vertexlist(addr);break;
            // case EDGE_LIST:  if((addr-m_bound_regs[4])%(4*128)==0) gen_prefetch_edgelist(addr); break;
            // case VISIT_LIST:   gen_prefetch_visitedlist(addr);break;
            default: break;
        }
    }

    void gen_prefetch_worklist(new_addr_type addr)//generate worklist prefetch, push reqs into req_q
    {
        printf("generate worklist prefetch\n");
        assert(addr>=m_bound_regs[0]&&addr<m_bound_regs[1]);
        mem_access_t* access = new mem_access_t(GLOBAL_ACC_R, addr, 128, false);
        m_req_q.push_back(access);
    }
    void gen_prefetch_vertexlist(new_addr_type addr)//generate vertexlist prefetch, push reqs into req_q
    {
        printf("generate vertexlist prefetch\n");
        assert(addr>=m_bound_regs[2]&&addr<m_bound_regs[3]);
        mem_access_t* access = new mem_access_t(GLOBAL_ACC_R, addr, 128, false);
        m_req_q.push_back(access);
    }
    void gen_prefetch_edgelist(new_addr_type addr)//generate edgelist prefetch, push reqs into req_q
    {
        printf("generate edgelist prefetch\n");
        for(unsigned i=0;i<4;i++){
            new_addr_type next_addr = addr + 128*(4+i);
            if(next_addr>=m_bound_regs[5]||next_addr<m_bound_regs[4])
                break;
            mem_access_t* access = new mem_access_t(GLOBAL_ACC_R, next_addr, 128, false);
            m_req_q.push_back(access);
        }
    }
    void gen_prefetch_edgelist_on_vertex(unsigned long long start_offset, unsigned long long end_offset)
    {
        printf("generate edgelist prefetch on vertex\n");
        unsigned long long length = 8*(end_offset-start_offset);
        if(length%128) length = length/128+1;
        else length /=128;

        unsigned max_lines = (length>8)?8:length;

        for(unsigned i=0;i<max_lines;i++){
            new_addr_type next_addr = m_bound_regs[4] + start_offset*8 + 128*i;
            if(next_addr >= m_bound_regs[5] || next_addr < m_bound_regs[4])
                break;
            mem_access_t* access = new mem_access_t(GLOBAL_ACC_R, next_addr, 128, false);
            m_req_q.push_back(access);
        }
    }
    void gen_prefetch_visitedlist(new_addr_type addr)//generate visited list prefetch, push reqs into req_q
    {
        printf("generate visitlist prefetch\n");
        assert(addr>=m_bound_regs[6]&&addr<m_bound_regs[7]);
        mem_access_t* access = new mem_access_t(GLOBAL_ACC_R, addr, 128, false);
        m_req_q.push_back(access);
    }

    mem_access_t* pop_from_top() {return m_req_q.front();}
    void del_req_from_top() {m_req_q.pop_front();}
    bool queue_empty() {return !m_req_q.size();}
    Bound_Reg m_bound_regs[10];

    
private:
    //worklist, vertexlist, edgelist, visitedlist, out_worklist. (start, end)
    //EWMA_Unit m_ewma;
    std::list<mem_access_t*> m_req_q;
    //Prefetch_Mode m_mode;
    unsigned m_max_queue_length;
    bool m_double_line;

    unsigned long long m_worklist_head, m_worklist_tail;
    unsigned long long m_prefetched_vid;
    unsigned long long m_prefetched_el_head, m_prefetched_el_tail;
    bool m_el_head_ready, m_el_tail_ready;
    unsigned long long m_cur_wl_idx;//使用CUDA api更新
    unsigned long long m_prefetched_wl_idx;
};

#endif
