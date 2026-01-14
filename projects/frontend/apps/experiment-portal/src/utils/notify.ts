import { emitToast } from './toastBus'
import { IS_TEST } from './env'

export function notifyError(message: string): void {
    if (IS_TEST) return
    const msg = (message || '').trim()
    if (!msg) return

    emitToast({
        kind: 'text',
        title: 'Ошибка',
        message: msg,
        durationMs: 6000,
    })
}

