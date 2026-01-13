export function getCookie(name: string): string | undefined {
    if (typeof document === 'undefined') return undefined
    const raw = document.cookie || ''
    if (!raw) return undefined

    const parts = raw.split(';')
    for (const part of parts) {
        const trimmed = part.trim()
        if (!trimmed) continue
        const idx = trimmed.indexOf('=')
        if (idx === -1) continue
        const key = decodeURIComponent(trimmed.slice(0, idx))
        if (key !== name) continue
        return decodeURIComponent(trimmed.slice(idx + 1))
    }
    return undefined
}

export function getCsrfToken(): string | undefined {
    return getCookie('csrf_token')
}

