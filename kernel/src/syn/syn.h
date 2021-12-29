#ifndef SYN_H_
#define SYN_H_

#include <os_types.h>
#include <proc.h>

/**
 * Менеджер объектов синхронизации в системе (мьютексы, семафоры, барьеры,...)
 */
void syn_allocator_init();
void syn_allocator_lock();
void syn_allocator_unlock();

int syn_create(syn_t *s, struct process *p);
int syn_delete (int id, int unlink, struct process *p);
int syn_open (char *pathname, enum syn_type type, struct process *p);
int syn_close (int id, struct process *p);
int syn_wait (int id, struct thread *thr, uint64_t timeout);
int syn_done (int id, struct thread *thr);

void syn_proc_finalize(struct process *p);

#endif /* SYN_H_ */
