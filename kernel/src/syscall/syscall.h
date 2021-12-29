#ifndef SYSCALL_H_
#define SYSCALL_H_

void syscall_handler(struct cpu_context *ctx, int syscall_num);



void sc_proc_create(struct thread *thr);
void sc_proc_kill(struct thread *thr);
void sc_proc_info(struct thread *thr);

void sc_thread_create(struct thread *thr);
void sc_thread_exit(struct thread *thr);
void sc_thread_kill(struct thread *thr);
void sc_thread_join(struct thread *thr);
void sc_thread_yield(struct thread *thr);
void sc_thread_yield_to(struct thread *thr);
void sc_thread_run(struct thread *thr);
void sc_thread_stop(struct thread *thr);
void sc_thread_sleep(struct thread *thr);
void sc_thread_prio(struct thread *thr);

void sc_mmap(struct thread *thr);
void sc_malloc (struct thread *thr);
void sc_mfree (struct thread *thr);

void sc_send (struct thread *thr);
void sc_receive (struct thread *thr);
void sc_channel_open (struct thread *thr);
void sc_channel_wait_connection (struct thread *thr);
void sc_channel_complete_connection (struct thread *thr);
void sc_channel_close (struct thread *thr);
void sc_connection_open (struct thread *thr);
void sc_connection_close (struct thread *thr);

void sc_syn_create(struct thread *thr);
void sc_syn_delete(struct thread *thr);
void sc_syn_open(struct thread *thr);
void sc_syn_close(struct thread *thr);
void sc_syn_wait(struct thread *thr);
void sc_syn_done(struct thread *thr);

void sc_irq_hook(struct thread *thr);
void sc_irq_release(struct thread *thr);
void sc_irq_ctrl(struct thread *thr);


void sc_time(struct thread *thr);

#endif /* SYSCALL_H_ */
