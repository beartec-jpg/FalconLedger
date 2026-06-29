import type { ButtonHTMLAttributes, PropsWithChildren } from 'react'
import clsx from 'clsx'

interface ButtonProps extends PropsWithChildren, ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'primary' | 'ghost' | 'danger'
}

export function Button({ variant = 'primary', className, children, ...props }: ButtonProps) {
  return (
    <button
      className={clsx(
        'rounded-lg px-4 py-2 text-sm font-medium transition-colors disabled:cursor-not-allowed disabled:opacity-50',
        variant === 'primary' && 'bg-accent text-white hover:bg-violet-500',
        variant === 'ghost' && 'bg-white/5 text-white hover:bg-white/10',
        variant === 'danger' && 'bg-red-500/20 text-red-200 hover:bg-red-500/30',
        className
      )}
      {...props}
    >
      {children}
    </button>
  )
}
