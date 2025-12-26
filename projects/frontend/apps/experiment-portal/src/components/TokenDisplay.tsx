import { useState } from 'react'
import './TokenDisplay.css'

interface TokenDisplayProps {
  token: string
  warning?: string
  onClose?: () => void
  showCloseButton?: boolean
}

function TokenDisplay({
  token,
  warning = '⚠️ Сохраните токен сейчас! Он больше не будет показан.',
  onClose,
  showCloseButton = true,
}: TokenDisplayProps) {
  const [copied, setCopied] = useState(false)

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(token)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
    } catch (err) {
      alert('Не удалось скопировать токен')
    }
  }

  return (
    <div className="token-display">
      {warning && <p className="token-warning">{warning}</p>}
      <div className="token-box">
        <label>Токен:</label>
        <div className="token-value">
          <code>{token}</code>
          <button
            type="button"
            className="btn btn-secondary btn-sm"
            onClick={handleCopy}
          >
            {copied ? 'Скопировано!' : 'Копировать'}
          </button>
          {showCloseButton && onClose && (
            <button
              type="button"
              className="btn btn-secondary btn-sm"
              onClick={onClose}
            >
              Закрыть
            </button>
          )}
        </div>
      </div>
    </div>
  )
}

export default TokenDisplay

