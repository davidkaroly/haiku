/*
 * Copyright 2008-2022 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_SETJMP_H_
#define _ARCH_SETJMP_H_


#define _SETJMP_BUF_SZ (11 + 1 + 2*8)
typedef int __jmp_buf[_SETJMP_BUF_SZ];

#endif	/* _ARCH_SETJMP_H_ */
