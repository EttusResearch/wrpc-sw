/**
 * @file extest.h
 *
 * @brief Common header file for extest's programs
 *
 * Copyright (C) CERN (www.cern.ch)
 * Author: Julian Lewis
 * 	   Michel Arruat
 */

#ifndef _EXTEST_H_
#define _EXTEST_H_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define MAX_ARG_COUNT   256	//!< maximum command line arguments
#define MAX_ARG_LENGTH  128	//!< max characters per argument
#define F_CLOSED        (-1)	//!< file closed

#define WHITE_ON_BLACK	"\033[40m\033[1;37m"
#define DEFAULT_COLOR         "\033[m"

//! Operator ID's
typedef enum {
	OprNOOP,
	OprNE,
	OprEQ,
	OprGT,
	OprGE,
	OprLT,
	OprLE,
	OprAS,
	OprPL,
	OprMI,
	OprTI,
	OprDI,
	OprAND,
	OprOR,
	OprXOR,
	OprNOT,
	OprNEG,
	OprLSH,
	OprRSH,
	OprINC,
	OprDECR,
	OprPOP,
	OprSTM,
	OprOPRS
} oprid_t;

struct operator {
	oprid_t id;	  //!< operator ID (one of @ref oprid_t)
	char	name[16]; //!< Human form
	char    help[32]; //!< help string
};

//!< atom types
typedef enum {
	Separator    = 0,  //!< [\t\n\r ,]
	Operator     = 1,  //!< [!#&*+-/:;<=>?]
	Open         = 2,  //!< [(]
	Close        = 3,  //!< [)]
	Comment      = 4,  //!< [%]
	Numeric      = 5,  //!< [0-9]
	Alpha        = 6,  //!< [a-zA-Z_]
	Open_index   = 7,  //!< [\[]
	Close_index  = 8,  //!< [\]]
	Illegal_char = 9,  //!< all the rest in the ASCII table
	Terminator   = 10, //!< [@\0]
	Bit          = 11, //!< [.]
	String	     = 12  //!< ["]
} atom_t;
#define Quotes String

/*! atom container
 * Example: 'oprd min 5 0x400' is formed by 4 atoms
 */
struct atom {
	unsigned int 	pos;  //!< position if @av_type is @Alpha or @Operator
	int      	val;  //!< value if @av_type is @Numeric
	atom_t   	type; //!< atom type
	char     	text[MAX_ARG_LENGTH]; //!< string representation
	unsigned int 	cmd_id; //!< command id (might be built-in or user-defined)
	oprid_t  	oid; //!< operator id (if any)
};

/*! @name Default commands for every test program
 *
 * Some default commands use ioctl calls to access the driver.
 * The user should provide these ioctl numbers to use these commands.
 * If a particular ioctl number is not provided, its command won't be issued.
 * IOCTL numbers can be obtained using @b debugfs
 */
enum built_in_cmd_id {
	CMD_NOCM = 0,	//!< llegal command
	CMD_QUIT,	//!< Quit test program
	CMD_HELP,	//!< Help on commands
	CMD_ATOMS,	//!< Atom list commands
	CMD_HIST,	//!< History
	CMD_SLEEP,	//!< Sleep sec
	CMD_MSLEEP,	//!< Sleep millisec
	CMD_SHELL,	//!< Shell command
	CMD_USR		//!< first available command for the user
};

//!< Command description
struct cmd_desc {
	int  valid;	//!< show command to the user? (1 - yes, 0 - no)
	int  id;	//!< id (user-defined && @def_cmd_id)
	char *name;	//!< spelling
	char *help;	//!< short help string
	char *opt;	//!< options (if any)
	int  comp;	//!< amount of compulsory options
	int  (*handle)(struct cmd_desc *, struct atom *); //!< handler
	int  pa;	//!< number of arguments to be passed to the handler
};

/*! @name extest's public API
 */
//@{
//!< User wants verbose command help
#define VERBOSE_HELP (-1)

//! Test program Error return codes
enum extest_error {
	TST_NO_ERR,		//!< cool
	TST_ERR_NOT_IMPL,	//!< function not implemented
	TST_ERR_NO_PARAM,	//!< compulsory parameter is not provided
	TST_ERR_WRONG_ARG,	//!< wrong command argument
	TST_ERR_SYSCALL,	//!< system call fails
	TST_ERR_LAST		//!< error idx
};

int  	extest_do_yes_no(char *question, char *extra);
int 	extest_compulsory_ok(struct cmd_desc *cmdd);
int 	extest_register_user_cmd(struct cmd_desc user_cmdlist[],
				 int user_cmd_nb);
int 	extest_run(char* prg_name, void (*user_sighndl)());
int	extest_is_last_atom(struct atom *atom);

const char * const extest_get_version();

//@}

#endif /* _EXTEST_H_ */
