#ifndef PLOCAL_SEM_H_
#define PLOCAL_SEM_H_

int plocal_sem_wait (syn_t *m, uint64_t timeout);
int plocal_sem_done (syn_t *m);

#endif /* PLOCAL_SEM_H_ */
