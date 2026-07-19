#ifndef ASSERT_DEF_H_
#define ASSERT_DEF_H_

#ifdef __cplusplus
 extern "C" {
#endif


#include "def.h"

bool assertInit(void);
void assertFailed(uint8_t *file, uint32_t line, uint8_t *expr);


#ifdef __cplusplus
 }
#endif


#endif 