#ifndef MSGNO_H
#define MSGNO_H

/* msgno - managing error codes and associated messages across
 * separate C libraries
 */

#if defined(__GNUC__)
#if defined(MSGNO)

#include <stdio.h>

#if defined(__GNUC__) && (__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 95))

#define MSG(fmt, args...) _msgno_printf("%s:%u:%s: " fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, ## args)
#define MNO(msgno) _msgno_printf("%s:%u:%s: %s\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno))
#define MNF(msgno, fmt, args...) _msgno_printf("%s:%u:%s: %s" fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno), ## args)

#define PMSG(fmt, args...) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u:%s: " fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, ## args))
#define PMNO(msgno) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u:%s: %s\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno)))
#define PMNF(msgno, fmt, args...) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u:%s: %s" fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno), ## args))

#define AMSG(fmt, args...) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u:%s: " fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, ## args))
#define AMNO(msgno) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u:%s: %s\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno)))
#define AMNF(msgno, fmt, args...) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u:%s: %s" fmt "\n", \
		__FILE__, __LINE__, __FUNCTION__, msgno_msg(msgno), ## args))

#else

#define MSG(fmt, args...) _msgno_printf("%s:%u: " fmt "\n", \
		__FILE__, __LINE__ , ## args)
#define MNO(msgno) _msgno_printf("%s:%u: %s\n", \
		__FILE__, __LINE__, msgno_msg(msgno))
#define MNF(msgno, fmt, args...) _msgno_printf("%s:%u: %s" fmt "\n", \
		__FILE__, __LINE__, msgno_msg(msgno) , ## args)

#define PMSG(fmt, args...) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u: " fmt "\n", __FILE__, __LINE__ , ## args))
#define PMNO(msgno) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u: %s\n", __FILE__, __LINE__, msgno_msg(msgno)))
#define PMNF(msgno, fmt, args...) (_msgno_buf_idx = sprintf(_msgno_buf, \
		"%s:%u: %s" fmt "\n", __FILE__, __LINE__, msgno_msg(msgno) , ## args))

#define AMSG(fmt, args...) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u: "fmt"\n", __FILE__, __LINE__ , ## args))
#define AMNO(msgno) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u: %s\n", __FILE__, __LINE__, msgno_msg(msgno)))
#define AMNF(msgno, fmt, args...) (_msgno_buf_idx += sprintf(_msgno_buf + _msgno_buf_idx, \
		"  %s:%u: %s" fmt "\n", __FILE__, __LINE__, msgno_msg(msgno) , ## args))

#endif
#else

#define MSG(fmt, args...)
#define MNO(msgno)
#define MNF(msgno, fmt, args...)
#define PMSG(fmt, args...)
#define PMNO(msgno)
#define PMNF(msgno, fmt, args...)
#define AMSG(fmt, args...)
#define AMNO(msgno)
#define AMNF(msgno, fmt, args...)

#endif
#else
#if defined(MSGNO)

#define MSG msgno_hdlr_msg
#define MNO msgno_hdlr_mno
#define MNF msgno_hdlr_mnf
#define PMSG msgno_hdlr_msg
#define PMNO msgno_hdlr_mno
#define PMNF msgno_hdlr_mnf
#define AMSG msgno_hdlr_msg
#define AMNO msgno_hdlr_mno
#define AMNF msgno_hdlr_mnf

#else

#define MSG msgno_noop_msg
#define MNO msgno_noop_mno
#define MNF msgno_noop_mnf
#define PMSG msgno_noop_msg
#define PMNO msgno_noop_mno
#define PMNF msgno_noop_mnf
#define AMSG msgno_noop_msg
#define AMNO msgno_noop_mno
#define AMNF msgno_noop_mnf

#endif
#endif

#define NULL_POINTER_ERR _builtin_codes[0].msgno

struct msgno_entry {
	int msgno;
	const char *msg;
};

extern struct msgno_entry _builtin_codes[];
extern char _msgno_buf[];
extern unsigned int _msgno_buf_idx;
extern int (*msgno_hdlr)(const char *fmt, ...);
void _msgno_printf(const char *fmt, ...);

int msgno_add_codes(struct msgno_entry *list);
const char *msgno_msg(int msgno);
int msgno_hdlr_stderr(const char *fmt, ...);

int msgno_hdlr_msg(const char *fmt, ...);
int msgno_hdlr_mno(int msgno);
int msgno_hdlr_mnf(int msgno, const char *fmt, ...);
int msgno_noop_msg(const char *fmt, ...);
int msgno_noop_mno(int msgno);
int msgno_noop_mnf(int msgno, const char *fmt, ...);

#endif /* MSGNO_H */

