#include "sdk_common.h"
#include "fds.h"
#include "fds_internal_defs.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "fstorage.h"
#include "nrf_error.h"
#include "SEGGER_RTT_Conf.h"
#include "SEGGER_RTT.h"
#define TIME_FILE_ID     0x0003
#define TIME_REC_KEY     0x0004

#define RATE_FILE_ID     0x1002
#define RATE_REC_KEY     0x2002

#define STEP_FILE_ID     0x1003
#define STEP_REC_KEY     0x2003

bool op_flag=false;

uint32_t rate[2],step[1];
uint32_t  time[1];
fds_record_desc_t   time_record_desc;

void time_store(uint32_t *data,uint16_t len)
{

  fds_record_t        record;
  fds_record_chunk_t  record_chunk;
  // Set up data.
  record_chunk.p_data         = data;
  record_chunk.length_words   = len;
  // Set up record.
  record.file_id                  = TIME_FILE_ID;
  record.key               = TIME_REC_KEY;
  record.data.p_chunks     = &record_chunk;
  record.data.num_chunks   = 1;
    
  ret_code_t ret = fds_record_write(&time_record_desc, &record);
  if (ret != FDS_SUCCESS)
  {
    // Handle error.
   // SEGGER_RTT_printf(0,"------------fds_record_write  fail------------- \n");
  }
}

void time_store2(void)
{
static uint32_t const m_deadbeef[3] = {0xAAAABBBB,0xCCCCDDDD,0xEEEEFFFF};
  fds_record_t        record;
  ret_code_t         ret;
  fds_record_chunk_t  record_chunk;
  fds_find_token_t    ftok;
  // Set up data.
  record_chunk.p_data         = m_deadbeef;
  record_chunk.length_words   = 3;
  // Set up record.
  record.file_id                  = TIME_FILE_ID;
  record.key               = TIME_REC_KEY;
  record.data.p_chunks     = &record_chunk;
  record.data.num_chunks   = 1;
  if (fds_record_find(TIME_FILE_ID, TIME_REC_KEY, &time_record_desc, &ftok) == FDS_ERR_NOT_FOUND)
  {
    ret_code_t ret = fds_record_write(&time_record_desc, &record);
    printf("------time_store2------1------------- \n");
    if (ret != FDS_SUCCESS)
    {
      // Handle error.
      printf("------------fds_record_write  fail------------- \n");
    }
  }
  else
  {
    
    ret=fds_record_update(&time_record_desc, &record);
    printf("------time_store2------2-------ret=%d------ \n",ret);
  }
}
uint16_t time_read(const uint32_t *p)
{
  fds_flash_record_t  flash_record;
  fds_find_token_t    ftok;
  uint16_t n=0;
  memset(&ftok, 0x00, sizeof(fds_find_token_t));
  // Loop until all records with the given key and file ID have been found.
  if (fds_record_find(TIME_FILE_ID, TIME_REC_KEY, &time_record_desc, &ftok) != FDS_SUCCESS)
  {
    if (fds_record_open(&time_record_desc, &flash_record) != FDS_SUCCESS)
    {
        // Handle error.
      printf("------------fds_record_open  fail------------- \n");
        return -1;
    }
    // Access the record through the flash_record structure.
    // Close the record when done.
    n=flash_record.p_header->tl.length_words;
    p=(uint32_t *)flash_record.p_data;
    printf("---------time_read----%d------------ \n",n);
    
    if (fds_record_close(&time_record_desc) != FDS_SUCCESS)
    {
        // Handle error.
      printf("------------fds_record_close  fail------------- \n");
    }
  }

  return n;
}


void time_read2(void)
{
  fds_flash_record_t  flash_record;
  fds_find_token_t    ftok;
  uint16_t n=0;
  uint32_t const *p;
  memset(&ftok, 0x00, sizeof(fds_find_token_t));
  // Loop until all records with the given key and file ID have been found.
  while (fds_record_find(TIME_FILE_ID, TIME_REC_KEY, &time_record_desc, &ftok) == FDS_SUCCESS)
  {
    if (fds_record_open(&time_record_desc, &flash_record) != FDS_SUCCESS)
    {
        // Handle error.
      printf("------------fds_record_open  fail------------- \n");
        return ;
    }
    // Access the record through the flash_record structure.
    // Close the record when done.
    n=flash_record.p_header->tl.length_words;
    p=flash_record.p_data;
    printf("---------time_read----%d------------ \n",n);
    //SEGGER_RTT_printf(0,"---------time_read----%d----p=%x-------- \n",n,*p);
    
    if (fds_record_close(&time_record_desc) != FDS_SUCCESS)
    {
        // Handle error.
      printf("------------fds_record_close  fail------------- \n");
    }
  }

}
void time_delete()
{
  ret_code_t ret = fds_record_delete(&time_record_desc);
  if (ret != FDS_SUCCESS)
  {
    // Error.
    printf("------------fds_record_delete  fail------------- \n");
  }
}





		#define FILE_ID     0x3200
		#define REC_KEY     0x4200
 ret_code_t fds_test_write(uint32_t  *data,uint16_t len)
//ret_code_t fds_test_write()
{

    uint8_t i;
    bool data_find_flag=false;
		static uint32_t const m_deadbeef[4] = {0xAAAAAAAA,0xDDDDDDDD,0xEEEEEEEE,0xFFFFFFFF};
		fds_record_t        record;
		fds_record_desc_t   record_desc;
		fds_record_chunk_t  record_chunk;
    fds_find_token_t    ftok={0};
    uint16_t le;
    le=len;
   // printf(" \r\n");
   // printf(" \r\n");
   // printf(" ------write-----\r\n");
		// Set up data.

		record_chunk.p_data         = data;//m_deadbeef;//data;
		record_chunk.length_words   =le;//4;//le;
		// Set up record.
		record.file_id              = FILE_ID;
		record.key              		= REC_KEY;
		record.data.p_chunks       = &record_chunk;
		record.data.num_chunks   = 1;
    while(fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) == FDS_SUCCESS)
    {
      data_find_flag=true;
    }
		if(data_find_flag==true)	
      {
        printf("-----------fds_record_update--------len=%d---- \r\n",le);
        ret_code_t ret = fds_record_update(&record_desc, &record);
        if (ret != FDS_SUCCESS)
        {
          printf("-----------fds_record_update------------ \r\n");
          return ret;
        }
        data_find_flag=false;
      }
    else
      {
        printf("-----------fds_record_write-----len=%d------- \r\n",le);
        //SEGGER_RTT_printf(0,"-----------fds_record_write-----1------- \r\n");
        ret_code_t ret = fds_record_write(&record_desc, &record);
        if (ret != FDS_SUCCESS)
        {
          printf("-----------fds_record_write------------ \r\n");
          return ret;
        }
        data_find_flag=false;
      }
		printf("Writing Record ID = %d \r\n",record_desc.record_id);
    for(i=0;i<len;i++)
      printf("write[%d]=0x%x \n",i,((const uint32_t *)record_chunk.p_data)[i]);
    printf(" --write end--\r\n"); 
   // printf(" \r\n");
   /// printf(" \r\n");
   
		return NRF_SUCCESS;
}

 ret_code_t fds_test_find_and_delete (void)
{

		fds_record_desc_t   record_desc;
		fds_find_token_t    ftok;
	
		ftok.page=0;
		ftok.p_addr=NULL;
		// Loop and find records with same ID and rec key and mark them as deleted. 
		while (fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) == FDS_SUCCESS)
		{
			fds_record_delete(&record_desc);
			printf("Deleted record ID: %d \r\n",record_desc.record_id);
		}
		// call the garbage collector to empty them, don't need to do this all the time, this is just for demonstration
	/*	ret_code_t ret = fds_gc();
		if (ret != FDS_SUCCESS)
		{
				return ret;
		}*/
		return NRF_SUCCESS;
}

 ret_code_t fds_read(uint32_t *p)
//ret_code_t fds_read(void)
{
    bool data_find_read_flag=false;
		fds_flash_record_t  flash_record;
		fds_record_desc_t   record_desc;
		fds_find_token_t    ftok ={0};//Important, make sure you zero init the ftok token
		uint32_t err_code;
    uint16_t len=0;
	
   // printf(" \r\n");
   // printf(" \r\n");
   // printf("****read*****\r\n"); 
		//SEGGER_RTT_printf(0,"Start searching... \r\n");
		// Loop until all records with the given key and file ID have been found.
		while (fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) == FDS_SUCCESS)
		{
      data_find_read_flag=true;
    }
		if(data_find_read_flag==true)
      {
          data_find_read_flag=false;
          err_code = fds_record_open(&record_desc, &flash_record);
          if ( err_code != FDS_SUCCESS)
          {
            printf("read error... \r\n");
            return -1;		
          }
          
          printf("read Record ID = %d---len = %d\r\n",record_desc.record_id,flash_record.p_header->tl.length_words);
          //SEGGER_RTT_printf(0,"len = %d\r\n",);
          //printf("Data = ");
          //data = (uint32_t *) flash_record.p_data;
          memcpy(p, flash_record.p_data, flash_record.p_header->tl.length_words);
          len=flash_record.p_header->tl.length_words;
        /*	for (uint8_t i=0;i<flash_record.p_header->tl.length_words;i++)
          {
            printf("0x%x \t",p[i]);
          }
          printf("\r\n");*/
          // Access the record through the flash_record structure.
          // Close the record when done.
          err_code = fds_record_close(&record_desc);
          if (err_code != FDS_SUCCESS)
          {
            return -1;	
          }
          
          return len;
        }
       printf("**read end**\r\n"); 
      // printf(" \r\n");
      // printf(" \r\n");
    
		
    return -1;
		
}
#define FAE 1
#if FAE
 ret_code_t fds_read_FAE(void)
{
                     //#define FILE_ID     0x1111
                     //#define REC_KEY     0x2222
                     fds_flash_record_t  flash_record;
                     fds_record_desc_t   record_desc;
                     fds_find_token_t    ftok ={0};//Important, make sure you zero init the ftok token
                     uint32_t *data;
                     uint32_t err_code;
                     
                     printf("Start searching... \r\n");
                     // Loop until all records with the given key and file ID have been found.
                     while (fds_record_find(FILE_ID, REC_KEY, &record_desc, &ftok) == FDS_SUCCESS)
                     {
                                           err_code = fds_record_open(&record_desc, &flash_record);
                                           if ( err_code != FDS_SUCCESS)
                                           {
                                                     return err_code;                   
                                           }
                                           
                                           printf("Found Record ID = %d\r\n",record_desc.record_id);
                                           printf("Data = ");
                                           data = (uint32_t *) flash_record.p_data;
                                           for (uint8_t i=0;i<flash_record.p_header->tl.length_words;i++)
                                           {
                                                     printf("0x%8x ",data[i]);
                                           }
                                           printf("\r\n");
                                           // Access the record through the flash_record structure.
                                           // Close the record when done.
                                           err_code = fds_record_close(&record_desc);
                                           if (err_code != FDS_SUCCESS)
                                           {
                                                     return err_code;         
                                           }
                     }
                     return NRF_SUCCESS;
                     
}

#endif

 void my_fds_evt_handler(fds_evt_t const * const p_fds_evt)
{
    switch (p_fds_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_fds_evt->result != FDS_SUCCESS)
            {
                // Initialization failed.
            }
            break;
				case FDS_EVT_WRITE:
						if (p_fds_evt->result == FDS_SUCCESS)
						{
							op_flag=true;
						}
						break;
        case FDS_EVT_UPDATE:
          	if (p_fds_evt->result == FDS_SUCCESS)
						{
							op_flag=true;
						}
						break;
        default:
            break;
    }
}

 ret_code_t fds_test_init (void)
{
	
		ret_code_t ret = fds_register(my_fds_evt_handler);
		if (ret != FDS_SUCCESS)
		{
     printf("------------fds_register  fail------------- \n");
					return ret;
				
		}
		ret = fds_init();
		if (ret != FDS_SUCCESS)
		{
      printf("------------fds_init  fail------------- \n");
				return ret;
		}
		
		return NRF_SUCCESS;
		
}