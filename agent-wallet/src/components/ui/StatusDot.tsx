import clsx from 'clsx'

export function StatusDot({ active }: { active: boolean }) {
  return <span className={clsx('inline-block size-2.5 rounded-full', active ? 'bg-green-400 shadow-[0_0_8px_#4ade80]' : 'bg-gray-500')} />
}
