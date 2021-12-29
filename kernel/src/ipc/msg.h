#ifndef MSG_H_
#define MSG_H_

#include <os_types.h>
#include <proc.h>

/** \brief Отправить сообщение
 * \param conid     Номер соединения
 * \param m         Отправляемое сообщение
 * \param timeout   Таймаут на отправку сообщения
 * \return Ошибки исполнения
 * */
int send (struct thread * const thr, const int conid, struct msg * const m, const uint64_t timeout, const unsigned long flags);

/** \brief Получить сообщение
 * \param chid      Номер канала
 * \param m[out]    Получаемое сообщение
 * \param timeout   Таймаут на получение
 * \return Номер канала для ответа или ошибки исполнения
 * \retval 0  Ответ не требуется
 * */
int receive (struct thread * const thr, const int chid, struct msg ** const m, const uint64_t timeout);

#endif /* MSG_H_ */
