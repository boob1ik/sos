#ifndef PLOCAL_MUTEX_H_
#define PLOCAL_MUTEX_H_

int plocal_mutex_wait (int id, int tid, syn_t *m, uint64_t timeout);
int plocal_mutex_done (int id, int tid, syn_t *m);

#endif /* PLOCAL_MUTEX_H_ */
