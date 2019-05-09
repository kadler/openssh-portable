/*
 *
 * Copyright (c) 2018 IBM Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef __PASE__

#include "includes.h"

#include "xmalloc.h"
#include "sshbuf.h"
#include "hostfile.h"
#include "auth.h"
#include "ssh_api.h"
#include "log.h"

#include <as400_types.h>
#include <as400_protos.h>

#include "port-pase.h"

static ILEpointer qsygetph __attribute__ ((aligned (16)));
static ILEpointer qsyrlsph __attribute__ ((aligned (16)));

static void pase_init() {
	unsigned long long mark;
	int rc;

	static int init_done = 1;
	if(!init_done) {
		return;
	}
	init_done = 0;

	debug3("pase_init");

	/* force sshd job to run in CCSID 37 */
	systemCL("CHGJOB CCSID(37)", 0);

	mark = _ILELOADX("QSYS/QSYPHANDLE", ILELOAD_LIBOBJ);
	if (mark == (unsigned long long) -1) {
		fatal("Error loading QSYS/QSYPHANDLE: %.100s\n", strerror(errno));
	}

	rc = _ILESYMX(&qsygetph, mark, "QsyGetProfileHandle");
	if (rc == -1) {
		fatal("Error resolving QSYS/QSYPHANDLE(QsyGetProfileHandle): %.100s\n", strerror(errno));
	}
	
	rc = _ILESYMX(&qsyrlsph, mark, "QsyReleaseProfileHandle");
	if (rc == -1) {
		fatal("Error resolving QSYS/QSYPHANDLE(QsyReleaseProfileHandle): %.100s\n", strerror(errno));
	}
}

/*
 * EBCDIC converters for IBM i user names
 * 
 * Requirements:
 * - User names are max 10 chars (space padded to 10)
 * - User names are all upper case
 * - The first character must be one of: A-Z, $, #, or @
 * - Remaining characters can be: A-Z, 0-9, $, #, @, or _
 * 
 * Due to EBCDIC code-page differences, $, @, _ are not
 * invariant. This would imply that we should have separate
 * conversion tables, but that would be a nightmare. Instead
 * we just force sshd to be in CCSID 37 and just use one table.
 * 
 * Note that these converters automatically convert lower case
 * to upper case so we don't need another step there and any
 * character that's not in the above allowable sets are converted
 * to NUL, which will give an error from the API so we don't have
 * to check for invalid names.
 */


/* In place conversion for ASCII -> EBCDIC */
void a2e(char* src, size_t src_len) {
	static const unsigned char table[256] =  {
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x40,0x00,0x00,0x7b,0x5b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x00,0x00,0x00,0x00,0x00,0x00,
		0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
		0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x00,0x00,0x00,0x00,0x6d,
		0x00,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,
		0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	};

	for(size_t i = 0; i < src_len; ++i) {
		src[i] = table[ (unsigned char)src[i] ];
	}
}

/* In place conversion for EBCDIC -> ASCII */
void e2a(char* src, size_t src_len) {
	static const unsigned char table[256] = {
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x6d,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x40,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,0x51,0x52,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x00,0x00,0x00,0x00,0x00,0x00,
		0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x00,0x00,0x00,0x00,0x00,0x00,
	};

	for(size_t i = 0; i < src_len; ++i) {
		src[i] = table[ (unsigned char)src[i] ];
	}
}

typedef struct {
	char handle[12];
} profile_handle_t;

/* API error code, from qusec.h */
typedef struct Qus_EC {
	int  Provided;
	int  Available;
	char Msg_Id[7];
	char Reserved;
	/*char Exception_Data[];*/           /* Varying length        */
} Qus_EC_t;

#define DEFAULT_ERROR_CODE { sizeof(Qus_EC_t), 0 }

static inline int check_error_code(Qus_EC_t* err, const char* func) {
	e2a(err->Msg_Id, sizeof(err->Msg_Id));

	/* they left us this nice reserved byte for a null-terminator :) */
	err->Reserved = '\0';

	if(err->Available) {
		debug3("%s msgid: %s, bytes: %d", func, err->Msg_Id, err->Available);
		return -1;
	}
	else {
		return 0;
	}
}

/* wrapper for QsyGetProfileHandle ILE function */
static int get_profile_handle(profile_handle_t* handle, const char * name,
                         	  const char * password, int password_len,
                         	  Qus_EC_t* error_code) {
	int rc;
	char name_upper[11];
	
	static const int return_type = RESULT_VOID;
	static const arg_type_t signature[] = {
		ARG_MEMPTR, /* unsigned char * profile handle */
		ARG_MEMPTR, /* char * user profile name */
		ARG_MEMPTR, /* char * user profile password */
		ARG_INT32,  /* int length of password */
		ARG_UINT32, /* CCSID of password */
		ARG_MEMPTR, /* Qus_EC_t* error code */
		ARG_END
	};
	
	struct {
		ILEarglist_base base __attribute__ ((aligned (16)));
		ILEpointer handle __attribute__ ((aligned (16)));
		ILEpointer name __attribute__ ((aligned (16)));
		ILEpointer password __attribute__ ((aligned (16)));
		int password_len;
		unsigned int password_ccsid;
		ILEpointer error_code __attribute__ ((aligned (16)));
	} arglist;
	
	if(error_code == NULL || error_code->Provided < (int)sizeof(Qus_EC_t)) {
		errno = EINVAL;
		return -1;
	}
	
	// Space pad out name to 10 characters
	rc = snprintf(name_upper, sizeof(name_upper), "%-10s", name);
	if(rc < 0 || rc > (int)sizeof(name_upper)) {
		errno = EINVAL;
		return -1;
	}
	a2e(name_upper, sizeof(name_upper));
	
	arglist.handle.s.addr = (address64_t) handle;
	arglist.name.s.addr = (address64_t) name_upper;
	arglist.password.s.addr = (address64_t) password;
	arglist.password_len = password_len;
	arglist.password_ccsid = 1208;
	arglist.error_code.s.addr = (address64_t) error_code;
	
	pase_init(0);

	rc = _ILECALL(&qsygetph, &arglist.base, signature, return_type);
	if(rc == ILECALL_NOERROR) {
		rc = check_error_code(error_code, __func__);
	}
	
	return rc;
}

/* wrapper for QsyReleaseProfileHandle ILE function */
static int release_profile_handle(profile_handle_t* handle,
								  Qus_EC_t* error_code) {
	int rc;
	
	static const int return_type = RESULT_VOID;
	static const arg_type_t signature[] = {
		ARG_MEMPTR, /* unsigned char* profile handle */
		ARG_MEMPTR, /* Qus_EC_t* error code */
		ARG_END
	};
	
	struct {
		ILEarglist_base base;
		ILEpointer handle;
		ILEpointer error_code;
	} arglist __attribute__ ((aligned (16)));
	
	arglist.handle.s.addr = (address64_t)handle;
	arglist.error_code.s.addr = (address64_t)error_code;
	
	pase_init(0);

	rc = _ILECALL(&qsyrlsph, &arglist.base, signature, return_type);
	if(rc == ILECALL_NOERROR) {
		rc = check_error_code(error_code, __func__);
	}
	
	return rc;
}

/*
 * Call get_profile_handle to authenticate the user.
 * If we get a profile handle, release it back.
 */
int
sys_auth_passwd(struct ssh *ssh, const char *password)
{
	int rc, authenticated = 0;
	Qus_EC_t err = DEFAULT_ERROR_CODE;
	profile_handle_t handle;

	Authctxt *ctxt = ssh->authctxt;

	rc = get_profile_handle(&handle, ctxt->pw->pw_name, password,
							strlen(password), &err);

	if(rc) {
		if (strcmp(err.Msg_Id, "CPF22E4") == 0) {
			ctxt->force_pwchange = 1;
			authenticated = 1;
		}
	}
	else {
		authenticated = 1;
		release_profile_handle(&handle, &err);
	}

	return authenticated;
}

#endif /* __PASE__ */