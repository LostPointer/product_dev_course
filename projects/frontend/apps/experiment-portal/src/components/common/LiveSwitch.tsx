import './LiveSwitch.scss'

interface LiveSwitchProps {
  live: boolean
  onChange: (live: boolean) => void
}

function LiveSwitch({ live, onChange }: LiveSwitchProps) {
  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault()
      onChange(!live)
    }
  }
  return (
    <div
      className={`liveswitch${live ? ' liveswitch--on' : ''}`}
      role="switch"
      aria-checked={live}
      tabIndex={0}
      onClick={() => onChange(!live)}
      onKeyDown={handleKeyDown}
    >
      <div className="liveswitch__track">
        <div className="liveswitch__thumb" />
      </div>
      <span className="liveswitch__dot" />
      <span className="liveswitch__label">{live ? 'Live' : 'History'}</span>
    </div>
  )
}

export default LiveSwitch
