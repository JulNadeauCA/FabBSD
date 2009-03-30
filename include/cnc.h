/*	Public domain	*/

#ifndef _CNC_H_
#define _CNC_H_

#include <sys/cnc.h>
#include <sys/cdefs.h>

__BEGIN_DECLS
const char	*CNC_GetError(void);
void		 CNC_SetError(const char *);
int		 CNC_Open(const char *);
void		 CNC_Close(void);
__END_DECLS
#endif /* _CNC_H_ */
