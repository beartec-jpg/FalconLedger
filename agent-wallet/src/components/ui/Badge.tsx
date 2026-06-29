import type { PropsWithChildren } from 'react'
import clsx from 'clsx'

interface BadgeProps extends PropsWithChildren {
  variant?: 'success' | 'default'
}

export function Badge({ variant = 'default', children }: BadgeProps) {
  return (
    <span
      className={clsx(
        'inline-flex items-center rounded-full px-2.5 py-1 text-xs font-semibold',
        variant === 'success' ? 'bg-green-500/20 text-green-300' : 'bg-accent/20 text-violet-200'
      )}
    >
      {children}
    </span>
  )
}
