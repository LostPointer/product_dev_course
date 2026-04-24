import type { SVGProps } from 'react'

type IconProps = SVGProps<SVGSVGElement>

export function SearchIcon({ width = 15, height = 15, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 20 20" fill="none" {...props}>
      <circle cx="9" cy="9" r="5.5" stroke="currentColor" strokeWidth="1.6" />
      <path d="m13.5 13.5 3 3" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
    </svg>
  )
}

export function UserIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 20 20" fill="none" {...props}>
      <circle cx="10" cy="7" r="3" stroke="currentColor" strokeWidth="1.5" />
      <path d="M4 16.5c.9-2.6 3.2-4 6-4s5.1 1.4 6 4" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
    </svg>
  )
}

export function EyeIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" {...props}>
      <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7S2 12 2 12z" />
      <circle cx="12" cy="12" r="3" />
    </svg>
  )
}

export function TeamIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" {...props}>
      <circle cx="9" cy="8" r="3" />
      <path d="M2 21a7 7 0 0 1 14 0" />
      <circle cx="17" cy="7" r="2.5" />
      <path d="M22 18a5 5 0 0 0-8-3.5" />
    </svg>
  )
}

export function FolderIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 20 20" fill="none" {...props}>
      <path
        d="M2.5 6.5A1.5 1.5 0 0 1 4 5h3.3a1.5 1.5 0 0 1 1.06.44l.94.94a1.5 1.5 0 0 0 1.06.44H16a1.5 1.5 0 0 1 1.5 1.5v6A1.5 1.5 0 0 1 16 15.82H4A1.5 1.5 0 0 1 2.5 14.3V6.5Z"
        stroke="currentColor" strokeWidth="1.5" strokeLinejoin="round"
      />
    </svg>
  )
}

export function TagIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 20 20" fill="none" {...props}>
      <path
        d="M10.5 3h5v5l-6.5 6.5a1.5 1.5 0 0 1-2.1 0l-2.9-2.9a1.5 1.5 0 0 1 0-2.1L10.5 3Z"
        stroke="currentColor" strokeWidth="1.5" strokeLinejoin="round"
      />
      <circle cx="12.5" cy="6.5" r=".9" fill="currentColor" />
    </svg>
  )
}

export function SensorIcon({ width = 14, height = 14, ...props }: IconProps) {
  return (
    <svg width={width} height={height} viewBox="0 0 20 20" fill="none" {...props}>
      <circle cx="10" cy="10" r="2" fill="currentColor" />
      <circle cx="10" cy="10" r="5" stroke="currentColor" strokeWidth="1.4" />
      <circle cx="10" cy="10" r="8" stroke="currentColor" strokeWidth="1.2" opacity=".5" />
    </svg>
  )
}
