'use client'

import { QRCodeSVG } from 'qrcode.react'
import { Card } from '@/components/ui/Card'

export function ReceiveCard({ address }: { address: string }) {
  return (
    <Card className="flex flex-col items-center">
      <h3 className="text-base font-semibold text-white">Receive qXRP</h3>
      <div className="mt-4 rounded-lg bg-white p-3">
        <QRCodeSVG value={address} size={120} />
      </div>
      <p className="mt-3 break-all text-center text-xs text-zinc-400">{address}</p>
    </Card>
  )
}
