/**
 * @file extest.c
 *
 * @brief Extensible Test program
 *
 * Copyright (C) CERN (www.cern.ch)
 * Author: Julian Lewis
 * 	   Michel Arruat
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <extest.h>

//!< Description of each operator.
static struct operator oprs[OprOPRS] = {
	{
		.id   = OprNOOP,
		.name = "?",
		.help = "???     Not an operator"
	},
	{ OprNE,	"#" ,	"Test:   Not equal"	},
	{ OprEQ,	"=" ,	"Test:   Equal"		},
	{ OprGT,	">" ,	"Test:   Greater than"	},
	{ OprGE,	">=",	"Test:   Greater than or equal"	},
	{ OprLT,	"<" ,	"Test:   Less than"	},
	{ OprLE,	"<=",	"Test:   Less than or equal"	},
	{ OprAS,	":=",	"Assign: Becomes equal"	},
	{ OprPL,	"+" ,	"Arith:  Add"		},
	{ OprMI,	"-" ,	"Arith:  Subtract"	},
	{ OprTI,	"*" ,	"Arith:  Multiply"	},
	{ OprDI,	"/" ,	"Arith:  Divide"	},
	{ OprAND,	"&" ,	"Bits:   AND"		},
	{ OprOR,	"!" ,	"Bits:   OR"		},
	{ OprXOR,	"!!",	"Bits:   XOR"		},
	{ OprNOT,	"##",	"Bits:   One's Complement"	},
	{ OprNEG,	"#-",	"Bits:	 Two's complement"	},
	{ OprLSH,	"<<",	"Bits:   Left shift"	},
	{ OprRSH,	">>",	"Bits:   Right shift"	},
	{ OprINC,	"++",	"Arith:  Increment"	},
	{ OprDECR,	"--",	"Arith:  Decrement"	},
	{ OprPOP,	";" ,	"Stack:  POP"		},
	{ OprSTM,	"->",	"Stack:  PUSH"		}
};

//! We divide the extended ASCII table in subsets of atom types (atom_t)
static char atomhash[256] = {
	10,9,9,9,9,9,9,9,9,0,0,9,9,0,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	0,1,12,1,9,4,1,9,2,3,1,1,0,1,11,1,5,5,5,5,5,5,5,5,5,5,1,1,1,1,1,1,
	10,6,6,6,6,6,6,6,6,6,6,6,6,6,6 ,6,6,6,6,6,6,6,6,6,6,6,6,7,9,8,9,6,
	9 ,6,6,6,6,6,6,6,6,6,6,6,6,6,6 ,6,6,6,6,6,6,6,6,6,6,6,6,9,9,9,9,9,
	9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9
};

static struct cmd_desc *_cmdlist = NULL;
static int _cmdlist_size = 0;
static struct atom _args_copy[MAX_ARG_COUNT + 1]; /* +1 for the terminator */
static void (*user_sig_hndl)() = NULL; // user sig handler

static const char * const libextest_version_s = "libextest version: "
	__GIT_VER__ ", by " __GIT_USR__ " " __TIME__ " " __DATE__;

/**
 * sighandler - SIGNAL handler
 *
 * @param sig: SIGNAL number
 *
 * We catch signals so that we can properly clean-up the session and exit
 */
static void sighandler(int sig)
{
	/*
	 * we use sys_siglist[] instead of strsignal() to stay compatible
	 * with old versions of glibc
	 */
	printf("\nEXIT: Signal %s received\n", sys_siglist[sig]);
	free(_cmdlist);
	if (user_sig_hndl)
		user_sig_hndl();
	exit(1);
}

/**
 * sighandler_init - register and configure the SIGNAL handler
 */
static void sighandler_init()
{
	struct sigaction act;

	act.sa_handler = sighandler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGFPE,  &act, 0);
	sigaction(SIGINT,  &act, 0);
	sigaction(SIGILL,  &act, 0);
	sigaction(SIGKILL, &act, 0);
	sigaction(SIGQUIT, &act, 0);
	sigaction(SIGSEGV, &act, 0);
	sigaction(SIGTERM, &act, 0);
}

static int check_named_parameter(char *buf)
{
	/*
	 * we are here because *buf = '-'
	 * check if next char is a letter or another '-' followed by many
	 * letters like:
	 * 	-[a-zA-Z]{1}
	 * 	--[a-zA-Z]*
	 */
	if ( (atomhash[(int)*(buf + 1)] == Alpha) ||
	     ( (*(buf + 1) == '-') &&
	       (atomhash[(int)*(buf + 2)] == Alpha) ) )
	       return 1;
	return 0;
}

static char *get_named_parameter(char *input, char *buf, struct atom *atom)
{
	atom_t 	atype;	 /* atom type */
	int 	i;

	/*
	 * parameter name and value can be 'Separator' separated or not.
	 * A named parameter with its value can be specified in several ways:
	 * "--arg1-1" or "--arg1 -1" or "-a-1" or "-a -1":
	 * Too face this issue, named parameter is copied using two loops.
	 * First loop copy '-' character(s)
	 * Second loop copy the parameter name itself
	 */
	atom->pos = (unsigned int)(buf - input);
	i = 0;
	atype = atomhash[(int)(*buf)];
	while (atype == Operator) {
		atom->text[i++] = *buf++;
		atype = atomhash[(int)*buf];
	}
	atype = atomhash[(int)(*buf)];
	while (atype == Alpha) {
		atom->text[i++] = *buf++;
		if (i >= MAX_ARG_LENGTH)
			break;
		atype = atomhash[(int)*buf];
	}
	atom->text[i] = '\0';
	atom->type    = String;
	return buf;
}

/**
 * get_atoms - Split the command and its arguments into atoms
 *
 * @param input - command to split
 * @param atoms - buffer to store atoms. [max: @ref MAX_ARG_COUNT elements]
 *
 * @return number of atoms processed
 */
static int get_atoms(char *input, struct atom *atoms)
{
	char 		*buf;	  /* goes through input */
	char 		*endptr;  /* marks the end within input of an atom */
	atom_t 	atype;	 /* atom type */
	int 	cnt = 0; /* keeps track of @atoms entries */
	int 	i, j;

	if ( input == NULL ||  *input  == '.')
		return cnt; /* repeat last command */

	bzero((void*)atoms, MAX_ARG_COUNT * sizeof(struct atom));
	buf = input;
	while (1) {
		if (cnt >= MAX_ARG_COUNT - 1) { /* avoid overflow */
			atoms[cnt].type = Terminator;
			break;
		}
		atype = atomhash[(int)*buf];
	reeval:
		switch (atype) {
		case Numeric:
			atoms[cnt].val  = (int)strtol(buf, &endptr, 0);
			snprintf(atoms[cnt].text, MAX_ARG_LENGTH, "%d",
				 atoms[cnt].val);
			atoms[cnt].type = Numeric;
			buf = endptr;
			cnt++;
			break;
		case Alpha:
			atoms[cnt].pos = (unsigned int)(buf - input);
			j = 0;
			while (atype == Alpha) {
				atoms[cnt].text[j++] = *buf++;
				if (j >= MAX_ARG_LENGTH)
					break;
				atype = atomhash[(int)*buf];
			}
			atoms[cnt].text[j] = '\0';
			atoms[cnt].type    = Alpha;
			atoms[cnt].cmd_id  = CMD_NOCM;
			for (i = 0; i < _cmdlist_size; ++i) {
				if (!strcmp(_cmdlist[i].name, atoms[cnt].text))
					atoms[cnt].cmd_id = _cmdlist[i].id;
			}
			cnt++;
			break;
		case Quotes:
			/* Strings are "enclosed between quotes" */
			atoms[cnt].pos = (unsigned int)(buf - input);
			j = 0;
			atype = atomhash[(int)(*(++buf))];
			while (atype != Quotes && atype != Terminator) {
				atoms[cnt].text[j++] = *buf++;
				if (j >= MAX_ARG_LENGTH)
					break;
				atype = atomhash[(int)*buf];
			}
			atoms[cnt].text[j] = '\0';
			atoms[cnt].type    = String;
			buf++; /* discard ending quotes */
			cnt++;
			break;
		case Separator:
			while (atype == Separator)
				atype = atomhash[(int)(*(++buf))];
			break;
		case Comment:
			/* Comments are %enclosed between percent marks% */
			atype = atomhash[(int)(*(++buf))];
			while (atype != Comment) {
				atype = atomhash[(int)(*(buf++))];
				if (atype == Terminator) /* no ending % */
					return cnt;
			}
			break;
		case Operator:
			/*
			 * Two cases:
			 * 	pick up negative Numerics
			 * 	or named parameters of the command
			 */
			if (*buf == '-') {
				/*
				 * Check if it is a named parameter
				 * Two syntaxes are supported:
				 * short one -[a-zA-Z]{1}
				 * long one  --[a-zA-Z]*
				 */
				if (check_named_parameter(buf)) {
					buf = get_named_parameter(input, buf,
								  &(atoms[cnt]));
					cnt++;
					break;
				}
				/* It's negative Numerics */
				char *c = buf + 1;
				if (*c && isdigit(*c)) {
					atype = Numeric;
					goto reeval;
				}
			}
			atoms[cnt].pos = (unsigned int)(buf - input);
			j = 0;
			while (atype == Operator) {
				atoms[cnt].text[j++] = *buf++;
				if (j >= MAX_ARG_LENGTH)
					break;
				atype = atomhash[(int)*buf];
			}
			atoms[cnt].text[j] = '\0';
			atoms[cnt].type = Operator;
			for (i = 0; i < OprOPRS; i++) {
				if (!strcmp(oprs[i].name, atoms[cnt].text)) {
					atoms[cnt].oid = oprs[i].id;
					break;
				}
			}
			cnt++;
			break;
		case Open:
		case Close:
		case Open_index:
		case Close_index:
		case Bit:
			atoms[cnt].type = atype;
			buf++;
			cnt++;
			break;
		case Illegal_char:
		case Terminator:
		default:
			atoms[cnt].type = atype;
			return cnt;
		}
	}
	return cnt;
}

/**
 * clone_args - clones @ref nr arguments from a group of atoms w/ termination
 *
 * @param to - address to copy to
 * @param from - address to copy from
 * @param nr - number of atoms to copy
 */
void clone_args(struct atom *to, struct atom *from, int nr)
{
	int i = 0;

	for (i = 0; i <= nr; i++) /* #0 is the cmd itself */
		memcpy(to + i, from + i, sizeof(struct atom));
	(to + i)->type = Terminator;
}

/**
 * get_cmd_by_id - get command using its id
 *
 * @param cmd_id - command id (from the command enum list)
 *
 * @return pointer to cmd descriptor on success
 * @return NULL if there is no command with id @ref cmd_id
 */
struct cmd_desc *get_cmd_by_id(int cmd_id)
{
	int i;

	for (i = 0; i < _cmdlist_size; ++i) {
		if (_cmdlist[i].id == cmd_id)
			return &_cmdlist[i];
	}
	return NULL;
}

struct cmd_desc *get_cmd_by_name(char *cmd_name)
{
	int i;

	for (i = 0; i < _cmdlist_size; ++i) {
		if (!strcmp(_cmdlist[i].name, cmd_name))
			return &_cmdlist[i];
	}
	return NULL;
}

static void atoms_init(struct atom *atoms, unsigned int elems)
{
	int i;

	bzero((void *)atoms, elems * sizeof(struct atom));

	for (i = 0; i < elems; i++) {
		atoms[i].type = Terminator;
	}
}

/**
 * param_amount - catch the amount of parameters for this command
 *
 * @param atoms - stream of atoms (user's command-line input, split into atoms)
 *
 * This function takes a stream of atoms, starting with a particular command.
 * It identifies the number of subsequent atoms that this first atom should
 * be fed with.
 *
 * @return number of parameters @b excluding the command itself
 */
static int param_amount(struct atom *atoms)
{
	int i = 1; /* don't count the command itself */

	/* "help" command is special */
	if (atoms[0].cmd_id == CMD_HELP &&
		atoms[1].type == Alpha && atoms[1].cmd_id != CMD_NOCM) {
		i++;
		goto out;
	}

	/* "atom list" command is special */
	if (atoms[0].cmd_id == CMD_ATOMS) {
		while (atoms[i].type != Terminator &&
			atoms[i].type != Illegal_char)
			i++;
		goto out;
	}

	while (atoms[i].type == Numeric ||
		atoms[i].type == Operator ||
		atoms[i].type == String) {
		if (atoms[i].type == Numeric &&	atoms[i + 1].type == Open)
			break;
		i++;
	}
out:
	return i - 1;
}

/**
 * do_cmd - Execute command described in @atoms
 *
 * @param idx   - index in command atom list to start execution from
 * @param atoms - command atoms list
 *
 * @return end arg index             - if OK
 * @return one of @ref tst_prg_err_t - in case of error
 */
static int do_cmd(int idx, struct atom *atoms)
{
	struct atom 	*ap;	/* atom pointer */
	struct cmd_desc *cdp;	/* command's description */
	int ret;		/* stores what the handler returns */
	int count = 1;		/* stores the Numeric before brackets (Open) */
	int rec;		/* for recursive do_cmd() in Open */
	int i;

	/*
	 * @todo
	 * test what happens when inside a (loop), there is an error.
	 * Would it catch it or not? Would it break badly?
	 */
	atoms_init(_args_copy, MAX_ARG_COUNT + 1);

	while (1) {
		ap = &atoms[idx];
		if (ap->type != Numeric && ap->type != Open)
			count = 1; /* set count to default */
		switch (ap->type) {
		case Numeric:
			count = ap->val;
			idx++;
			break;
		case String:
			idx++;
			break;
		case Open:
			idx++;
			for (i = 0; i < count; i++)
				rec = do_cmd(idx, atoms);
			idx = rec;
			count = 1;
			break;
		case Alpha:
			cdp = get_cmd_by_id(ap->cmd_id);
			cdp->pa = param_amount(ap);
			clone_args(_args_copy, ap, cdp->pa);
			/*
			 * We don't do anything (yet) with ret.
			 * It could be used to implement a richer interface,
			 * but for the time being we'll leave it like that.
			 */
			ret = (*cdp->handle)(cdp, _args_copy);
			/* keep compiler silent */
			(void) ret;
			idx += cdp->pa + 1;
			break;
		case Close:
			idx++;
			return idx;
		case Terminator:
			return idx;
		default:
			idx++;
			return idx;
		}
	}
}

static void set_prompt(char *buf, size_t len, char *prg_name, char *host,
		       int cmd_index)
{
//	snprintf(buf, len, WHITE_ON_BLACK "%s:%s[%02d] > "
//			DEFAULT_COLOR, host, prg_name, cmd_index);
	snprintf(buf, len, "%s:%s[%02d] > ", host, prg_name, cmd_index);
	buf[len - 1] = '\0';
}

/* add a line to the history if it's not a duplicate of the previous */
static void extest_add_history(char *line)
{
	HIST_ENTRY *prev;
	int save_it;

	if (!line[0])
		return;

	using_history();
	prev = previous_history();
	/* save it if there's no previous line or if they're different */
	save_it = !prev || strcmp(line, prev->line);
	using_history();

	if (save_it)
		add_history(line);
}

/****************************
 * built-in command handlers
 ****************************/

/**
 * hndl_illegal - Illegal command handler
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_illegal(struct cmd_desc* cmdd, struct atom *atoms)
{
	printf("%s --> %s\n", atoms->text, cmdd->help);
	return 1;
}

/**
 * hndl_quit - Quit test program
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return nothing; it just cleans up and exits
 */
static int hndl_quit(struct cmd_desc* cmdd, struct atom *atoms)
{
	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("\nexit the test program\n");
		return 1;
	}
	exit(EXIT_SUCCESS);
}

/**
 * cmd_lnm - command's longest name
 *
 * @return strlen of the longest command
 */
static int cmd_lnm(void)
{
	static int len = 0, i;

	if (len)
		return len;
	for (i = 0; i < _cmdlist_size; ++i) {
		if (strlen(_cmdlist[i].name) > len)
			len = strlen(_cmdlist[i].name);
	}
	return len;
}

/**
 * cmd_lopt - command's longest option
 *
 * @return strlen of the longest command option
 */
static int cmd_lopt(void)
{
	static int len = 0, i;

	if (len)
		return len;
	for (i = 0; i < _cmdlist_size; ++i) {
		if (strlen(_cmdlist[i].opt) > len)
			len = strlen(_cmdlist[i].opt);
	}
	return len;
}

/**
 * hndl_help - User help
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_help(struct cmd_desc* cmdd, struct atom *atoms)
{
	struct cmd_desc *cdp = NULL; /* command description ptr */
	int i, j;

	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s - display full command list\n"
			"%s o - help on the operators\n"
			"%s cmd - help on the command 'cmd'\n",
			cmdd->name, cmdd->name, cmdd->name);
		goto out;
	}

	if (atoms[1].type == Alpha && !strcmp(atoms[1].text, "o")) {
		printf("\nOPERATORS:\n\n");
		for (i = 1; i < OprOPRS; i++)
			printf("Id: %2d Opr: %s \t--> %s\n",
				oprs[i].id, oprs[i].name, oprs[i].help);
	} else if (atoms[1].type == Alpha &&
		(cdp = get_cmd_by_name(atoms[1].text))) {
		(*cdp->handle)(cdp, (struct atom *)VERBOSE_HELP);
	} else {
		printf("Valid COMMANDS:\n%-*s %-*s %-*s %-s\n",
			(int)strlen("#xx:"), "Idx", cmd_lnm(), "Name",
			(int)strlen("[  ] ->") + cmd_lopt(), "Params",
			"Description");
		for (i = 0, j = 0; i < _cmdlist_size; ++i) {
			if (_cmdlist[i].valid)
				printf("#%2d: %-*s [ %-*s ] -> %s\n", ++j,
					cmd_lnm(), _cmdlist[i].name, cmd_lopt(),
					_cmdlist[i].opt, _cmdlist[i].help);
		}
		printf("\nType \"h name\" to get complete command help\n");
	}
out:
	return 1;
}

/**
 * hndl_history - Command history print-out
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_history(struct cmd_desc* cmdd, struct atom *atoms)
{
	HIST_ENTRY **list;
	int i;

	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s - display command history\n", cmdd->name);
		goto out;
	}
	list = history_list();
	if (list) {
		for (i = 0; list[i]; i++)
			printf("Cmd[%02d] %s\n", i, list[i]->line);
	}
out:
	return 1;
}

/**
 * hndl_atoms - Print out the command line split into atoms
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * Useful for debugging
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_atoms(struct cmd_desc* cmdd, struct atom *atoms)
{
	struct cmd_desc *dp;
	struct atom 	*ap;
	int i = 0;

	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s - split the command line input into atoms\n",
			cmdd->name);
		goto out;
	}

	printf("Arg list: Consists of %d atoms.\n", cmdd->pa);
	for (i = 0; i < cmdd->pa; i++) {
		ap = &atoms[i + 1]; /* skip command itself */
		switch (ap->type) {
		case Numeric:
			printf("Arg[%02d] Num: %d\n", i + 1, ap->val);
			break;
		case Alpha:
			if (ap->cmd_id) {
				dp = get_cmd_by_id(ap->cmd_id);
				printf("Arg[%02d] Cmd: %d Name: %s Help: %s\n",
					i + 1, ap->cmd_id, dp->name, dp->help);
			} else /* illegal command, i.e. 'CMD_NOCM' case */
				printf("Arg[%02d] Unknown Command: %s\n",
					i + 1, ap->text);
			break;
		case Operator:
			if (ap->oid) {
				int id = ap->oid;
				printf("Arg[%02d] Operator: %s Help: %s\n",
					i + 1, oprs[id].name, oprs[id].help);
			}
			else
				printf("Arg[%02d] Unknown Operator: %s\n",
					i + 1, ap->text);
			break;
		case String:
			printf("Arg[%02d] String: \"%s\"\n", i + 1, ap->text);
			break;
		case Open:
			printf("Arg[%02d] Opn: (\n", i + 1);
			break;
		case Close:
			printf("Arg[%02d] Cls: )\n", i + 1);
			break;
		case Open_index:
			printf("Arg[%02d] Opn: [\n", i + 1);
			break;
		case Close_index:
			printf("Arg[%02d] Cls: ]\n", i + 1);
			break;
		case Bit:
			printf("Arg[%02d] Bit: .\n", i + 1);
			break;
		case Terminator:
			printf("Arg[%02d] End: @\n", i + 1);
			break;
		default:
			printf("Arg[%02d] ???\n", i + 1);
		}
	}
out:
	return 1;
}

/**
 * hndl_sleep - sleep for a few seconds
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_sleep(struct cmd_desc* cmdd, struct atom *atoms)
{
	int seconds;

	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s [n] - put the process to sleep for 'n' seconds\n"
			"\tNote that n=1s is the default value.\n",
			cmdd->name);
		goto out;
	}
	atoms++;
	if (atoms->type == Numeric)
		seconds = atoms->val;
	else
		seconds = 1;
	sleep(seconds);
out:
	return 1;
}

/**
 * hndl_msleep - sleep for a few milliseconds
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_msleep(struct cmd_desc* cmdd, struct atom *atoms)
{
	int usec = 1000; /*default */

	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s [n] - put the process to sleep for 'n' milliseconds\n"
			"\tNote that n=1ms is the default value.\n",
			cmdd->name);
		goto out;
	}
	atoms++;
	if (atoms->type == Numeric)
		usec = atoms->val * 1000;

	usleep(usec);
out:
	return 1;
}
/**
 * hndl_shell - execute a shell command
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return >= 0 - on success
 * @return tst_prg_err_t - on failure
 */
static int hndl_shell(struct cmd_desc* cmdd, struct atom *atoms)
{
	if (atoms == (struct atom*)VERBOSE_HELP) {
		printf("%s \"shellcmd args\" - execute a shell command.\n"
			"Note that shellcmd and its arguments need to be "
			"enclosed in double quotes. These can't be escaped "
			"so use single quotes (if needed) inside args.\n",
			cmdd->name);
		goto out;
	}

	if ((++atoms)->type != String)
		return -TST_ERR_WRONG_ARG;

	system(atoms->text);
out:
	return 1;
}

//!< Standard commands definitions
static struct cmd_desc _built_in_cmdlist[CMD_USR] = {
	[CMD_NOCM]
	{
		.valid  = 0,
		.id     = CMD_NOCM,
		.name   = "???",
		.help   = "Illegal command",
		.opt    = "",
		.comp   = 0,
		.handle = hndl_illegal,
	},
	[CMD_QUIT]
	{
		1, CMD_QUIT, "q", "Quit test program", "", 0, hndl_quit,
	},
	[CMD_HELP]
	{
		1, CMD_HELP, "h", "Help on commands", "o c", 0, hndl_help,
	},
	[CMD_ATOMS]
	{
		1, CMD_ATOMS, "a", "Atom list commands", "", 0, hndl_atoms,
	},
	[CMD_HIST]
	{
		1, CMD_HIST, "his", "History", "", 0, hndl_history,
	},
	[CMD_SLEEP]
	{
		1, CMD_SLEEP, "s", "Sleep seconds", "Seconds", 0, hndl_sleep,
	},
	[CMD_MSLEEP]
	{
		1, CMD_MSLEEP, "ms", "Sleep milliseconds", "MilliSecs", 0, hndl_msleep,
	},
	[CMD_SHELL]
	{
		1, CMD_SHELL, "sh", "Shell command", "Unix Cmd", 1, hndl_shell,
	},
};

/********************************************
 * extest's User's API
 *******************************************/

const char * const extest_get_version()
{
	return libextest_version_s;
}

/**
 * do_yes_no - Get user answer (y/n)
 *
 * @param question - prompt the user with it
 * @param extra    - extra argument to the question (can be NULL)
 *
 * @return 1 - user replied 'yes'
 * @return 0 - user replied 'no'
 */
int extest_do_yes_no(char *question, char *extra)
{
	int reply;

ask:
	printf("%s", question);
	if (extra)
		printf(" %s", extra);
	printf("? (y/n) > ");
	reply = getchar();
	if (reply == EOF)
		return 0;
	else if (reply != '\n') {
		/* escape subsequent characters and the ending '\n' */
		while (getchar() != '\n')
			;
	}
	if (reply != 'y' && reply != 'n' && /* accept y/n & Y/N */
		reply != 'Y' && reply != 'N') {
		printf("answer y/n only\n");
		goto ask;
	}
	return reply == 'y';
}

/**
 * compulsory_ok - checks that compulsory parameters are passed
 *
 * @param cmdd  - command description
 * @param atoms - command atoms list
 *
 * @return 1 - ok
 * @return 0 - not ok
 */
int extest_compulsory_ok(struct cmd_desc *cmdd)
{
	return cmdd->pa == cmdd->comp;
}

/**
 * is_last_atom - checks if @ref atom is the last in the list to be processed
 *
 * @param atom - command atoms list
 *
 * An atom is considered to be the last one of a list if it is either
 * the terminator or the atom preceding the terminator
 *
 * @return 1 - if it's the last one
 * @return 0 - if it isn't
 */
int extest_is_last_atom(struct atom *atom)
{
	if (atom->type == Terminator)
		return 1;
	if ((atom + 1)->type == Terminator)
		return 1;
	return 0;
}
/*********************************************************************/
/**
 * extest_register_user_cmd - Register user commands
 *
 * @param user_cmds - user commands list
 * @param user_cmds_nb - user commands count
 *
 * This function it is meant to register the list of specific user commands
 * which will be added to the built-in ones.
 * Note: user command's id should be greater than CMD_USR otherwise commands are
 * not registered.
 *
 * @return 0 on success or -1 on failure
 */
int extest_register_user_cmd(struct cmd_desc user_cmdlist[], int user_cmd_nb)
{
	int i;

	/*
	 * check that none of the user command got an id in conflict with the
	 * built-in commands id.
	 */
	for (i = 0; i < user_cmd_nb; ++i) {
		if (user_cmdlist[i].id < CMD_USR) {
			fprintf(stderr,
				"User cmd'id should be greater than CMD_USR.\n");
			return -1;
		}
	}
	/*
	 * build cmdlist grouping built-in and user commands
	 */
	_cmdlist = calloc(user_cmd_nb + CMD_USR, sizeof(struct cmd_desc));
	if (_cmdlist == NULL) {
		fprintf(stderr, "Allocating cmdlist failed: %s\n",
			strerror(errno));
		return -1;
	}
	memcpy(_cmdlist, _built_in_cmdlist, sizeof(struct cmd_desc) * CMD_USR);
	memcpy(&_cmdlist[CMD_USR], user_cmdlist,
	       sizeof(struct cmd_desc) * user_cmd_nb);
	_cmdlist_size = CMD_USR + user_cmd_nb;
	return 0;
}

/**
 * extest_init - Initialise extest
 *
 * @param argc - argument counter
 * @param  char *argv[] - argument values
 *
 * This function it is meant to run as long as the test program process runs.
 * It will only return when there is a severe failure -- such as a signal.
 *
 * @return EXIT_FAILURE - on failure
 */
int extest_run(char *prg_name, void (*user_sighandler)())
{
	struct atom cmd_atoms[MAX_ARG_COUNT];
	char host[32] = { 0 };
	char prompt[128]; /* prompt string */
	char *cmd; /* command pointer */
	int cmdindx = 0;

	sighandler_init();
	user_sig_hndl = user_sighandler; // store user signal handler
	gethostname(host, 32);
	using_history();

	while(1) {
		bzero(prompt, sizeof(prompt));
		if (isatty(fileno(stdin)))
			set_prompt(prompt, sizeof(prompt), prg_name, host,
				   cmdindx++);
		cmd = readline(prompt);
		if (!cmd) {
			free(_cmdlist);
			break; //leave infinite loop
		}
		extest_add_history(cmd);
		atoms_init(cmd_atoms, MAX_ARG_COUNT);
		get_atoms(cmd, cmd_atoms); /* split cmd into atoms */
		do_cmd(0, cmd_atoms); /* execute atoms */
		free(cmd);
	}
	return 0;
}
