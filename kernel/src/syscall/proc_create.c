#include <proc.h>
#include <mem\vm.h>
#include <mem\kmem.h>
#include <common\namespace.h>

mem_attributes_t attr = {
    .shared = MEM_SHARED_OFF, //
    .exec = MEM_EXEC_NEVER, //
    .type = MEM_TYPE_NORMAL, //
    .inner_cached = MEM_CACHED_WRITE_BACK, //
    .outer_cached = MEM_CACHED_WRITE_BACK, //
    .process_access = MEM_ACCESS_RW, //
    .os_access = MEM_ACCESS_RW //
        };

// args = (const struct proc_header *hdr, struct proc_attr *attr)
void sc_proc_create (struct thread *thr)
{
    struct proc_header *phdr = (struct proc_header *) thr->uregs->basic_regs[0], *copyhdr;
    struct proc_attr *pattr = (struct proc_attr *) thr->uregs->basic_regs[1];
    // TODO какие-то проверки безопасности если нужно

    // возможны 2 варианта:
    // - заголовок находится в памяти, не доступной текущему процессу-родителю
    // - заголовок находится в памяти, доступной текущему процессу-родителю,
    // в любом случае необходимо его скопировать в память ядра и
    // там он останется в структуре процесса

    struct mmap *mm = thr->proc->mmap;
    int pfree = 0;
    size_t pathname_length = 0;

    if (!vm_map_lookup(mm, (void *) phdr, sizeof(struct proc_header))) {
        // Заголовок вне карты памяти текущего процесса, то есть необходимо подключать к доступу 1 страницу памяти,
        // основной метод запуска нового процесса, который был заранее подгружен при запуске системы
        // в определенное зарезервированное место.
        // Внимание! В этом случае заголовок должен быть выровнен на страницу и
        // не должен превышать одну страницу вместе со строкой имени pathname.
        // Библиотека os-rtl гарантирует это, если сборка программного модуля ведется полностью на ее основе.
        if( ((size_t)phdr & (mmu_page_size() - 1)) != 0 ) {
            thr->uregs->basic_regs[CPU_REG_0] = ERR_ILLEGAL_ARGS;
            return;
        }
        if(vm_map(thr->proc, phdr, 1, attr) != OK) {
            thr->uregs->basic_regs[CPU_REG_0] = ERR_BUSY;
            return;
        }
        if( (phdr->pathname != NULL) && (
                ((((size_t)phdr->pathname) + PATHNAME_MAX_BUF) & (~(mmu_page_size() - 1))) != (size_t)phdr) ) {
            // строка pathname находится за пределами страницы
            vm_free(thr->proc, (void *) phdr);
            thr->uregs->basic_regs[CPU_REG_0] = ERR_ILLEGAL_ARGS;
            return;
        }
        pfree = 1;
    }
    int seglen;
    // необходима предварительная проверка заголовка
    // чтобы потом выделить память, здесь же можно
    // выполнить подсчет хэша заголовка или его расшифровку TODO
    if (phdr->magic != PROC_HEADER_MAGIC) {
        if(pfree) {
            vm_free(thr->proc, (void *) phdr);
        }
        thr->uregs->basic_regs[CPU_REG_0] = ERR;
        return;
    }
    seglen = sizeof(struct proc_seg) * phdr->proc_seg_cnt;
    if(phdr->pathname != NULL) {
        pathname_length = strnlen(phdr->pathname, PATHNAME_MAX_LENGTH - PROCS_PATHNAME_ADD_LEN);
        if(pathname_length == PATHNAME_MAX_LENGTH - PROCS_PATHNAME_ADD_LEN) {
            if(pfree) {
                vm_free(thr->proc, (void *) phdr);
            }
            thr->uregs->basic_regs[CPU_REG_0] = ERR_PATHNAME_TOO_LONG;
            return;
        }
    }
    copyhdr = (struct proc_header *) kmalloc(
            sizeof(struct proc_header) + seglen + pathname_length + PROCS_PATHNAME_ADD_LEN + 16);
    memcpy(copyhdr, phdr, sizeof(struct proc_header) + seglen);
    copyhdr->pathname = (void *)copyhdr + sizeof(struct proc_header) + seglen + 8;
    if(pathname_length > 0) {
        // скопируем строку pathname, убирая лишние не обязательные разделители вначале и в конце, если есть;
        // при этом, если был только один или два разделителя, они будут заменены на пустую строку
        if( (phdr->pathname[0] == NAMESPACE_DEFAULT_SEPARATOR) || (phdr->pathname[0] == NAMESPACE_DEFAULT_ALT_SEPARATOR) ) {
            strcpy(copyhdr->pathname, phdr->pathname + 1);
            if( (pathname_length > 1) && ((phdr->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_SEPARATOR) ||
                    (phdr->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_ALT_SEPARATOR)) ) {
                copyhdr->pathname[pathname_length - 2] = '\0';
            }
        } else {
            strcpy(copyhdr->pathname, phdr->pathname);
            if( (phdr->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_SEPARATOR) ||
                    (phdr->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_ALT_SEPARATOR) ) {
                copyhdr->pathname[pathname_length - 1] = '\0';
            }
        }
    } else {
        copyhdr->pathname[0] = '\0';
    }

    if (pfree) {
        vm_free(thr->proc, (void *) phdr);
    }

    // TODO сделать поддержку мэпинга несколькими процессами одних и тех же регионов
    // для запуска нескольких идентичных процессов с одного образа, если в нем отсутствуют
    // регионы чт/зп, возможно добавив счетчик использования в page.c на каждый такой регион
    // для освобождения региона только после cnt = 0;
    pattr->tid = DYNAMIC_ALLOC_ID;
    pattr->pid = DYNAMIC_ALLOC_ID;
    int ret = proc_init(copyhdr, pattr, thr->proc);
    if (ret == OK) {
        thr->uregs->basic_regs[CPU_REG_0] = pattr->pid;
        if(!isKernelPID(pattr->pid)) {
            // а пока блокируем создание нескольких процессов с одного образа захватом и удержанием
            // страницы заголовка до завершения процесса
            // расчитываем что сейчас поток нового процесса еще не был запущен и
            // значит точно не завершился и не закрылся процесс, не совсем чисто конечно...
            // при этом захват может не состояться, если в proc_init он выполнен,
            // поэтому возвращаемое значение нас не интересует
            //vm_map(get_proc(pattr->pid), phdr, 1, attr);
        }
    } else {
        kfree(copyhdr);
        thr->uregs->basic_regs[CPU_REG_0] = ret;
    }
}
