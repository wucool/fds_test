#include "fds.h"
#include "fds_internal_defs.h"
 uint16_t time_read(const uint32_t *p);
void time_store(uint32_t *data,uint16_t len);
void time_delete();
extern bool op_flag;
void time_read2(void);
void time_store2(void);
ret_code_t fds_read(uint32_t *p);
//ret_code_t fds_read(void);
ret_code_t fds_test_init (void);
ret_code_t fds_test_find_and_delete (void);
ret_code_t fds_test_write(uint32_t *data,uint16_t len);
//ret_code_t fds_test_write(void);
 ret_code_t fds_read_FAE(void);
