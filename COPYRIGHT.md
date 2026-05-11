# qXRP Copyright And Licensing Strategy

Copyright (c) 2026 qXRP Team. All rights reserved.

This document explains how licensing is intended to work for qXRP while the project remains based on the upstream XRPLF/rippled codebase.

## Licensing Model

qXRP intentionally separates inherited upstream code from qXRP-original work.

- Upstream XRPL code remains under the original ISC license.
- qXRP-original files and qXRP-specific modifications are intended to follow the project protection policy in [LICENSE](LICENSE).
- The project intends to require explicit qXRP attribution headers on new files and meaningful modifications.

## Protection Strategy

The project's goal is to protect the first two years of qXRP-specific work from easy copycat forks while still preserving the ability to collaborate in public.

- During the first 2 years after first public release, qXRP-original work is intended to be distributed under AGPL-3.0 terms.
- After that period, the same material is intended to be made available under MIT terms.
- Upstream ISC code remains upstream ISC code regardless of this policy.

## File Header Expectations

New qXRP files and substantial qXRP-original modifications should carry a short notice that identifies:

- Copyright owner or team.
- Year of first publication.
- That the file is qXRP-original or qXRP-modified material.
- The applicable qXRP license policy for the protection period.

## Contributor Expectations

Anyone contributing qXRP-original work should understand that the repository may later distribute that work under the post-protection MIT policy described in [LICENSE](LICENSE).

Contributors should not submit code they cannot legally license under the project terms.

## Practical Review Rule

When reviewing a change, ask three questions:

1. Is the file inherited upstream code, qXRP-original code, or a mix of both?
2. Does the file header clearly state the correct attribution and policy?
3. Does the change respect the boundary between ISC upstream material and qXRP-original additions?

## Legal Note

This document is a project policy guide, not legal advice. Before a public release, the project should obtain formal review of the final license text and contributor process.
