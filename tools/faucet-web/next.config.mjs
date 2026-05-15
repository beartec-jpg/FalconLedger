/** @type {import('next').NextConfig} */
const nextConfig = {
  // Vercel deploys automatically — no extra config needed
  // xrpl uses Node.js crypto; force nodejs runtime (not edge)
  experimental: {
    serverComponentsExternalPackages: ['xrpl'],
  },
}

export default nextConfig
