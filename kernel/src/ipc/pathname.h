#ifndef _PATHNAME_H_
#define _PATHNAME_H_

#include <os_types.h>

typedef void* pathname_t;

void pathname_init ();

pathname_t* create_pathname (char *pathname);
pathname_t* lock_pathname (char *pathname, char **suffix);
void unlock_pathname (pathname_t *pathname);
void delete_pathname (pathname_t *pathname);
void set_pathname_ref (pathname_t *pathname, void *ref);
void* get_pathname_ref (pathname_t *pathname);

#endif /* _PATHNAME_H_ */
