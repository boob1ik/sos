#ifndef PLOCAL_SEM_H_
#define PLOCAL_SEM_H_

int plocal_sem_wait (int id, int tid, syn_t *m, uint64_t timeout);
int plocal_sem_done (int id, int tid, syn_t *m);

#endif /* PLOCAL_SEM_H_ */
