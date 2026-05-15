// Rate limiting — uses Upstash Redis when env vars present, in-memory fallback for dev

import { Ratelimit } from '@upstash/ratelimit'
import { Redis } from '@upstash/redis'

const REQUESTS = parseInt(process.env.RATE_LIMIT_REQUESTS ?? '1', 10)
const WINDOW_SECONDS = parseInt(process.env.RATE_LIMIT_WINDOW_SECONDS ?? '86400', 10)

// --------------------------------------------------------------------------
// Upstash Redis limiter (production)
// --------------------------------------------------------------------------
function makeRedisLimiter(): Ratelimit | null {
  if (!process.env.KV_REST_API_URL || !process.env.KV_REST_API_TOKEN) return null
  const redis = new Redis({
    url: process.env.KV_REST_API_URL,
    token: process.env.KV_REST_API_TOKEN,
  })
  return new Ratelimit({
    redis,
    limiter: Ratelimit.slidingWindow(REQUESTS, `${WINDOW_SECONDS}s`),
    prefix: 'qxrp_faucet',
  })
}

// --------------------------------------------------------------------------
// In-memory fallback (development / no Redis)
// --------------------------------------------------------------------------
const memStore = new Map<string, { count: number; resetAt: number }>()

function memCheck(key: string): { success: boolean; reset: Date } {
  const now = Date.now()
  const entry = memStore.get(key)
  if (!entry || now > entry.resetAt) {
    memStore.set(key, { count: 1, resetAt: now + WINDOW_SECONDS * 1000 })
    return { success: true, reset: new Date(now + WINDOW_SECONDS * 1000) }
  }
  if (entry.count < REQUESTS) {
    entry.count++
    return { success: true, reset: new Date(entry.resetAt) }
  }
  return { success: false, reset: new Date(entry.resetAt) }
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------
let limiter: Ratelimit | null = null

export interface LimitResult {
  success: boolean
  /** ISO string of when the rate limit resets */
  reset: string
}

export async function checkRateLimit(key: string): Promise<LimitResult> {
  if (!limiter) limiter = makeRedisLimiter()

  if (limiter) {
    const r = await limiter.limit(key)
    return { success: r.success, reset: new Date(r.reset).toISOString() }
  }

  // fallback
  const r = memCheck(key)
  return { success: r.success, reset: r.reset.toISOString() }
}
