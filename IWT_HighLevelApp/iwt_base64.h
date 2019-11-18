#pragma once

#include <string.h>
#ifndef _JWT_BASE64_H_
#define _JWT_BASE64_H_
void jwt_urlsafe_base64_encode(char* result, const char* str, size_t len);
#endif /* _JWT_BASE64_H_ */

