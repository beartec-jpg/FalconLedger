import type { PropsWithChildren } from 'react'
import clsx from 'clsx'

interface CardProps extends PropsWithChildren {
  className?: string
}

export function Card({ className, children }: CardProps) {
  return <div className={clsx('rounded-xl border border-white/10 bg-card p-5 shadow-lg shadow-black/20', className)}>{children}</div>
}
